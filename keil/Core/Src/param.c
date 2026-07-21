/*
 * param.c — 电机参数 + Flash 存取（W25Q64）
 * 变量定义 + 存储/读取 都在这一个文件
 * Flash 地址 0x50000（字库 0x00000~0x45080 之后）
 */

#include "param.h"
#include "w25q64.h"
#include "delay.h"

/* ============================================================
 * 变量定义
 * ============================================================ */
volatile int16_t left_speed  = 0;
volatile int16_t right_speed = 0;
bool pwm_flag = 1;

volatile float kp = 1.00f;
volatile float ki = 0.00f;
volatile float kd = 0.00f;
volatile float kp_spd = 0.05f;   /* 速度环比例 */
volatile float ki_spd = 0.001f;   /* 速度环积分 */
volatile bool PID_flag = 1;
volatile float PID_step = 0.0025f;
volatile float spd_step = 0.005f;
/* ============================================================
 * 内部: 参数所在扇区基址
 * ============================================================ */
#define _SECTOR_BASE  (PARAM_FLASH_ADDR & ~(W25Q64_SECTOR_SIZE - 1))

/* ============================================================
 * 从 Flash 加载参数
 * ============================================================ */
bool PARAM_Load(void)
{
    ParamBlock pb;

    W25Q64_ReadData(PARAM_FLASH_ADDR, (uint8_t *)&pb, sizeof(pb));

    if (pb.magic != PARAM_MAGIC)
        return false;

    kp = pb.kp;
    ki = pb.ki;
    kd = pb.kd;
    kp_spd = pb.kp_spd;
    ki_spd = pb.ki_spd;
    return true;
}

/* ============================================================
 * 保存参数到 Flash
 * ============================================================ */
bool PARAM_Save(void)
{
    ParamBlock pb;
    uint8_t check[16], i;
    int8_t ret = W25Q64_OK;

    pb.magic = PARAM_MAGIC;
    pb.kp    = kp;
    pb.ki    = ki;
    pb.kd    = kd;
    pb.kp_spd = kp_spd;
    pb.ki_spd = ki_spd;

    /* 先读当前 16 字节，判断是否已被擦除 */
    W25Q64_ReadData(_SECTOR_BASE, check, 16);
    for (i = 0; i < 16; i++)
        if (check[i] != 0xFF) break;

    if (i < 16) {
        ret = W25Q64_EraseSector(_SECTOR_BASE);
        if (ret != W25Q64_OK) return false;
        delay(5);
    }


    W25Q64_WriteEnable();
    ret = W25Q64_WritePage(PARAM_FLASH_ADDR, (const uint8_t *)&pb, sizeof(pb));
    return (ret == W25Q64_OK);
}
