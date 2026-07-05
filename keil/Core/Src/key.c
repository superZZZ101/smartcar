#include "key.h"
#include "ti_msp_dl_config.h"
#include "delay.h"

/* ============================================================
 * 硬件引脚映射
 * ============================================================ */


/* 按键电平（假设按下为低电平） */
#define KEY_PRESSED    0
#define KEY_RELEASED   1

/* 消抖延时 (ms) */
#define KEY_DEBOUNCE_MS   20

/* ============================================================
 * 状态
 * ============================================================ */
static uint8_t lastUp      = KEY_RELEASED;
static uint8_t lastDown    = KEY_RELEASED;
static uint8_t lastEnter   = KEY_RELEASED;
static uint8_t startupDone = 0;  // 启动后首次按键只消费不触发

/* ============================================================
 * 读一个按键（0 = 按下, 非0 = 松开）
 * ============================================================ */
static uint8_t KEY_Read(GPIO_Regs *port, uint32_t pin) {
    return (DL_GPIO_readPins(port, pin) & pin) ? KEY_RELEASED : KEY_PRESSED;
}

/* ============================================================
 * Init
 * ============================================================ */
void KEY_Init(void) {
    startupDone = 0;
}

/* ============================================================
 * Scan — 返回按键事件（边沿触发，消抖后）
 * ============================================================ */
KeyEvent KEY_Scan(void) {
    uint8_t up    = KEY_Read(KEY_PORT, KEY_UP_PIN);
    uint8_t down  = KEY_Read(KEY_PORT, KEY_DOWN_PIN);
    uint8_t enter = KEY_Read(KEY_PORT, KEY_ENTER_PIN);

    // 检测到按下 → 消抖
    if (up == KEY_PRESSED)    { delay(KEY_DEBOUNCE_MS); up    = KEY_Read(KEY_PORT, KEY_UP_PIN); }
    if (down == KEY_PRESSED)  { delay(KEY_DEBOUNCE_MS); down  = KEY_Read(KEY_PORT, KEY_DOWN_PIN); }
    if (enter == KEY_PRESSED) { delay(KEY_DEBOUNCE_MS); enter = KEY_Read(KEY_PORT, KEY_ENTER_PIN); }

    KeyEvent event = KEY_NONE;

    // 边沿检测：上次松开 → 这次按下
    if (up == KEY_PRESSED && lastUp == KEY_RELEASED)
        event = KEY_UP;
    else if (down == KEY_PRESSED && lastDown == KEY_RELEASED)
        event = KEY_DOWN;
    else if (enter == KEY_PRESSED && lastEnter == KEY_RELEASED)
        event = KEY_ENTER;

    // 启动后第一次按键只消费不触发
    if (event != KEY_NONE && !startupDone) {
        startupDone = 1;
        event = KEY_NONE;
    }

    // 保存当前状态
    lastUp    = up;
    lastDown  = down;
    lastEnter = enter;

    return event;
}
