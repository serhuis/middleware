#include "luna_ota.h"
#include "esp_flash_partitions.h"
#include "esp_log.h"
// #include "esp_ota_ops.h"
// #include "esp_partition.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdio.h>
#include <string.h>

#define FILENAME_FILTER "luna_app"

#define OAD_HDR_SIZE (0xa8)
#define MAX_BLOCK_SIZE (240)
#define PADDING (8)
#define MAX_TX_RETRY (3)
#define ACK (0xcc)
#define NACK (0x33)

#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_BUF(buf, len) ESP_LOG_BUFFER_HEXDUMP(__FILE__, buf, len, ESP_LOG_INFO)

#define LUNA_OTA_EVENT(x) (1 << (x))

typedef struct __attribute__((packed))
{
    uint32_t length;     // firmware length in bytes (including padding)
    uint8_t blockSize;   // block size (last block can be of less size)
    uint8_t blocksTotal; // total blocks of
    uint8_t padding;     // bytes to align to 4-x border
} ota_config_t;

typedef enum
{
    OAD_IDLE = 0, //!< OAD module is not performing any action
    OAD_CHECK_MODE,
    OAD_CONFIG, //!< OAD module is configuring for a download
    OAD_ERASE,
    OAD_DOWNLOAD, //!< OAD module is receiving an image
    OAD_CHECK_DL, //!< OAD module is validating an received image
    OAD_COMPLETE, ////!< OAD module has completed a download
    OAD_ERROR,
} oadStage_t;

typedef struct
{
    size_t length;
    uint8_t data[MAX_BLOCK_SIZE + 2];
} luna_ota_buffer_t;

typedef struct
{
    bool _img_header_checked;
    oadStage_t _stage;
    int _sent_num;
    uint8_t _retry;
} luna_ota_status_t;

typedef struct
{
    char file_name[32];
    char version[8];
} luna_ota_info_t;

static const char _magic_string[] = "ZhagaService";

// OTA file stuffs
static volatile char *_base_path = NULL;
static luna_ota_info_t _luna_ota_info = {0};

// OTA process staffs
static const char *oadStageStr[] = {
    "OAD_STAGE_IDLE",
    "OAD_STAGE_CHECK_MODE",
    "OAD_STAGE_CONFIG",
    "OAD_STAGE_ERASE",
    "OAD_STAGE_DOWNLOAD",
    "OAD_STAGE_CHECK_DL",
    "OAD_STAGE_COMPLETE",
    "OAD_ERROR"};
static ota_config_t _ota_config = {0};
static volatile luna_ota_buffer_t _send_buf;
static luna_ota_status_t _ota_status = {0};

//  freeRTOS stuffs
static TaskHandle_t xLunaOtaTask = NULL;
static TimerHandle_t _xtimer;
static StaticTimer_t _xTimerBuffer;

// Main OTA task
static void luna_ota_task(void *pvParameters);

// OTA process functions
static esp_err_t luna_ota_get_info(char *const filename);
static void luna_ota_stage_update(void);
static void luna_ota_load_cmd(const uint8_t cmd);
static size_t luna_ota_load_data(const uint8_t cmd, uint8_t *data, const size_t length);
static size_t luna_ota_load_block(void);
static uint8_t luna_ota_lrc(uint8_t *const buf, size_t buflen);
static size_t luna_ota_send_all(void);

// timeot callback on no response state
static void luna_ota_on_timeout(void);

// callback to send via BLE or other transport
static size_t (*_luna_ota_send_fn)(uint8_t *const data, const size_t length) = NULL;
static size_t (*_luna_ota_rcv_fn)(uint8_t *const data, const size_t length) = NULL;
static size_t (*_luna_ota_reset_fn)(const uint8_t flag) = NULL;
static void vTimerCallback(TimerHandle_t xTimer);

esp_err_t luna_ota_init(char * const base_path,
                        size_t (*send_fn)(uint8_t *const data, const size_t length),
                        size_t (*rcv_fn)(uint8_t *const data, const size_t length),
                        size_t (*reset_fn)(const uint8_t flag))
{
    _base_path = base_path;
    _luna_ota_send_fn = send_fn;
    _luna_ota_rcv_fn = rcv_fn;
    _luna_ota_reset_fn = reset_fn;
    LOGI("base_path: %s, _luna_ota_send_fn: %x, _luna_ota_reset_fn: %x", _base_path, _luna_ota_send_fn, _luna_ota_reset_fn);
    return ESP_OK;
}

esp_err_t luna_ota_start(char *const fullpath)
{
    LOGI("%s", fullpath);
    LOG_BUF(_luna_ota_info.file_name, sizeof(_luna_ota_info.file_name));
    LOGI("%s", _luna_ota_info.file_name);

    if (NULL == strstr(fullpath, _luna_ota_info.file_name))
    {
        LOGE("%s is not a  Luna OTA image file!: %s", fullpath);
        return ESP_FAIL;
    }
    if (ESP_OK != luna_ota_get_info((char *const)fullpath))
    {
        LOGE("OTA image error!! Upload new firmware file!");
        return ESP_FAIL;
    }

    if (NULL == _luna_ota_reset_fn)
    {
        LOGE("Error resetting device!");
        return ESP_FAIL;
    }

    bzero(&_ota_status, sizeof(_ota_status));

    if (_luna_ota_reset_fn)
    {
        LOGI("Resetting device!");
        _luna_ota_reset_fn(1);
    };
    xTaskCreate(luna_ota_task, "luna_ota_task", 2048, NULL, 5, &xLunaOtaTask);
    _xtimer = xTimerCreateStatic("Timer", pdMS_TO_TICKS(100), pdFALSE, (void *)0, vTimerCallback, &_xTimerBuffer);

    xTaskNotifyGive(xLunaOtaTask);

    return ESP_OK;
}

esp_err_t luna_ota_abort(esp_err_t error)
{

    if (error != 0)
        LOGE("Exiting task due to error: %x", error);

    if (NULL != _luna_ota_reset_fn)
        _luna_ota_reset_fn(0);
    LOGI("%x", error);
    bzero(&_ota_status, sizeof(_ota_status));
    xTimerStop(_xtimer, 0);
    xTimerDelete(_xtimer, 0);
    vTaskDelete(xLunaOtaTask);
    return error;
}

static void vTimerCallback(TimerHandle_t xTimer)
{
    // uint8_t return_value[] = {ACK};
    //  luna_ota_rcv_evt(return_value, 1);
    luna_ota_send_all();
}

void luna_ota_rcv_evt(const size_t length)
{
    LOGD("%d", length);
    uint8_t rx_buf[16] = {NACK};
    if (_luna_ota_rcv_fn != NULL)
    {
        _luna_ota_rcv_fn(rx_buf, length);
    }
    if (rx_buf[0] != ACK)
    {
        _ota_status._stage = OAD_ERROR;
        //        luna_ota_abort(rx_buf[0]);
        //        return;
    }
    xTaskNotifyGive(xLunaOtaTask);
}
static volatile imgHdr_t fw_header;
esp_err_t luna_ota_get_info(char *const fullpath)
{
    struct stat file_stat;

    if (stat(fullpath, &file_stat) == -1)
    {
        LOGE("Failed to stat file : %s", fullpath);
        return ESP_FAIL;
    }
    _ota_config.length = file_stat.st_size;

    FILE *fd = fopen(fullpath, "r");
    if (!fd)
    {
        LOGE("Failed to open file : %s", fullpath);
        return ESP_FAIL;
    }

    size_t read_size = fread(&fw_header, 1, OAD_HDR_SIZE, fd);
    if (read_size <= 0)
    {
        LOGE("ERROR reading file %d, %s", read_size, fullpath);
        fclose(fd);
        return ESP_FAIL;
    }
    fclose(fd);

    LOGD("Image ID: %s", &fw_header.fixedHdr.imgID);
    LOGD("crc32: 0x%04x", fw_header.fixedHdr.crc32);
    LOGD("bimVer: %d", fw_header.fixedHdr.bimVer);
    LOGD("crc status: 0x%02x", fw_header.fixedHdr.crcStat);
    sprintf(_luna_ota_info.version, "%c.%c.%c", fw_header.fixedHdr.softVer[0], fw_header.fixedHdr.softVer[1], fw_header.fixedHdr.softVer[2]);
    LOGD("Version: %s", _luna_ota_info.version);

    _ota_config.blockSize = MAX_BLOCK_SIZE;
    _ota_config.blocksTotal = _ota_config.length / MAX_BLOCK_SIZE;
    _ota_config.blocksTotal += (_ota_config.length % MAX_BLOCK_SIZE) ? 1 : 0;
    _ota_config.padding = _ota_config.length % PADDING;
    _ota_status._img_header_checked = true;
    return ESP_OK;
}

void luna_ota_clear_info(char *const filename)
{
    if (strstr(filename, FILENAME_FILTER))
    {
        unlink(filename);
        bzero(&_ota_status, sizeof(_ota_status));
        bzero(&_ota_config, sizeof(_ota_config));
        bzero(&_luna_ota_info, sizeof(_luna_ota_info));
    }
}

static void luna_ota_task(void *pvParameters)
{
    LOGI();
    static bool autoconnect;
    for (;;)
    {
        int ret = ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(5000));
        if (0 == ret)
        {
            LOGE("Timeout elapsed: %d", ret);
            luna_ota_on_timeout();
        }
        else
        {
            luna_ota_stage_update();
            if (_ota_status._stage == OAD_CHECK_MODE)
                luna_ota_load_data(OAD_CHECK_MODE, (uint8_t *)_magic_string, strlen(_magic_string));
            if (_ota_status._stage == OAD_CONFIG)
                luna_ota_load_data(_ota_status._stage, (uint8_t *)&_ota_config, sizeof(_ota_config));
            if (_ota_status._stage == OAD_ERASE)
            {
                uint8_t start_addr[] = {0};
                luna_ota_load_data(_ota_status._stage, &start_addr, sizeof(start_addr));
            }
            if (_ota_status._stage == OAD_DOWNLOAD)
                if (luna_ota_load_block() == 0)
                    _ota_status._stage = OAD_CHECK_DL;
            if (_ota_status._stage == OAD_CHECK_DL)
                luna_ota_load_cmd(_ota_status._stage);
            if (_ota_status._stage >= OAD_COMPLETE)
            {
                luna_ota_abort(0);
                vTaskDelete(NULL);
            }
            else
                xTimerStart(_xtimer, 0);
        }
    }
}

static size_t luna_ota_send_all(void)
{
    if (_luna_ota_send_fn)
        return _luna_ota_send_fn(_send_buf.data, _send_buf.length);
    return 0;
}

static void luna_ota_stage_update(void)
{
    _ota_status._retry = 0;
    if ((_ota_status._stage == OAD_DOWNLOAD) && (_ota_status._sent_num < _ota_config.length))
        return;
    ++_ota_status._stage;
    LOGD("Stage: %s", oadStageStr[_ota_status._stage]);
}

static size_t luna_ota_load_block(void)
{
    char fname[32] = "\0";
    sprintf(fname, "%s/%s", _base_path, _luna_ota_info.file_name);
    LOGD("%s", fname);
    FILE *fp = fopen(fname, "r");

    if (!fp)
    {
        LOGE("Failed to read file : %s", fname);
        return 0;
    }
    size_t read_size = 0;
    if (ESP_OK == fseek(fp, _ota_status._sent_num, SEEK_SET))
    {
        bzero(&_send_buf, sizeof(_send_buf));
        _send_buf.data[0] = OAD_DOWNLOAD;
        read_size = fread(&_send_buf.data[1], 1, MAX_BLOCK_SIZE, fp);
        if (read_size > 0)
        {
            LOGD("Read: %d, ofset: %d", read_size, _ota_status._sent_num);
        }

        _send_buf.data[read_size + 1] = luna_ota_lrc(&_send_buf.data[1], read_size);
        _ota_status._sent_num += read_size;
        _send_buf.length = read_size + 2;
        LOGD("Progress: %d/%d", _ota_status._sent_num, _ota_config.length);
        // LOG_BUF(_send_buf.data, _send_buf.length);
    }
    fclose(fp);
    return _send_buf.length;
}

static void luna_ota_on_timeout(void)
{
    LOGW("Retry sending; %d", _ota_status._retry);
    luna_ota_send_all();
    if (++_ota_status._retry >= MAX_TX_RETRY)
    {
        luna_ota_abort(0);
    }
}

static void luna_ota_load_cmd(uint8_t cmd)
{
    LOGI();
    bzero(&_send_buf, sizeof(_send_buf));
    _send_buf.data[0] = cmd;
    _send_buf.data[1] = luna_ota_lrc(&_send_buf.data[1], 1);
    _send_buf.length = 2;
    LOG_BUF(&_send_buf.data, 2);
}

static size_t luna_ota_load_data(const uint8_t cmd, uint8_t *const data, const size_t length)
{
    LOGD();
    if ((!data) || (length <= 0) || length > (sizeof(_send_buf.data) + 2))
        return 0;

    bzero(&_send_buf, sizeof(_send_buf));
    _send_buf.data[0] = cmd;
    memcpy(&_send_buf.data[1], data, length);
    _send_buf.data[length + 1] = luna_ota_lrc(data, length);
    _send_buf.length = length + 2;
    return (_send_buf.length);
}

static uint8_t luna_ota_lrc(uint8_t *const buf, size_t buflen)
{
    uint8_t lrc = 55;
    for (int i = 0; i < buflen; i++)
        lrc += buf[i];
    LOGD("buf: %02x, length: %d, lrc: %02x", buf, buflen, lrc);
    return lrc;
}

extern bool luna_ota_getUpdateAvailable(void)
{

    char entrypath[64];
    struct dirent *entry = NULL;
    struct stat entry_stat;
    sprintf(entrypath, "%s/", _base_path);
    const size_t dirpath_len = strlen(entrypath);

    if (_ota_status._img_header_checked)
        return true;

    bool available = false;
    DIR *dir = opendir(entrypath);
    if (!dir)
    {
        LOGE("Failed to stat dir : %s", entrypath);
        return false;
    }

    do
    {
        entry = readdir(dir);
        if (entry && (ESP_OK == strncmp(entry->d_name, FILENAME_FILTER, strlen(FILENAME_FILTER))))
        {

            strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
            if (stat(entrypath, &entry_stat) == -1)
            {
                LOGE("Failed to stat: %s", entrypath);
                continue;
            }
            LOGD("Found %s : (%d bytes)", entry->d_name, entry_stat.st_size);

            if (!_ota_status._img_header_checked)
            {
                bzero(&_luna_ota_info, sizeof(_luna_ota_info));
                if (strlcpy(_luna_ota_info.file_name, entry->d_name, sizeof(_luna_ota_info.file_name)))
                {
                    luna_ota_get_info(entrypath);
                    available = true;
                }
            }
        }
    } while (entry != NULL);
    closedir(dir);
    return available;
}

const char *luna_ota_getUpdateVersion(void)
{

    return _luna_ota_info.version;
}

const char *luna_ota_getUpdateFname(void)
{
    return _luna_ota_info.file_name;
}

int luna_ota_get_progress(void)
{
    if (OAD_IDLE < _ota_status._stage)
        return (_ota_status._stage == OAD_DOWNLOAD) ? _ota_status._sent_num : 1;
    return 0;
}

char * luna_ota_get_status(void)
{
    if(OAD_DOWNLOAD == _ota_status._stage) return "Loading";
    if(OAD_CHECK_MODE == _ota_status._stage) return "Mode check";
    if(OAD_ERASE == _ota_status._stage) return "Erasing";
    if(OAD_CHECK_DL == _ota_status._stage) return "Verifying";
    if(OAD_COMPLETE == _ota_status._stage) return "Complete!";
    return "Idle";
}

int luna_ota_get_fwLength(void)
{
    return _ota_config.length;
}