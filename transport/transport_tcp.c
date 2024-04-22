/*
 * transport_tcp.c
 *
 *  Created on: Apr 20, 2022
 *      Author: serh
 */
#include <stdarg.h>
#include "esp_log.h"
#include "transport_tcp.h"

#include "../coder/flag_coder.h"
// Logging
static const char *TAG = __FILE__;

#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogW(format, ...) ESP_LOGW("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogE(format, ...) ESP_LOGE("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGBUF(data, len) ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO)

static int _socket = -1;
static encoder_t _coder;
static uint8_t _rx_buffer[MAX_TRANSPORT_BUFFER] = {0};
static uint8_t _tx_buffer[MAX_TRANSPORT_BUFFER] = {0};
static size_t _rx_rd_idx = 0, _rx_wr_idx = 0;
static size_t _tx_rd_idx = 0, _tx_wr_idx = 0;
static drv_send_fn _txFunc = NULL;
static transport_t *_tcp_transport = NULL;

static size_t TransportTcp_decode(Mtor_mode_t mode, uint8_t *const buffer, const size_t length);
static size_t TransportTcp_encode(Mtor_mode_t mode, uint8_t *const buffer, const size_t length);
static uint8_t *TransportTcp_getTxBuffer(void);
static size_t TransportTcp_getTxLength(void);
static size_t TransportTcp_getTxFree(void);
static size_t TransportTcp_sendBuffer(uint8_t *const data, const size_t len);
static size_t TransportTcp_send(uint8_t *const data, const size_t len);

extern void TransportTcp_init(transport_t *transport, drv_send_fn send_drv)
{
    if ((!transport) || (!send_drv))
        return;

    // Flag_Init(&_coder);

    _tcp_transport = transport;
    _txFunc = send_drv;
    _tcp_transport->fn_send = TransportTcp_sendBuffer;
    _tcp_transport->decode = (void *)TransportTcp_decode;
    _tcp_transport->encode = (void *)TransportTcp_encode;
    _tcp_transport->notify = NULL;           // inited in mediator
    _tcp_transport->direct_transport = NULL; // inited in mediator
    _tcp_transport->out_buffer = TransportTcp_getTxBuffer;
    _tcp_transport->out_length = TransportTcp_getTxFree;
    _tcp_transport->to_send = TransportTcp_getTxLength;

    LOGI("%0.2x %d", (uint32_t)_tcp_transport, _tcp_transport->transport_id);
}

extern void TransportTcp_msgCb(int msg_id, ...)
{
    va_list list;
    uint32_t var;

    va_start(list, msg_id);

    switch (msg_id)
    {
    case DrvInited:
        LOGD("DrvInited.", va_arg(list, uint32_t));
        break;
    case PeerConnected:
        _socket = va_arg(list, uint32_t);
        LOGD("PeerConnected [ %d ].", _socket);
        break;
    case PeerDisconnected:
        LOGD("PeerDisconnected [ %d ].", va_arg(list, uint32_t));
        _socket = -1;
        break;
    case DrvDataReceived:
    {
        /// int id = var;   ///unused
        uint8_t *buf = va_arg(list, uint8_t*);
        size_t len = va_arg(list, int);

        if (len > sizeof(MAX_TRANSPORT_BUFFER) - _rx_wr_idx)
            _rx_wr_idx = 0;
        LOGD("%0.2x >>> %0.2x %d", buf, &_rx_buffer[_rx_wr_idx], len);
        uint8_t * dest = memcpy(&_rx_buffer[_rx_wr_idx], buf, len);
        if (dest && _tcp_transport->notify)
            _tcp_transport->notify(_tcp_transport->transport_id, dest, len);
        _rx_wr_idx += len;
    }
    break;
    case DrvDataSent:
        LOGI("Sent %d bytes.", va_arg(list, int));
        break;
    case DrvError:
        espLogE("DrvError.", va_arg(list, int));
        break;
    default:
        LOGI("Tcp socket message undefined");
    }
    va_end(list);
}
static size_t TransportTcp_send(uint8_t *const data, const size_t len)
{
    if ((_txFunc != NULL) && (_socket >= 0) && (len > 0))
    {
        return _txFunc(_socket, data, len);
    }
    return 0;
}

static size_t TransportTcp_sendBuffer(uint8_t *const data, const size_t len)
{
    LOGD("%d %d %d ", _tx_rd_idx, _tx_wr_idx, len);
    //LOGBUF(&_tx_buffer[_tx_rd_idx], _tx_wr_idx);
    if ((_txFunc != NULL) && (_socket >= 0)/* && (len > 0)*/)
    {
        LOGD("%d %d %d ", _tx_rd_idx, _tx_wr_idx, len);
        size_t sent = _txFunc(_socket, &_tx_buffer[_tx_rd_idx], _tx_wr_idx - _tx_rd_idx);
        LOGD("Sent: %d of %d", sent, _tx_wr_idx - _tx_rd_idx );
        _tx_wr_idx = _tx_rd_idx = 0;
        return sent;
    }
    return 0;
}

static size_t TransportTcp_decode(Mtor_mode_t mode, uint8_t *const buffer, const size_t length)
{
    size_t rx_len = _rx_wr_idx - _rx_rd_idx;
    if ((NULL == buffer) || (0 == length))
        return rx_len;

    LOGD("%d %0.2x", mode, buffer);
    //LOGBUF(&_rx_buffer[_rx_rd_idx], rx_len);

    int (*decfn)(int *, int, int *, int) = (ModeBoot == mode) ? _coder.decodeBoot : _coder.decode;
    size_t decoded = 0;
    size_t ie_len = Flag_getDataLength(&_rx_buffer[_rx_rd_idx], rx_len);
    LOGD("%d, %d", ie_len, rx_len);
    if (ie_len > 0)
    {
        // decoded = decfn(&_rx_buffer[_rx_rd_idx], rx_len, buffer, length);
        decoded = Flag_getData(&_rx_buffer[_rx_rd_idx], rx_len, buffer, length);
        LOGD("decoded:%d", decoded);
    }
    _rx_rd_idx += decoded;
    if (0 == decoded)
        _rx_wr_idx = _rx_rd_idx = 0;
    LOGD("%d %0.2x %0.2x ", decoded, buffer, length);
    return ie_len;
}

static size_t TransportTcp_encode(Mtor_mode_t mode, uint8_t *const buffer, const size_t length)
{

    size_t tx_size = sizeof(_tx_buffer) - _tx_wr_idx;
    if (!buffer && length > tx_size)
        return 0;    
    
    LOGD("%d 0x%0.2x %d", mode, buffer, length);
    if (/*_coder.encode && _coder.encodeBoot && */ mode != ModeDirect)
    {
        if (ModeBoot == mode)
        {
            // length = Flag_addHeader(buffer, length, &_tx_buffer[_tx_wr_idx], tx_size);
            // buffer = &_tx_buffer[_tx_wr_idx];
            tx_size -= Flag_addHeader(buffer, length, &_tx_buffer[_tx_wr_idx], tx_size);
        }
        _tx_wr_idx += Flag_addFlags(buffer, length, &_tx_buffer[_tx_wr_idx], tx_size);
        LOGD("0x%0.2x %d", buffer, _tx_wr_idx);
    }
    else
    {
        memcpy(&_tx_buffer[_tx_wr_idx], buffer, length);
        if ((_tx_wr_idx += length) >= (sizeof(_tx_buffer) - 2))
            _tx_wr_idx = 0;
        LOGD("%d %d", _tx_wr_idx, length);
    }
    LOGD("%d", _tx_wr_idx);
    //LOGBUF(_tx_buffer, _tx_wr_idx);
    return _tx_wr_idx;
}

static uint8_t *TransportTcp_getTxBuffer(void)
{
    return &_tx_buffer[_tx_wr_idx];
}

static size_t TransportTcp_getTxLength(void)
{
    return _tx_wr_idx;
}

static size_t TransportTcp_getTxFree(void)
{
    return BUF_SIZE(_tx_buffer) - _tx_wr_idx;
}