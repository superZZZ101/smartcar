#ifndef __KEY_H
#define __KEY_H

#include "stdint.h"

/* ============================================================
 * 三按键模块 — UP / DOWN / ENTER
 * 支持消抖和边沿检测（按下触发一次）
 * ============================================================ */

typedef enum {
    KEY_NONE   = 0,
    KEY_UP     = 1,
    KEY_DOWN   = 2,
    KEY_ENTER  = 3,
} KeyEvent;

/**
 * @brief 按键初始化（配置 GPIO 方向，已由 SysConfig 完成时可留空）
 */
void KEY_Init(void);

/**
 * @brief 扫描按键，返回按下事件（单次触发）
 *        需在主循环中周期性调用（建议 10~20ms 一次）
 */
KeyEvent KEY_Scan(void);

#endif
