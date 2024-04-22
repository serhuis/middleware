#pragma once

#include <stdint.h>
#include <stddef.h>
#include "host/ble_uuid.h"


extern void zhaga_gatt_init(void);
extern void zhaga_gatt_exchange_mtu(uint16_t conn_handle);
extern void zhaga_gatt_disc_all(uint16_t conn_handle, void(*on_connected_cb)(void));
extern void zhaga_gatt_subscribe(uint16_t conn_handle, ble_uuid_t *svc_uuid, ble_uuid_t *chr_uuid);
extern int zhaga_gatt_write(uint16_t conn_handle, uint8_t *const data, size_t length);
extern int zhaga_gatt_send_reset(uint16_t conn_handle, const uint8_t flag);
extern bool zhaga_gatt_is_oad();
extern size_t zhaga_gatt_on_notify(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om);
extern size_t zhaga_gatt_read(uint8_t * const buf, const size_t len);
