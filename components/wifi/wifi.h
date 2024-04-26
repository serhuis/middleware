/* Common functions for protocol examples, to establish Wi-Fi or Ethernet connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event.h"


#define NVMEMORY_AP_CONFIG "NV_AP_CONFIG"
#define NVMEMORY_STA_CONFIG "NV_STA_CONFIG"


#if !defined (CONFIG_EXAMPLE_CONNECT_ETHERNET) && !defined (CONFIG_ZHAGA_CONNECT_WIFI)
// This is useful for some tests which do not need a network connection
#define EXAMPLE_INTERFACE NULL
#endif


/**
 * @brief Configure Wi-Fi or Ethernet, connect, wait for IP
 *
 * This all-in-one helper function is used in protocols examples to
 * reduce the amount of boilerplate in the example.
 *
 * It is not intended to be used in real world applications.
 * See examples under examples/wifi/getting_started/ and examples/ethernet/
 * for more complete Wi-Fi or Ethernet initialization code.
 *
 * Read "Establishing Wi-Fi or Ethernet Connection" section in
 * examples/protocols/README.md for more information about this function.
 *
 * @return ESP_OK on successful connection
 */
esp_err_t wifi_init(void);

/**
 * Counterpart to example_connect, de-initializes Wi-Fi or Ethernet
 */
esp_err_t wifi_disconnect(void);


extern esp_err_t wifi_ap_get_config(wifi_config_t *  cfg);
extern esp_err_t wifi_ap_set_config(wifi_config_t* cfg);
extern esp_err_t wifi_get_ip(char* str_addr);
extern char* wifi_get_ip_str(void);
extern esp_err_t wifi_get_mac(char* str_addr);
extern char * wifi_get_mac_str(void);
extern bool wifi_sta_get_enable(void);
extern char* wifi_sta_get_status(void);
extern esp_err_t wifi_sta_set_enable(bool new_enable);
extern esp_err_t wifi_sta_get_config(wifi_config_t * cfg);
extern esp_err_t wifi_sta_set_config(wifi_config_t * cfg);
extern esp_err_t wifi_ap_reset(void);