/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* HTTP File Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "http.h"
#include "ws.h"

#include "esp_ota_ops.h"
#include "esp_partition.h"

#define LOGI(format, ...)                                                     \
  ESP_LOGI ("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__,       \
            ##__VA_ARGS__)
#define LOGD(format, ...)                                                     \
  ESP_LOGD ("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__,       \
            ##__VA_ARGS__)
#define LOGW(format, ...)                                                     \
  ESP_LOGW ("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__,       \
            ##__VA_ARGS__)
#define LOGE(format, ...)                                                     \
  ESP_LOGE ("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__,       \
            ##__VA_ARGS__)

#define IS_FILE_EXT(filename, ext)                                            \
  (strcasecmp (&filename[strlen (filename) - sizeof (ext) + 1], ext) == 0)

static httpd_handle_t _server = NULL;
static const httpd_uri_t ws = { .uri = "/ws",
                                .method = HTTP_GET,
                                .handler = ws_handler,
                                .user_ctx = NULL,
                                .is_websocket = true };

/* Function to start the file server */
esp_err_t
Http_server_start (const char *base_path)
{
  static struct file_server_data *server_data = NULL;

  if (server_data)
    {
      LOGE ("File server already started");
      return ESP_ERR_INVALID_STATE;
    }
  LOGD ("%s", base_path);

  /* Allocate memory for server data */
  server_data = calloc (1, sizeof (struct file_server_data));
  if (!server_data)
    {
      LOGE ("Failed to allocate memory for server data");
      return ESP_ERR_NO_MEM;
    }
  strlcpy (server_data->base_path, base_path, sizeof (server_data->base_path));

  httpd_config_t config = HTTPD_DEFAULT_CONFIG ();
  config.max_uri_handlers = 16;
  config.max_resp_headers = config.max_uri_handlers;

  config.uri_match_fn = httpd_uri_match_wildcard;

  LOGD ("Starting HTTP Server on port: '%d'", config.server_port);
  if (httpd_start (&_server, &config) != ESP_OK)
    {
      LOGE ("Failed to start file server!");
      return ESP_FAIL;
    }
    LOGI ("HTTP Server on port: '%d'", config.server_port);
  httpd_uri_t file_config = { .uri = "/",
                              .method = HTTP_GET,
                              .handler = config_get_handler,
                              .user_ctx = server_data };

  httpd_register_uri_handler (_server, &file_config);
  file_config.uri = "/config*";
  file_config.method = HTTP_POST;
  file_config.handler = config_post_handler;
  file_config.user_ctx = server_data;
  httpd_register_uri_handler (_server, &file_config);

  file_config.uri = "/update*";
  file_config.method = HTTP_GET;
  file_config.handler = update_get_handler;
  file_config.user_ctx = server_data;
  httpd_register_uri_handler (_server, &file_config);

  file_config.uri = "/update/*";
  file_config.method = HTTP_POST;
  file_config.handler = update_post_handler;
  file_config.user_ctx = server_data;
  httpd_register_uri_handler (_server, &file_config);

  file_config.uri = "/bt*";
  file_config.method = HTTP_GET;
  file_config.handler = btconfig_get_handler;
  file_config.user_ctx = server_data;
  httpd_register_uri_handler (_server, &file_config);

  file_config.uri = "/bt*";
  file_config.method = HTTP_POST;
  file_config.handler = btconfig_post_handler;
  file_config.user_ctx = server_data;
  httpd_register_uri_handler (_server, &file_config);

  file_config.uri = "/dali*";
  file_config.method = HTTP_GET;
  file_config.handler = dali_get_handler;
  file_config.user_ctx = server_data;
  httpd_register_uri_handler (_server, &file_config);

  httpd_register_uri_handler (_server, &ws);


  /// URI handler for getting uploaded files
  httpd_uri_t file_download = {
    .uri = "/files/*", // Match all URIs of type /path/to/file
    .method = HTTP_GET,
    .handler = download_get_handler,
    .user_ctx = server_data // Pass server data as context
  };
  httpd_register_uri_handler (_server, &file_download);

  /// URI handler for uploading files to server
  httpd_uri_t file_upload = {
    .uri = "/upload/*", // Match all URIs of type /upload/path/to/file
    .method = HTTP_POST,
    .handler = upload_post_handler,
    .user_ctx = server_data // Pass server data as context
  };
  httpd_register_uri_handler (_server, &file_upload);

  /* URI handler for deleting files from server */
  httpd_uri_t file_delete = {
    .uri = "/delete/*", // Match all URIs of type /delete/path/to/file
    .method = HTTP_POST,
    .handler = delete_post_handler,
    .user_ctx = server_data // Pass server data as context
  };
  httpd_register_uri_handler (_server, &file_delete);

  return ESP_OK;
}

/* Set HTTP response content type according to file extension */
extern esp_err_t
set_content_type_from_file (httpd_req_t *req, const char *filename)
{
  if (IS_FILE_EXT (filename, ".pdf"))
    {
      return httpd_resp_set_type (req, "application/pdf");
    }
  else if (IS_FILE_EXT (filename, ".html"))
    {
      return httpd_resp_set_type (req, "text/html");
    }
  else if (IS_FILE_EXT (filename, ".jpeg"))
    {
      return httpd_resp_set_type (req, "image/jpeg");
    }
  else if (IS_FILE_EXT (filename, ".ico"))
    {
      return httpd_resp_set_type (req, "image/x-icon");
    }
  /* This is a limited set only */
  /* For any other type always set as plain text */
  return httpd_resp_set_type (req, "text/plain");
}
