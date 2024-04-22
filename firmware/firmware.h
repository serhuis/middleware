#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "esp_vfs.h"

typedef struct {
    char        filename[64];
    FILE *      fd;
    char *      version;
    int         status;
    size_t      total_length;
    int         n_blocks;
    size_t      block_size;
    uint8_t     align_size;
}firmware_t;

extern bool firmware_open(char * const filename, firmware_t * fw);
extern bool firmware_close(firmware_t * fw);
extern char * firmware_info(firmware_t * fw);