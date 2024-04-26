#pragma once
#include "http.h"

extern void ws_init(void (*cb)(int msg_id, ...));
extern esp_err_t ws_handler(httpd_req_t *req);
extern size_t ws_send_async(int id, char * const data, const size_t length);
