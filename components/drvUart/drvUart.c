#include <stdio.h>
#include "string.h"
#include "drvUart.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"

// Logging
static const char *TAG = __FILE__;

#define LOGI(format, ...) ESP_LOGI(TAG, "%d: %s: " format, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGD(format, ...) ESP_LOGD(TAG, "%d: %s: " format, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW(TAG, "%d: %s: " format, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE(TAG, "%d: %s: " format, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define LOGBUF(data, len) ESP_LOG_BUFFER_HEX(TAG, data, len)

#if defined CONFIG_UART_1_PORT
#define UART_PORT UART_NUM_1
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)
#else 
#define UART_PORT UART_NUM_0
#define TXD_PIN (GPIO_NUM_21)
#define RXD_PIN (GPIO_NUM_20)
#endif

#if defined CONFIG_BAUD_9600
#define UART_BAUD (9600)
#elif defined CONFIG_BAUD_14400
#define UART_BAUD (14400)
#elif defined CONFIG_BAUD_19200
#define UART_BAUD (19200)
#elif defined CONFIG_BAUD_28800
#define UART_BAUD (28800)
#elif defined CONFIG_BAUD_38400
#define UART_BAUD (38400)
#elif defined CONFIG_BAUD_57600
#define UART_BAUD (57600)
#elif defined CONFIG_BAUD_115200
#define UART_BAUD (115200)
#elif defined CONFIG_BAUD_256000
#define UART_BAUD (256000)
#endif

#if defined CONFIG_5_DATA_BITS
#define DATA_BITS (UART_DATA_5_BITS)
#elif defined CONFIG_6_DATA_BITS
#define DATA_BITS (UART_DATA_6_BITS)
#elif defined CONFIG_7_DATA_BITS
#define DATA_BITS (UART_DATA_7_BITS)
#else
#define DATA_BITS (UART_DATA_8_BITS)
#endif

#if defined CONFIG_UART_PARITY_EVEN
#define PARITY (UART_PARITY_EVEN)
#elif defined CONFIG_UART_PARITY_ODD
#define PARITY (UART_PARITY_ODD)
#else
#define PARITY (UART_PARITY_DISABLE)
#endif

#if defined CONFIG_STOP_BITS_1
#define STOP_BITS (UART_STOP_BITS_1)
#elif defined CONFIG_STOP_BITS_1_5
#define STOP_BITS (UART_STOP_BITS_1_5)
#elif defined CONFIG_STOP_BITS_2
#define STOP_BITS (UART_STOP_BITS_2)
#else
#define STOP_BITS (UART_STOP_BITS_1)
#endif

#if defined CONFIG_FLOWCTRL_RTS
#define FLOW_CTRL (UART_HW_FLOWCTRL_RTS)
#elif defined CONFIG_FLOWCTRL_CTS
#define FLOW_CTRL (UART_HW_FLOWCTRL_CTS)
#elif defined CONFIG_FLOWCTRL_CTS_RTS
#define FLOW_CTRL (UART_HW_FLOWCTRL_CTS_RTS)
#else
#define FLOW_CTRL (UART_HW_FLOWCTRL_DISABLE)
#endif

static const int BUF_SIZE = 512;
// static void Uart_rx_task(void *arg);
static void Uart_event_task(void *pvParameters);
static user_msg _uartMsg = NULL;

static QueueHandle_t _uart_queue;

extern void Uart_init(user_msg cb)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD,
        .data_bits = DATA_BITS,
        .parity = PARITY,
        .stop_bits = STOP_BITS,
        .flow_ctrl = FLOW_CTRL,
        .source_clk = UART_SCLK_APB,
    };
    _uartMsg = cb;
    // We won't use a buffer for sending data.
    /// uart_driver_install(_uart, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_driver_install(UART_PORT, BUF_SIZE * 2, BUF_SIZE * 2, 20, &_uart_queue, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /// xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(Uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
    LOGI("UART: %d", UART_PORT);
}

extern size_t Uart_send(uint8_t uart, uint8_t *const data, const size_t len)
{
    LOGD("UART: %d", UART_PORT);
    //ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO);
    return uart_write_bytes(UART_PORT, data, len);
    ;
}

static size_t Uart_rcv(uint8_t * buf, size_t size, uint32_t wait)
{
    return uart_read_bytes(UART_PORT, buf, size, wait);
}

/*
static void Uart_rx_task(void *arg)
{
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE+1);
    while (1) {
            const int rxBytes = uart_read_bytes(_uart, data, BUF_SIZE, portTICK_PERIOD_MS);
            if (rxBytes > 0) {
                data[rxBytes] = 0;
                if(_uartMsg != NULL){
                    _uartMsg(_uart, data, rxBytes);
                }
            }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    free(data);
}
*/

static void Uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t *dtmp = (uint8_t *)calloc(1, BUF_SIZE);
    for (;;)
    {
        // Waiting for UART event.
        if (xQueueReceive(_uart_queue, (void *)&event, (TickType_t)portMAX_DELAY))
        {
            LOGD("uart[%d] event:", UART_PORT);
            switch (event.type)
            {
            // Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                bzero(dtmp, BUF_SIZE);
                LOGD("%d", event.size);
                const int rxBytes = Uart_rcv(dtmp, event.size, portMAX_DELAY);
                if (_uartMsg != NULL)
                {
                    _uartMsg(UART_PORT, dtmp, event.size);
                }
                break;
            // Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                LOGW("hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(UART_PORT);
                xQueueReset(_uart_queue);
                break;
            // Event of UART ring buffer full
            case UART_BUFFER_FULL:
                LOGW("ring buffer full");
                // If buffer full happened, you should consider encreasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(UART_PORT);
                xQueueReset(_uart_queue);
                break;
            // Event of UART RX break detected
            case UART_BREAK:
                LOGW("uart rx break");
                break;
            // Event of UART parity check error
            case UART_PARITY_ERR:
                LOGD("uart parity error");
                break;
            // Event of UART frame error
            case UART_FRAME_ERR:
                LOGE("uart frame error");
                break;
            // Others
            default:
                LOGW("uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}