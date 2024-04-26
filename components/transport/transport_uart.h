#pragma once
#include <stdio.h>
#include "transport_defs.h"

extern void TransportUart_init(transport_t * transport, drv_send_fn send_drv);
extern void TransportUart_drv_rx_cb( const int drv_id, ...);
