#include "http.h"
#include "wifi.h"
#include "sdkconfig.h"

#include "fnEsp.h"
#include "esp_ota.h"


static esp_err_t config_post_ap_save(void *arg);
static esp_err_t config_post_sta_save(void *arg);
static esp_err_t config_post_reboot(void *arg);

static struct Cmd _commands[] = {
    {.cmd = "apsave", .fn = config_post_ap_save},
    {.cmd = "stasave", .fn = config_post_sta_save},
    {.cmd = "reboot", .fn = config_post_reboot},
    {.cmd = NULL, .fn = NULL}};

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0); // Response body can be empty
    return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */

static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    //    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    //    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    extern const unsigned char favicon_ico_start[] asm("_binary_jooby_yellow_fav_png_start");
    extern const unsigned char favicon_ico_end[] asm("_binary_jooby_yellow_fav_png_end");

    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

static esp_err_t config_get_send_header(httpd_req_t *req)
{
    /* Send HTML file header */
    extern const unsigned char header_start[] asm("_binary_header_html_start");
    extern const unsigned char header_end[] asm("_binary_header_html_end");
    httpd_resp_send_chunk(req, (const char *)header_start, (header_end - header_start));
    httpd_resp_sendstr_chunk(req, "<body><div><h2>");
    httpd_resp_sendstr_chunk(req, Esp_Ota_getName());
    httpd_resp_sendstr_chunk(req, "</h2></div>");
    return ESP_OK;
}
static esp_err_t config_get_send_footer(httpd_req_t *req)
{
    /* Send HTML file header */
    extern const unsigned char footer_start[] asm("_binary_footer_wifi_html_start");
    extern const unsigned char footer_end[] asm("_binary_footer_wifi_html_end");
    httpd_resp_send_chunk(req, (const char *)footer_start, (footer_end - footer_start));
    return ESP_OK;
}
static esp_err_t config_get_send_version(httpd_req_t *req)
{
    httpd_resp_sendstr_chunk(req, "<div></h3>Version: <b>");
    httpd_resp_sendstr_chunk(req, Esp_Ota_getVersion());
    httpd_resp_sendstr_chunk(req, "</h3><div><br>");
    return ESP_OK;
}
static esp_err_t config_get_send_tabs(httpd_req_t *req)
{

    return httpd_resp_sendstr_chunk(req, "<div class=\"tab\"> \
  <button class=\"tablinks\" onclick=\"openTab(event, id, \'ApSettings\')\" id=\"idApSet\">Access point settings</button> \
  <button class=\"tablinks\" onclick=\"openTab(event, id, \'StaSettings\')\" id=\"idStaSet\">Client settings</button> \
  <button class=\"tablinks\" onclick=\"openTab(event, id, \'WiFiReset\')\" id=\"idReboot\">Wifi reset</button> \
</div>");
}
static esp_err_t config_get_send_ap_settings(httpd_req_t *req)
{
    char *ap_settings_form =
        "<form method=\"POST\" action=\"/config?apsave\">Hide SSID: \
        <select name=\"hidden\"> \
            <option value=\"1\" %s>Enabled</option> \
            <option value=\"0\" %s>Disabled</option> \
        </select> <br><br> \
        <input type=\"submit\" value=\"Save sittings\"></form></div>";
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    // Wifi AP settings
    wifi_config_t cfg;
    wifi_ap_get_config(&cfg);
    sprintf(chunk, "<div id=\"ApSettings\" class=\"tabcontent\"><br>SSID: <b>%s </b><br>", cfg.ap.ssid);
    httpd_resp_sendstr_chunk(req, chunk);
    (cfg.ap.ssid_hidden) ? sprintf(chunk, ap_settings_form, "selected", "") : sprintf(chunk, ap_settings_form, "", "selected");
    httpd_resp_sendstr_chunk(req, chunk);
    return ESP_OK;
}
static esp_err_t config_get_send_sta_settings(httpd_req_t *req)
{
    char *sta_settings_form = "<form method=\"POST\" action=\"/config?stasave\">STA mode: \
    <select name=\"staEnabled\"> \
        <option value=\"1\" %s = \"1\">Enabled</option> \
        <option value=\"0\" %s = \"0\">Disabled</option> \
    </select><br> \
    WiFi Network: <input type=\"text\" name=\"ssid\" value=\"\"><br> \
    WiFi Password: <input type=\"password\" name=\"password\" value=\"\" id=\"pass_id\"><br> \
    <input type=\"checkbox\" onclick=\"fnShowPass()\">Show Password<br> \
    <input type=\"submit\" value=\"Save settings\"></form></div>";
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    wifi_config_t cfg;

    // Wifi sta settings
    wifi_sta_get_config(&cfg);
    sprintf(chunk, "<div id=\"StaSettings\" class=\"tabcontent\"><br>"
                   "Network: <b>%s </b><br>",
            cfg.sta.ssid);
    httpd_resp_sendstr_chunk(req, chunk);
    char *sta_status = wifi_sta_get_status();
    sprintf(chunk, "Status: <b>%s</b><br>", sta_status);
    httpd_resp_sendstr_chunk(req, chunk);
    if (!strncmp(sta_status, "Connected", strlen("Connected")))
    {
        sprintf(chunk, "IP:<a href=\"http://%s\" target=\"_blank\" ><b>  %s</b></a><br>", wifi_get_ip_str(), wifi_get_ip_str());
        httpd_resp_sendstr_chunk(req, chunk);
        sprintf(chunk, "MAC:<b>  %s</b>", wifi_get_mac_str());
        httpd_resp_sendstr_chunk(req, chunk);
    }
    (wifi_sta_get_enable()) ? sprintf(chunk, sta_settings_form, "selected", "") : sprintf(chunk, sta_settings_form, "", "selected");
    httpd_resp_sendstr_chunk(req, chunk);
    return ESP_OK;
}
static esp_err_t config_get_send_reset(httpd_req_t *req)
{
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    sprintf(chunk, "<div id=\"WiFiReset\" class=\"tabcontent\"><br><form method=\"POST\" action=\"/config?reboot\"><input type=\"SUBMIT\" value=\"Reboot\"></form></div>");
    httpd_resp_sendstr_chunk(req, chunk);
    return ESP_OK;
}

extern esp_err_t config_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));

    ESP_LOGI("", "%s:%d: %s: %s %s %s ", __FILE__, __LINE__, __FUNCTION__, req->uri, filepath, filename);
    if (strcmp(filename, "/index.html") == 0)
    {
        return index_html_get_handler(req);
    }
    else if (strcmp(filename, "/favicon.ico") == 0)
    {
        return favicon_get_handler(req);
    }

    config_get_send_header(req);
    config_get_send_version(req);
    config_get_send_tabs(req);
    config_get_send_ap_settings(req);
    config_get_send_sta_settings(req);
    config_get_send_reset(req);
    config_get_send_footer(req);
    // Clear user data buffer
    bzero(((struct file_server_data *)req->user_ctx)->scratch, SCRATCH_BUFSIZE);
    return httpd_resp_sendstr_chunk(req, NULL);
    ;
}


static esp_err_t config_post_ap_save(void *arg)
{
    char *pvalue = strchr((char *)arg, '=');
    ESP_LOGI(__FILE__, "%d: %s: %02x %s", __LINE__, __FUNCTION__, pvalue, pvalue);
    uint8_t hidden = (uint8_t)atoi(++pvalue);

    ESP_LOGI(__FILE__, "%d: %s: %s, %d", __LINE__, __FUNCTION__, arg, hidden);
    return FnEsp_setApHidden(&hidden);
    ;
}
static esp_err_t config_post_sta_save(void *arg)
{
    char *str_en = 0, ssid_len = 0, pass_len = 0;
    str_en = http_get_value_string(arg, NULL);
    uint8_t en = (uint8_t)atoi(str_en);
    char *ssid = http_get_value_string(str_en, (uint8_t *)&ssid_len);
    char *pass = http_get_value_string(ssid, (uint8_t *)&pass_len);
    ESP_LOGI(__FILE__, "%d %s: Gor settings: en:%d, ssid: %s: password: %s", __LINE__, __FUNCTION__, en, ssid, pass);
    size_t buflen = sizeof(en) + ssid_len + 1 + pass_len + 1;
    char *buf = malloc(buflen);
    int i = 0;
    buf[i++] = en;
    buf[i++] = ssid_len;
    memcpy(&buf[i], ssid, ssid_len);
    i += ssid_len;
    buf[i++] = pass_len;
    memcpy(&buf[i], pass, pass_len);
    esp_err_t err = FnEsp_setStaSettings((void *)buf);
    free(buf);
    buf = NULL;

    return err;
}
static esp_err_t config_post_reboot(void *arg)
{
    // ESP_LOGI(__FILE__, "%d: Gor user context: %s: %s", __LINE__, __FUNCTION__, arg);
    FnEsp_systemReset();
    return ESP_OK;
}

extern esp_err_t config_post_handler(httpd_req_t *req)
{
    ESP_LOGI(__FILE__, "%d: %s: %s", __LINE__, __FUNCTION__, req->uri);
    ESP_LOGI(__FILE__, "%d: %s: %d", __LINE__, __FUNCTION__, req->content_len);

    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    char cmd[16] = "\0";
    size_t cmdlen = http_cmd_from_uri(req->uri, cmd);
    if (cmdlen > 0)
    {
        ESP_LOGI(__FILE__, "%d: %s: %s", __LINE__, __FUNCTION__, cmd);

        if (0 < req->content_len)
        {
            int received;
            bzero(buf, SCRATCH_BUFSIZE);
            if ((received = httpd_req_recv(req, buf, req->content_len)) <= 0)
            {
                ESP_LOGE(__FILE__, "Reception failed!");
                // Respond with 500 Internal Server Error
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
                return ESP_FAIL;
            }
            ESP_LOGD(__FILE__, "%d: Gor user context: %s: %s", __LINE__, __FUNCTION__, buf);
        }

        int i = 0;
        while (NULL != _commands[i].cmd)
        {
            ESP_LOGD(__FILE__, "%d: %s: %s", __LINE__, __FUNCTION__, cmd);
            ESP_LOGD(__FILE__, "%d: %s: %s", __LINE__, __FUNCTION__, _commands[i].cmd);
            if (0 == strncmp(cmd, _commands[i].cmd, cmdlen))
            {
                if (_commands[i].fn)
                {
                    if (0 > _commands[i].fn(buf))
                    {
                        bzero(buf, SCRATCH_BUFSIZE);
                        sprintf(buf, "Error executing: %s !", _commands[i].cmd);
                        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, buf);
                        ESP_LOGE(__FILE__, "%s", buf);
                        break;
                    }
                }
                httpd_resp_set_status(req, "303 See Other");
                httpd_resp_set_hdr(req, "Location", "/");
                break;
            }
            i++;
        }

        if (i >= sizeof(_commands) / sizeof(struct Cmd))
        {
            bzero(buf, SCRATCH_BUFSIZE);
            sprintf(buf, "Unsupported command: %s !", cmd);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, buf);
            ESP_LOGE(__FILE__, "%s", buf);
        }
    }

#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif

    // httpd_resp_sendstr(req, "Settings set OK");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}