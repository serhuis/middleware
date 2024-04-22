/*
 * tcp_server.c
 *
 *  Created on: 10 ���. 2022 �.
 *      Author: serhiy
 */

#include "tcp_server.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "sdkconfig.h"
#include <lwip/netdb.h>
#include <string.h>
#include <sys/param.h>

#include "../wifi/wifi.h"
#include "esp_netif_ip_addr.h"

#define PORT CONFIG_ZHAGA_TCP_PORT
#define KEEPALIVE_IDLE CONFIG_ZHAGA_TCP_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL CONFIG_ZHAGA_TCP_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT CONFIG_ZHAGA_TCP_KEEPALIVE_COUNT

 /**
  * @brief Indicates that the file descriptor represents an invalid
  * (uninitialized or closed) socket
  *
  * Used in the TCP server structure `sock[]` which holds list of active clients
  * we serve.
  */
#define INVALID_SOCK (-1)

  /**
   * @brief Time in ms to yield to all tasks when a non-blocking socket would
   * block
   *
   * Non-blocking socket operations are typically executed in a separate task
   * validating the socket status. Whenever the socket returns `EAGAIN` (idle
   * status, i.e. would block) we have to yield to all tasks to prevent lower
   * priority tasks from starving.
   */
#define YIELD_TO_ALL_MS (50)

   // Logging
static const char* TAG = __FILE__;
#define logI(format, ...)                                                     \
  ESP_LOGI ("", "%s:%d: %s: " format, (char *)__FILE__, __LINE__,             \
            (char *)__FUNCTION__, ##__VA_ARGS__)
#define logD(format, ...)                                                     \
  ESP_LOGD ("", "%s:%d: %s: " format, (char *)__FILE__, __LINE__,             \
            (char *)__FUNCTION__, ##__VA_ARGS__)
#define logW(format, ...)                                                     \
  ESP_LOGW ("", "%s:%d: %s: " format, (char *)__FILE__, __LINE__,             \
            (char *)__FUNCTION__, ##__VA_ARGS__)
#define logE(format, ...)                                                     \
  ESP_LOGE ("", "%s:%d: %s: " format, (char *)__FILE__, __LINE__,             \
            (char *)__FUNCTION__, ##__VA_ARGS__)
#define espLogHex(data, len)                                                  \
  ESP_LOG_BUFFER_HEXDUMP (TAG, data, len, ESP_LOG_INFO)

static TaskHandle_t _socketTaskHandle = NULL;

static char rx_buffer[512];
static int addr_family = 0;
/// static int ip_protocol = 0;
static struct addrinfo hints = { .ai_socktype = SOCK_STREAM };
static struct addrinfo* address_info;
static int listen_sock = INVALID_SOCK;
static const size_t _max_socks = CONFIG_LWIP_MAX_SOCKETS - 1;
static int sock[CONFIG_LWIP_MAX_SOCKETS - 1];
static int _peerId = -1;

static esp_netif_ip_info_t _netif_ip_info = { 0 };

static void (* _notify)(int, ...) = NULL;

static void log_socket_error(const char* tag, const int sock, const int err,
  const char* message);
static void tcp_server_task(void* pvParameters);
static void on_wifi_sta_connected(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data);
static void on_wifi_sta_disconnected(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data);
static void on_wifi_ap_connected(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data);
static void on_wifi_ap_disconnected(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data);
static void on_sta_got_ip(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data);

extern void
tcp_server_init(user_msg msgCb)
  {
  _notify = msgCb;

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, &on_sta_got_ip, NULL, NULL));
  //  ESP_ERROR_CHECK (esp_event_handler_instance_register (
  //      IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &on_wifi_ap_connected, NULL,
  //      NULL));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    WIFI_EVENT, IP_EVENT_STA_LOST_IP, &on_wifi_sta_disconnected, NULL,
    NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &on_wifi_ap_disconnected,
    NULL, NULL));

  xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5,
    &_socketTaskHandle);
  // vTaskSuspend (_socketTaskHandle);
  tcp_server_stop();
  }

extern void
tcp_server_deinit(void)
  {
  tcp_server_stop();

  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
    IP_EVENT, IP_EVENT_STA_GOT_IP, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
    IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, NULL));

  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
    WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
    WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
    WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, NULL));

  vTaskDelete(_socketTaskHandle);
  }

void
tcp_server_start(void)
  {
  logD();
  // Prepare a list of file descriptors to hold client's sockets, mark all of
  // them as invalid, i.e. available
  listen_sock = INVALID_SOCK;
  for (int i = 0; i < sizeof(sock) / sizeof(int); i++)
    {
    sock[i] = INVALID_SOCK;
    }

  // Translating the hostname or a string representation of an IP to
  // address_info
  char host[16] = "";

  // wifi_get_ip(addr); int res =
  sprintf(host, IPSTR, IP2STR(&_netif_ip_info.ip));
  logI("host: %s", host);
  uint8_t res = getaddrinfo(host, "3000", &hints, &address_info);
  if (res != 0 || address_info == NULL)
    {
    logE("couldn't get hostname for `%p` "
      "getaddrinfo() returns %d, addrinfo=%p",
      host, res, address_info);
    return;
    }

  // Creating a listener socket
  listen_sock = socket(address_info->ai_family, address_info->ai_socktype,
    address_info->ai_protocol);

  // Marking the socket as non-blocking
  int flags = fcntl(listen_sock, F_GETFL);
  if (fcntl(listen_sock, F_SETFL, flags | O_NONBLOCK) == -1)
    {
    log_socket_error(TAG, listen_sock, errno,
      "Unable to set socket non blocking");
    tcp_server_stop();
    }
  logD("Socket marked as non blocking");

  // Binding socket to the given address
  int err = bind(listen_sock, address_info->ai_addr, address_info->ai_addrlen);

  if (err != 0)
    {
    log_socket_error(TAG, listen_sock, errno, "Socket unable to bind");
    return;
    }
  // Set queue (backlog) of pending connections to one (can be more)
  err = listen(listen_sock, 1);
  if (err != 0)
    {
    log_socket_error(TAG, listen_sock, errno,
      "Error occurred during listen");
    return;
    }
  logI("Socket %d on %s:%d listening...", listen_sock, host, PORT);
  vTaskResume(_socketTaskHandle);
  }

extern void
tcp_server_stop(void)
  {
  logI();
  vTaskSuspend(_socketTaskHandle);

  for (int i = 0; i < _max_socks; ++i)
    {
    if (sock[i] != INVALID_SOCK)
      {
      logI("Close socket %d ", sock[i]);
      close(sock[i]);
      sock[i] = INVALID_SOCK;
      }
    }
  if (listen_sock != INVALID_SOCK)
    {
    logI("Close listen socket %d ", listen_sock);
    close(listen_sock);
    listen_sock = INVALID_SOCK;
    }
  free(address_info);
  }

// wifi event handlers
static void
on_sta_got_ip(void* arg, esp_event_base_t event_base, int32_t event_id,
  void* event_data)
  {
  ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
  logD("STA: Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
  _netif_ip_info = event->ip_info;
  logI("_netif_ip_info.ip: " IPSTR, IP2STR(&_netif_ip_info.ip));
  tcp_server_stop();
  tcp_server_start();
  }
static void
on_wifi_sta_disconnected(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data)
  {
  ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
  logD("STA:  Disconnected " IPSTR, IP2STR(&event->ip_info.ip));
  tcp_server_stop();
  tcp_server_start();
  }
static void
on_wifi_ap_connected(void* arg, esp_event_base_t event_base, int32_t event_id,
  void* event_data)
  {
  esp_netif_ip_info_t* ip_info = (esp_netif_ip_info_t*)event_data;
  logD("AP: sta connected " IPSTR, IP2STR(&ip_info->ip));
  tcp_server_stop();
  tcp_server_start();
  }
static void
on_wifi_ap_disconnected(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data)
  {
  esp_netif_ip_info_t* ip_info = (esp_netif_ip_info_t*)event_data;
  logD("AP: sta disconnected " IPSTR, IP2STR(&ip_info->ip));
  tcp_server_stop();
  tcp_server_stop();
  }

/**
 * @brief Utility to log socket errors
 *
 * @param[in] tag Logging tag
 * @param[in] sock Socket number
 * @param[in] err Socket errno
 * @param[in] message Message to print
 */
static void
log_socket_error(const char* tag, const int sock, const int err,
  const char* message)
  {
  logE("%s: [sock=%d]: %s error=%d: %s", tag, sock, message, err,
    strerror(err));
  }

static void
log_progress()
  {
  static char stat[] = { '\\', '|', '/', '-' };
  static uint8_t idx;
  if (++idx >= 4)
    idx = 0;
  printf("%c\r", stat[idx]);
  }

/**
 * @brief Tries to receive data from specified sockets in a non-blocking way,
 *        i.e. returns immediately if no data.
 *
 * @param[in] tag Logging tag
 * @param[in] sock Socket for reception
 * @param[out] data Data pointer to write the received data
 * @param[in] max_len Maximum size of the allocated space for receiving data
 * @return
 *          >0 : Size of received data
 *          =0 : No data available
 *          -1 : Error occurred during socket read operation
 *          -2 : Socket is not connected, to distinguish between an actual
 * socket error and active disconnection
 */
static int
try_receive(const char* tag, const int sock, char* data, size_t max_len)
  {
  int len = recv(sock, data, max_len, 0);
  if (len < 0)
    {
    if (errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK)
      {
      return 0;
      }
    if (errno == ENOTCONN)
      {
      logW("[sock=%d]: Connection closed", sock);
      return -2;
      }
    log_socket_error(__FUNCTION__, sock, errno,
      "Error occurred during receiving");
    return -1;
    }
  logI("len: %d", len);
  return len;
  }

/**
 * @brief Sends the specified data to the socket. This function blocks until
 * all bytes got sent.
 *
 * @param[in] tag Logging tag
 * @param[in] sock Socket to write data
 * @param[in] data Data to be written
 * @param[in] len Length of the data
 * @return
 *          >0 : Size the written data
 *          -1 : Error occurred during socket write operation
 */
extern size_t
tcp_server_send(const int sock, const char* data, const size_t len)
  {
  logD("sending: %d; %s", len, data);
  int to_write = len;
  while (to_write > 0)
    {
    int written = send(sock, data + (len - to_write), to_write, 0);
    if (written < 0 && errno != EINPROGRESS && errno != EAGAIN
      && errno != EWOULDBLOCK)
      {
      log_socket_error(TAG, sock, errno, "Error occurred during sending");
      return -1;
      }
    logD("sent %d", written);
    if (_notify != NULL)
      _notify(DrvDataSent, written);
    to_write -= written;
    }
  logD("sent %d", len - to_write);
  return len;
  }

/**
 * @brief Returns the string representation of client's address (accepted on
 * this server)
 */
static inline char*
get_clients_address(struct sockaddr_storage* source_addr)
  {
  static char address_str[128];
  char* res = NULL;
  // Convert ip address to string
  if (source_addr->ss_family == PF_INET)
    {
    res = inet_ntoa_r(((struct sockaddr_in*)source_addr)->sin_addr,
      address_str, sizeof(address_str) - 1);
    }
#ifdef CONFIG_LWIP_IPV6
  else if (source_addr->ss_family == PF_INET6)
    {
    res = inet6_ntoa_r(((struct sockaddr_in6*)source_addr)->sin6_addr,
      address_str, sizeof(address_str) - 1);
    }
#endif
  if (!res)
    {
    address_str[0]
      = '\0'; // Returns empty string if conversion didn't succeed
    }
  return address_str;
  }

static void
tcp_server_task(void* pvParameters)
  {
  addr_family = (int)pvParameters;
  while (1)
    {
    if (listen_sock == INVALID_SOCK)
      vTaskSuspend(0);

    struct sockaddr_storage
      source_addr; // Large enough for both IPv4 or IPv6
    socklen_t addr_len = sizeof(source_addr);
    // Find a free socket
    int new_sock_index = 0;
    for (new_sock_index = 0; new_sock_index < _max_socks; ++new_sock_index)
      {
      if (sock[new_sock_index] == INVALID_SOCK)
        {
        break;
        }
      }
    if (new_sock_index < _max_socks)
      {
      sock[new_sock_index] = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
      _peerId = new_sock_index;

      if (sock[new_sock_index] < 0)
        {
        if (errno == EWOULDBLOCK)
          {
          // sock[new_sock_index] = INVALID_SOCK;
          logD("No pending connections...");
          }
        else
          {
          log_socket_error(__FUNCTION__, listen_sock, errno,
            "Error when accepting connection");
          goto error;
          }
        }
      else
        {
        // We have a new client connected -> print it's address
        logI("[sock=%d]: Connection accepted from IP:%s, "
          "new_sock_index = %d",
          sock[new_sock_index], get_clients_address(&source_addr),
          new_sock_index);

        // ...and set the client's socket non-blocking
        int flags = fcntl(sock[new_sock_index], F_GETFL);
        if (fcntl(sock[new_sock_index], F_SETFL, flags | O_NONBLOCK) == -1)
          {
          log_socket_error(__FUNCTION__, sock[new_sock_index], errno, "Unable to set socket non blocking");
          goto error;
          }
        logI("[sock=%d]: Socket marked as non blocking", sock[new_sock_index]);
        if (_notify != NULL)
          _notify(PeerConnected, sock[new_sock_index]);
        }
      }
    // We serve all the connected clients in this loop
    for (int i = 0; i < _max_socks; ++i)
      {
      if (sock[i] != INVALID_SOCK)
        {
        int len = try_receive(TAG, sock[i], rx_buffer, sizeof(rx_buffer));
        if (len < 0)
          {
          // Error occurred within this client's socket -> close and
          // mark invalid
          logD("[sock=%d]: try_receive() returned %d ", sock[i], len);
          if (_notify != NULL)
            _notify(PeerDisconnected, sock[i]);
          logD("Closing sock = %d at i:%d :", sock[i], i);
          close(sock[i]);
          sock[i] = INVALID_SOCK;
          }
        else if (len > 0)
          {
          // Received some data -> echo back
          logI("[sock=%d]: Received %d in %0.2x", sock[i], len, rx_buffer);
          ESP_LOG_BUFFER_HEXDUMP(TAG, rx_buffer, len, ESP_LOG_INFO);
          if (_notify != NULL)
            _notify(DrvDataReceived, rx_buffer, len);
          }

        } // one client's socket
      }     // for all sockets
    // Yield to other tasks
    log_progress();
    vTaskDelay(pdMS_TO_TICKS(YIELD_TO_ALL_MS));
    }

error:
  log_socket_error(__FUNCTION__, __LINE__, errno, "ERROR");
  tcp_server_stop();
  }
