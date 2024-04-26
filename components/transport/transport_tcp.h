#pragma once
#include "transport_defs.h"
#include "mediator.h"

extern void TransportTcp_init(transport_t * transport, drv_send_fn send_drv);
extern void TransportTcp_msgCb(int msg_id, ...);
