/*
 * led.c — MSPM0G3507 LED 控制驱动
 *
 * 引脚:
 *   LED: GPIOB.22 (低电平点亮)
 *
 * 依赖:
 *   - ti_msp_dl_config.h (LED_PORT, LED_LIGHT_PIN 宏)
 *   - MSPM0 DriverLib    (DL_GPIO_*)
 */

#include "led.h"

/**
 * @brief  点亮 LED
 */
void led_on(void)
{
    DL_GPIO_setPins(LED_PORT, LED_LIGHT_PIN);
}

/**
 * @brief  熄灭 LED
 */
void led_off(void)
{
    DL_GPIO_clearPins(LED_PORT, LED_LIGHT_PIN);
}
