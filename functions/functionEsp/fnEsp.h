/*
 * fnEsp.h
 *
 *  Created on: May 13, 2022
 *      Author: serh
 */

#pragma once
#include <stdio.h>

extern int FnEsp_getVersion(uint8_t * buf, ... );
extern int FnEsp_getPinState(uint8_t * , ... );
extern uint32_t FnEsp_setPinState(uint8_t const * buf, ... );
extern int FnEsp_getPinMode(uint8_t * buf, ... );
extern int FnEsp_setPinMode(uint8_t const * buf, ... );
extern int FnEsp_getApSsid(int arg, ... );
extern int FnEsp_setApSsid(uint8_t const * buf, ... );
extern int FnEsp_getApPass(uint8_t * buf, ... );
extern int FnEsp_setApPass(uint8_t const * buf, ... );
extern int FnEsp_getApHidden(uint8_t * buf, ... );
extern int FnEsp_setApHidden(uint8_t const * buf, ... );
extern int FnEsp_resetApSettings (uint8_t *buf, ...);
extern int FnEsp_getStaSettings(uint8_t * buf, ... );
extern int FnEsp_setStaSettings(uint8_t const * buf, ... );
extern int FnEsp_getStaMac(uint8_t * buf, ... );
extern int FnEsp_getApMac(uint8_t * buf, ... );
extern int FnEsp_systemReset();