/*
 * w25q64.h — W25Q64 SPI NOR Flash 驱动（GPIO 模拟 SPI）
 *
 * 引脚定义 (来自 ti_msp_dl_config.h):
 *   CS:  W25Q64_cs_PORT / W25Q64_cs_PIN   (GPIOA.27)
 *   CLK: W25Q64_clk_PORT / W25Q64_clk_PIN (GPIOB.25)
 *   DI:  W25Q64_di_PORT / W25Q64_di_PIN   (GPIOB.20, MOSI)
 *   DO:  W25Q64_do_PORT / W25Q64_do_PIN   (GPIOA.25, MISO)
 *
 * 用法:
 *   W25Q64_Init();
 *   W25Q64_EraseSector(0);            // 擦除第 0 个 4KB 扇区
 *   W25Q64_WritePage(0, buf, len);    // 写入一页 (最多 256 字节)
 *   W25Q64_ReadData(0, buf, len);     // 读取数据
 */

#ifndef W25Q64_H
#define W25Q64_H

#include <stdint.h>
#include "ti_msp_dl_config.h"
/* ============================================================
 * W25Q64 容量定义
 * ============================================================ */
#define W25Q64_PAGE_SIZE      256      /* 页大小 */
#define W25Q64_SECTOR_SIZE    4096     /* 扇区大小 — 4KB */
#define W25Q64_BLOCK32_SIZE   32768    /* 32KB 块 */
#define W25Q64_BLOCK64_SIZE   65536    /* 64KB 块 */
#define W25Q64_TOTAL_SIZE     8388608  /* 8 MB */

/* ============================================================
 * 常用指令码 (指令表)
 * ============================================================ */
#define W25Q64_CMD_WRITE_ENABLE         0x06
#define W25Q64_CMD_WRITE_DISABLE        0x04
#define W25Q64_CMD_READ_STATUS_REG1     0x05
#define W25Q64_CMD_READ_STATUS_REG2     0x35
#define W25Q64_CMD_WRITE_STATUS_REG     0x01
#define W25Q64_CMD_PAGE_PROGRAM         0x02
#define W25Q64_CMD_READ_DATA            0x03
#define W25Q64_CMD_FAST_READ            0x0B
#define W25Q64_CMD_SECTOR_ERASE         0x20
#define W25Q64_CMD_BLOCK_ERASE_32K      0x52
#define W25Q64_CMD_BLOCK_ERASE_64K      0xD8
#define W25Q64_CMD_CHIP_ERASE           0xC7
#define W25Q64_CMD_POWER_DOWN           0xB9
#define W25Q64_CMD_RELEASE_POWER_DOWN   0xAB
#define W25Q64_CMD_READ_MANUF_ID        0x90
#define W25Q64_CMD_READ_JEDEC_ID        0x9F
#define W25Q64_CMD_READ_UNIQUE_ID       0x4B

/* ============================================================
 * 状态寄存器位
 * ============================================================ */
#define W25Q64_SR1_BUSY                 (1 << 0)   /* 忙标志 */
#define W25Q64_SR1_WEL                 (1 << 1)   /* 写使能锁存 */
#define W25Q64_SR1_BP0                 (1 << 2)   /* 块保护位 0 */
#define W25Q64_SR1_BP1                 (1 << 3)   /* 块保护位 1 */
#define W25Q64_SR1_BP2                 (1 << 4)   /* 块保护位 2 */
#define W25Q64_SR1_TB                  (1 << 5)   /* 顶/底保护 */
#define W25Q64_SR1_SEC                 (1 << 6)   /* 扇区保护 */
#define W25Q64_SR1_SRP                 (1 << 7)   /* 状态寄存器保护 */

/* ============================================================
 * 错误/状态码
 * ============================================================ */
#define W25Q64_OK       0
#define W25Q64_ERR      (-1)
#define W25Q64_TIMEOUT  (-2)

/* ============================================================
 * 函数声明
 * ============================================================ */

/**
 * @brief  初始化 W25Q64 引脚 (GPIO 输出/输入配置已在 SYSCFG_DL_init 完成)
 *         此函数仅确保 CS 默认置高 (片选无效)
 */
void W25Q64_Init(void);

/**
 * @brief  读取 JEDEC ID (0x9F) — 验证芯片是否存在
 * @return JEDEC ID 值 (0xEF4017 表示 W25Q64JV), 失败返回 0
 */
uint32_t W25Q64_ReadID(void);

/**
 * @brief  读取状态寄存器 1
 * @return 状态寄存器值
 */
uint8_t W25Q64_ReadStatusReg1(void);

/**
 * @brief  等待芯片空闲 (BUSY 位清零)
 * @param  timeoutMs 超时时间 (ms), 0 表示不超时一直等
 * @return W25Q64_OK / W25Q64_TIMEOUT
 */
int8_t W25Q64_WaitBusy(uint32_t timeoutMs);

/**
 * @brief  清除块保护 (BP0-BP2), 使能全片擦写
 */
void W25Q64_DisableProtection(void);

/**
 * @brief  写使能 (发送 0x06)
 */
void W25Q64_WriteEnable(void);

/**
 * @brief  写禁止 (发送 0x04)
 */
void W25Q64_WriteDisable(void);

/**
 * @brief  擦除一个 4KB 扇区
 * @param  addr 扇区起始地址 (必须是 4KB 对齐)
 * @return W25Q64_OK / W25Q64_TIMEOUT
 */
int8_t W25Q64_EraseSector(uint32_t addr);

/**
 * @brief  擦除一个 32KB 块
 * @param  addr 块起始地址 (必须是 32KB 对齐)
 * @return W25Q64_OK / W25Q64_TIMEOUT
 */
int8_t W25Q64_EraseBlock32K(uint32_t addr);

/**
 * @brief  擦除一个 64KB 块
 * @param  addr 块起始地址 (必须是 64KB 对齐)
 * @return W25Q64_OK / W25Q64_TIMEOUT
 */
int8_t W25Q64_EraseBlock64K(uint32_t addr);

/**
 * @brief  全片擦除 (CHIP ERASE)
 * @return W25Q64_OK / W25Q64_TIMEOUT
 */
int8_t W25Q64_ChipErase(void);

/**
 * @brief  写入一页 (最多 256 字节，地址必须在同一页内)
 * @param  addr 写入起始地址
 * @param  data 数据缓冲区
 * @param  len  数据长度 (1 ~ 256)
 * @return W25Q64_OK / W25Q64_TIMEOUT / W25Q64_ERR
 */
int8_t W25Q64_WritePage(uint32_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief  写入任意长度数据 (自动跨页，自动扇区擦除以保证写入正确性)
 * @param  addr     写入起始地址
 * @param  data     数据缓冲区
 * @param  len      数据长度
 * @param  doErase  是否在执行写入前进行扇区擦除 (1=擦除, 0=不擦除)
 * @return W25Q64_OK / W25Q64_TIMEOUT / W25Q64_ERR
 *
 * @note   NOR Flash 只能将 1 写为 0。如果目标区域可能含有旧数据，
 *          建议 doErase = 1。如果确保目标区域已被擦除 (全 FF)，可传 0。
 */
int8_t W25Q64_Write(uint32_t addr, const uint8_t *data, uint32_t len, uint8_t doErase);

/**
 * @brief  读取数据
 * @param  addr 读取起始地址
 * @param  buf  读取缓冲区
 * @param  len  读取长度
 */
void W25Q64_ReadData(uint32_t addr, uint8_t *buf, uint32_t len);

#endif /* W25Q64_H */
