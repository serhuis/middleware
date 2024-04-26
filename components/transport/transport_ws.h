#pragma once
#include <stdio.h>
#include "transport_defs.h"

extern void TransportWs_init(transport_t * transport, drv_send_fn send_drv);
extern void TransportWs_msgCb(int msg_id, ...);
