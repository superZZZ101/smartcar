/*
 * OLED.h — 128x64 I2C OLED 驱动 (SH1106/SSD1306 兼容)
 *
 * 中文: GB2312 16x16 全角字库 (W25Q64)
 * 英文: 内建 8x16 半角字库
 * 布局: 4 行, Column=1~16 (半角 8px 单位), 中文占 2 格
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
#define OLED_COLS          16      /* 每行半角 8px 单位 */
#define OLED_COLS_FW       8       /* 每行全角 16px 单位 (仅旧 ChineseString 用) */

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

/* 像素级混排: px = 像素 X 坐标 (0~127) */
void OLED_ShowChinesePx(uint8_t Line, uint16_t px, uint16_t gbCode);
void OLED_ShowHalfWidthAtPx(uint8_t Line, uint16_t px, char ch);

/* 混排显示: Column=1~16 (半角 8px 单位), 中文占 2 格, 英文占 1 格 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Ch);
void OLED_ShowString(uint8_t Line, uint8_t Column, const uint8_t *str);

/* 数字显示 */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/* 菜单渲染 (混排: 英文半角 8px, 中文全角 16px) */
void OLED_FillRow(uint8_t Line, uint8_t on);
void OLED_MenuRow(uint8_t Line, const uint8_t *text, uint8_t selected);

#endif
