#pragma once
#include "stddef.h"

size_t str2hex(char *str, const size_t str_len, uint8_t *arr, size_t arr_len);
size_t hex2str(uint8_t *const arr, const size_t arr_len, char *str, const size_t str_len);