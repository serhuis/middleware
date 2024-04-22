#pragma once

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_http_server.h"

#if defined CONFIG_ESP_HTTPS_SERVER_ENABLE
#include "esp_https_server.h"
#include "esp_tls.h"
#endif

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "zhaga_central.h"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (1000*1024) // 200 KB
#define MAX_FILE_SIZE_STR "1000 KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  4096


#define HTTP_LOGI(tag, format, ...)	ESP_LOGI(tag, "%d: %s: " format, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#define HTTP_LOGD(tag, format, ...)	ESP_LOGD(tag, "%d: %s: " format, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#define HTTP_LOGW(tag, format, ...)	ESP_LOGW(tag, "%d: %s: " format, __LINE__, __FUNCTION__, ## __VA_ARGS__)
#define HTTP_LOGE(tag, format, ...)	ESP_LOGE(tag, "%d: %s: " format, __LINE__, __FUNCTION__, ## __VA_ARGS__)

struct Cmd
{
    char *cmd;
    esp_err_t (*fn)(void *arg);
};

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

extern esp_err_t Http_server_start(const char *base_path);
extern esp_err_t Https_server_start(const char *base_path);

// Files operations handlers
extern const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize);
extern esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename);

extern esp_err_t download_get_handler(httpd_req_t *req);
extern esp_err_t upload_post_handler(httpd_req_t *req);
extern esp_err_t delete_post_handler(httpd_req_t *req);

// Firmware update handlers
extern esp_err_t update_get_handler(httpd_req_t *req);
extern esp_err_t update_post_handler(httpd_req_t *req);

// WiFi config handlers
extern esp_err_t config_get_handler(httpd_req_t *req);
extern esp_err_t config_post_handler(httpd_req_t *req);

// BLE config handlers
extern esp_err_t btconfig_get_handler(httpd_req_t *req);
extern esp_err_t btconfig_post_handler(httpd_req_t *req);

// DALI
esp_err_t dali_get_handler(httpd_req_t *req);
//esp_err_t dali_post_handler(httpd_req_t *req);

extern size_t http_cmd_from_uri(const char *uri, char *buf);
extern char * http_get_value_string(void *uri, uint8_t *len);

extern size_t str2hex(char * str, const size_t str_len, uint8_t *arr, size_t arr_len);
size_t hex2str(uint8_t * const arr, const size_t arr_len, char *str, const size_t str_len);