#include "http.h"

size_t http_cmd_from_uri(const char *uri, char *buf)
{
    const char *start = strchr(uri, '?');
    const char *end = strchr(uri, '\0');
    const char *eq = strchr(uri, '=');
    size_t l = -1;
    if (start)
    {
        l = 0;
        ESP_LOGD(__FILE__, "%d: %s: %s", __LINE__, __FUNCTION__, start);
        if (((eq - start) > 0) || ((end - start) > 0))
        {
        }
        l = (eq) ? eq - (start + 1) : end - (start + 1);
        memcpy(buf, (start + 1), l);
    }

    return l;
}

char *http_get_value_string(void *uri, uint8_t *len)
{
    char *str = strchr((char *)uri, '=') + 1;
    ESP_LOGD("", "%s:%d: %s %s %02x", __FILE__, __LINE__, uri, str, len);
    if (len)
    {
        *len = MIN(strlen(str), strchr(str, '&') - (str));
        ESP_LOGD("", "%s:%d: %s: %d %s", __FILE__, __LINE__, __FUNCTION__, *len, str);
    }
    ESP_LOGD("", "%s:%d: %s: %02x %s", __FILE__, __LINE__, __FUNCTION__, len, str);
    return str;
}
