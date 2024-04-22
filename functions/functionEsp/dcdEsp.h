#pragma once
#include <stdio.h>

#define TOPIC_ESP				( 0x71 )
typedef enum {TypeGet, TypeSet, TypeArgs}method_type_t;

extern void DcdEsp_init(void);
extern int DcdEsp_receive(uint8_t const * in_data, const size_t in_len, uint8_t * out_data, const size_t out_len);
