/* Common functions for protocol examples, to establish Wi-Fi or Ethernet connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <string.h>
#include "sdkconfig.h"
#include "esp_event.h"
#include "wifi.h"
#include "esp_wifi_default.h"
#include "esp_mac.h"

#include "esp_netif.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "nv_memory.h"

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

#define EXAMPLE_DO_CONNECT CONFIG_ZHAGA_CONNECT_WIFI

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
    .sta = {
        .ssid = CONFIG_WIFI_STA_SSID,
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
static void wifi_sta_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_sta_disconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void wifi_ap_started(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_ap_client_connected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_ap_client_disconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_ap_client_ipassigned(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void wifi_sta_set_status(const char *str_status);

// Logging
static const char *TAG = __FILE__;
#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s: " format, TAG, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogHex(data, len) ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_DEBUG)

#define CONFIG_WIFI_AP_DEF_SSID "@Zhaga-"
static void wifi_ap_init()
{
    _wifi_ap_netif = esp_netif_create_default_wifi_ap();
    char ssid[64] = CONFIG_WIFI_AP_SSID_PREFIX;
    ///    char pass [32] = CONFIG_WIFI_AP_PASSWORD;

    // If no ap settins saved - save defaults
    esp_wifi_get_config(ESP_IF_WIFI_AP, &_wifi_ap_cfg);
    if (memcmp(ssid, _wifi_ap_cfg.ap.ssid, strlen(ssid)))
        wifi_ap_reset();

    LOGI("SSID: %s, ssid_len: %d, PASS: %s %d", _wifi_ap_cfg.ap.ssid, _wifi_ap_cfg.ap.ssid_len, _wifi_ap_cfg.ap.password, _wifi_ap_cfg.ap.authmode);
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_START, &wifi_ap_started, NULL, NULL));
    //    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &wifi_ap_client_connected, NULL, NULL));
    //    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &wifi_ap_client_disconnected, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_ap_client_ipassigned, NULL, NULL));
}

esp_err_t wifi_disconnect(void)
{
    return esp_wifi_disconnect();
    ;
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
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(_wifi_ap_netif));
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(_wifi_sta_netif));
    esp_netif_destroy(_wifi_ap_netif);
    esp_netif_destroy(_wifi_sta_netif);
}

///==========	WIFI EVENT HANDLERS
static void wifi_ap_started(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    _ap_started = true;
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(_wifi_ap_netif, &ip_info);
    _ip_addr = ip_info.ip;
    LOGD("AP started. IP: " IPSTR " mask: " IPSTR " gateway: " IPSTR, 
         IP2STR(&_ip_addr), IP2STR(&ip_info.netmask), IP2STR(&ip_info.gw));
    LOGD("AP started. IP: " IPSTR " mask: " IPSTR " gateway: " IPSTR,
         IP2STR(&_ip_addr), IP2STR(&ip_info.netmask), IP2STR(&ip_info.gw));
}
static void wifi_ap_client_connected(void *arg, esp_event_base_t event_base,
                                     int32_t event_id, void *event_data)
{
    /*
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(_wifi_ap_netif, &ip_info);
    LOGI("AP: " IPSTR " mask: " IPSTR " gateway: " IPSTR ,
            IP2STR(&ip_info.ip), IP2STR(&ip_info.netmask), IP2STR(&ip_info.gw));

    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
    LOGI("station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    */
}
static void wifi_ap_client_disconnected(void *arg, esp_event_base_t event_base,
                                        int32_t event_id, void *event_data)
{
    /*
    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
    LOGI("station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
*/
}

static void wifi_ap_client_ipassigned(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data)
{
    ip_event_ap_staipassigned_t *evt = (ip_event_ap_staipassigned_t *)event_data;
    LOGI("Assigned IP" IPSTR, IP2STR(&evt->ip));
}

static void wifi_sta_disconnected(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    esp_err_t err = ESP_ERR_WIFI_CONN;
    LOGD("Wi-Fi disconnected, trying to reconnect...%d of %d", _reconnect_counter, CONFIG_WIFI_STA_MAX_RECONNECT);
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

static void wifi_sta_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    /*
    if (!is_our_netif(TAG, event->esp_netif)) {
            espLogW("Got IPv4 from another interface \"%s\": ignored" IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
            return;
        }
      */
    _sta_connected = true;
    LOGD("Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    wifi_sta_set_status("Connected.");
    _ip_addr = event->ip_info.ip;
    LOGD(IPSTR, IP2STR(&_ip_addr));
    espLogHex(&_ip_addr, sizeof(_ip_addr));
}

static void wifi_sta_set_status(const char *str_status)
{
    bzero(_sta_status_str, sizeof(_sta_status_str));
    sprintf(_sta_status_str, str_status);
}
