#include <esp_log.h>
#include <stdarg.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "functionBle.h"
#include "zhaga_central.h"

static TimerHandle_t _xtimer;
static StaticTimer_t _xTimerBuffer;

static void (*dcd_evt_handler)(int evt, ...) = NULL;
static void vTimerCallback(TimerHandle_t xTimer);

void FnBle_init(void (*evt_handler)(int evt, ...))
{
    dcd_evt_handler = evt_handler;
    _xtimer = xTimerCreateStatic("Timer", pdMS_TO_TICKS(500), pdFALSE, (void *)0, vTimerCallback, &_xTimerBuffer);
    // xTimerStart(_xtimer, pdMS_TO_TICKS(5000));
}

static void vTimerCallback(TimerHandle_t xTimer)
{
    ESP_LOGI("", "%s:%d dcd_evt_handler: %02x", __FILE__, __LINE__, dcd_evt_handler);
    if (dcd_evt_handler)
    {
        // xTimerStart(_xtimer, pdMS_TO_TICKS(5000));
        int found = scan_get_count();
        ESP_LOGI("", "%s:%d scan_get_count(): %x", __FILE__, __LINE__, found);
        if (found > 0)
        {
            struct node_item *n;
            SLIST_HEAD(, node_item) *list = (struct node_item *)scan_get_nodes();

            SLIST_FOREACH(n, list, next)
            {
                char rssi[] = {"-120 db"};
                sprintf(rssi, "%d db", n->node.rssi);
                ESP_LOGI("", "%s:%d %s\t<%s>,\trssi:%d", __FILE__, __LINE__, n->node.name, addr_str(n->node.address.val), n->node.rssi);
                size_t st_len = strlen(n->node.name);
                dcd_evt_handler((void *)&FnBle_getDevList, n->node.name, st_len);
            }
        }
    }
}

extern int FnBle_getPairedName(char *buf, ...)
{
    ESP_LOGD("", "%s:%d:", __FILE__, __LINE__);
    char *name = zhaga_cent_get_paired_name();
    size_t n_len = strlen(name);
    ESP_LOGD("", "%s:%d: %s %d", __FILE__, __LINE__, name, n_len);
    va_list list;
    va_start(list, buf);
    size_t len = va_arg(list, size_t);
    va_end(list);
    if (n_len > len)
        return 0;
    memcpy(buf, name, n_len);
    ESP_LOGD("", "%s:%d: %s %d", __FILE__, __LINE__, buf, n_len);
    return n_len;
}

extern int FnBle_setPairedName(char *buf, ...)
{
    va_list list;
    va_start(list, buf);
    size_t len = va_arg(list, size_t);
    va_end(list);

    return zhaga_cent_set_paired_name(buf);
}

int FnBle_getPairedAddress(uint8_t *buf, ...)
{
    ESP_LOGD("", "%s:%d:", __FILE__, __LINE__);
    va_list list;
    va_start(list, buf);
    size_t len = va_arg(list, size_t);
    va_end(list);
    return ESP_OK;
    return zhaga_cent_get_paired_addr(buf, len);
}

int FnBle_setPairedAddress(uint8_t *buf, ...)
{
    ESP_LOGD("", "%s:%d:", __FILE__, __LINE__);
    // return ESP_OK;
    return zhaga_cent_set_paired_addr(buf);
}

int FnBle_setPairedPasskey(uint32_t pk, ...)
{
    ESP_LOGD("", "%s:%d:", __FILE__, __LINE__);
    // return ESP_OK;
    return zhaga_cent_set_paired_passkey(pk);
}

extern int FnBle_getAutoconnect(uint8_t *buf, ...)
{
    ESP_LOGD("", "%s:%d:", __FILE__, __LINE__);
    bool connect = zhaga_cent_get_autoconnect();
    va_list list;
    va_start(list, buf);
    size_t len = va_arg(list, size_t);
    va_end(list);
    if (len < sizeof(connect))
        return 0;
    memcpy(buf, &connect, sizeof connect);
    return sizeof(connect);
}

extern int FnBle_setAutoconnect(uint8_t *buf, ...)
{
    ESP_LOGI("", "%s:%d:", __FILE__, __LINE__);
    zhaga_cent_set_autoconnect((bool)buf[0]);
    return 0;
}

int FnBle_getStatus(char *buf, ...)
{
    ESP_LOGD("", "%s:%d", __FILE__, __LINE__);
    char *status = zhaga_cent_get_status();
    size_t st_len = strlen(status);
    va_list list;
    va_start(list, buf);
    size_t len = va_arg(list, size_t);
    va_end(list);
    if (len < st_len)
        return 0;
    memcpy(buf, status, st_len);
    ESP_LOGD("", "%s:%d status: %s", __FILE__, __LINE__, status);
    return st_len;
}

extern int FnBle_setConnPaired(uint8_t *buf, ...)
{
    ESP_LOGI("", "%s:%d:%s(): %d", __FILE__, __LINE__, __FUNCTION__, *buf);
    if (0 != *(bool *)buf)
        zhaga_cent_connect_paired();
    else
        zhaga_cent_disconnect();
    return 0;
}
int FnBle_getDevList(int *buf, ...)
{
    ESP_LOGI("", "%s:%d", __FILE__, __LINE__);
    xTimerStart(_xtimer, pdMS_TO_TICKS(500));
    return 0;
}