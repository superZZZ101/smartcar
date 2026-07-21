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

/* PID 参数 */
extern volatile float kp;
extern volatile float ki;
extern volatile float kd;
extern volatile float kp_spd;   /* 速度环比例 */
extern volatile float ki_spd;   /* 速度环积分 */
extern volatile bool PID_flag;
extern volatile float PID_step;
extern volatile float spd_step;

/* ============================================================
 * Flash 存储（W25Q64）
 * 字库从 0x00000 开始，约 282752 字节
 * 参数存 0x50000，扇区对齐，互不干扰
 * ============================================================ */
#define PARAM_FLASH_ADDR    0x00050000
#define PARAM_MAGIC         0x50505253  /* "SPRP" */

typedef struct {
    uint32_t  magic;
    float     kp;
    float     ki;
    float     kd;
    float     kp_spd;   /* 速度环比例 (v2) */
    float     ki_spd;   /* 速度环积分 (v2) */
} ParamBlock;

bool PARAM_Load(void);
bool PARAM_Save(void);

#endif
