#ifndef __UART_H__
#define __UART_H__
#include "ti_msp_dl_config.h"
#include "stdint.h"

/* UART 实例宏 — SysConfig 生成 UART_0_INST, 没有则用 UART0 */
#ifndef UART_0_INST
#define UART_0_INST    UART0
#endif
uint8_t uart_getc(void);
void uart_putc(uint8_t c);
uint8_t uart_available(void);  /* 返回 1 = 有数据可读, 0 = 无数据 */
void uart_puts(const char *str);
void uart_puthex(const uint8_t *data, uint32_t len);
#endif