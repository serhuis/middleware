/*
 * flag_coder.h
 *
 *  Created on: Apr 27, 2022
 *      Author: serh
 */

#ifndef COMPONENTS_TRANSPORT_FLAG_CODER_H_
#define COMPONENTS_TRANSPORT_FLAG_CODER_H_
#include <stdio.h>
#include "coder_defs.h"

extern void Flag_Init(encoder_t * encoder);
extern size_t Flag_addFlags(uint8_t const * in_data, size_t in_len, uint8_t * out_data, size_t out_len);
extern size_t Flag_rmFlags(uint8_t const * in_data, size_t in_len, uint8_t  const * out_data, size_t out_len);
extern size_t Flag_addHeader(uint8_t const * in_data, size_t in_len, uint8_t * out_data, size_t out_len);
extern size_t Flag_rmHeader(uint8_t  const *in_data, size_t in_len, uint8_t const * out_data, size_t out_len);
extern size_t Flag_getData(uint8_t const * in_data, size_t in_len, uint8_t const * out_data, size_t out_len);
extern size_t Flag_getDataLength(uint8_t const * in_data, size_t in_len);
extern size_t Flag_search(uint8_t  const * in_data, size_t in_len, uint8_t * out_data, size_t out_len);
#endif /* COMPONENTS_TRANSPORT_FLAG_CODER_H_ */
