
#include <stdio.h>
#include "esp_log.h"
#include "utils.h"

size_t str2hex(char *str, const size_t str_len, uint8_t *arr, size_t arr_len)
{
    size_t pos = 0, i = 0;
    if (str_len > (arr_len << 1))
        return 0;
    for (; pos < str_len; pos += 2, i++)
    {
        if (!sscanf(str + pos, "%02hhx", &arr[i]))
            return 0;
    }
    return i;
}

size_t hex2str(uint8_t *const arr, const size_t arr_len, char *str, const size_t str_len)
{
    size_t pos = 0, i = 0;
    if (str_len < (arr_len << 1))
        return 0;
    for (; i < arr_len; i++)
        pos += sprintf(str + pos, "%02hhx", arr[i]);
    return pos;
}
