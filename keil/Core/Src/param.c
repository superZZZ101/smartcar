/*
 * param.c — 电机参数 + Flash 存取（W25Q64）
 *
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
        return false;  /* 无有效数据，保持默认 0 */

    left_speed  = pb.left_speed;
    right_speed = pb.right_speed;
    pwm_flag    = pb.pwm_flag;
    return true;
}

/* ============================================================
 * 保存参数到 Flash
 * ============================================================ */
bool PARAM_Save(void)
{
    ParamBlock pb;
    uint8_t check[16], i;
    int8_t ret;

    pb.magic       = PARAM_MAGIC;
    pb.left_speed  = left_speed;
    pb.right_speed = right_speed;
    pb.pwm_flag    = pwm_flag;
    pb._rsv        = 0;

    /* 先读扇区前 16 字节，判断是否已被擦除 */
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
