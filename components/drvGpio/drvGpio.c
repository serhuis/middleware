#include <stdio.h>
#include "esp_log.h"
#include "drvGpio.h"

#define DEF_STM_RESET_IO        ( 0 )
#define DEF_STM_BOOT0_IO        ( 1 )

#if defined CONFIG_STM_RESET_PIN
#define GPIO_STM_RESET      CONFIG_STM_RESET_PIN
#else 
#define GPIO_STM_RESET      DEF_STM_RESET_IO
#endif
#if defined CONFIG_STM_BOOT0_PIN
#define GPIO_STM_BOOT      CONFIG_STM_BOOT0_PIN
#else 
#define GPIO_STM_BOOT      DEF_STM_BOOT0_IO
#endif
#define GPIO_WAKE_UP       CONFIG_WAKE_UP_PIN

#define GPIO_IMPUT_PIN_SEL  ((1ULL<<GPIO_STM_RESET) | (1ULL<<GPIO_STM_BOOT) | (1ULL<<GPIO_STM_BOOT))

static gpio_num_t
Gpio_obsolete_convert(gpio_num_t pin_num){
    ESP_LOGD(__FILE__, "%d: requested pin: %d", __LINE__, pin_num);
    if(CONFIG_STM_RESET_PIN_ESP8266 == pin_num)
        pin_num = GPIO_STM_RESET;
    if(CONFIG_STM_BOOT0_PIN_ESP8266 == pin_num)
        pin_num = GPIO_STM_BOOT; 
    ESP_LOGD(__FILE__, "%d: module pin: %d", __LINE__, pin_num);
    return pin_num;
}

extern esp_err_t Gpio_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_IMPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    return gpio_config(&io_conf);
}

extern esp_err_t Gpio_getDirection(gpio_num_t gpio_num){
    return ESP_FAIL;    
}
extern esp_err_t Gpio_setDirection(gpio_num_t gpio_num, uint32_t dir){
    ESP_LOGE(__FILE__, "%s %d: requested mode: %d for pin: %d", __FUNCTION__, __LINE__, dir, gpio_num);
    gpio_mode_t mode = (GPIO_MODE_DEF_OUTPUT & dir) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    ESP_LOGE(__FILE__, "%s %d: converted mode: %d for pin: %d", __FUNCTION__, __LINE__, mode, gpio_num);
    return gpio_set_direction(Gpio_obsolete_convert(gpio_num), mode);    
}

extern uint8_t Gpio_getLevel(gpio_num_t gpio_num){
    return gpio_get_level(Gpio_obsolete_convert(gpio_num));
}

extern esp_err_t Gpio_setLevel(gpio_num_t gpio_num, uint32_t level){
    return gpio_set_level(Gpio_obsolete_convert(gpio_num), level);    
}