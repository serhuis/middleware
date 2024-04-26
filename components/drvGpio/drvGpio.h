#pragma once
#include "driver/gpio.h"

extern esp_err_t Gpio_init();
extern esp_err_t Gpio_getDirection(gpio_num_t gpio_num);
extern esp_err_t Gpio_setDirection(gpio_num_t gpio_num, uint32_t level);
extern uint8_t Gpio_getLevel(gpio_num_t gpio_num);
extern esp_err_t Gpio_setLevel(gpio_num_t gpio_num, uint32_t level);
