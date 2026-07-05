#ifndef PARAM_H
#define PARAM_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 电机参数
 * ============================================================ */
extern volatile int16_t left_speed;
extern volatile int16_t right_speed;
extern bool pwm_flag;

/* ============================================================
 * Flash 存储（W25Q64）
 * 字库从 0x00000 开始，约 282752 字节
 * 参数存 0x50000，扇区对齐，互不干扰
 * ============================================================ */
#define PARAM_FLASH_ADDR    0x00050000
#define PARAM_MAGIC         0x50505253  /* "SPRP" */

typedef struct {
    uint32_t  magic;
    int16_t   left_speed;
    int16_t   right_speed;
    uint8_t   pwm_flag;
    uint8_t   _rsv;
} ParamBlock;

bool PARAM_Load(void);
bool PARAM_Save(void);

#endif
