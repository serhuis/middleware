#include "functionBle.h"
#include "esp_log.h"
#include "mediator.h"

typedef enum
{
    TypeGet,
    TypeSet,
    TypeArgs
} method_type_t;

typedef struct
{
    uint8_t id;
    method_type_t type;
    uint8_t req_len;
    int (*fn)(uint8_t * const buf, ...);
} dcd_cmd_t;

typedef enum
{
    BLE_GET_PAIRED_NAME = 1,
    BLE_GET_PAIRED_ADDR = 2,
    BLE_SET_PAIRED_ADDR = 3,
    BLE_SET_PAIRED_PASS = 4,
    BLE_GET_AUTOCONNECT = 5,
    BLE_SET_AUTOCONNECT = 6,
    BLE_GET_STATUS = 7,
    BLE_SET_CONN_PAIRED = 8,
    BLE_GET_DEV_LIST = 9,
    BLE_SET_PAIRED_BY_NAME = 10,
    CMD_UNKNOWN,
} ble_cmd_id_t;

static dcd_cmd_t _cmd[] = {
    {BLE_GET_PAIRED_NAME,   TypeGet, FUNC_MIN_HEADER_SIZE, FnBle_getPairedName},
    {BLE_SET_PAIRED_BY_NAME, TypeSet, FUNC_MIN_HEADER_SIZE, FnBle_setPairedName},
    {BLE_GET_PAIRED_ADDR,   TypeGet, FUNC_MIN_HEADER_SIZE, FnBle_getPairedAddress},
    {BLE_SET_PAIRED_ADDR,   TypeSet, FUNC_MIN_HEADER_SIZE, FnBle_setPairedAddress},
    {BLE_SET_PAIRED_PASS,   TypeGet, FUNC_MIN_HEADER_SIZE, FnBle_setPairedPasskey},
    {BLE_GET_AUTOCONNECT,   TypeGet, FUNC_MIN_HEADER_SIZE, FnBle_getAutoconnect},
    {BLE_SET_AUTOCONNECT,   TypeSet, FUNC_MIN_HEADER_SIZE, FnBle_setAutoconnect},
    {BLE_GET_STATUS,        TypeGet, FUNC_MIN_HEADER_SIZE, FnBle_getStatus},
    {BLE_SET_CONN_PAIRED,   TypeSet, FUNC_MIN_HEADER_SIZE, FnBle_setConnPaired},
    {BLE_GET_DEV_LIST,      TypeGet, FUNC_MIN_HEADER_SIZE, FnBle_getDevList},
    
    };

void (*_notify_cb)(int evt, ...) = NULL;

static uint8_t DcdBle_get_cmd_id(uint32_t handler)
{
    for (int i = 0; i < sizeof(_cmd) / sizeof(dcd_cmd_t); i++)
    {
        if (_cmd[i].fn == handler)
            return _cmd[i].id;
    }
    return CMD_UNKNOWN;
}

void DcdBle_event_handler(int evt, ...)
{
    uint8_t cmd_id = DcdBle_get_cmd_id(evt);
    if (cmd_id == CMD_UNKNOWN)
        return;

    va_list list;
    va_start(list, evt);
    uint8_t *data = (uint8_t *)va_arg(list, uint8_t *);
    size_t length = (size_t)va_arg(list, size_t);
    va_end(list);

    ESP_LOGD("", "%s:%d evt: %x, Cmd: %d", __FILE__, __LINE__, evt, cmd_id);
    if (NULL != _notify_cb)
        _notify_cb(TOPIC_BLE, cmd_id, data, length);
}

extern void DcdBle_init(void (*cb)(int evt, ...))
{
    _notify_cb = cb;
}

extern int DcdBle_receive(uint8_t const *in_data, const size_t in_len, uint8_t *out_data, const size_t out_len)
{
    uint8_t topic = in_data[1];
    uint8_t method = in_data[2];

    if (in_len < FUNC_MIN_HEADER_SIZE)
        return Mtor_nAck(topic, method, RESP_INFO_ELEMENT_ERROR, -1, out_data);

    out_data[0] = FUNC_MIN_HEADER_SIZE;
    out_data[1] = topic;
    out_data[2] = method;
    ESP_LOGI("", "%s:%d: %02x %02x %02x ", __FILE__, __LINE__, out_data[0], topic, method);

    for (int i = 0; i < sizeof(_cmd) / sizeof(dcd_cmd_t); i++)
    {
        if (_cmd[i].id == method && _cmd[i].fn != NULL)
        {
            if (in_data[0] < _cmd[i].req_len)
                return Mtor_nAck(topic, method, RESP_INFO_ELEMENT_ERROR, -1, out_data);

            int resp_len;
            if (_cmd[i].type == TypeGet)
                resp_len = _cmd[i].fn(&out_data[3], out_len - FUNC_MIN_HEADER_SIZE);
            else if (_cmd[i].type == TypeSet)
                resp_len = _cmd[i].fn(&in_data[3], in_len - FUNC_MIN_HEADER_SIZE);
            else
            {
                resp_len = _cmd[i].fn(&in_data[3], in_len - FUNC_MIN_HEADER_SIZE, &out_data[3], out_len - FUNC_MIN_HEADER_SIZE);
            }

            if (resp_len == ESP_FAIL)
                return Mtor_nAck(topic, method, RESP_PARAMETER_ERROR, -1, out_data);

            out_data[0] += (resp_len & 0xFF);
            return (out_data[0] + (resp_len & 0xC0000000));
        }
    }
    return Mtor_nAck(topic, method, RESP_ATTR_NOT_FOUND, -1, out_data);
}
