#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "scan.h"
#include "misc.h"

extern void zhaga_cent_init(void);
extern void zhaga_cent_scan(void);
extern void zhaga_cent_connect(uint8_t *const addr);
extern void zhaga_cent_connect_paired(void);
extern char * zhaga_cent_get_status(void);
extern esp_err_t zhaga_cent_disconnect(void);
extern bool zhaga_cent_get_autoconnect(void);
extern void zhaga_cent_set_autoconnect(const int connect);

extern size_t zhaga_cent_send(uint8_t *const data, const size_t length);
extern size_t zhaga_cent_receive(uint8_t *const data, const size_t length);
extern size_t zhaga_cent_reset(const uint8_t flag);


extern esp_err_t zhaga_cent_set_paired_addr(uint8_t *const addr);
extern esp_err_t zhaga_cent_set_paired_passkey(const uint32_t passkey);

extern esp_err_t zhaga_cent_reset_paired(void);

extern size_t zhaga_cent_get_paired_addr(uint8_t *const addr, const size_t length);
extern char * zhaga_cent_get_paired_name(void);

//extern esp_err_t zhaga_cent_set_paired(uint8_t *const addr, uint32_t passkey);
extern esp_err_t zhaga_cent_set_paired_name(char *const name);