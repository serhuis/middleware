/* Common functions for protocol examples, to establish Wi-Fi or Ethernet
   connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <string.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nv_memory.h"
#include "sdkconfig.h"
#include "wifi.h"

#ifdef CONFIG_EXAMPLE_CONNECT_IPV6
#define MAX_IP6_ADDRS_PER_NETIF (5)
#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR (s_active_interfaces * 2)

#if defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_LOCAL_LINK)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_LINK_LOCAL
#elif defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_GLOBAL)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_GLOBAL
#elif defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_SITE_LOCAL)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_SITE_LOCAL
#elif defined(CONFIG_EXAMPLE_CONNECT_IPV6_PREF_UNIQUE_LOCAL)
#define EXAMPLE_CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_UNIQUE_LOCAL
#endif // if-elif CONFIG_EXAMPLE_CONNECT_IPV6_PREF_...

#else
#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR (s_active_interfaces)
#endif

#define EXAMPLE_DO_CONNECT \
    CONFIG_ZHAGA_CONNECT_WIFI || CONFIG_EXAMPLE_CONNECT_ETHERNET

#if CONFIG_WIFI_STA_SCAN_METHOD_FAST
#define WIFI_STA_SCAN_METHOD WIFI_FAST_SCAN
#elif CONFIG_WIFI_STA_SCAN_METHOD_ALL_CHANNEL
#define WIFI_STA_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#endif

#if CONFIG_WIFI_STA_CONNECT_AP_BY_SIGNAL
#define WIFI_STA_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_WIFI_STA_CONNECT_AP_BY_SECURITY
#define EXAMPLE_WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#endif

#if CONFIG_WIFI_STA_AUTH_OPEN
#define WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_WIFI_STA_AUTH_WEP
#define WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_WIFI_STA_AUTH_WPA_PSK
#define WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_WIFI_STA_AUTH_WPA2_PSK
#define WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_WIFI_STA_AUTH_WPA_WPA2_PSK
#define WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_WIFI_STA_AUTH_WPA2_ENTERPRISE
#define WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_ENTERPRISE
#elif CONFIG_WIFI_STA_AUTH_WPA3_PSK
#define WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_WIFI_STA_AUTH_WPA2_WPA3_PSK
#define WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_WIFI_STA_AUTH_WAPI_PSK
#define WIFI_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

const wifi_config_t WIFI_STA_DEF_CFG = {
    .sta = {.ssid = CONFIG_WIFI_STA_SSID,
            .password = CONFIG_WIFI_STA_PASSWORD,
            .scan_method = WIFI_STA_SCAN_METHOD,
            .sort_method = WIFI_STA_SORT_METHOD,
            .threshold.authmode = WIFI_AUTH_MODE_THRESHOLD}};

static esp_netif_t *_wifi_sta_netif = NULL;
static esp_netif_t *_wifi_ap_netif = NULL;
static uint _reconnect_counter = 0;
static bool _ap_started = false;
static bool _sta_connected = false;
esp_ip4_addr_t _ip_addr;

wifi_mode_t _wifi_mode = WIFI_MODE_APSTA;
char _sta_status_str[32] = "\0";
wifi_config_t _wifi_sta_cfg = WIFI_STA_DEF_CFG;
wifi_config_t _wifi_ap_cfg;

//
static void wifi_start(void);
static void wifi_stop(void);
static void wifi_sta_init(void);
static void wifi_ap_init(void);

// events handlers
static void wifi_sta_got_ip(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data);
static void wifi_sta_disconnected(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data);

static void wifi_ap_started(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data);
static void wifi_ap_client_connected(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data);
static void wifi_ap_client_disconnected(void *arg, esp_event_base_t event_base,
                                        int32_t event_id, void *event_data);
static void wifi_ap_client_ipassigned(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data);

static void wifi_sta_set_status(const char *str_status);

// Logging
static const char *TAG = __FILE__;
#define LOGI(format, ...) \
    ESP_LOGI("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) \
    ESP_LOGD("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) \
    ESP_LOGW("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) \
    ESP_LOGE("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogHex(data, len) \
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_DEBUG)

///==========	STA EVENT HANDLERS
static void wifi_sta_disconnected(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    esp_err_t err = ESP_ERR_WIFI_CONN;
    LOGD("Wi-Fi disconnected, trying to reconnect...%d of %d", _reconnect_counter,
         CONFIG_WIFI_STA_MAX_RECONNECT);
    if (++_reconnect_counter < CONFIG_WIFI_STA_MAX_RECONNECT)
    {
        err = esp_wifi_connect();
        if (ESP_ERR_WIFI_SSID == err)
            wifi_sta_set_status("SSID of AP which station connects is invalid!");
        else
            wifi_sta_set_status("Disconnected!");
    }
    else
    {
        err = esp_wifi_disconnect();
        /// esp_wifi_stop();
        // wifi_ap_init();
        _sta_connected = false;
        wifi_sta_set_status("Disconnected!");
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(_wifi_ap_netif, &ip_info);
        _ip_addr = ip_info.ip;
    }
}

static void wifi_sta_got_ip(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    /*
    if (!is_our_netif(TAG, event->esp_netif)) {
            espLogW("Got IPv4 from another interface \"%s\": ignored" IPSTR,
    esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip)); return;
        }
      */
    _sta_connected = true;
    LOGD("Got IPv4 event: Interface \"%s\" address: " IPSTR,
         esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    wifi_sta_set_status("Connected.");
    _ip_addr = event->ip_info.ip;
    LOGD(IPSTR, IP2STR(&_ip_addr));
}

#define DEFAULT_SCAN_LIST_SIZE (32)
static wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE] = {0};
static void wifi_start(void)
{
    LOGI();
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    esp_err_t err;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_get_mode(&_wifi_mode));
    LOGD("mode: %d", _wifi_mode);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_sta_init();

    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    for (int i = 0; i < number; i++)
    {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        if (!strncmp((char *)ap_info[i].ssid, CONFIG_WIFI_STA_SSID,
                     strlen(CONFIG_WIFI_STA_SSID)))
        {
            ESP_ERROR_CHECK(esp_wifi_connect());
            return;
        }
    }
    if (WIFI_MODE_AP == (WIFI_MODE_AP & _wifi_mode))
    {
        wifi_ap_init();
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT)
    {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(
        esp_wifi_clear_default_wifi_driver_and_handlers(_wifi_ap_netif));
    ESP_ERROR_CHECK(
        esp_wifi_clear_default_wifi_driver_and_handlers(_wifi_sta_netif));
    esp_netif_destroy(_wifi_ap_netif);
    esp_netif_destroy(_wifi_sta_netif);
}

static void wifi_sta_init()
{
    _wifi_sta_netif = esp_netif_create_default_wifi_sta();
    wifi_config_t cfg = {0};

#if defined CONFIG_ZHAGA_NVS_ENABLE
    if (0 > NVMemory_getBlob(NVMEMORY_STA_CONFIG, &cfg, sizeof(wifi_config_t)))
    {
        LOGE("NVMEMORY_STA_CONFIG read ERROR");
    }
    else
        LOGI("NVMEMORY_STA_CONFIG read OK: got ssid: %s, password: %s",
             cfg.sta.ssid, cfg.sta.password);
#else
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &cfg));
#endif
    _wifi_sta_cfg = cfg;

    if (0 == strlen((void *)_wifi_sta_cfg.sta.ssid) ||
        0 == strlen((void *)_wifi_sta_cfg.sta.password))
        _wifi_sta_cfg = WIFI_STA_DEF_CFG;
    ESP_ERROR_CHECK(wifi_sta_set_config(&_wifi_sta_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_sta_disconnected, NULL,
        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_sta_got_ip, NULL, NULL));
}

static void wifi_ap_init(void)
{
    _wifi_ap_netif = esp_netif_create_default_wifi_ap();
    char ssid[64] = CONFIG_WIFI_AP_SSID_PREFIX;
    ///    char pass [32] = CONFIG_WIFI_AP_PASSWORD;

    // If no ap settins saved - save defaults
    ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_AP, &_wifi_ap_cfg));
    if (memcmp(ssid, _wifi_ap_cfg.ap.ssid, strlen(ssid)))
        wifi_ap_reset();

    LOGI("SSID: %s, ssid_len: %d, PASS: %s %d", _wifi_ap_cfg.ap.ssid,
         _wifi_ap_cfg.ap.ssid_len, _wifi_ap_cfg.ap.password,
         _wifi_ap_cfg.ap.authmode);
    //  ESP_ERROR_CHECK (esp_event_handler_instance_register (
    //      WIFI_EVENT, WIFI_EVENT_AP_START, &wifi_ap_started, NULL, NULL));
    //    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
    //    WIFI_EVENT_AP_STACONNECTED, &wifi_ap_client_connected, NULL, NULL));
    //    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
    //    WIFI_EVENT_AP_STADISCONNECTED, &wifi_ap_client_disconnected, NULL,
    //    NULL));
    // ESP_ERROR_CHECK (esp_event_handler_instance_register (
    //      IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_ap_client_ipassigned,
    //      NULL,
    //    NULL));
}

static void wifi_sta_set_status(const char *str_status)
{
    bzero(_sta_status_str, sizeof(_sta_status_str));
    sprintf(_sta_status_str, str_status);
}

// External API
esp_err_t wifi_init(void)
{
    wifi_start();
    return ESP_OK;
}

esp_err_t wifi_disconnect(void) { return esp_wifi_disconnect(); }
