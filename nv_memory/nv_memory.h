#pragma once
#include <stdio.h>
#include <string.h>
#include "esp_err.h"


esp_err_t NvMemory_init(void);
esp_err_t NVMemory_getBlob(char * const key, uint8_t * out_buffer, size_t len);
esp_err_t NVMemory_setBlob(char * const key, uint8_t * in_buffer, size_t len);
