#pragma once
typedef struct {
	size_t 			(*encode)(uint8_t const * in_data, size_t in_len, uint8_t const * out_data, size_t out_len);
	size_t 			(*encodeBoot)(uint8_t * const in_data, size_t in_len, uint8_t * const out_data, size_t out_len);
	size_t			(*decode)(uint8_t * const in_data, size_t in_len, uint8_t ** const out_data, size_t * out_len);
	size_t			(*decodeBoot)(uint8_t * const in_data, size_t in_len, uint8_t * const out_data, size_t out_len);
}encoder_t;