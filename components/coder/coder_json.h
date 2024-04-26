#pragma once
#include <stdio.h>
#include "coder_defs.h"

size_t coder_json_encode(uint8_t const *in_data, size_t in_len, uint8_t const *out_data, size_t out_len);
size_t coder_json_encodeBoot(uint8_t *const in_data, size_t in_len, uint8_t *const out_data, size_t out_len);
size_t coder_json_decode(uint8_t *const in_data, size_t in_len, uint8_t *const out_data, size_t out_len);
size_t coder_json_decodeBoot(uint8_t *const in_data, size_t in_len, uint8_t *const out_data, size_t out_len);
