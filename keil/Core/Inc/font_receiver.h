/**
 * font_receiver.h — MCU 通过 UART 接收字库并写入 W25Q64
 *
 * 协议 (v2, 带逐块回读校验):
 *   PC → MCU:  同步头 4B (EB 90 EB 90)
 *   PC → MCU:  文件长度 4B (小端)
 *   MCU → PC:  0x06 (ACK 长度确认)
 *   MCU:       擦除 W25Q64 扇区
 *   MCU → PC:  0x06 (ACK 擦除完成)
 *   PC → MCU:  字库数据 (每块 256B)
 *   MCU:       写入 W25Q64 → 回读 → 发回 PC
 *   MCU → PC:  回读的 256B 数据
 *   PC → MCU:  0x06 (ACK 比对通过) 或 0x15 (NAK 终止)
 *   ⋮ 重复直到全部发送完毕
 *   MCU → PC:  0x00 (写入完成)
 *   PC → MCU:  CRC32 4B (小端)
 *   MCU → PC:  0x06 (校验通过)
 *   MCU → PC:  0x15 (校验失败) + MCU 的 CRC32 4B (小端)
 *
 * 使用:
 *   W25Q64_Init();
 *   UART 已初始化 (115200 8N1)
 *   FontReceiver_Run(GB2312_FONT_FLASH_ADDR);
 */

#ifndef FONT_RECEIVER_H
#define FONT_RECEIVER_H
#include <stdint.h>
#include "stdio.h"
/**
 * @brief  通过 UART 接收 GB2312 字库并写入 W25Q64
 *
 * 会阻塞等待 PC 端发送同步头, 接收完整字库数据,
 * 写入 W25Q64 后做 CRC 校验确保数据正确。
 *
 * @param flashAddr  W25Q64 中字库起始地址 (通常是 0 或 312)
 * @return  0=成功, -1=协议错误, -2=Flash 操作失败
 */
int FontReceiver_Run(uint32_t flashAddr);

/**
 * @brief  独立校验 W25Q64 中字库数据的 CRC
 *
 * 通过 UART 接收 'V' 命令 + 地址(4B) + 长度(4B),
 * 算出 CRC 发回给 PC, 用于对比 Flash 数据完整性。
 *
 * 协议:
 *   PC → MCU:  'V' (0x56)
 *   PC → MCU:  flashAddr 4B (小端)
 *   PC → MCU:  fileLen 4B (小端)
 *   MCU → PC:  CRC32 4B (小端)
 */
void FontReceiver_VerifyFlash(void);

#endif /* FONT_RECEIVER_H */
