/*
 * OLED.h — 128x64 I2C OLED 驱动 (SH1106/SSD1306 兼容)
 *
 * 所有字符使用 GB2312 16x16 字库 (从 W25Q64 读取)
 * ASCII 自动映射为全角显示
 * 布局: 4 行 x 8 列
 *
 * 引脚: SCL=PB2, SDA=PB3
 */

#ifndef __OLED_H
#define __OLED_H

#include "stdint.h"
#include "ti_msp_dl_config.h"

#define OLED_WIDTH         128
#define OLED_HEIGHT        64
#define OLED_PAGES         8
#define OLED_LINES         4
#define OLED_COLS          8       /* 每行 8 个全角字符 */

/* 字库地址 */
#define GB2312_FONT_FLASH_ADDR      0
#define GB2312_ZONE_COUNT           94
#define GB2312_BYTES_PER_CHAR       32

/* 底层 */
void OLED_Init(void);
void OLED_Clear(void);
void OLED_SetCursor(uint8_t Page, uint8_t Col);
void OLED_WriteCommand(uint8_t cmd);
void OLED_WriteData(uint8_t data);

/* GB2312 中文/全角显示 (Line=1~4, Column=1~8) */
void OLED_SetFontFlashBase(uint32_t addr);
int8_t OLED_ShowChinese(uint8_t Line, uint8_t Column, uint16_t gbCode);
void OLED_ShowChineseString(uint8_t Line, uint8_t Column, const uint8_t *str);

/* ASCII → 全角 GB2312 显示 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Ch);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *str);

/* 数字显示 */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/* 菜单渲染 */
void OLED_FillRow(uint8_t Line, uint8_t on);
void OLED_MenuRow(uint8_t Line, const uint8_t *text, uint8_t selected);

#endif
