/**
 * font_receiver.c — MCU 通过 UART 接收 GB2312 字库并写入 W25Q64
 *
 * 与 PC 端 gen_send_font.py 配合, 协议流程:
 *
 *   PC → MCU  同步头    EB 90 EB 90           (4 字节)
 *   PC → MCU  长度      文件总字节数           (4 字节 LE)
 *   MCU → PC  ACK      0x06                   (长度确认)
 *   MCU                擦除 W25Q64 扇区
 *   MCU → PC  ACK      0x06                   (擦除完成)
 *   PC → MCU  数据块   每块 256 字节           (依次发送)
 *   MCU → PC  ACK      0x06                   (每块确认)
 *   MCU → PC  DONE     0x00                   (写入完成)
 *   PC → MCU  CRC      全字库 CRC32           (4 字节 LE)
 *   MCU → PC  ACK      0x06                   或 NAK 0x15 + MCU CRC
 *
 * 调用前需初始化:
 *   - UART @ 115200 8N1
 *   - W25Q64_Init()
 *
 * 调用:
 *   FontReceiver_Run(GB2312_FONT_FLASH_ADDR);
 */

#include "font_receiver.h"
#include "uart.h"
#include "w25q64.h"
#include "led.h"

/* ──────────────────────────────────────────────────────────
 * 协议常量
 * ────────────────────────────────────────────────────────── */
#define SYNC_WORD         { 0xEB, 0x90, 0xEB, 0x90 }
#define SYNC_LEN          4
#define ACK               0x06
#define NAK               0x15
#define DONE              0x00
#define ERR               0xFF
#define BLOCK_SIZE        256U
#define CRC_BUF_SIZE      256U

/* 同步头等待超时: 约 1 秒 (假定 uart 无数据超时由驱动层处理) */
#define SYNC_TIMEOUT_MS   3000U

/* ──────────────────────────────────────────────────────────
 * CRC32 (与 PC 端 python crc32() 一致)
 * ────────────────────────────────────────────────────────── */
static uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320UL;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ──────────────────────────────────────────────────────────
 * 底层收发
 * ────────────────────────────────────────────────────────── */

/**
 * 清空 UART RX 缓冲区 — 防止自收发干扰协议
 */
static void uart_flush_rx(void)
{
    while (uart_available())
        uart_getc();
}


/**
 * 接收 4 字节小端无符号整数
 */
static uint32_t recv_u32(void)
{
    uint32_t val;
    val  = (uint32_t)uart_getc();
    val |= (uint32_t)uart_getc() << 8;
    val |= (uint32_t)uart_getc() << 16;
    val |= (uint32_t)uart_getc() << 24;
    return val;
}

/**
 * 发送 4 字节小端无符号整数
 */
static void send_u32(uint32_t val)
{
    uart_putc((uint8_t)(val >> 0));
    uart_putc((uint8_t)(val >> 8));
    uart_putc((uint8_t)(val >> 16));
    uart_putc((uint8_t)(val >> 24));
}

/* ──────────────────────────────────────────────────────────
 * 同步头检测
 * ────────────────────────────────────────────────────────── */

/**
 * 等待 PC 发送同步头 (EB 90 EB 90)
 *
 * 状态机方式匹配, 支持模糊起始 (如前 2 字节就乱,
 * 但后续又对上), 不会因 EB 90 出现两次而复位。
 *
 * @return 0=同步成功, -1=超时
 */
static int wait_sync(void)
{
    const uint8_t sync[SYNC_LEN] = SYNC_WORD;
    uint8_t match = 0;

    for (uint32_t timeout = 0; timeout < SYNC_TIMEOUT_MS * 1000UL; timeout++)
    {
        uint8_t b = uart_getc();

        if (b == sync[match])
        {
            match++;
        }
        else if (b == sync[0])
        {
            match = 1;          /* 重新从第 2 字节开始匹配 */
        }
        else
        {
            match = 0;          /* 完全失配, 从头来 */
        }

        if (match == SYNC_LEN)
        {
            return 0;
        }
    }

    return -1;  /* 超时 */
}

/* ──────────────────────────────────────────────────────────
 * 扇区擦除
 * ────────────────────────────────────────────────────────── */

/**
 * 擦除 W25Q64 中 [addr, addr+len) 范围覆盖的所有扇区 (4KB 对齐)
 *
 * @param addr  起始地址
 * @param len   字节数
 * @return 0=成功, -1=擦除失败
 */
static int erase_range(uint32_t addr, uint32_t len)
{
    uint32_t start = addr & ~(W25Q64_SECTOR_SIZE - 1U);
    uint32_t end   = (addr + len + W25Q64_SECTOR_SIZE - 1U) & ~(W25Q64_SECTOR_SIZE - 1U);

    for (uint32_t s = start; s < end; s += W25Q64_SECTOR_SIZE)
    {
        if (W25Q64_EraseSector(s) != W25Q64_OK)
        {
            return -1;
        }
    }
    return 0;
}

/* ──────────────────────────────────────────────────────────
 * 从 W25Q64 指定区域计算 CRC32
 * ────────────────────────────────────────────────────────── */

static uint32_t calc_flash_crc(uint32_t addr, uint32_t len)
{
    uint8_t buf[CRC_BUF_SIZE];
    uint32_t crc = 0xFFFFFFFFUL;

    while (len > 0)
    {
        uint16_t chunk = (len >= CRC_BUF_SIZE) ? CRC_BUF_SIZE : (uint16_t)len;
        W25Q64_ReadData(addr, buf, chunk);
        for (uint16_t i = 0; i < chunk; i++)
        {
            crc ^= buf[i];
            for (int b = 0; b < 8; b++)
            {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320UL;
                else
                    crc >>= 1;
            }
        }
        addr += chunk;
        len  -= chunk;
    }

    return crc ^ 0xFFFFFFFFUL;
}

/* ──────────────────────────────────────────────────────────
 * 公开 API
 * ────────────────────────────────────────────────────────── */

/**
 * @brief  通过 UART 接收字库并写入 W25Q64
 *
 * 阻塞执行, 完成整个协议流程后返回。
 *
 * @param flashAddr  W25Q64 目标起始地址
 * @return  0=成功, -1=协议错误/超时/校验失败, -2=Flash 操作失败
 */
int FontReceiver_Run(uint32_t flashAddr)
{
    uint8_t  blockBuf[BLOCK_SIZE];

    /* ── 1. 等待同步头 ── */
    if (wait_sync() != 0)
    {
        uart_putc(ERR);
        return -1;
    }

    /* ── 2. 接收文件长度 ── */
    uint32_t fileLen = recv_u32();
    if (fileLen == 0 || fileLen > 512U * 1024U)
    {
        uart_putc(NAK);
        return -1;
    }

    /* ── 3. ACK 长度确认 ── */
    uart_putc(ACK);

    /* ── 4. 解除写保护 + 擦除 ── */
    W25Q64_DisableProtection();

    if (erase_range(flashAddr, fileLen) != 0)
    {
        uart_putc(ERR);
        return -2;
    }

    /* ── 5. ACK 擦除完成 ── */
    uart_putc(ACK);

    /* 5b. 清空 RX — 排除自身 TX 回环到 RX 的可能 */
    uart_flush_rx();

    /* ── 6. 逐块接收、写入、回读并发送到 PC 比对 ── */
    uint32_t remaining = fileLen;
    uint32_t writeAddr = flashAddr;

    while (remaining > 0)
    {
        uint16_t chunk = (remaining >= BLOCK_SIZE) ? BLOCK_SIZE : (uint16_t)remaining;

        /* 从 UART 接收一块 */
        for (uint16_t i = 0; i < chunk; i++)
        {
            blockBuf[i] = uart_getc();
        }

        /* 写入 W25Q64 */
        if (W25Q64_Write(writeAddr, blockBuf, chunk, 1) != W25Q64_OK)
        {
            uart_putc(ERR);
            return -2;
        }

        /* ───── 回读并发送到 PC ───── */
        uint8_t verifyBuf[BLOCK_SIZE];
        W25Q64_ReadData(writeAddr, verifyBuf, chunk);

        /* 将回读数据全部发回 PC */
        for (uint16_t i = 0; i < chunk; i++)
        {
            uart_putc(verifyBuf[i]);
        }

        /* 清空 RX — 排除回显字节自收自扰 */
        uart_flush_rx();

        /* 等待 PC 的校验结果: ACK=通过, NAK=失败 */
        uint8_t result = uart_getc();
        if (result != ACK)
        {
            return -1;
        }

        writeAddr += chunk;
        remaining -= chunk;
    }

    /* ── 7. 通知 PC 写入完成 ── */
    uart_putc(DONE);

    /* ── 8. 接收 PC 端 CRC 并校验 ── */
    uint32_t expectedCRC = recv_u32();
    uint32_t actualCRC   = calc_flash_crc(flashAddr, fileLen);

    if (actualCRC == expectedCRC)
    {
        uart_putc(ACK);
        return 0;
    }

    /* 校验失败: NAK + MCU 计算的 CRC */
    uart_putc(NAK);
    send_u32(actualCRC);
    return -1;
}

/**
 * @brief  独立校验 W25Q64 中字库数据的 CRC
 *
 * 由 PC 端 verify_font.py 通过 'V' 命令调用。
 * MCU 从 W25Q64 读回数据计算 CRC 并发送给 PC。
 *
 * 协议:
 *   PC → MCU  'V'          (1 字节命令)
 *   PC → MCU  flashAddr    (4 字节 LE, 起始地址)
 *   PC → MCU  fileLen      (4 字节 LE, 长度)
 *   MCU → PC  crc32        (4 字节 LE, 计算结果)
 */
void FontReceiver_VerifyFlash(void)
{
    uint32_t verifyAddr = recv_u32();
    uint32_t verifyLen  = recv_u32();

    if (verifyLen == 0 || verifyLen > 1024U * 1024U)
    {
        send_u32(0xFFFFFFFFUL);
        return;
    }

    uint32_t crc = calc_flash_crc(verifyAddr, verifyLen);
    send_u32(crc);
}
