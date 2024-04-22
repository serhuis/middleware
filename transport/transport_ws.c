#include <stdarg.h>
#include "esp_log.h"
#include "mediator.h"
#include "transport_ws.h"
#include "coder_json.h"

#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGBUF(data, len) ESP_LOG_BUFFER_HEXDUMP(__FILE__, data, len, ESP_LOG_INFO)

static volatile uint8_t _rx_buffer[MAX_TRANSPORT_BUFFER] = {0};
static volatile uint8_t _tx_buffer[MAX_TRANSPORT_BUFFER] = {0};
static size_t _rx_rd_idx = 0, _rx_wr_idx = 0;
static size_t _tx_rd_idx = 0, _tx_wr_idx = 0;
static drv_send_fn _txFunc = NULL;
static transport_t *_transport = NULL;

static size_t transportWs_send(uint8_t *const data, const size_t len);
static uint8_t *transportWs_getTxBuffer(void);
static size_t transportWs_decode(Mtor_mode_t mode, uint8_t *buffer, size_t length);
static size_t transportWs_encode(Mtor_mode_t mode, uint8_t *buffer, size_t length);
static size_t TransportWs_getTxFree(void);
static size_t TransportWs_getRxLength(void);
static size_t TransportWs_getTxLength(void);
static size_t transportWs_add_rx(uint8_t *const data, const size_t len, uint8_t *ret);
static size_t TransportWs_add_tx(uint8_t *const data, const size_t len, uint8_t *ret);

extern void TransportWs_init(transport_t *transport, drv_send_fn send_drv)
{
    if ((!transport) || (!send_drv))
        return;
    _transport = transport;
    _txFunc = send_drv;
    _transport->fn_send = transportWs_send;
    _transport->decode = transportWs_decode;
    _transport->encode = transportWs_encode;
    _transport->notify = NULL;           // inited in mediator
    _transport->direct_transport = NULL; // inited in mediator
    _transport->out_buffer = transportWs_getTxBuffer;
    _transport->out_length = TransportWs_getTxFree;
    _transport->to_send = TransportWs_getTxLength;
    _transport->add_tx = TransportWs_add_tx;

    LOGD("id: %d, %x", _transport->transport_id, (uint32_t)_transport);
    LOGD("fn_send: %d, %x", _transport->fn_send, (uint32_t)_transport->fn_send);
}

extern void TransportWs_msgCb(int msg_id, ...)
{
    va_list list;
    uint32_t sender;

    LOGD("%0.2x", (uint32_t)_transport);

    va_start(list, msg_id);
    sender = va_arg(list, uint32_t);

    switch (msg_id)
    {
    case DrvInited:
        LOGD("DrvInited.", sender);
        break;
    case PeerConnected:
        LOGD("PeerConnected [ %d ].", sender);
        break;
    case PeerDisconnected:
        LOGD("PeerDisconnected [ %d ].", sender);
        break;
    case DrvDataReceived:
    {
        uint8_t *buf = (uint8_t *)va_arg(list, int);
        size_t len = va_arg(list, int);
        LOGD("Received from %d %d bytes", sender, len);
        uint8_t *ret = NULL;
        size_t added = transportWs_add_rx(buf, len, ret);
        if (NULL != _transport->notify)
        {
            _transport->notify(_transport->transport_id, &_rx_buffer[_rx_rd_idx], added);
            LOGD("Sent to mtor: %d, _transport->transport_id: %d", added, _transport->transport_id);
        }
        LOGD("Received: %s; total %d bytes", &_rx_buffer[_rx_rd_idx], added);
    }
    break;
    case DrvDataSent:
        LOGD("DrvDataSent.");
        break;
    case DrvError:
        LOGE("DrvError.", sender);
        break;
    default:
        LOGW("Web socket message undefined");
    }
    va_end(list);
}

static size_t transportWs_add_rx(uint8_t *const data, const size_t len, uint8_t *ret)
{
    if (len > (sizeof(MAX_TRANSPORT_BUFFER) - _rx_wr_idx) + 3)
        _rx_wr_idx = 0;
    memcpy(&_rx_buffer[_rx_wr_idx], data, len);
    _rx_wr_idx += len;
    return len;
}

static size_t TransportWs_getRxLength(void)
{
    return (_rx_wr_idx - _rx_rd_idx);
}

size_t TransportWs_add_tx(uint8_t *const data, const size_t len, uint8_t *ret)
{

    if (_tx_wr_idx == _tx_rd_idx)
    {
        _tx_wr_idx = _tx_rd_idx = 0;
        bzero(_tx_buffer, sizeof(_tx_buffer));
    }
    if (len > (sizeof(_tx_buffer) - _tx_wr_idx))
        return 0;
    mempcpy(&_tx_buffer[_tx_wr_idx], data, len);
    _tx_wr_idx += len;
    return (_tx_wr_idx - _tx_rd_idx);
}

static uint8_t *transportWs_getTxBuffer(void)
{
    LOGD();
    return &_tx_buffer[_tx_wr_idx];
}

static size_t TransportWs_getTxLength(void)
{
    return (_tx_wr_idx - _tx_rd_idx);
}

static size_t TransportWs_getTxFree(void)
{
    return (sizeof(_tx_buffer) - _tx_wr_idx);
}

static size_t transportWs_send(uint8_t *const data, const size_t len)
{
    LOGD("%x, %d", data, len);
    uint8_t *pdata = (data == NULL) ? &_tx_buffer[_tx_rd_idx] : data;
    size_t l = (len) ? len : TransportWs_getTxLength();
    size_t sent = 0;

    if (_txFunc && (l > 0))
    {
        LOGD("_txFunc: %x, pdata:%s, l: %d", _txFunc, pdata, l);
        sent += _txFunc(0, pdata, l);
    }
    if (_tx_wr_idx == (_tx_rd_idx += sent))
    {
        _tx_wr_idx = _tx_rd_idx = 0;
        bzero(_tx_buffer, sizeof(_tx_buffer));
    }
    return sent;
}

static size_t transportWs_decode(Mtor_mode_t mode, uint8_t *buffer, size_t length)
{
    LOGD("%d %x %d", mode, buffer, length);
    size_t decoded = 0;
    if (NULL == buffer || length == 0)
    {
        LOGD("%d", _rx_wr_idx - _rx_rd_idx);
        return _rx_wr_idx - _rx_rd_idx;
    }

    LOGD("%x %d", buffer, length);
    decoded = coder_json_decode(&_rx_buffer[_rx_rd_idx], _rx_wr_idx - _rx_rd_idx, buffer, length);
    LOGD("%d %x %d ", decoded, buffer, length);
    if (0 == decoded)
        _rx_wr_idx = _rx_rd_idx = 0;
    //LOGBUF(buffer, decoded);
    _rx_rd_idx += decoded;
    return decoded;
}

static size_t transportWs_encode(Mtor_mode_t mode, uint8_t *buffer, size_t length)
{
    LOGD("%d %x %d, %x", mode, buffer, length, _tx_buffer);
    size_t encoded = coder_json_encode(buffer, length, _tx_buffer, sizeof(_tx_buffer) - _tx_wr_idx);
    _tx_wr_idx = encoded;
    return encoded;
}
