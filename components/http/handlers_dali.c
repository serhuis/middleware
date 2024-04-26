#include "http.h"

static esp_err_t dali_get_header(httpd_req_t *req)
{
    extern const unsigned char header_start[] asm("_binary_header_html_start");
    extern const unsigned char header_end[] asm("_binary_header_html_end");
    httpd_resp_send_chunk(req, (const char *)header_start, (header_end - header_start));
    httpd_resp_sendstr_chunk(req, "<body><div><h2>""Zhaga Dali CLI""</h2></div>");
    return ESP_OK;
}
static esp_err_t dali_get_cli(httpd_req_t *req)
{
    extern const unsigned char dali_start[] asm("_binary_dali_html_start");
    extern const unsigned char dali_end[] asm("_binary_dali_html_end");
    httpd_resp_send_chunk(req, (const char *)dali_start, (dali_end - dali_start));
    return ESP_OK;
}

static esp_err_t dali_get_ws(httpd_req_t *req)
{
    extern const unsigned char ws_start[] asm("_binary_ws_html_start");
    extern const unsigned char ws_end[] asm("_binary_ws_html_end");
    httpd_resp_send_chunk(req, (const char *)ws_start, (ws_end - ws_start));
    return 0;
}

static esp_err_t dali_get_footer(httpd_req_t *req)
{
    extern const unsigned char footer_start[] asm("_binary_footer_bt_html_start");
    extern const unsigned char footer_end[] asm("_binary_footer_bt_html_end");
    httpd_resp_send_chunk(req, (const char *)footer_start, (footer_end - footer_start));
    return ESP_OK;
}


esp_err_t dali_get_handler(httpd_req_t *req)
{
    dali_get_header(req);
    dali_get_ws(req);
    dali_get_cli(req);
    dali_get_footer(req);
    return httpd_resp_sendstr_chunk(req, NULL);
}
/*
esp_err_t dali_post_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "dali");
    return httpd_resp_sendstr_chunk(req, NULL);
}
*/