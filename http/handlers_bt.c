#include "esp_log.h"

#include "wifi.h"
#include "http.h"
#include "sdkconfig.h"
#include "zhaga_central.h"

#define BT_URI "/bt/"
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

static esp_err_t bt_set_config (void *arg);
static esp_err_t bt_connect (void *arg);
static esp_err_t bt_bind_connect (void *arg);
static esp_err_t bt_disconnect (void *arg);
static esp_err_t bt_set_autoconnect (void *arg);
static esp_err_t bt_reboot (void *arg);
static esp_err_t bt_scan (void *arg);

static struct Cmd _commands[]
    = { { .cmd = "scan", .fn = bt_scan },
        { .cmd = "bind_connect", .fn = bt_bind_connect },
        { .cmd = "connect", .fn = bt_connect },
        { .cmd = "disconnect", .fn = bt_disconnect },
        { .cmd = "set_autoconnect", .fn = bt_set_autoconnect },
        { .cmd = "set_config", .fn = bt_set_config },
        { .cmd = "reboot", .fn = bt_reboot },
        { .cmd = "", .fn = NULL } };

static esp_err_t
bt_get_header (httpd_req_t *req)
{
  /* Send HTML file header */
  extern const unsigned char header_start[] asm ("_binary_header_html_start");
  extern const unsigned char header_end[] asm ("_binary_header_html_end");
  httpd_resp_send_chunk (req, (const char *)header_start,
                         (header_end - header_start));
  httpd_resp_sendstr_chunk (req, "<body><div><h2>"
                                 "Zhaga bluetooth"
                                 "</h2></div>");
  return ESP_OK;
}

static esp_err_t
bt_get_footer (httpd_req_t *req)
{
  extern const unsigned char script_start[] asm ("_binary_redirect_js_start");
  extern const unsigned char script_end[] asm ("_binary_redirect_js_end");
  httpd_resp_send_chunk (req, (const char *)script_start,
                         (script_end - script_start));

  extern const unsigned char footer_start[] asm (
      "_binary_footer_bt_html_start");
  extern const unsigned char footer_end[] asm ("_binary_footer_bt_html_end");
  httpd_resp_send_chunk (req, (const char *)footer_start,
                         (footer_end - footer_start));
  return ESP_OK;
}

static esp_err_t
bt_get_paired (httpd_req_t *req)
{
  /*
  char str_addr[17] = "xx-xx-xx-xx-xx-xx";
  char *paired_name = zhaga_cent_get_paired_name();
  char *status = zhaga_cent_get_status();
  bool autoconnect = zhaga_cent_get_autoconnect();

  httpd_resp_sendstr_chunk(req, "<div><hr><table>"
                                "<tr><td>Status:</td><td><b> ");
  httpd_resp_sendstr_chunk(req, status);
  httpd_resp_sendstr_chunk(req, "</b></td></tr>");
  uint8_t addr[6] = {0};
  if (0 < zhaga_cent_get_paired_addr(addr, sizeof(addr)))
  {
      sprintf(str_addr, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx",
              addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
      httpd_resp_sendstr_chunk(req, "<tr><td>Paired device:</td>");
      httpd_resp_sendstr_chunk(req, "<form id=\"form_paired\">");
      httpd_resp_sendstr_chunk(req, "<td><b><input type=\"text\" name=\"addr\"
  value=\"");
      //httpd_resp_sendstr_chunk(req, str_addr);
      httpd_resp_sendstr_chunk(req, paired_name);

      httpd_resp_sendstr_chunk(req, "\"readonly/>");
      httpd_resp_sendstr_chunk(req, "</b></td></tr>");

      httpd_resp_sendstr_chunk(req, "<tr><td></td><td>"
                                    "<button type=\"submit\"
  formmethod=\"POST\" formaction=\"/bt?connect\">Connect</button>"
                                    "<button type=\"submit\"
  formmethod=\"POST\" formaction=\"/bt?disconnect\">Disconnect</button>"
                                    "</form></td></tr>");
  }
  httpd_resp_sendstr_chunk(req, "<form method=\"POST\"
  action=\"/bt?set_autoconnect\"><tr><td>Autoconnect:</td><td>"
                                "<input id=\"id_auto\" type=\"text\"
  name=\"auto_connect\" value=\" \" hidden />"
                                "<input type=\"checkbox\"
  name=\"check_autoconnect\" "); httpd_resp_sendstr_chunk(req, autoconnect ? "
  checked " : " ");

  httpd_resp_sendstr_chunk(req, "onchange=\"id_auto.value = this.checked;
  form.submit()\"/></form></td></tr>"); httpd_resp_sendstr_chunk(req,
  "</table></div>");
  */
  extern const unsigned char bt_start[] asm ("_binary_bt_html_start");
  extern const unsigned char bt_end[] asm ("_binary_bt_html_end");
  httpd_resp_send_chunk (req, (const char *)bt_start, (bt_end - bt_start));
  return ESP_OK;
}

__attribute__((unused)) static esp_err_t
bt_get_dev_list (httpd_req_t *req) 
{
  struct node_item *n;
  SLIST_HEAD (, node_item) *list = (struct node_item *)scan_get_nodes ();

  httpd_resp_sendstr_chunk (
      req, "<hr><div><table><form>"
           "<tr><td><label for=\"nodes\">Select a device:</label></td>"
           "<td><select name=\"nodes\" id=\"nodes\">\"");

  if (scan_get_count () > 0)
    {
      SLIST_FOREACH (n, list, next)
      {
        char *addr = addr_str (n->node.address.val);
        char rssi[] = { "-00 db" };
        sprintf (rssi, "%d db", n->node.rssi);

        LOGD ("%s\t<%s>,\trssi:%d", n->node.name,
              addr_str (n->node.address.val), n->node.rssi);
        httpd_resp_sendstr_chunk (req, "<option value=\"");
        httpd_resp_sendstr_chunk (req, addr);
        httpd_resp_sendstr_chunk (req, "\">");
        httpd_resp_sendstr_chunk (req, n->node.name);
        //            httpd_resp_sendstr_chunk(req, ", rssi: ");
        //            httpd_resp_sendstr_chunk(req, rssi);
        httpd_resp_sendstr_chunk (req, "</option>");
      }
    }
  httpd_resp_sendstr_chunk (
      req,
      "</select></td></tr>"
      "<tr><td>Passkey:</td>"
      "<td><input type=\"password\" name=\"bt_passkey\" "
      "id=\"pass_id\"></td></tr>"
      "<tr><td></td><td><input type=\"checkbox\" "
      "onclick=\"fnShowPass(pass_id)\">Show Password</td></tr>"
      "<tr><td></td><td><button formmethod=\"POST\" "
      "formaction=\"/bt?bind_connect\">Pair and connect</button></td></tr>"
      "<tr><td></td><td><button formmethod=\"POST\" "
      "formaction=\"/bt?scan\">Scan</td></tr>");
  //     httpd_resp_sendstr_chunk(req, );
  httpd_resp_sendstr_chunk (req, "</table></form></div>");
  return 0;
}

static esp_err_t
bt_get_utils (httpd_req_t *req)
{
  extern const unsigned char utils_start[] asm ("_binary_utils_html_start");
  extern const unsigned char utils_end[] asm ("_binary_utils_html_end");
  httpd_resp_send_chunk (req, (const char *)utils_start, (utils_end - utils_start));
  return ESP_OK;
}

static esp_err_t
btconfig_get_ws (httpd_req_t *req)
{

  extern const unsigned char ws_start[] asm ("_binary_ws_html_start");
  extern const unsigned char ws_end[] asm ("_binary_ws_html_end");
  httpd_resp_send_chunk (req, (const char *)ws_start, (ws_end - ws_start));
  return ESP_OK;
}

esp_err_t
btconfig_get_handler (httpd_req_t *req)
{
  bt_get_header (req);
  bt_get_paired (req);
  // bt_get_dev_list(req);
  // httpd_resp_sendstr_chunk(req, "<button type=\"button\"
  // onclick=redirectFunc(\"/update\")>Goto UPDATE page</button><br>");
  bt_get_utils(req);
  btconfig_get_ws (req);
  bt_get_footer (req);
  return httpd_resp_sendstr_chunk (req, NULL);
}

esp_err_t
btconfig_post_handler (httpd_req_t *req)
{
  LOGD ("%s, %d, %s", req->uri, req->content_len, req->user_ctx);

  char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
  char cmd[16] = "\0";
  size_t cmdlen = http_cmd_from_uri (req->uri, cmd);
  if (cmdlen > 0)
    {
      LOGD ("%d: %s", cmdlen, cmd);
      if (0 < req->content_len)
        {
          int received;
          bzero (buf, SCRATCH_BUFSIZE);
          if ((received = httpd_req_recv (req, buf, req->content_len)) <= 0)
            {
              LOGE ("Reception failed!");
              // Respond with 500 Internal Server Error
              httpd_resp_send_err (req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Failed to receive data");
              return ESP_FAIL;
            }
        }
      int i = 0;
      while (NULL != _commands[i].cmd)
        {
          LOGD ("%s", cmd);
          if (0 == strncmp (cmd, _commands[i].cmd, cmdlen))
            {
              if (_commands[i].fn)
                {
                  if (0 > _commands[i].fn (buf))
                    {
                      bzero (buf, SCRATCH_BUFSIZE);
                      sprintf (buf, "Error executing: %s !", _commands[i].cmd);
                      httpd_resp_send_err (req,
                                           HTTPD_500_INTERNAL_SERVER_ERROR, 0);
                      LOGE ("%s", buf);
                      break;
                    }
                }
              break;
            }
          i++;
        }
      if (i >= sizeof (_commands) / sizeof (struct Cmd))
        {
          bzero (buf, SCRATCH_BUFSIZE);
          sprintf (buf, "Unsupported command: %s !", cmd);
          httpd_resp_send_err (req, HTTPD_500_INTERNAL_SERVER_ERROR, buf);
          LOGE ("%s", buf);
        }
      else
        {
          httpd_resp_set_status (req, "303 See Other");
          httpd_resp_set_hdr (req, "Location", BT_URI);
        }
    }

  return httpd_resp_sendstr_chunk (req, NULL);
}
#define ADDR_STR_LEN (17)
esp_err_t
bt_set_config (void *arg)
{
  LOGI ("%s", arg);
  // char addr[ADDR_STR_LEN] = {0};
  char *addr = http_get_value_string (arg, NULL);
  // memcpy(addr, arg, ADDR_STR_LEN);
  uint8_t bt_addr[6];
  LOGI ("%s", addr);
  sscanf (addr, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", &bt_addr[5], &bt_addr[4],
          &bt_addr[3], &bt_addr[2], &bt_addr[1], &bt_addr[0]);
  char *pass = http_get_value_string ((char *)addr + ADDR_STR_LEN, NULL);
  uint32_t pk = atoi (pass);
  esp_err_t err = zhaga_cent_set_paired_addr (bt_addr);
  err += zhaga_cent_set_paired_passkey (pk);
  return err;
}

esp_err_t
bt_connect (void *arg)
{
  LOGI ("%s", addr_str (arg));
  zhaga_cent_connect_paired ();
  return ESP_OK;
}

esp_err_t
bt_bind_connect (void *arg)
{
  
  LOGI ("%s", arg);
  /*char *addr = http_get_value_string (arg, NULL);
  uint8_t bt_addr[6];
  LOGI ("%s", addr);
  sscanf (addr, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx", &bt_addr[5], &bt_addr[4],
          &bt_addr[3], &bt_addr[2], &bt_addr[1], &bt_addr[0]);
  char *pass = http_get_value_string ((char *)addr + ADDR_STR_LEN, NULL);
  uint32_t pk = atoi (pass);
  esp_err_t err = zhaga_cent_set_paired (bt_addr, pk);
  if (ESP_OK == err)
    {
      err += zhaga_cent_disconnect ();
      err += bt_connect (bt_addr);
    }
*/
  return 0;
}

esp_err_t
bt_disconnect (void *arg)
{
  esp_err_t err = zhaga_cent_disconnect ();
  if (err)
    LOGE ("%d", err);
  return err;
}

static esp_err_t
bt_set_autoconnect (void *arg)
{
  char *state = http_get_value_string (arg, NULL);
  strstr (state, "true") ? zhaga_cent_set_autoconnect (1)
                         : zhaga_cent_set_autoconnect (0);
  return ESP_OK;
}

esp_err_t
bt_reboot (void *arg)
{
  LOGI ("%s", arg);
  return ESP_FAIL;
}

static esp_err_t
bt_scan (void *arg)
{
  zhaga_cent_scan ();
  return ESP_OK;
}