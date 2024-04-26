/*
 * tcp_server.h
 *
 *  Created on: 10 ���. 2022 �.
 *      Author: serhiy
 */

#ifndef MODULES_TCP_SERVER_TCP_SERVER_H_
#define MODULES_TCP_SERVER_TCP_SERVER_H_

#include "esp_netif.h"
#include "../transport/transport_defs.h"

extern void tcp_server_init(user_msg msgCb);
extern void tcp_server_start(void);
extern void tcp_server_stop(void);
extern size_t tcp_server_send(const int sock, const char * data, const size_t len);
#endif /* MODULES_TCP_SERVER_TCP_SERVER_H_ */
