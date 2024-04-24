#include <stdio.h>
#include "esp_log.h"
#include "mediator.h"
// #include "ringbuf.h"

ESP_EVENT_DEFINE_BASE(MEDIATOR_EVENTS);

#define MTOR_IN_BUFF_SIZE 512
#define MTOR_OUT_BUFF_SIZE 512

#define INVALID_ID (-1)

// Logging
//static const char* TAG = __FILE__;
#define LOGI(format, ...) ESP_LOGI("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE("", "%s:%d: %s: " format, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGBUF(data, len) ESP_LOG_BUFFER_HEXDUMP(__FILE__, data, len, ESP_LOG_INFO)

static esp_event_loop_handle_t _mtor_event_loop;
// static void Mtor_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data);
static void Mtor_event_handler(void* handler_args, esp_event_base_t base, int evt, void* event_data);
static size_t Mtor_send(int id, uint8_t* const data, const size_t len);
static size_t Mtor_send_async(void* data);
static int Mtor_receive(uint8_t const* in_data, const size_t in_len, uint8_t* out_data, const size_t out_len);
static transport_t* Mtor_getTransportBySubId(int topic);

static Mtor_mode_t _mode;
static volatile mediator_t _mtor = { .transports = {0} };

/// static RingbufferType_t _mtorInBuff, _mtorOutBuff;

extern void Mtor_init()
    {

    esp_event_loop_args_t loop_with_task_args = {
        .queue_size = 5,
        .task_name = "Mediator_task", // task will be created
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY };
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_with_task_args, &_mtor_event_loop));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(_mtor_event_loop, MEDIATOR_EVENTS, ESP_EVENT_ANY_ID, Mtor_event_handler, NULL, NULL));

    for (int i = 0; i < CONFIG_REVEIVER_CNT; i++)
        {
        _mtor.receivers[i].topic = -1;
        _mtor.receivers[i].fnRcv = NULL;
        }
    }

static void Mtor_event_handler(void* handler_args, esp_event_base_t base, int evt, void* event_data)
    {
    LOGD("%d", evt);
    struct msg_t
        {
        int src_id;
        uint8_t* data;
        size_t len;
        } msg;

    switch (evt)
        {
        case DataReceived:
        {
        static struct msg_t msg;
        size_t ie_len = 0;
        size_t code_len = 0, out_len = 0;
        uint8_t* code_data = NULL;
        msg = *(struct msg_t*)(event_data);
        uint8_t* resp_enc = NULL;

        LOGD("DataReceived: %d, transport: %d", DataReceived, msg.src_id);
        // LOGI("msg.src_id = %d, msg.data = %x, msg.len = %d", msg.src_id, msg.data, msg.len);
        // LOGBUF(msg.data, msg.len);
        transport_t* transport = Mtor_getTransportById(msg.src_id);
        do
            {
            if (NULL == transport->decode)
                {
                LOGE("No decoder found for transport %d %x", msg.src_id, transport->decode);
                break;
                }
            code_len = transport->decode(ModeNormal, 0, 0);
            LOGD("code_len: %d", code_len);
            if (0 == code_len)
                break;

            code_data = malloc(code_len);
            bzero(code_data, code_len);
            ie_len = transport->decode(ModeNormal, code_data, code_len);

            LOGD("%x %d %d ", code_data, code_len, ie_len);
            // LOGBUF(code_data, ie_len);
            if (ie_len > 0)
                {
                uint8_t resp[128] = { 0 };
                LOGD("code_data = %x, ie_len %d", code_data, ie_len);

                out_len = Mtor_receive(code_data, ie_len, resp, sizeof(resp));
                if (0 < (out_len & 0x0000FFFF))
                    {
                    if (CHANGE_MODE & out_len)
                        {
                        _mode = (BOOT_MODE_FLAG & out_len) ? ModeBoot : ModeNormal;
                        out_len &= 0xFFFF;
                        }
                    LOGD("%x: , %d:", resp, out_len);
                    out_len = transport->encode(ModeNormal, resp, out_len);
                    }
                }

            if (!ie_len || !out_len)
                { // encode()
                if ((NULL != transport->direct_transport) && (transport->direct_transport->encode))
                    {
                    if (0 == ie_len)
                        _mode = (ModeBoot == _mode) ? ModeBoot : ModeDirect;
                    out_len = transport->direct_transport->encode(_mode, code_data, code_len);
                    }
                }
            free(code_data);
            LOGD("%d %d", ie_len, out_len);
            } while (ie_len);
            Mtor_send(transport->transport_id, NULL, 0);
        }
        break;
        case DataSent:
            LOGD("DataSent");
            break;
        case ModeChanged:
        {
        }
        break;
        case Notification:
        {
        Mtor_send_async(event_data);
        }
        break;
        case Error:
        {
        }
        break;
        default:
            LOGE("Unknown event!");
        }
    }

static size_t Mtor_send_async(void* notification) {
    static struct noti_t
        {
        uint8_t topic;
        uint8_t method;
        uint8_t* data;
        uint8_t len;
        } noti;
    noti = *(struct noti_t*)(notification);
    LOGI("Got notification: %x %x %x %s", noti.len, noti.topic, noti.method, noti.data);
    transport_t* transport = NULL;
        do {
            transport = Mtor_getTransportBySubId(noti.topic);
            LOGI("transport notification: %d", transport);
            if (transport)
                {
                LOGI("Found transport: %d for subscription topic: %x", transport->transport_id, noti.topic);
                if (transport->encode)
                    {
                    uint8_t* arr = malloc(noti.len + 3);
                    arr[0] = noti.len + 3;
                    arr[1] = noti.topic;
                    arr[2] = noti.method;
                    memcpy(&arr[3], noti.data, noti.len);
                    transport->encode(ModeNormal, arr, arr[0]);
                    free(arr);
                    }
                if (transport->fn_send){
                    transport->fn_send(NULL, 0); // Send whole tx buffer
                    break;
                }
                }
            } while (transport);
            return 0;
    }

static int Mtor_receive(uint8_t const* in_data, const size_t in_len, uint8_t* out_data, const size_t out_len)
    {
    int idx = 0, i;
    size_t respLen = 0;
    size_t l = 0;

    // LOGBUF(in_data, in_len);
    if (in_len < FUNC_MIN_HEADER_SIZE)
        return 0;

    for (i = 0; i < CONFIG_REVEIVER_CNT; i++)
        {
        LOGD("receiver: %0.2x, in_data: 0x%0.2x, in_len: %d", _mtor.receivers[i].fnRcv, in_data, in_len);
        if ((_mtor.receivers[i].topic == in_data[1]))
            {
            l = _mtor.receivers[i].fnRcv(in_data, in_len, out_data, out_len);
            LOGBUF(in_data, in_len);
            respLen += l; // & 0x3FFFFFFF;
            break;
            }
        }
    return respLen;
    }

static size_t Mtor_send(int id, uint8_t* const data, const size_t len)
    {
    transport_t* transport = Mtor_getTransportById(id);
    size_t ret = 0;
//    LOGI("transport: %0.2x, id: %d transport->direct_transport: %0.2x", transport, transport->transport_id, transport->direct_transport);
    if (transport)
        {
        ret = transport->fn_send(data, len);
        if (transport->direct_transport)
            {
//            LOGI("transport->direct_transport id: %0.2x, len: %d", transport->direct_transport, len);
            ret = transport->direct_transport->fn_send(data, len);
            }
        }
    LOGD("Sent %d", ret, id);
    return ret;
    }

static transport_t* Mtor_getTransportBySubId(int topic)
    {
        static int i;

    for (; i < CONFIG_ASYNC_QUEUE_LENGTH; ++i)
        {
        LOGI("Explore transport %d %x", i, _mtor.subscriptions[i].transport);
        for (int j = 0; j < CONFIG_REVEIVER_CNT; j++)
            {
            LOGD("Search topic %x, have topic %x", topic, _mtor.subscriptions[i].topics[j]);
            if (topic == _mtor.subscriptions[i].topics[j])
                return _mtor.subscriptions[i].transport;
            }
        }
    i = 0;
    return NULL;
    }

extern int Mtor_registerTransport(transport_t* transport)
    {
    for (int i = 0; i < CONFIG_TRANSPORT_CNT; i++)
        {
        if (NULL == _mtor.transports[i])
            {
            transport->transport_id = i + 1;
            transport->notify = Mtor_rcvCb;
            _mtor.transports[i] = transport;
            LOGD("Added transport id %d for 0x%0.2x", _mtor.transports[i]->transport_id, _mtor.transports[i]);
            return i;
            }
        }
    return -1;
    }

extern transport_t* Mtor_getTransportById(int id)
    {
    for (int i = 0; i < CONFIG_TRANSPORT_CNT; i++)
        {
        if (id == _mtor.transports[i]->transport_id)
            return _mtor.transports[i];
        }
    LOGD("No transport withid: %d", id);
    return NULL;
    }

extern void Mtor_MakeBridge(void* tr1, void* tr2)
    {
    LOGD("tr1: %0.2x,  tr2: %0.2x", tr1, tr2);
    if (tr1 && tr2)
        {
        ((transport_t*)tr1)->direct_transport = (transport_t*)tr2;
        ((transport_t*)tr2)->direct_transport = (transport_t*)tr1;
        LOGD("tr1: %x tr1->direct_transport: %x, tr2: %x tr2->direct_transport: %x",
            tr1, ((transport_t*)tr1)->direct_transport, tr2, ((transport_t*)tr2)->direct_transport);
        }
    }

extern void Mtor_registerReceiver(int topic, rcv_fn fn_rcv)
    {
    for (int i = 0; i < CONFIG_REVEIVER_CNT; i++)
        {
        if (_mtor.receivers[i].topic == -1)
            {
            _mtor.receivers[i].topic = topic;
            _mtor.receivers[i].fnRcv = fn_rcv;
            return;
            }
        }
    }

void Mtor_registerSubscription(transport_t* tr, uint8_t topic)
    {
    transport_t* transport = NULL;
    int idx = 0;
    for (; idx < sizeof(_mtor.subscriptions) / sizeof(subscription_t); idx++)
        {
        LOGD("_mtor.subscriptions[%d].topic", idx, topic);
        if ((_mtor.subscriptions[idx].transport == tr) || (NULL == _mtor.subscriptions[idx].transport))
            {
            _mtor.subscriptions[idx].transport = tr;
            break;
            }
        }
    if (idx < sizeof(_mtor.subscriptions) / sizeof(subscription_t))
        {
        for (int i = 0; i < sizeof(_mtor.subscriptions[idx].topics); i++)
            {
            LOGD("_mtor.subscriptions[%d].topic[%02x]", idx, _mtor.subscriptions[idx].topics[i]);
            if (_mtor.subscriptions[idx].topics[i] == 0)
                {
                _mtor.subscriptions[idx].topics[i] = topic;
                LOGD("_mtor.subscriptions[%d].topic[%02x]: %x, transport: %x",
                    idx, i, _mtor.subscriptions[idx].topics[i], _mtor.subscriptions[idx].transport);
                return;
                }
            }
        }
    }

extern void Mtor_rcvCb(int evt, ...)
    {
    va_list list;
    va_start(list, evt);
    struct msg_t
        {
        int src_id;
        uint8_t* data;
        size_t len;
        } msg = { evt, (uint8_t*)va_arg(list, uint8_t*), va_arg(list, size_t) };
        va_end(list);
        LOGD("msg.src_id:%d, msg.data:%s, msg.len:%d", msg.src_id, msg.data, msg.len);
        esp_event_post_to(_mtor_event_loop, MEDIATOR_EVENTS, DataReceived, &msg, sizeof(struct msg_t) / sizeof(char), portMAX_DELAY);
    }

void Mtor_notifyCb(const uint8_t topic, const uint8_t method, uint8_t* const data, const uint8_t length)
    {
    struct noti_t
        {
        uint8_t topic;
        uint8_t method;
        uint8_t* data;
        uint8_t len;
        } noti = { topic, method, data, length };
        LOGD("noti.src_id:%x, noti.method: %x, noti.data:%s, noti.len:%d", noti.topic, noti.method, noti.data, noti.len);
        esp_event_post_to(_mtor_event_loop, MEDIATOR_EVENTS, Notification, &noti, noti.len, portMAX_DELAY);
    }

//--------------------------------------------------------------------

size_t Mtor_nAck(uint8_t fn_id, uint8_t attr, LsRespStatus status, int8_t idx, uint8_t* const buf)
    {
    LOGD("%0.2x %d %d", fn_id, attr, status);
    if (!fn_id || !buf)
        return 0;
    buf[0] = 3;
    buf[1] = fn_id;
    buf[2] = attr;
    if (idx >= 0)
        {
        buf[0]++;
        buf[3] = idx;
        }

    if (status != RESP_OK)
        {
        buf[1] |= 0x80;
        if (idx >= 0)
            {
            buf[4] = (uint8_t)status;
            buf[0]++;
            }
        else
            {
            buf[3] = (uint8_t)status;
            }
        }

    return buf[0];
    }
