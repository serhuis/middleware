#include "esp_ota.h"
#include "errno.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
//#include "esp_flash_partitions.h"
#include "esp_app_format.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define BUFFSIZE 4096
#define HASH_LEN 32 /* SHA-256 digest length */

#define FILENAME_FILTER "ZhagaEsp"

typedef enum
{
    FAILED = -1,
    IDLE = 0,
    STARTED,
    IN_PROGRESS,
    FINISHED,
    RESTARTING
} ota_status_t;

static const char *TAG = __FILE__;

static char *_base_path = NULL;
static char _otaFwFilename[32] = "\0";
static ota_status_t _ota_status = IDLE;
static int _updated_size = 0;
static size_t _fw_bin_size = 0;
static esp_ota_handle_t _update_handle = 0;
static esp_partition_t *running = NULL;
static esp_partition_t *update_partition = NULL;
static esp_app_desc_t running_app_info;
static bool _img_header_checked = false;
static TaskHandle_t _otaTaskHandle = NULL;

// static const char *_version = PROJECT_VER;

static esp_app_desc_t _update_info = {0};

static void
print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i)
    {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s: %s", label, hash_print);
}

static void ESP_ota_task(void *pvParameter);

extern void
Esp_Ota_init(char *const base_path)
{
    esp_err_t err;

    _base_path = base_path;
    running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        ESP_LOGD(TAG, "Running partition state: %d", ota_state);
        if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
        {
            ESP_LOGD(TAG, "Project name: %s", running_app_info.project_name);
            ESP_LOGD(TAG, "Build time: %s %s", running_app_info.date, running_app_info.time);
            ESP_LOGD(TAG, "Running firmware version: %s", running_app_info.version);
            ESP_LOGD(TAG, "IDF version: %s", running_app_info.idf_ver);
        }
    }

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    update_partition = esp_ota_get_next_update_partition(NULL);

//    assert(update_partition != NULL);
    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)", running->type, running->subtype, running->address);
    ESP_LOGI(TAG, "Update partition type %d subtype %d (offset 0x%08x)", update_partition->type, update_partition->subtype, update_partition->address);
    ///    ESP_LOGI(TAG, "Got Version %s", _version);
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1)
        ;
}

static void ESP_ota_error_log(esp_err_t err)
{
    if (ESP_OK == err)
        return;
    ESP_LOGE(TAG, "ESP_ota_error_log! ERROR: (%d: %s)", err, esp_err_to_name(err));
    esp_ota_abort(_update_handle);
    _ota_status = FAILED;
    task_fatal_error();
}

static char fw_buffer[1024];

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
extern esp_err_t
ESP_ota_start_update(char *const filename)
{
    FILE *fd = NULL;
    struct stat file_stat;

    if (NULL == strstr(filename, _otaFwFilename)){
        ESP_LOGE("", "%s:%d: No ESP OTA image file!: %s", __FILE__, __LINE__, filename);
        return ESP_FAIL;
    }


    char filepath[FILE_PATH_MAX];
    sprintf("%s/%s", _base_path, filename);

    if (stat(_otaFwFilename, &file_stat) == -1)
    {
        ESP_LOGE(__FILE__, "Failed to stat file : %s", _otaFwFilename);
        return ESP_FAIL;
    }
    fd = fopen(_otaFwFilename, "r");
    if (!fd)
    {
        ESP_LOGE(__FILE__, "Failed to read file : %s", _otaFwFilename);
        return ESP_FAIL;
    }
    fclose(fd);
    _img_header_checked = false;
    _updated_size = 0;
    _fw_bin_size = file_stat.st_size;
    ESP_LOGI(__FILE__, "Found file : %s (%d bytes)", _otaFwFilename, _fw_bin_size);
    xTaskCreate(ESP_ota_task, "ESP_ota_task", 4096, _otaFwFilename, 5, _otaTaskHandle);
    return ESP_OK;
}

static void
ESP_ota_task(void *pvParameter)
{
    esp_err_t err;
    FILE *fd = fopen(_otaFwFilename, "r");
    size_t read_size;
    size_t app_info_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    do
    {
        read_size = fread(fw_buffer, 1, sizeof(fw_buffer), fd);
        if (read_size <= 0)
            break;

        if (!_img_header_checked)
        {
            esp_app_desc_t new_app_info;
            if (read_size > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
            {
                memcpy(&new_app_info, &fw_buffer[app_info_offset], sizeof(esp_app_desc_t));
                ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
                esp_app_desc_t invalid_app_info;
                if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
                }

                if (last_invalid_app != NULL)
                {
                    if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
                    {
                        ESP_LOGW(TAG, "New version is the same as invalid version.");
                        ESP_LOGW(TAG, "Previously, there was an attempt to launch the firmware with %s version, but it failed.",
                                 invalid_app_info.version);
                        ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
                    }
                }
                /*
                                if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
                                {
                                    ESP_LOGW(TAG, "Current running version is the same as a new");
                                }
                */
                _img_header_checked = true;
                ESP_ota_error_log(esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &_update_handle));
                ESP_LOGI(TAG, "esp_ota_begin succeeded");
            }
            else
                ESP_ota_error_log(ESP_ERR_INVALID_SIZE);
        }
        ESP_ota_error_log(esp_ota_write(_update_handle, (const void *)fw_buffer, read_size));

        _updated_size += read_size;
        // ESP_LOGI(TAG, "Updated %d of %d", _updated_size, _fw_bin_size);
        _ota_status = IN_PROGRESS;
        vTaskDelay(pdMS_TO_TICKS(10));
    } while (read_size != 0);
    fclose(fd);

    ESP_LOGW(TAG, "Total Write binary data length: %d", _updated_size);
    err = esp_ota_end(_update_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
        {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        ESP_ota_error_log(err);
    }
    _ota_status = FINISHED;
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "$d esp_ota_set_boot_partition failed!", __LINE__);
        ESP_ota_error_log(err);
    }
    ESP_LOGI(TAG, "Prepare to restart system!");
    _ota_status = RESTARTING;
    vTaskDelay(pdMS_TO_TICKS(1100));
    esp_restart();
    vTaskDelete(NULL);
    _img_header_checked = false;
}

extern char *Esp_Ota_getName(void)
{
    return running_app_info.project_name;
}

extern char *Esp_Ota_getBuildDate(void)
{
    return running_app_info.date;
}

extern char *Esp_Ota_getBuildTime(void)
{
    return running_app_info.time;
}

extern char *Esp_Ota_getVersion(void)
{
    return running_app_info.version;
}

extern char *Esp_Ota_getIdfVersion(void)
{
    return running_app_info.idf_ver;
}

extern int Esp_Ota_getStatus(void)
{
    return _ota_status;
}

extern int Esp_Ota_getProgress(void)
{
    return _updated_size;
}

extern int Esp_Ota_getFwLength(void)
{
    return _fw_bin_size;
}

extern bool Esp_Ota_getUpdateAvailable()
{

    char entrypath[64];
    struct dirent *entry;
    struct stat entry_stat;
    sprintf(entrypath, "%s/", _base_path);
    const size_t dirpath_len = strlen(entrypath);

    DIR *dir = opendir(_base_path);
    if (!dir)
    {
        ESP_LOGE(__FILE__, "Failed to stat dir : %s", entrypath);
        return false;
    }
    //// Iterate over all files / folders and fetch their names and sizes
    do
    {
        entry = readdir(dir);
        if (entry && (ESP_OK == strncmp(entry->d_name, FILENAME_FILTER, strlen(FILENAME_FILTER))))
        {

            strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
            ESP_LOGD("", "%s:%d: %s %s ", __FILE__, __LINE__, _base_path, entrypath);
            if (stat(entrypath, &entry_stat) == -1)
            {
                ESP_LOGE(__FILE__, "Failed to stat: %s", entrypath);
                ///_update_info = {0};
                _img_header_checked = false;
                continue;
            }
            ESP_LOGD("", "%s:%d:  Found %s : (%d bytes)", __FILE__, __LINE__, entry->d_name, entry_stat.st_size);

            if (!_img_header_checked)
            {
                FILE *fd = fopen(entrypath, "r");
                if (!fd)
                {
                    ESP_LOGE("", "%s:%d: Failed to read file : %s", __FILE__, __LINE__, entrypath);
                    _img_header_checked = false;
                    ///_update_info = {.project_name = "", .version = "", .date = "", .time = ""};
                    continue;
                }
                bzero(_otaFwFilename, sizeof(_otaFwFilename));
                strlcpy(_otaFwFilename, entry->d_name, sizeof(_otaFwFilename));
                ESP_LOGD("", "%s:%d %s", __FILE__, __LINE__, _otaFwFilename);
                if (fread(fw_buffer, 1, sizeof(fw_buffer), fd) > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
                {
                    memcpy(&_update_info, &fw_buffer[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    _img_header_checked = true;
                    ESP_LOGD(TAG, "Update project_name: %s", _update_info.project_name);
                    ESP_LOGD(TAG, "Update version: %s", _update_info.version);
                    ESP_LOGD(TAG, "Update date: %s", _update_info.date);
                    ESP_LOGD(TAG, "Update time: %s", _update_info.time);
                }
                fclose(fd);
                break;
            }
        }
    } while (entry != NULL);
    closedir(dir);
    return _img_header_checked;
}

extern char *Esp_Ota_getUpdateFname(void)
{
    return _otaFwFilename;
}

extern char *Esp_Ota_getUpdateVersion(void)
{
    return _update_info.version;
}
extern char *Esp_Ota_getUpdateDate(void)
{
    return _update_info.date;
}
extern char *Esp_Ota_getUpdateTime(void)
{
    return _update_info.time;
}
extern char *Esp_Ota_getUpdateIdfVersion(void)
{
    return _update_info.idf_ver;
}

void Esp_Ota_clearUpdateInfo(char *const filename)
{
    if (strstr(filename, FILENAME_FILTER))
    {
        unlink(filename);
        _img_header_checked = false;
        bzero(fw_buffer, sizeof(fw_buffer));
        bzero(_otaFwFilename, sizeof(_otaFwFilename));
        bzero(&_update_info, sizeof(_update_info));
    }
}