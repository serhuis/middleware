#include "transport_uart.h"

#include "esp_log.h"
#include "mediator.h"

#include "../coder/flag_coder.h"
// #include "ringbuffer.h"

// Logging
static const char *TAG = __FILE__;

#define espLogI(format, ...) ESP_LOGI("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogD(format, ...) ESP_LOGD("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogW(format, ...) ESP_LOGW("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogE(format, ...) ESP_LOGE("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define espLogHex(data, len) ESP_LOG_BUFFER_HEX(TAG, data, len)

static int _drv_id = -1;
static encoder_t _coder = {.decode = NULL, .decodeBoot = NULL, .encode = NULL, .encodeBoot = NULL};
static volatile uint8_t _rx_buffer[MAX_TRANSPORT_BUFFER] = {0};
static volatile uint8_t _tx_buffer[MAX_TRANSPORT_BUFFER] = {0};
static size_t _rx_rd_idx = 0, _rx_wr_idx = 0;
static size_t _tx_rd_idx = 0, _tx_wr_idx = 0;
static transport_t *_uart_transport = NULL;
static drv_send_fn _txFunc = NULL;

static size_t TransportUart_decode(uint8_t mode, uint8_t *data, size_t len);
static size_t TransportUart_encode(uint8_t mode, uint8_t *data, size_t len);
static size_t TransportUart_send(uint8_t *data, size_t len);
static uint8_t *TransportUart_getTxBuffer(void);
static size_t TransportUart_getTxLength(void);
static size_t TransportUart_getTxFree(void);

extern void TransportUart_init(uint8_t id, transport_t *transport, drv_send_fn send_drv)
{
    _uart_transport = transport;
    _drv_id = id;
    _txFunc = send_drv;
    transport->fn_send = TransportUart_send;
    transport->notify = NULL;           // inited in mediator
    transport->direct_transport = NULL; // inited in mediator
    transport->out_buffer = TransportUart_getTxBuffer;
    transport->to_send = TransportUart_getTxLength;
    transport->out_length = TransportUart_getTxFree;
    Flag_Init(&_coder);
    (void)_coder;
    transport->decode = TransportUart_decode;
    transport->encode = TransportUart_encode;
    espLogD("%0.2x %0.2x", id, transport);
}

extern void TransportUart_drv_rx_cb(const int drv_id, ...)
{

    va_list list;
    va_start(list, drv_id);

    uint8_t *buf = (uint8_t *)va_arg(list, uint8_t *);
    size_t len = va_arg(list, size_t);
    va_end(list);

    if (len > MAX_TRANSPORT_BUFFER)
        return;
    if (len > sizeof(MAX_TRANSPORT_BUFFER) - _rx_wr_idx)
        _rx_wr_idx = 0;

    memcpy((void *)&_rx_buffer[_rx_rd_idx], buf, len);
    espLogI("&_rx_wr_idx: %d &_rx_buffer[_rx_wr_idx]: %0.2x", _rx_wr_idx, (void *)&_rx_buffer[_rx_wr_idx]);
    ESP_LOG_BUFFER_HEXDUMP(TAG, (void *)&_rx_buffer[_rx_wr_idx], len, ESP_LOG_DEBUG);
    if (_uart_transport->notify)
        _uart_transport->notify(_uart_transport->transport_id, (void *)&_rx_buffer[_rx_wr_idx], len);
    _rx_wr_idx += len;
}

static size_t TransportUart_decode(uint8_t mode, uint8_t *buffer, size_t length)
{
    espLogD();
    size_t rx_len = _rx_wr_idx - _rx_rd_idx;
    if (!(buffer && length))
        return rx_len;

    size_t decoded = 0;
    size_t ie_len = Flag_getDataLength(&_rx_buffer[_rx_rd_idx], rx_len);
    espLogD("%d, %d", ie_len, rx_len);
    if (ie_len > 0)
    {
        decoded = Flag_getData(&_rx_buffer[_rx_rd_idx], rx_len, buffer, length);
        espLogD("decoded:%d", decoded);
    }
    _rx_rd_idx += decoded;
    if (0 == decoded)
        _rx_wr_idx = _rx_rd_idx = 0;
    espLogD("%d %0.2x %0.2x ", decoded, buffer, length);
    return ie_len;
}

static size_t TransportUart_encode(uint8_t mode, uint8_t *buffer, size_t length)
{
    size_t tx_size = sizeof(_tx_buffer) - _tx_wr_idx;
    espLogD("%d 0x%0.2x %d", mode, buffer, length);

    if (NULL == buffer || 0 == length)
        return tx_size;

    if (_coder.encode && *buffer && length <= tx_size)
    {
        _tx_wr_idx += (mode == ModeBoot)
                          ? Flag_rmHeader(buffer, length, (uint8_t *)&_tx_buffer[_tx_wr_idx], tx_size)
                          : Flag_addFlags(buffer, length, (uint8_t *)&_tx_buffer[_tx_wr_idx], tx_size);
        espLogD("0x%0.2x %d", buffer, length);
    }
    espLogD("%d", _tx_wr_idx);
    return _tx_wr_idx;
}

static size_t TransportUart_send(uint8_t *data, size_t len)
{
    uint8_t *pdata = data;
    size_t tosend = len;
    if (!(pdata && tosend))
    {
        pdata = &_tx_buffer[_tx_rd_idx];
        tosend = _tx_wr_idx - _tx_rd_idx;
    }

    espLogD("%d %d", _tx_wr_idx, len);
    if (_txFunc)
    {
        size_t sent = _txFunc(_drv_id, pdata, tosend);
        if ((_tx_rd_idx += sent) == _tx_wr_idx)
            _tx_wr_idx = _tx_rd_idx = 0;
        espLogD("Sent %d of %d", sent, len);
        return sent;
    }
    return 0;
}

static uint8_t *TransportUart_getTxBuffer(void)
{
    espLogD("%0.2x ", &_tx_buffer[_tx_wr_idx]);
    return (void *)&_tx_buffer[_tx_wr_idx];
}

static size_t TransportUart_getTxLength(void)
{
    espLogD("%d ", _tx_wr_idx);
    return _tx_wr_idx;
}

static size_t TransportUart_getTxFree(void)
{
    espLogD("%d ", BUF_SIZE(_tx_buffer) - _tx_wr_idx);
    return BUF_SIZE(_tx_buffer) - _tx_wr_idx;
}
