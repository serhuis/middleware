/*
 * fnEsp.c
 *
 *  Created on: May 13, 2022
 *      Author: serh
 */
#include "fnEsp.h"
#include "dcdEsp.h"
#include "esp_log.h"
#include "stdarg.h"
#include "string.h"

#include "wifi.h"
#include "drvGpio.h"
#include "board.h"
#include "esp_ota.h"

static const char *TAG = __FILE__;
#define espLogI(format, ...) ESP_LOGI(TAG, "%d: %s: " format, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogD(format, ...) ESP_LOGD(TAG, "%d: %s: " format, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogW(format, ...) ESP_LOGW(TAG, "%d: %s: " format, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogE(format, ...) ESP_LOGE(TAG, "%d: %s: " format, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogHex(data, len) ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO)

struct args
{
    uint8_t *in_buf;
    size_t in_length;
    uint8_t *out_buf;
    size_t out_length;
} arrr;
/*
static void FnEsp_getArgs(struct args *aaa, int type, va_list l)
{

    va_list list;
    //  va_start (list, buf);
    va_copy(list, l);

//        aaa->in_buf = *(uint8_t *)list;
//        aaa->in_length =  va_arg(list, size_t);
//        aaa->out_buf =  va_arg(list, uint8_t *);
//        aaa->out_length =  va_arg(list, size_t);

    espLogI("%d %0.2x %0.2x %0.2x %0.2x", type, *(uint8_t *)list, va_arg(list, size_t), va_arg(list, size_t), va_arg(list, size_t));
    // va_end(list);
    espLogI("%d %0.2x %0.2x %0.2x %0.2x", type, aaa->in_buf, aaa->in_length, aaa->out_buf, aaa->out_length);
}
*/
extern int
FnEsp_getVersion(uint8_t *buf, ...)
{
    va_list list;
    char *ver = Esp_Ota_getVersion();
    size_t ver_len = strlen(ver);

    va_start(list, buf);
    size_t len = va_arg(list, size_t);
    va_end(list);

    if (len < ver_len)
        return -1;

    memcpy(buf, ver, ver_len);
    ESP_LOGI("", "%s:%d %s(): %s", __FILE__, __LINE__, __FUNCTION__, ver);
    return ver_len;
}

//****************	GPIO API		****************
extern int
FnEsp_getPinState(uint8_t *in_buf, ...)
{

    struct args ar;
    va_list list;
    va_start(list, in_buf);

    ar.in_buf = in_buf;
    ar.in_length = va_arg(list, size_t);
    ar.out_buf = va_arg(list, uint8_t *);
    ar.out_length = va_arg(list, size_t);

    va_end(list);

    if (ar.in_buf && ar.out_buf && ar.in_length && ar.out_length >= 0)
    {
        int idx = 0;
        ar.out_buf[idx++] = *ar.in_buf;
        ar.out_buf[idx++] = Gpio_getLevel(*ar.in_buf);
        espLogI("%0.2x %0.2x", *ar.in_buf, ar.out_buf[idx - 1]);
        return idx;
    }
    return ESP_FAIL;
}

extern uint32_t
FnEsp_setPinState(uint8_t const *buf, ...)
{
    uint8_t pin_num = *buf++;
    uint8_t pin_state = *buf;

    __uint32_t ret = 0;
    if (CONFIG_STM_BOOT0_PIN_ESP8266 == pin_num)
        ret |= 0x80000000 | (pin_state << 30);

    ret += Gpio_setLevel(pin_num, pin_state);
    ESP_LOGI(__FILE__, "%d: %d: %d: %0.2x", __LINE__, pin_num, pin_state, ret);
    return (ret);
}

extern int
FnEsp_getPinMode(uint8_t *buf, ...)
{
    return ESP_FAIL;
}

extern int
FnEsp_setPinMode(uint8_t const *buf, ...)
{
    uint8_t pin_num = *buf++;
    uint8_t pin_dir = *buf;

    return Gpio_setDirection(pin_num, pin_dir);
}

//****************	AP SETTINGS		****************
extern int FnEsp_getApSsid(int arg, ...)
{
    va_list list;
    // buffer for response
    va_start(list, arg);
    // buffer length for response
    uint8_t *buf = (uint8_t *)arg;
    size_t len = va_arg(list, size_t);
    va_end(list);
    wifi_config_t cfg;
    //  ESP_LOGI(__FILE__, "%d: %d: %d: %0.2x", __LINE__, pin_num, pin_state, ret);
    if (len < sizeof(cfg.ap.ssid))
        return ESP_FAIL;

    if (ESP_OK == wifi_ap_get_config(&cfg))
    {
        strcpy((char *)buf, (char *)cfg.ap.ssid);
        return strlen((char *)buf);
    }
    return ESP_FAIL;
}
extern int FnEsp_setApSsid(uint8_t const *buf, ...)
{
    va_list list;
    // buffer for response
    va_start(list, buf);
    // buffer length for response
    size_t len = va_arg(list, size_t);
    va_end(list);
    wifi_config_t cfg;
    if (len < sizeof(cfg.ap.ssid))
    {
        wifi_ap_get_config(&cfg);
        bzero(cfg.ap.ssid, sizeof(cfg.ap.ssid));
        strcpy((char *)&cfg.ap.ssid, (void *)buf);
        cfg.ap.ssid_len = strlen((char *)cfg.ap.ssid);
        return wifi_ap_set_config(&cfg);
        ;
    }
    return ESP_FAIL;
}
extern int FnEsp_getApHidden(uint8_t *buf, ...)
{
    va_list list;
    // buffer for response
    va_start(list, buf);
    // buffer length for response
    // size_t len = va_arg(list, size_t);
    va_end(list);

    wifi_config_t cfg;
    wifi_ap_get_config(&cfg);
    *buf = cfg.ap.ssid_hidden;
    espLogI("Hidden:%d", cfg.ap.ssid_hidden);
    return sizeof(cfg.ap.ssid_hidden);
}
extern int FnEsp_setApHidden(uint8_t const *buf, ...)
{
    espLogI("%d", *buf);
    wifi_config_t cfg;
    wifi_ap_get_config(&cfg);
    cfg.ap.ssid_hidden = *buf;
    espLogI("%d", cfg.ap.ssid_hidden);
    return wifi_ap_set_config(&cfg);
}

extern int FnEsp_getApPass(uint8_t *buf, ...)
{
    va_list list;
    // buffer for response
    va_start(list, buf);
    // buffer length for response
    size_t len = va_arg(list, size_t);
    va_end(list);
    wifi_config_t cfg;
    if (len < sizeof(cfg.ap.ssid))
        return ESP_FAIL;

    wifi_ap_get_config(&cfg);
    strcpy((void *)buf, (char *)cfg.ap.password);
    return strlen((char *)cfg.ap.password);
}

extern int FnEsp_setApPass(uint8_t const *buf, ...)
{
    va_list list;
    // buffer for response
    va_start(list, buf);
    // buffer length for response
    size_t len = va_arg(list, size_t);
    va_end(list);
    wifi_config_t cfg;
    if (len < sizeof(cfg.ap.ssid))
    {
        wifi_ap_get_config(&cfg);
        bzero(cfg.ap.password, sizeof(cfg.ap.password));
        strcpy((char *)&cfg.ap.password, (void *)buf);
        wifi_ap_set_config(&cfg);
        return ESP_OK;
    }
    return ESP_FAIL;
}
extern int FnEsp_getApMac(uint8_t *buf, ...)
{
    if (ESP_OK == wifi_get_mac((void *)buf))
        return strlen((void *)buf);
    return ESP_FAIL;
}

extern int FnEsp_resetApSettings(uint8_t *buf, ...)
{
    return wifi_ap_reset();
}
//****************	STA SETTINGS	****************
extern int FnEsp_getStaSettings(uint8_t *buf, ...)
{
    va_list list;
    // buffer for response
    va_start(list, buf);
    // buffer length for response
    // size_t len = va_arg(list, size_t);
    va_end(list);

    wifi_config_t cfg;
    int idx = -1;
    if (ESP_OK == wifi_sta_get_config(&cfg))
    {
        idx = 0;
        buf[idx++] = wifi_sta_get_enable();
        buf[idx++] = strlen((void *)cfg.sta.ssid);
        memcpy(&buf[idx], cfg.sta.ssid, buf[idx - 1]);
        idx += buf[idx - 1];
        buf[idx++] = strlen((void *)cfg.sta.password);
        memcpy(&buf[idx], cfg.sta.password, buf[idx - 1]);
        idx += buf[idx - 1];
    }
    return idx;
}
extern int FnEsp_setStaSettings(uint8_t const *buf, ...)
{
    va_list list;

    // request data buffer
    va_start(list, buf);
    // request buffer length
    // size_t in_len = va_arg(list, size_t);
    va_end(list);

    wifi_config_t cfg;
    int idx = -1;
    if (ESP_OK == wifi_sta_get_config(&cfg))
    {
        idx = 0;
        bzero(cfg.sta.ssid, sizeof(cfg.sta.ssid));
        bzero(cfg.sta.password, sizeof(cfg.sta.password));

        bool en = buf[idx++];
        int ssid_len = buf[idx++];
        espLogHex(&buf[idx], ssid_len);
        memcpy((void *)cfg.sta.ssid, &buf[idx], ssid_len);
        idx += ssid_len;
        int pass_len = buf[idx++];
        espLogHex(&buf[idx], pass_len);
        memcpy((void *)cfg.sta.password, &buf[idx], pass_len);
        if (en && (CONFIG_WIFI_SSID_LENGTH_MIN > ssid_len || CONFIG_WIFI_SSID_LENGTH_MAX < ssid_len ||
                   CONFIG_WIFI_PASSWORD_LENGTH_MIN > pass_len || CONFIG_WIFI_PASSWORD_LENGTH_MAX < pass_len))
            return ESP_FAIL;
        wifi_sta_set_enable(en);
        return wifi_sta_set_config(&cfg);
    }
    return ESP_FAIL;
}
extern int FnEsp_getStaMac(uint8_t *buf, ...)
{
    return ESP_FAIL;
}

extern int
FnEsp_systemReset()
{
    system_reset();
    return 0;
}