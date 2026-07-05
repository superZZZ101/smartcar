/*
 * uart.c — MSPM0G3507 UART 驱动 (轮询模式, 使用 DL_UART 标准库)
 *
 * 提供串口收发基础函数, 用于调试输出和字库传输
 *
 * 依赖:
 *   - ti_msp_dl_config.h (UART0_INST 宏定义)
 *   - MSPM0 DriverLib    (DL_UART_*)
 *
 * UART 配置 (SysConfig):
 *   Tx: GPIOA.10
 *   Rx: GPIOA.11
 *   波特率: 115200 8N1
 */

#include "uart.h"

/**
 * @brief  串口发送一个字节 (阻塞)
 * @param  c  要发送的字节
 */
void uart_putc(uint8_t c)
{
    DL_UART_transmitDataBlocking(UART_0_INST, c);
}

/**
 * @brief  串口接收一个字节 (阻塞)
 * @return 接收到的字节
 */
uint8_t uart_getc(void)
{
    return DL_UART_receiveDataBlocking(UART_0_INST);
}

/**
 * @brief  检查串口是否有数据可读
 * @return 1 = 有数据, 0 = 无数据
 */
uint8_t uart_available(void)
{
    return !DL_UART_isRXFIFOEmpty(UART_0_INST);
}

/**
 * @brief  串口发送字符串
 * @param  str  以 \0 结尾的字符串
 */
void uart_puts(const char *str)
{
    while (*str)
        uart_putc((uint8_t)*str++);
}

/**
 * @brief  串口发送十六进制数据, 带空格分隔, 末尾加换行
 * @param  data  数据缓冲区
 * @param  len   数据长度
 *
 * 输出格式: "12 34 AB CD ...\r\n"
 */
void uart_puthex(const uint8_t *data, uint32_t len)
{
    static const char hex[] = "0123456789ABCDEF";
    for (uint32_t i = 0; i < len; i++)
    {
        uart_putc((uint8_t)hex[(data[i] >> 4) & 0x0F]);
        uart_putc((uint8_t)hex[data[i] & 0x0F]);
        uart_putc(' ');
    }
    uart_puts("\r\n");
}
