#pragma once

#include "transport_defs.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(MEDIATOR_EVENTS); // declaration of the task events family

#define FUNC_MIN_HEADER_SIZE    ( 3 )
#define FUNC_ACK_SIZE           ( 3 )

#define CHANGE_MODE             ( 0x80000000 )
#define BOOT_MODE_FLAG          ( 0x40000000 )
#define NORM_MODE_FLAG          ( 0x00000000 )

typedef enum
{
    ModeNormal = 0,
    ModeBoot,
    ModeDirect,
    ModeUnknown
} Mtor_mode_t;

typedef enum
{
    RESP_OK,
    RESP_GENERAL_ERROR,
    RESP_INFO_ELEMENT_ERROR,
    RESP_FUNC_NOT_FOUND,
    RESP_ATTR_NOT_FOUND,
    RESP_PARAMETER_ERROR
} LsRespStatus;

typedef size_t (*rcv_fn)(uint8_t const *rcv_data, const size_t rcv_len, uint8_t const *resp_data, const size_t resp_len);
typedef enum
{
    DataReceived,
    DataSent,
    ModeChanged,
    Notification, // async message from fn level on attribute changed
    Error

} MtorEvent_t;

typedef struct
{
    int     topic;
    rcv_fn  fnRcv;
} receiver_t;

typedef struct
{
    transport_t *transport;
    uint8_t     topics[CONFIG_REVEIVER_CNT];
} subscription_t;

typedef struct
{
    transport_t     *transports[CONFIG_TRANSPORT_CNT];
    receiver_t      receivers[CONFIG_REVEIVER_CNT];
    subscription_t  subscriptions[CONFIG_ASYNC_QUEUE_LENGTH];
} mediator_t;

extern void Mtor_init();
extern int  Mtor_registerTransport(transport_t *transport);
extern void Mtor_registerReceiver(int topic, rcv_fn fn_rcv);
extern void Mtor_registerSubscription(transport_t * tr, uint8_t topic);
extern transport_t * Mtor_getTransportById(int id);
extern void Mtor_MakeBridge(void * tr1_id, void * tr2_id);
extern void Mtor_rcvCb(int evt, ...);
extern void Mtor_notifyCb(const uint8_t topic, const uint8_t method, uint8_t * const data, const uint8_t length);
extern size_t Mtor_nAck(uint8_t id, uint8_t attr, LsRespStatus status, int8_t idx, uint8_t *const buf);
