/**
 * @file main.c
 * @brief OLED + GB2312 ??????
 */
#pragma clang diagnostic ignored "-Winvalid-source-encoding"
#include "ti_msp_dl_config.h"
#include "OLED.h"
#include "delay.h"
#include "key.h"
#include "led.h"
#include "oled_menu.h"
#include "w25q64.h"
#include "bsp_IR_io.h"
#include "Encode.h"
#include "line_following.h"
#include "PWM.h"
#include <stdbool.h>
#include <stdio.h>
#include <uart.h>
#include "param.h"

static char LText[12];
static char RText[12];

void pwm_setleftspeed();
void pwm_setrightspeed();
void pwm_setflag();
void pwm_updateText(void);

static void blink_times(uint8_t n) {
    uint8_t i;
    for (i = 0; i < n; i++) { led_on(); delay(150); led_off(); delay(150); }
}

#define DEFINE_BLINK_FUNC(n) static void blink_##n(void) { blink_times(n); }
DEFINE_BLINK_FUNC(1) DEFINE_BLINK_FUNC(2) DEFINE_BLINK_FUNC(3) DEFINE_BLINK_FUNC(4)
DEFINE_BLINK_FUNC(7) DEFINE_BLINK_FUNC(8) DEFINE_BLINK_FUNC(9) DEFINE_BLINK_FUNC(10)

static const OLED_MenuItem mainItems[5];

static const OLED_MenuItem subMenu3[] = {
    {(uint8_t*)"P", NULL, 0, blink_1},
    {(uint8_t*)"I", NULL, 0, blink_2},
    {(uint8_t*)"D", NULL, 0, blink_3},
    {(uint8_t*)"back",     mainItems, 5}
};

static const OLED_MenuItem subMenu2[] = {
    {(uint8_t*)"????2-1", NULL, 0, blink_3},
    {(uint8_t*)"????2-2", NULL, 0, blink_4},
    {(uint8_t*)"back",     mainItems, 5}
};

  // ?????
static OLED_MenuItem subMenu1[] = {
    {(uint8_t*)LText, NULL, 0, pwm_setleftspeed},
    {(uint8_t*)RText, NULL, 0, pwm_setrightspeed},
    {(uint8_t*)"+/-", NULL, 0, pwm_setflag},
    {(uint8_t*)"back",     mainItems, 5}
};

static const OLED_MenuItem subMenu4[] = {
    {(uint8_t*)"????4-1", NULL, 0, blink_7},
    {(uint8_t*)"????4-2", NULL, 0, blink_8},
    {(uint8_t*)"back",     mainItems, 5}
};

static const OLED_MenuItem subMenu5[] = {
    {(uint8_t*)"????5-1", NULL, 0, blink_9},
    {(uint8_t*)"????5-2", NULL, 0, blink_10},
    {(uint8_t*)"back",     mainItems, 5}
};

static const OLED_MenuItem mainItems[5] = {
    {(uint8_t*)"pwm",   subMenu1,  OLED_MENU_COUNT(subMenu1)},
    {(uint8_t*)"123", subMenu2, OLED_MENU_COUNT(subMenu2)},
    {(uint8_t*)"PID",   subMenu3,  OLED_MENU_COUNT(subMenu3)},
    {(uint8_t*)"duoji",       subMenu4,  OLED_MENU_COUNT(subMenu4)},
    {(uint8_t*)"K210",  subMenu5,  OLED_MENU_COUNT(subMenu5)},
};


OLED_Menu menu;

int main(void) {
    SYSCFG_DL_init(); W25Q64_Init(); delay(5);
    W25Q64_ReadID(); led_on();
    PARAM_Load();
    KEY_Init(); OLED_Init();
		pwm_updateText();
    OLED_SetFontFlashBase(GB2312_FONT_FLASH_ADDR);
    OLED_Menu_Init(&menu);
    OLED_Menu_SetRoot(&menu, mainItems, 5);
    OLED_Menu_Draw(&menu);
//	pwm_setleftspeed();

//		pwm_setrightspeed();

	printf("R:%d\r\n",right_speed);
	printf("L:%d\r\n",left_speed);
while (1) {

    KeyEvent key = KEY_Scan();
	printf("R:%d\r\n",right_speed);
    switch (key) {
        case KEY_UP:    OLED_Menu_Prev(&menu); break;
        case KEY_DOWN:  OLED_Menu_Next(&menu); break;
        case KEY_ENTER: OLED_Menu_Enter(&menu); break;
        default: break;
    }
}
}

void pwm_setleftspeed(){
    if(pwm_flag) left_speed+=100; else left_speed-=100;
//	if(left_speed>=1000) left_speed=1000;
//	else if(left_speed<=-1000) left_speed=-1000;
    pwm_updateText(); //PWM_Run(left_speed,right_speed);
    PARAM_Save();
}

void pwm_setrightspeed(){
    if(pwm_flag) right_speed+=100; else right_speed-=100;
//		if(right_speed>=1000) right_speed=1000;
//	else if(right_speed<=-1000) right_speed=-1000;
    pwm_updateText(); //PWM_Run(left_speed,right_speed);
	printf("%d\r\n",right_speed);
    PARAM_Save();
}

void pwm_setflag(){ pwm_flag = !pwm_flag; PARAM_Save(); }

void pwm_updateText(void) {
    snprintf(LText, sizeof(LText), "L:%d", left_speed);
    snprintf(RText, sizeof(RText), "R:%d", right_speed);
//		printf("%d",right_speed);
}
/* --- printf �ض��� (Keil MicroLIB) --- */
int fputc(int ch, FILE *f)
{
    (void)f;
    uart_putc((uint8_t)ch);
    if (ch == '\n') uart_putc('\r');
    return ch;
}
