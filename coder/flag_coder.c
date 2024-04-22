/*
 * flag_coder.c
 *
 *  Created on: Apr 27, 2022
 *      Author: serh
 */

#include "flag_coder.h"

#include "esp_log.h"
#include "string.h"

#define HEADER_SIZE 0x02
#define FLAG_CHAR 0xFF
#define BOOT_CODE 0x70

// Logging
static const char *TAG = __FILE__;

#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGBUF(data, len) ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO)

extern void Flag_Init(encoder_t *encoder)
{
    if (encoder)
    {
        // encoder->decode = (void*)&Flag_search;
        encoder->decode = (void *)&Flag_getData;
        encoder->decodeBoot = (void *)&Flag_rmHeader;
        encoder->encode = (void *)&Flag_addFlags;
        encoder->encodeBoot = (void *)&Flag_addHeader;
    }
}

size_t Flag_addFlags(uint8_t const *in_data, size_t in_len, uint8_t *out_data, size_t out_len)
{
    LOGD("0x%p %d 0x%p %d", (void *)in_data, in_len, (void *)out_data, out_len);
    if (!(in_data && in_len))
        return;
    if (out_len < in_len + 2)
        return 0;
    size_t in_idx = 0, out_idx = 0;
    do
    {
        if (in_data[in_idx] < 3)
            ++in_idx;
        else
        {
            out_data[out_idx] = out_data[out_idx + in_data[in_idx] + 1] = FLAG_CHAR;
            memcpy(&out_data[out_idx + 1], &in_data[in_idx], in_data[in_idx]);
            out_idx += out_data[out_idx + 1] + 2;
            in_idx += in_data[in_idx];
        }
    } while ((in_idx < in_len) && (out_idx + 2 < out_len));
    LOGD("%p %d", (void *)out_data, out_idx);
//    LOGBUF(out_data, out_idx);
    return out_idx;
}

size_t Flag_rmFlags(uint8_t const *in_data, size_t in_len, uint8_t const *out_data, size_t out_len)
{
    if (out_len < in_len - 2)
        return 0;
    size_t in_idx = 0, out_idx = 0;

    do
    {
        if (in_data[in_idx] == FLAG_CHAR && in_data[in_data[in_idx + 1] + 1] == FLAG_CHAR)
        {
            memcpy(&out_data[out_idx], &in_data[in_idx + 1], in_data[in_idx + 1]);
            out_idx += out_data[0];
        }
    } while (++in_idx < in_len);
    return out_idx;
}

size_t Flag_addHeader(uint8_t const *in_data, size_t in_len, uint8_t *out_data, size_t out_len)
{
    if (out_len < in_len + 3)
        return 0;
    uint8_t *temp = malloc(out_len);

    LOGI("%p %d %p %d ", in_data, in_len, out_data, out_len);
    temp[0] = in_len + HEADER_SIZE;
    temp[1] = BOOT_CODE;
    memcpy(&temp[HEADER_SIZE], in_data, in_len);
    memcpy(out_data, temp, temp[0]);
    free(temp);
    LOGBUF(out_data, out_data[0]);
    return out_data[0];
}

size_t Flag_rmHeader(uint8_t const *in_data, size_t in_len, uint8_t const *out_data, size_t out_len)
{
    LOGI();
    if (in_len < HEADER_SIZE || out_len < in_len - HEADER_SIZE)
        return 0;

    memcpy(out_data, &in_data[HEADER_SIZE], in_len - HEADER_SIZE);
    return (in_len - HEADER_SIZE);
}

size_t Flag_getData(uint8_t const *in_data, size_t in_len, uint8_t const *out_data, size_t out_len)
{
    if (out_len < in_len - 2)
        return 0;
    size_t in_idx = 0;
    do
    {
        if (in_data[in_idx] == FLAG_CHAR && in_data[in_data[in_idx + 1] + 1] == FLAG_CHAR)
        {
            memcpy(out_data, &in_data[in_idx + 1], in_data[in_idx + 1]);
            return out_data[0] + 2;
        }
    } while (++in_idx < in_len);
    return 0;
}

size_t Flag_getDataLength(uint8_t const *in_data, size_t in_len)
{
    size_t in_idx = 0;
    LOGD("in_data[in_idx+1]: %x, ", in_data[in_idx + 1]);
    //LOGBUF(in_data, in_len);
    do
    {
        if (in_data[in_idx++] == FLAG_CHAR && in_data[in_data[in_idx] + 1] == FLAG_CHAR)
        {
            if ((in_data[in_idx] >= 3))
                return in_data[in_idx];
        }
    } while (in_idx < in_len);
    return 0;
}

size_t Flag_search(uint8_t const *in_data, size_t in_len, uint8_t *out_data, size_t out_len)
{
    if ((out_len) < in_len - 2)
        return 0;

    size_t in_idx = 0;
    out_data = &in_data[in_idx + 1];
    out_len = *out_data;
    LOGD("%p %p", (void *)in_data, (void *)*in_data);
    do
    {
        if (in_data[in_idx] == FLAG_CHAR && in_data[in_data[in_idx + 1] + 1] == FLAG_CHAR)
        {
            out_data = &in_data[in_idx + 1];
            out_len = in_data[in_idx + 1];
            LOGD("%p %p", (void *)out_data, *out_data);
            return out_len;
        }
    } while (++in_idx < in_len);
    return 0;
}
