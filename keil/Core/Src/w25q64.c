/*
 * w25q64.c — W25Q64 SPI NOR Flash 驱动 (GPIO 模拟 SPI, Mode 0)
 *
 * SPI Mode 0 时序:
 *   CLK 空闲为低
 *   MOSI 数据在 CLK 下降沿改变
 *   MISO 数据在 CLK 上升沿采样
 *
 * 依赖:
 *   - ti_msp_dl_config.h (引脚宏定义及 SYSCFG_DL_init 已调用)
 *   - delay.h           (delay(uint16_t ms))
 *   - MSPM0 DriverLib   (DL_GPIO_*)
 *
 * 引脚定义 (来自 ti_msp_dl_config.h):
 *   CS:  GPIOA.27
 *   CLK: GPIOB.25
 *   DI:  GPIOB.20 (MOSI)
 *   DO:  GPIOA.25 (MISO)
 */

#include "w25q64.h"
#include "ti_msp_dl_config.h"
#include "delay.h"

/* ============================================================
 * 微秒级阻塞延时 (基于 delay_cycles)
 *   CPUCLK_FREQ = 32 MHz => 1 us ~= 32 cycles
 *   如果实际 CPU 频率有变化, 修改 DELAY_US_SCALE 即可
 * ============================================================ */
#define DELAY_US_SCALE  (CPUCLK_FREQ / 1000000UL)   /* ~= 32 */

static inline void delay_us(uint32_t us)
{
    delay_cycles(us * DELAY_US_SCALE);
}

/* ============================================================
 * SPI 硬件抽象 — GPIO 位带操作宏
 *
 * 将所有 SPI 引脚操作封装为宏, 方便移植
 * ============================================================ */

/* --- CS (片选) --- */
#define W25Q64_CS_LOW()    DL_GPIO_clearPins(W25Q64_cs_PORT, W25Q64_cs_PIN)
#define W25Q64_CS_HIGH()   DL_GPIO_setPins(W25Q64_cs_PORT, W25Q64_cs_PIN)

/* --- CLK (时钟) --- */
#define W25Q64_CLK_LOW()   DL_GPIO_clearPins(W25Q64_clk_PORT, W25Q64_clk_PIN)
#define W25Q64_CLK_HIGH()  DL_GPIO_setPins(W25Q64_clk_PORT, W25Q64_clk_PIN)

/* --- MOSI (DI) — 写数据到 Flash --- */
#define W25Q64_MOSI_LOW()  DL_GPIO_clearPins(W25Q64_di_PORT, W25Q64_di_PIN)
#define W25Q64_MOSI_HIGH() DL_GPIO_setPins(W25Q64_di_PORT, W25Q64_di_PIN)

/* --- MISO (DO) — 从 Flash 读数据 --- */
#define W25Q64_MISO_READ() \
    ((DL_GPIO_readPins(W25Q64_do_PORT, W25Q64_do_PIN) & W25Q64_do_PIN) ? 1U : 0U)

/* ============================================================
 * SPI 速度控制
 *   在 bit-bang 模式下, 通过延时调整 CLK 半周期
 *   默认半周期延时 1 us => SCK ~= 500 kHz (兼顾可靠性与速度)
 *   若接线较长或干扰大, 可调大 SPI_HALF_CYCLE_US
 * ============================================================ */
#define SPI_HALF_CYCLE_US  1

/* ============================================================
 * 内部函数: SPI 收发一个字节 (MSB first)
 *
 * 时序流程 (每 bit):
 *   1) CLK 低电平时设置 MOSI
 *   2) CLK 上升沿时采样 MISO
 *   3) CLK 下降沿准备下一位
 *
 * @param txByte  要发送的字节
 * @return 接收到的字节
 * ============================================================ */
static uint8_t W25Q64_SwapByte(uint8_t txByte)
{
    uint8_t rxByte = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        /* MOSI: 在 CLK 低电平时设置数据 */
        if (txByte & 0x80)
            W25Q64_MOSI_HIGH();
        else
            W25Q64_MOSI_LOW();

        txByte <<= 1;                   /* 准备下一位 */

        /* CLK 上升沿 — Flash 输出 MISO, 主机采样 */
        W25Q64_CLK_HIGH();
        delay_us(SPI_HALF_CYCLE_US);

        rxByte <<= 1;
        rxByte  |= W25Q64_MISO_READ();  /* 读取 MISO */

        /* CLK 下降沿 */
        W25Q64_CLK_LOW();
        delay_us(SPI_HALF_CYCLE_US);
    }

    return rxByte;
}

/* ============================================================
 * 内部函数: 发送连续字节 (不关心 MISO 返回值)
 * ============================================================ */
static void W25Q64_SendBytes(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        W25Q64_SwapByte(data[i]);
}

/* ============================================================
 * 内部函数: 接收连续字节 (MOSI 一直发 0xFF 以产生时钟)
 * ============================================================ */
static void W25Q64_RecvBytes(uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        buf[i] = W25Q64_SwapByte(0xFF);
}

/* ============================================================
 * 内部函数: 发送 3 字节地址 (MSB first, 大端)
 *   W25Q64 使用 24-bit 地址总线, 最大寻址 16 MB
 * ============================================================ */
static inline void W25Q64_SendAddr24(uint32_t addr)
{
    uint8_t addrBuf[3];
    addrBuf[0] = (uint8_t)(addr >> 16);     /* 地址高 8 位 */
    addrBuf[1] = (uint8_t)(addr >>  8);     /* 地址中 8 位 */
    addrBuf[2] = (uint8_t)(addr);            /* 地址低 8 位 */
    W25Q64_SendBytes(addrBuf, 3);
}

/* ============================================================
 * 内部函数: 发送单字节指令
 *
 * SendCmd:    拉低 CS -> 发指令, 不拉高 CS (调用者继续使用总线)
 * SendCmdEnd: 拉低 CS -> 发指令 -> 拉高 CS (单指令收尾)
 * ============================================================ */
static void W25Q64_SendCmd(uint8_t cmd)
{
    W25Q64_CS_LOW();
    W25Q64_SwapByte(cmd);
    /* 不拉高 CS — 调用者继续使用总线或自己拉高 */
}

static void W25Q64_SendCmdEnd(uint8_t cmd)
{
    W25Q64_SendCmd(cmd);
    W25Q64_CS_HIGH();
}

/* ============================================================
 * 初始化 W25Q64
 *
 * 设置 CS/CLK/MOSI 为默认空闲电平:
 *   CS  = 高 (片选无效)
 *   CLK = 低 (SPI Mode 0 空闲)
 *   MOSI = 低
 * 延时 1 ms 让 Flash 稳定
 * ============================================================ */
void W25Q64_Init(void)
{
    W25Q64_CS_HIGH();       /* 片选无效 */
    W25Q64_CLK_LOW();       /* SPI Mode 0: CLK 空闲为低 */
    W25Q64_MOSI_LOW();      /* MOSI 默认低 */
    delay(1);               /* 等待 Flash 就绪 */
}

/* ============================================================
 * 读取 JEDEC ID (指令 0x9F)
 *
 * 返回值:
 *   0xEF4017 — W25Q64JV (Winbond, 8 MB)
 *   0x000000 — 读取失败 (芯片未响应)
 * ============================================================ */
uint32_t W25Q64_ReadID(void)
{
    uint32_t id = 0;

    W25Q64_CS_LOW();
    W25Q64_SwapByte(W25Q64_CMD_READ_JEDEC_ID);
    id  = (uint32_t)W25Q64_SwapByte(0xFF) << 16;   /* Manufacturer ID */
    id |= (uint32_t)W25Q64_SwapByte(0xFF) << 8;     /* Memory Type     */
    id |= (uint32_t)W25Q64_SwapByte(0xFF);           /* Capacity        */
    W25Q64_CS_HIGH();

    return id;
}

/* ============================================================
 * 读状态寄存器 1 (指令 0x05)
 *
 * 状态寄存器 BIT 定义:
 *   Bit 0: BUSY  — 1=忙, 0=空闲
 *   Bit 1: WEL   — 写使能锁存
 *   Bit 2-4: BP0-2 — 块保护
 *   Bit 5: TB    — 顶/底保护
 *   Bit 6: SEC   — 扇区保护
 *   Bit 7: SRP   — 状态寄存器保护
 * ============================================================ */
uint8_t W25Q64_ReadStatusReg1(void)
{
    W25Q64_CS_LOW();
    W25Q64_SwapByte(W25Q64_CMD_READ_STATUS_REG1);
    uint8_t sr = W25Q64_SwapByte(0xFF);
    W25Q64_CS_HIGH();
    return sr;
}

/* ============================================================
 * 清除块保护 (BP0-BP2), 使能全片擦写
 *
 * 出厂默认 BP0-BP2 = 1, 芯片处于保护状态
 * 调用此函数后全片可读写擦除
 * ============================================================ */
void W25Q64_DisableProtection(void)
{
    W25Q64_WriteEnable();

    W25Q64_CS_LOW();
    W25Q64_SwapByte(0x01);  /* Write Status Register 指令 */
    W25Q64_SwapByte(0x00);  /* 写入 0x00: 清除 BP0-BP2, TB, SEC, SRP */
    W25Q64_CS_HIGH();

    W25Q64_WaitBusy(50);    /* 等待状态寄存器更新完成 */
}

/* ============================================================
 * 等待芯片空闲 (带超时机制)
 *
 * @param timeoutMs  超时毫秒数, 0 表示无限等待
 * @return W25Q64_OK (0) 或 W25Q64_TIMEOUT (-2)
 * ============================================================ */
int8_t W25Q64_WaitBusy(uint32_t timeoutMs)
{
    uint32_t elapsed = 0;

    while (W25Q64_ReadStatusReg1() & W25Q64_SR1_BUSY)
    {
        if (timeoutMs > 0)
        {
            if (++elapsed >= timeoutMs)
                return W25Q64_TIMEOUT;
        }
        delay(1);
    }

    return W25Q64_OK;
}

/* ============================================================
 * 写使能 (指令 0x06) / 写禁止 (指令 0x04)
 *
 * 每次擦除/写入前必须先发送写使能 (Write Enable)
 * 操作完成后建议发送写禁止 (Write Disable) 防止误写
 * ============================================================ */
void W25Q64_WriteEnable(void)
{
    W25Q64_SendCmdEnd(W25Q64_CMD_WRITE_ENABLE);
}

void W25Q64_WriteDisable(void)
{
    W25Q64_SendCmdEnd(W25Q64_CMD_WRITE_DISABLE);
}

/* ============================================================
 * 扇区擦除 (4 KB, 指令 0x20)
 *
 * 将指定 4 KB 扇区全部擦为 0xFF
 * 地址必须 4 KB 对齐
 * 擦除时间: 典型值 < 400 ms
 *
 * @param addr  扇区起始地址 (4 KB 对齐)
 * @return W25Q64_OK / W25Q64_TIMEOUT
 * ============================================================ */
int8_t W25Q64_EraseSector(uint32_t addr)
{
    /* 如果需要检查 4 KB 对齐, 可解除下面注释 */
    /* if (addr & (W25Q64_SECTOR_SIZE - 1)) return W25Q64_ERR; */

    W25Q64_WriteEnable();

    W25Q64_CS_LOW();
    W25Q64_SwapByte(W25Q64_CMD_SECTOR_ERASE);
    W25Q64_SendAddr24(addr);
    W25Q64_CS_HIGH();

    return W25Q64_WaitBusy(1000);   /* 扇区擦除最长约 1 秒 */
}

/* ============================================================
 * 块擦除 32 KB (指令 0x52)
 *
 * 地址必须 32 KB 对齐
 * 擦除时间: 典型值 < 2 秒
 * ============================================================ */
int8_t W25Q64_EraseBlock32K(uint32_t addr)
{
    W25Q64_WriteEnable();

    W25Q64_CS_LOW();
    W25Q64_SwapByte(W25Q64_CMD_BLOCK_ERASE_32K);
    W25Q64_SendAddr24(addr);
    W25Q64_CS_HIGH();

    return W25Q64_WaitBusy(3000);   /* 最长 3 秒 */
}

/* ============================================================
 * 块擦除 64 KB (指令 0xD8)
 *
 * 地址必须 64 KB 对齐
 * 擦除时间: 典型值 < 4 秒
 * ============================================================ */
int8_t W25Q64_EraseBlock64K(uint32_t addr)
{
    W25Q64_WriteEnable();

    W25Q64_CS_LOW();
    W25Q64_SwapByte(W25Q64_CMD_BLOCK_ERASE_64K);
    W25Q64_SendAddr24(addr);
    W25Q64_CS_HIGH();

    return W25Q64_WaitBusy(5000);   /* 最长 5 秒 */
}

/* ============================================================
 * 全片擦除 (指令 0xC7)
 *
 * 将整片 8 MB 擦为 0xFF
 * 擦除时间: 可能超过 40 秒
 * ============================================================ */
int8_t W25Q64_ChipErase(void)
{
    W25Q64_WriteEnable();

    W25Q64_CS_LOW();
    W25Q64_SwapByte(W25Q64_CMD_CHIP_ERASE);
    W25Q64_CS_HIGH();

    return W25Q64_WaitBusy(100000); /* 全片擦除最长约 100 秒 */
}

/* ============================================================
 * 写一页 (最大 256 字节, 指令 0x02)
 *
 * 起始地址和长度必须在同一页内, 不能跨页
 * 页编程时间: 典型值 < 0.7 ms
 *
 * @param addr  写入起始地址
 * @param data  数据缓冲区
 * @param len   数据长度 (1 ~ 256)
 * @return W25Q64_OK / W25Q64_TIMEOUT / W25Q64_ERR
 * ============================================================ */
int8_t W25Q64_WritePage(uint32_t addr, const uint8_t *data, uint16_t len)
{
    if (len == 0 || len > W25Q64_PAGE_SIZE)
        return W25Q64_ERR;

    W25Q64_WriteEnable();

    W25Q64_CS_LOW();
    W25Q64_SwapByte(W25Q64_CMD_PAGE_PROGRAM);
    W25Q64_SendAddr24(addr);
    W25Q64_SendBytes(data, len);
    W25Q64_CS_HIGH();

    return W25Q64_WaitBusy(50);     /* 页编程最长约 50 ms */
}

/* ============================================================
 * 写入任意长度 (自动跨页 + 可选扇区擦除)
 *
 * NOR Flash 特性: 只能将 1 写成 0, 不能将 0 写成 1
 * 因此写入前必须确保目标扇区已被擦除 (全 0xFF)
 *
 * @param addr    写入起始地址
 * @param data    数据缓冲区
 * @param len     数据长度
 * @param doErase 1 = 跨扇区时自动擦除, 0 = 不擦除
 *                首次使用或扇区有旧数据时建议传 1
 *                若确保扇区已擦除, 传 0 可加速
 * @return W25Q64_OK / W25Q64_TIMEOUT / W25Q64_ERR
 * ============================================================ */
int8_t W25Q64_Write(uint32_t addr, const uint8_t *data, uint32_t len, uint8_t doErase)
{
    while (len > 0)
    {
        /* 计算当前页内剩余空间 */
        uint16_t pageOffset = (uint16_t)(addr & (W25Q64_PAGE_SIZE - 1));
        uint16_t chunk = W25Q64_PAGE_SIZE - pageOffset;
        if (chunk > len) chunk = (uint16_t)len;

        /* 若开启自动擦除, 且当前地址为扇区起始, 先擦除扇区 */
        if (doErase && (addr & (W25Q64_SECTOR_SIZE - 1)) == 0 && pageOffset == 0)
        {
            int8_t ret = W25Q64_EraseSector(addr);
            if (ret != W25Q64_OK) return ret;
        }

        /* 写入当前页 */
        int8_t ret = W25Q64_WritePage(addr, data, chunk);
        if (ret != W25Q64_OK) return ret;

        /* 更新地址/指针/剩余长度 */
        addr += chunk;
        data += chunk;
        len  -= chunk;
    }

    return W25Q64_OK;
}

/* ============================================================
 * 读取数据 (指令 0x03)
 *
 * 无长度限制, 可跨页连续读取
 *
 * @param addr  读取起始地址
 * @param buf   读取缓冲区
 * @param len   读取长度
 * ============================================================ */
void W25Q64_ReadData(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25Q64_CS_LOW();
    W25Q64_SwapByte(W25Q64_CMD_READ_DATA);
    W25Q64_SendAddr24(addr);
    W25Q64_RecvBytes(buf, len);
    W25Q64_CS_HIGH();
}
