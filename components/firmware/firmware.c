#include "firmware.h"
#include "esp_log.h"

extern bool firmware_open(char *const filename, firmware_t *fw)
{
        FILE *fd = NULL;
    struct stat file_stat;

    if (stat(filename, &file_stat) == -1)
    {
        ESP_LOGE(__FILE__, "Failed to stat file : %s", filename);
        return ESP_FAIL;
    }
    fd = fopen(filename, "r");
    if (!fd)
    {
        ESP_LOGE(__FILE__, "Failed to read file : %s", filename);
        return ESP_FAIL;
    }
    sprintf(fw->filename, "%s", filename);
    fw->total_length = file_stat.st_size;
    fclose(fd);
    return false;
}
extern char *firmware_info(firmware_t *fw)
{
    return NULL;
}
extern bool firmware_close(firmware_t *fw)
{
return false;
}