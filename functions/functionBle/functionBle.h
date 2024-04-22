#pragma once

#include <stdio.h>
#include "transport_defs.h"
//fnBle
extern void FnBle_init( void (*evt_handler)(int evt, ...) );
extern int FnBle_getPairedName(char * buf, ... );
extern int FnBle_setPairedName(char * buf, ... );
extern int FnBle_getPairedAddress(uint8_t * buf, ... );
extern int FnBle_setPairedAddress(uint8_t * buf, ... );
extern int FnBle_setPairedPasskey(uint32_t pk, ... );
extern int FnBle_getAutoconnect(uint8_t * buf, ... );
extern int FnBle_setAutoconnect(uint8_t * buf, ... );
extern int FnBle_getStatus(char *buf, ...);
extern int FnBle_setConnPaired(uint8_t *buf, ...);
extern int FnBle_getDevList(int * buf, ...);
// dcdBle
#define TOPIC_BLE				( 0x72 )
//typedef enum {TypeGet, TypeSet, TypeArgs}method_type_t;

extern void DcdBle_init(void(*cb)(int evt, ...));
extern void DcdBle_event_handler(int evt, ...);
extern int DcdBle_receive(uint8_t const * in_data, const size_t in_len, uint8_t * out_data, const size_t out_len);
