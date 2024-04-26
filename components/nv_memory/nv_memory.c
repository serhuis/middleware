#include "nv_memory.h"
#include "nvs_flash.h"
#include "esp_log.h"

#if defined CONFIG_ZHAGA_NVS_ENABLE
#define STORAGE_NAMESPACE CONFIG_NVS_STORAGE_NAME
#endif

#define TAG __FILE__
esp_err_t NvMemory_init(void)
{
    // nvs_flash_erase();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
#if defined CONFIG_ZHAGA_NVS_ENABLE
    if (ESP_OK == ret)
    {
        nvs_handle_t my_handle;
        // MUST be with NVS_READWRITE permissions!!!  NVS_READONLY doesn't open!!!
        ret = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
        if (ESP_OK == ret)
        {
            ESP_LOGI(TAG, "%d, %s(): NV storage \"%s\" opened!", __LINE__, __FUNCTION__, STORAGE_NAMESPACE);
            nvs_close(my_handle);
        }
    }
#endif
    return ret;
}
#if defined CONFIG_ZHAGA_NVS_ENABLE
esp_err_t NVMemory_getBlob(char * const  key, uint8_t *const out_buffer, size_t len)
{

    nvs_handle_t my_handle;
    // MUST be with NVS_READWRITE permissions!!!  NVS_READONLY doesn't open!!!
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK)
    {
        size_t required_size = 0; // value will default to 0, if not set yet in NVS
        err = nvs_get_blob(my_handle, key, NULL, &required_size);
        if ((err == ESP_OK) && (required_size > 0) && !(required_size > len))
        {
            err = nvs_get_blob(my_handle, key, out_buffer, &required_size);
        }
        nvs_close(my_handle);
    }
    return err;
}
esp_err_t NVMemory_setBlob(char *const key, uint8_t *in_buffer, const size_t len)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    size_t required_size = 0; // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, key, NULL, &required_size);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND)
    {
        err = nvs_set_blob(my_handle, key, in_buffer, len);
        if (err == ESP_OK)
        {
            err = nvs_commit(my_handle);
        }
    }
    nvs_close(my_handle);
    ESP_LOGD("", "%s:%d, %s(): NV storage error: %x !", TAG, __LINE__, __FUNCTION__, err);
    return err;
}
#endif