#include "http.h"
#include "esp_ota.h"
#include "luna_ota.h"
#include "wifi.h"

#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s(): " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOG_BUF(buf, len) ESP_LOG_BUFFER_HEXDUMP(__FILE__, buf, len, ESP_LOG_INFO)

extern esp_err_t update_get_handler(httpd_req_t *req)
{
    int esp_ota_status = Esp_Ota_getStatus();
    int ota_progress = Esp_Ota_getProgress();
    int ota_length = Esp_Ota_getFwLength();
    bool need_upload = !(Esp_Ota_getUpdateAvailable() && luna_ota_getUpdateAvailable());

    extern const unsigned char header_start[] asm("_binary_header_html_start");
    extern const unsigned char header_end[] asm("_binary_header_html_end");
    httpd_resp_send_chunk(req, (const char *)header_start, (header_end - header_start));

    /* Add file upload form and script which on execution sends a POST request to /upload */
    extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
    extern const unsigned char upload_script_end[] asm("_binary_upload_script_html_end");

    if ((esp_ota_status > 0) || (luna_ota_get_progress()))
    {
        httpd_resp_sendstr_chunk(req, " <meta http-equiv=\"refresh\" content=\"2\">");
    }

    httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_end - upload_script_start);
    extern const unsigned char script_start[] asm("_binary_redirect_js_start");
    extern const unsigned char script_end[] asm("_binary_redirect_js_end");
    httpd_resp_send_chunk(req, (const char *)script_start, (script_end - script_start));

    httpd_resp_sendstr_chunk(req, "<body>"
                                  "<h2>Firmware update</h2>"
                                  "<table class=\"fixed\" border=\"1\"><col width=\"100px\" /><col width=\"250px\" /><col width=\"70px\" />"
                                  "<tbody>");

    uint8_t addr[6] = {0};
    if (0 < zhaga_cent_get_paired_addr(addr, sizeof(addr)))
    {
        char str_addr[17] = "xx-xx-xx-xx-xx-xx";
        sprintf(str_addr, "%hhx-%hhx-%hhx-%hhx-%hhx-%hhx",
                addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

        httpd_resp_sendstr_chunk(req, "<div>Paired: <b> ");
        httpd_resp_sendstr_chunk(req, zhaga_cent_get_paired_name());
        httpd_resp_sendstr_chunk(req, "</b><br>Status: <b> ");
        httpd_resp_sendstr_chunk(req, zhaga_cent_get_status());
        httpd_resp_sendstr_chunk(req, "</div>");
    }

    /*
        httpd_resp_sendstr_chunk(req, "<tr><td>Current firmware: </td><td colspan=\"3\"> version:  ");
        httpd_resp_sendstr_chunk(req, Esp_Ota_getVersion());
        httpd_resp_sendstr_chunk(req, "( ");
        httpd_resp_sendstr_chunk(req, Esp_Ota_getBuildDate());
        httpd_resp_sendstr_chunk(req, " )</td></tr>");
    */
    httpd_resp_sendstr_chunk(req, "<tr><td>ESP32 update: </td>");

    if (Esp_Ota_getUpdateAvailable())
    {
        httpd_resp_sendstr_chunk(req, "<td> version: ");
        httpd_resp_sendstr_chunk(req, Esp_Ota_getUpdateVersion());
        httpd_resp_sendstr_chunk(req, " ( ");
        httpd_resp_sendstr_chunk(req, Esp_Ota_getUpdateDate());
        httpd_resp_sendstr_chunk(req, " )");
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete/");
        httpd_resp_sendstr_chunk(req, Esp_Ota_getUpdateFname());
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form></td>");

        if (esp_ota_status > 0)
        {
            char *str_progress =
                "<td><label for=\"fw_update\">Updating...</label><br>"
                "<progress id=\"fw_update\" max=\"%d\" value=\"%d\"></progress></td>";
            char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
            sprintf(chunk, str_progress, ota_length, ota_progress);
            httpd_resp_sendstr_chunk(req, chunk);
        }
        else
        {
            httpd_resp_sendstr_chunk(req, "<td><form method=\"post\" action=\"/update/");
            httpd_resp_sendstr_chunk(req, Esp_Ota_getUpdateFname());
            httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Update</button></form></td>");
        }
        httpd_resp_sendstr_chunk(req, "</tr>");
    }
    else
    {
        httpd_resp_sendstr_chunk(req, "<td colspan=\"2\">"
                                      "No updates available."
                                      "</td></tr>");
    }

    httpd_resp_sendstr_chunk(req, "<tr><td>Luna update: </td>");
    if (luna_ota_getUpdateAvailable())
    {
        httpd_resp_sendstr_chunk(req, "<td> version: ");
        httpd_resp_sendstr_chunk(req, luna_ota_getUpdateVersion());
        httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete/");
        httpd_resp_sendstr_chunk(req, luna_ota_getUpdateFname());
        httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form></td>");
        if (luna_ota_get_progress())
        {
            char *luna_progress =
                "<td><label for=\"luna_update\">%s</label><br>"
                "<progress id=\"luna_update\" max=\"%d\" value=\"%d\"></progress></td>";
            char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
            sprintf(chunk, luna_progress, luna_ota_get_status(), luna_ota_get_fwLength(), luna_ota_get_progress());
            httpd_resp_sendstr_chunk(req, chunk);
        }
        else
        {
            httpd_resp_sendstr_chunk(req, "<td><form method=\"post\" action=\"/update/");
            httpd_resp_sendstr_chunk(req, luna_ota_getUpdateFname());
            httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Update</button></form></td>");
        }
        httpd_resp_sendstr_chunk(req, "</tr>");
    }
    else
    {
        httpd_resp_sendstr_chunk(req, "<td colspan=\"2\">"
                                      "No updates available."
                                      "</td></tr>");
    }
    httpd_resp_sendstr_chunk(req, "</tbody></table>");
    if (need_upload)
        httpd_resp_sendstr_chunk(req, "<div> Select file to upload</div>"
                                      "<input id=\"upload_file\" type=\"file\" onchange=\"upload(id)\">"
                                      "<div hidden id=\"div_load\">"
                                      "<label for=\"upload_progress\">Uploading...</label><br>"
                                      "<progress id=\"upload_progress\"max=\"100\" value=\"0\"></progress></div>");

    httpd_resp_sendstr_chunk(req, "<button type=\"button\" onclick=redirectFunc(\"/bt\")>Goto BT page </button><br>");
    // Send remaining chunk of HTML file to complete it
    httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
    return httpd_resp_sendstr_chunk(req, NULL);
}

extern esp_err_t update_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    char *const filename = (char *) get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/update") - 1, sizeof(filepath));

    LOGI("%s %s", filepath, filename);
    LOGI("%s %s", req->uri, filename);
    luna_ota_start(filename);
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/update");
    return httpd_resp_sendstr_chunk(req, NULL);
}


extern esp_err_t upload_post_handler(httpd_req_t *req)
{
    FILE *fd = NULL;
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));

    LOGI("%s %s", filepath, filename);
    if (!filename)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    if (filename[strlen(filename) - 1] == '/')
    {
        ESP_LOGE(__FILE__, "%d: Invalid filename : %s", __LINE__, filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0)
    {
        ESP_LOGE(__FILE__, "$d: File already exists : %s", __LINE__, filepath);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    if (req->content_len > MAX_FILE_SIZE)
    {
        ESP_LOGE(__FILE__, "%d: File too large : %d bytes", __LINE__, req->content_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than " MAX_FILE_SIZE_STR "!");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd)
    {
        ESP_LOGE(__FILE__, "%d: Failed to create file : %s", __LINE__, filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    LOGI("Receiving file : %s...", filename);
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;
    int remaining = req->content_len;

    while (remaining > 0)
    {

        ESP_LOGD(__FILE__, "%d: Remaining size : %d", __LINE__, remaining);
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(__FILE__, "File reception failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }
        if (received && (received != fwrite(buf, 1, received, fd)))
        {
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(__FILE__, "File write failed!");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }
        remaining -= received;
    }

    fclose(fd);
    ESP_LOGI(__FILE__, "%d: File reception complete", __LINE__);

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/update");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

/* Handler to delete a file from the server */
extern esp_err_t delete_post_handler(httpd_req_t *req)
{
    LOGD("%s", req->uri);
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;
    char * filename = (char *) get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                       req->uri + sizeof("/delete") - 1, sizeof(filepath));
    LOGD("%s, %s ", req->uri, filename);
    if (!filename)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }
    if (filename[strlen(filename) - 1] == '/')
    {
        ESP_LOGW(__FILE__, "Invalid filename : %s", filename);
    }

    if (stat(filename, &file_stat) == -1)
    {
        ESP_LOGE(__FILE__, "File does not exist : %s", filename);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }
    LOGD("Deleting file : %s", filename);
    Esp_Ota_clearUpdateInfo(filename);
    luna_ota_clear_info(filename);

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/update");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}
