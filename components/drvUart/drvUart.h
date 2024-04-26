#pragma once


//typedef enum {UART_0, UART_1, UART_NONE = -1} uart_id_t;
//typedef void(*rxCb)(const int id, uint8_t * const data, const uint16_t len);
typedef void (*user_msg)(int msg_id, ...);


extern void Uart_init(user_msg cb);
extern size_t Uart_send(uint8_t uart, uint8_t * const data, const size_t len );
