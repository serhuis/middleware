#include <string.h>
#include "wifi.h"
#include "nv_memory.h"


// Logging
static const char *TAG = __FILE__;
#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogHex(data, len) ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_DEBUG)


const static wifi_config_t WIFI_AP_DEF_CFG = {
    .ap = {
        .ssid = CONFIG_WIFI_AP_SSID_PREFIX,
        .ssid_len = strlen(CONFIG_WIFI_AP_SSID_PREFIX),
        .channel = CONFIG_WIFI_AP_CHANNEL,
        .password = CONFIG_WIFI_AP_PASSWORD,
        .max_connection = CONFIG_WIFI_AP_MAX_CONN,
        .authmode = WIFI_AUTH_WPA_WPA2_PSK}
        };
extern const wifi_config_t WIFI_STA_DEF_CFG;

extern esp_ip4_addr_t _ip_addr;
extern char _sta_status_str[];
extern wifi_mode_t _wifi_mode;
extern wifi_config_t _wifi_sta_cfg;
extern wifi_config_t _wifi_ap_cfg;

///============================================================================
///	external API
char *wifi_sta_get_status(void)
{
    return _sta_status_str;
}

bool wifi_sta_get_enable(void)
{
    return ((_wifi_mode == WIFI_MODE_STA) || (_wifi_mode == WIFI_MODE_APSTA)) ? true : false;
}
esp_err_t wifi_sta_set_enable(bool new_enable)
{
    bool sta_en = ((_wifi_mode == WIFI_MODE_STA) || (WIFI_MODE_APSTA == _wifi_mode));
    esp_err_t err = ESP_FAIL;
    if (sta_en != new_enable)
    {
        _wifi_mode = (new_enable) ? WIFI_MODE_APSTA : WIFI_MODE_AP;
        err = esp_wifi_set_mode(_wifi_mode);
        if (new_enable)
            esp_wifi_connect();
    }
    return err;
}

esp_err_t wifi_sta_get_config(wifi_config_t *conf)
{
    LOGD("SSID:%s password:%s rssi:%d", _wifi_sta_cfg.sta.ssid, _wifi_sta_cfg.sta.password, _wifi_sta_cfg.sta.threshold.rssi);
    LOGD("SSID:%s password:%s rssi:%d", conf->sta.ssid, conf->sta.password, conf->sta.threshold.rssi);
    conf->sta = _wifi_sta_cfg.sta;
    return (0 == memcmp(&conf->sta, &_wifi_sta_cfg.sta, sizeof(wifi_config_t))) ? ESP_OK : ESP_FAIL;
}

esp_err_t wifi_sta_set_config(wifi_config_t *cfg)
{
    esp_err_t err = ESP_FAIL;

    LOGD("SSID:%s password:%s rssi:%d", cfg->sta.ssid, cfg->sta.password, cfg->sta.threshold.rssi);
    _wifi_sta_cfg.sta = cfg->sta;

    err = esp_wifi_set_config(WIFI_IF_STA, &_wifi_sta_cfg);
    LOGD("SSID:%s password:%s rssi:%d", _wifi_sta_cfg.sta.ssid, _wifi_sta_cfg.sta.password, _wifi_sta_cfg.sta.threshold.rssi);
#if defined CONFIG_ZHAGA_NVS_ENABLE
    if (0 > NVMemory_setBlob(NVMEMORY_STA_CONFIG, (uint8_t *)cfg, sizeof(wifi_config_t)))
    {
        LOGE("NVMEMORY_STA_CONFIG write ERROR");
    }
    else
        LOGD("NVMEMORY_STA_CONFIG write OK: got ssid: %s, password: %s", cfg->sta.ssid, cfg->sta.password);
#endif
    return err;
}

esp_err_t wifi_ap_get_config(wifi_config_t *cfg)
{
    esp_wifi_get_config(WIFI_IF_AP, &_wifi_ap_cfg);
#if defined CONFIG_ZHAGA_NVS_ENABLE
    if (0 > NVMemory_getBlob(NVMEMORY_AP_CONFIG, cfg, sizeof(wifi_config_t)))
    {
        LOGE("%s Rrite ERROR", NVMEMORY_AP_CONFIG);
    }
    else
        LOGD("%s Read OK: got ssid: %s, password: %s", NVMEMORY_AP_CONFIG, cfg->ap.ssid, cfg->ap.password);
#endif
    memcpy((void *)&cfg->ap, (void *)&_wifi_ap_cfg.ap, sizeof(_wifi_ap_cfg.ap));
    LOGD("SSID:%s password:%s ssid_len:%d hiddn: %d", _wifi_ap_cfg.ap.ssid, _wifi_ap_cfg.ap.password, _wifi_ap_cfg.ap.ssid_len, _wifi_ap_cfg.ap.ssid_hidden);
    return ESP_OK;
}

esp_err_t wifi_ap_set_config(wifi_config_t *cfg)
{
    esp_err_t err = ESP_FAIL;
    /// wifi_config_t * ap_cfg = (wifi_config_t*)cfg;
    bzero(&_wifi_ap_cfg.ap.ssid, sizeof(_wifi_ap_cfg.ap.ssid));
    bzero(&_wifi_ap_cfg.ap.password, sizeof(_wifi_ap_cfg.ap.password));

    _wifi_ap_cfg.ap.ssid_len = cfg->ap.ssid_len;
    memcpy(&_wifi_ap_cfg.ap.ssid, cfg->ap.ssid, cfg->ap.ssid_len);
    memcpy(&_wifi_ap_cfg.ap.password, cfg->ap.password, strlen(&cfg->ap.password));
    _wifi_ap_cfg.ap.ssid_hidden = cfg->ap.ssid_hidden;

    err = esp_wifi_set_config(WIFI_IF_AP, &_wifi_ap_cfg);
    //LOGI("SSID:%s password:%s hidden:%d", _wifi_ap_cfg.ap.ssid, _wifi_ap_cfg.ap.password, _wifi_ap_cfg.ap.ssid_hidden);
#if defined CONFIG_ZHAGA_NVS_ENABLE
    if (0 > NVMemory_setBlob(NVMEMORY_AP_CONFIG, cfg, sizeof(wifi_config_t)))
    {
        LOGE("%s write ERROR", NVMEMORY_AP_CONFIG);
    }
    else
        LOGI("%s write OK: got ssid: %s, password: %s", NVMEMORY_AP_CONFIG, cfg->ap.ssid, cfg->ap.password);
#endif
    return err;
}

esp_err_t wifi_get_ip(char *str_addr)
{
    sprintf(str_addr, IPSTR, IP2STR(&_ip_addr));
    return ESP_OK;
}

char *wifi_get_ip_str(void)
{
    static char ip_str[16] = "\0";
    sprintf(ip_str, IPSTR, IP2STR(&_ip_addr));
    return ip_str;
}

esp_err_t wifi_get_mac(char *str_addr)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    sprintf(str_addr, MACSTR, MAC2STR(mac));
    return ESP_OK;
}

char *wifi_get_mac_str(void)
{
    uint8_t mac[6];
    static char str_addr[16] = "\0";
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    sprintf(str_addr, MACSTR, MAC2STR(mac));
    return str_addr;
}

esp_err_t wifi_ap_reset(void)
{
    wifi_config_t ap_conf = WIFI_AP_DEF_CFG;
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
    // LOGI("AP config reseted to default: " MACSTR, MAC2STR(mac));
    sprintf((char *)&ap_conf.ap.ssid[strlen((char *)ap_conf.ap.ssid)], MACSTR, MAC2STR(mac));
    ap_conf.ap.ssid_len = strlen((char *)ap_conf.ap.ssid);
    _wifi_ap_cfg = ap_conf;
    LOGI("AP config reseted to default: SSID: %s, PASS: %s, hide: %d", _wifi_ap_cfg.ap.ssid, _wifi_ap_cfg.ap.password, _wifi_ap_cfg.ap.ssid_hidden);
    return wifi_ap_set_config(&ap_conf);
}

