#include "esp_log.h"
#include "string.h"
#include "coder_json.h"
#include "utils.h"
#include "cJSON.h"
#include "cJSON_Utils.h"

#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGBUF(data, len) ESP_LOG_BUFFER_HEXDUMP(__FILE__, data, len, ESP_LOG_INFO)

size_t coder_json_encode(uint8_t const *in_data, size_t in_len, uint8_t const *out_data, size_t out_len)
{
    LOGD("in_data:%x in_len:%d out_data:%x out_len%d", in_data, in_len, out_data, out_len);
    //LOGBUF(in_data, in_len);
    static uint8_t idx = 0;
    cJSON *arr = NULL;

    if (in_len < 3)
        return 0;

    char *pdata = malloc(in_len);
    bzero(pdata, in_len);
    memcpy(pdata, &in_data[3], in_len - 3);
    //LOGBUF(pdata, in_len);

    arr = cJSON_Parse((void *)out_data);
    if (NULL == arr)
    {
        arr = cJSON_CreateArray();
    }

    cJSON *item = cJSON_CreateObject();
    if (NULL == item)
        goto end;

    char str_id[5] = {'\0'};
    hex2str(&in_data[1], 2, str_id, sizeof(str_id));
    cJSON_AddItemToObject(item, "id", cJSON_CreateString(str_id));
    hex2str(&in_data[3], in_len - 3, pdata, sizeof(pdata));
    //memcpy(pdata, &in_data[3], in_len - 3);
    
    cJSON *item_value = cJSON_CreateString(pdata);

    cJSON_AddItemToObject(item, "value", item_value);
    LOGD("item_value: %s", cJSON_Print(item_value));
    cJSON_AddItemToArray(arr, item);
    char *js_str = cJSON_Print(arr);
    sprintf(out_data, "%s", cJSON_PrintUnformatted(arr));
    LOGD("Encoded JSON: %s, length: %d", out_data, strlen((void *)out_data));

end:
    free(pdata);
    cJSON_Delete(arr);
    return strlen((void *)out_data);
}
size_t coder_json_encodeBoot(uint8_t *const in_data, size_t in_len, uint8_t *const out_data, size_t out_len)
{
    LOGI();
    return 0;
}
size_t coder_json_decode(uint8_t *const in_data, size_t in_len, uint8_t *const out_data, size_t out_len)
{
    LOGD("in_data:%x in_len:%d out_data:%x out_len%d", in_data, in_len, out_data, out_len);
    cJSON *item = NULL;
    uint8_t *dest = out_data;
    static char *json_end = NULL;

    char *p = (NULL == json_end) ? in_data : ++json_end;

    while (p < &in_data[in_len])
    {

        item = cJSON_ParseWithOpts(p, &json_end, false);
        if (item && (cJSON_IsObject(item)))
        {
            char *str = cJSON_Print(item);
            LOGD("%s", str);
            cJSON_free(str);

            cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
            cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
            cJSON *value = cJSON_GetObjectItemCaseSensitive(item, "value");
            uint16_t attr_id = 0xAA55, value_len = strlen(value->valuestring);
            uint8_t * val = malloc(value_len);
            *dest++ = (1 + sizeof(attr_id) + value_len);
            memcpy(dest, &attr_id, 2);
            if (str2hex(id->valuestring, 4, &attr_id, 2))
            {
                LOGD("id string: %s -> %02hhx", id->valuestring, attr_id);
                dest = mempcpy(dest, &attr_id, sizeof(uint16_t));
                uint8_t l = str2hex(value->valuestring, value_len, val, value_len);
                dest = mempcpy(dest, val, value_len);
                //dest = mempcpy(dest, value->valuestring, value_len);
                LOGD("value string: %s -> %02hhx", value->valuestring, val);
                //LOGBUF(val, l);


            }
            free (val);
            cJSON_Delete(item);
            p = json_end;
            return (dest - out_data);
        }
        p++;
    }
    json_end = NULL;
    return 0;
}

size_t coder_json_decodeBoot(uint8_t *const in_data, size_t in_len, uint8_t *const out_data, size_t out_len)
{
    LOGD();
    return 0;
}