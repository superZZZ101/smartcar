/*
 * oled.c — 128x64 I2C OLED 驱动 (GB2312 + 半角 ASCII)
 *
 * 中文: GB2312 16x16 字模 (W25Q64 存储)
 * ASCII: 内嵌 8x16 半角点阵字库
 * 菜单渲染: 中英文混排, 中文全角/英文半角
 */

#include "OLED.h"
#include "ti_msp_dl_config.h"
#include "delay.h"
#include "w25q64.h"
#include "OLED_Font.h"

/* ============================================================
 * I2C 引脚操作
 * ============================================================ */
#define OLED_I2C_SCL(x)  ((x) ? DL_GPIO_setPins(OLED_PORT, OLED_SCL_PIN) : DL_GPIO_clearPins(OLED_PORT, OLED_SCL_PIN))
#define OLED_I2C_SDA(x)  ((x) ? DL_GPIO_setPins(OLED_PORT, OLED_SDA_PIN) : DL_GPIO_clearPins(OLED_PORT, OLED_SDA_PIN))
#define OLED_ADDR        0x78

#define DELAY_US_SCALE  (CPUCLK_FREQ / 1000000UL)
static inline void delay_us(uint32_t us) { delay_cycles(us * DELAY_US_SCALE); }

static void i2c_start(void) {
    OLED_I2C_SDA(1); OLED_I2C_SCL(1); delay_us(2);
    OLED_I2C_SDA(0); delay_us(2); OLED_I2C_SCL(0); delay_us(2);
}
static void i2c_stop(void) {
    OLED_I2C_SDA(0); OLED_I2C_SCL(1); delay_us(2);
    OLED_I2C_SDA(1); delay_us(2);
}
static void i2c_send_byte(uint8_t dat) {
    for (uint8_t i = 0; i < 8; i++) {
        OLED_I2C_SDA((dat & 0x80) ? 1 : 0); dat <<= 1; delay_us(1);
        OLED_I2C_SCL(1); delay_us(2); OLED_I2C_SCL(0); delay_us(1);
    }
    OLED_I2C_SDA(1); OLED_I2C_SCL(1); delay_us(2); OLED_I2C_SCL(0); delay_us(1);
}
static void i2c_write_bulk(const uint8_t *data, uint16_t len) {
    i2c_start(); i2c_send_byte(OLED_ADDR); i2c_send_byte(0x40);
    for (uint16_t i = 0; i < len; i++) i2c_send_byte(data[i]); i2c_stop();
}

void OLED_WriteCommand(uint8_t cmd) {
    i2c_start(); i2c_send_byte(OLED_ADDR); i2c_send_byte(0x00);
    i2c_send_byte(cmd); i2c_stop();
}
void OLED_WriteData(uint8_t data) {
    i2c_start(); i2c_send_byte(OLED_ADDR); i2c_send_byte(0x40);
    i2c_send_byte(data); i2c_stop();
}
void OLED_SetCursor(uint8_t Page, uint8_t Col) {
    OLED_WriteCommand(0xB0 | Page);
    OLED_WriteCommand(0x10 | ((Col & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (Col & 0x0F));
}

void OLED_Clear(void) {
    uint8_t zeros[128] = {0};
    for (uint8_t p = 0; p < OLED_PAGES; p++) {
        OLED_SetCursor(p, 0); i2c_write_bulk(zeros, 128);
    }
}

void OLED_Init(void) {
    delay(100);
    OLED_WriteCommand(0xAE); OLED_WriteCommand(0xD5); OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8); OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3); OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40);
    OLED_WriteCommand(0xA1); OLED_WriteCommand(0xC8);
    OLED_WriteCommand(0xDA); OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81); OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9); OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB); OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4); OLED_WriteCommand(0xA6);
    OLED_WriteCommand(0x8D); OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF); OLED_Clear();
}

/* ============================================================
 * GB2312 16x16 字模读取
 * ============================================================ */
static uint32_t g_fontAddr = GB2312_FONT_FLASH_ADDR;
void OLED_SetFontFlashBase(uint32_t addr) { g_fontAddr = addr; }

static int32_t gb2312_offset(uint16_t code) {
    uint8_t hi = (uint8_t)(code >> 8), lo = (uint8_t)(code & 0xFF);
    if (hi < 0xA1 || hi > 0xF7 || lo < 0xA1 || lo > 0xFE) return -1;
    return (int32_t)((hi - 0xA1) * GB2312_ZONE_COUNT + (lo - 0xA1)) * GB2312_BYTES_PER_CHAR;
}

/* 绘制 HZK16 字模到 OLED page 模式 */
static void draw_16x16(uint8_t page, uint8_t col, const uint8_t *bmp) {
    uint8_t buf[16];
    /* 上半 */
    for (uint8_t c = 0; c < 16; c++) {
        uint8_t v = 0;
        for (uint8_t r = 0; r < 8; r++) {
            uint8_t px;
            if (c < 8) px = (bmp[r*2] >> (7-c)) & 1;
            else       px = (bmp[r*2+1] >> (15-c)) & 1;
            if (px) v |= (1 << r);
        }
        buf[c] = v;
    }
    OLED_SetCursor(page, col); i2c_write_bulk(buf, 16);
    /* 下半 */
    for (uint8_t c = 0; c < 16; c++) {
        uint8_t v = 0;
        for (uint8_t r = 0; r < 8; r++) {
            uint8_t sr = r + 8, px;
            if (c < 8) px = (bmp[sr*2] >> (7-c)) & 1;
            else       px = (bmp[sr*2+1] >> (15-c)) & 1;
            if (px) v |= (1 << r);
        }
        buf[c] = v;
    }
    OLED_SetCursor(page+1, col); i2c_write_bulk(buf, 16);
}

/* ============================================================
 * 8x16 半角 ASCII 渲染 (内建字库)
 * ============================================================ */
static void draw_8x16(uint8_t page, uint8_t col, const uint8_t *bmp) {
    uint8_t buf[8];
    /* 上半: rows 0-7 */
    for (uint8_t c = 0; c < 8; c++) {
        uint8_t v = 0;
        for (uint8_t r = 0; r < 8; r++) {
            if ((bmp[r] >> (7 - c)) & 1) v |= (1 << r);
        }
        buf[c] = v;
    }
    OLED_SetCursor(page, col); i2c_write_bulk(buf, 8);
    /* 下半: rows 8-15 */
    for (uint8_t c = 0; c < 8; c++) {
        uint8_t v = 0;
        for (uint8_t r = 0; r < 8; r++) {
            if ((bmp[r + 8] >> (7 - c)) & 1) v |= (1 << r);
        }
        buf[c] = v;
    }
    OLED_SetCursor(page + 1, col); i2c_write_bulk(buf, 8);
}

/* ============================================================
 * GB2312 中文显示
 * ============================================================ */
int8_t OLED_ShowChinese(uint8_t Line, uint8_t Column, uint16_t gbCode) {
    if (Line < 1 || Line > OLED_LINES) return -1;
    if (Column < 1 || Column > OLED_COLS_FW) return -1;

    int32_t off = gb2312_offset(gbCode);
    if (off < 0) return -1;

    uint8_t bmp[32];
    W25Q64_ReadData(g_fontAddr + (uint32_t)off, bmp, 32);
    draw_16x16((Line - 1) * 2, (Column - 1) * 16, bmp);
    return 0;
}

void OLED_ShowChineseString(uint8_t Line, uint8_t Column, const uint8_t *str) {
    uint8_t col = Column;
    while (*str) {
        if (col > OLED_COLS_FW) break;
        OLED_ShowChinese(Line, col, ((uint16_t)str[0] << 8) | str[1]);
        str += 2; col++;
    }
}

/* ============================================================
 * 像素级位置显示 (混排用)
 * ============================================================ */

/** @brief 在指定像素 X 位置显示 GB2312 16x16 全角字符 */
void OLED_ShowChinesePx(uint8_t Line, uint16_t px, uint16_t gbCode) {
    if (Line < 1 || Line > OLED_LINES) return;
    if (px > OLED_WIDTH - 16) return;

    int32_t off = gb2312_offset(gbCode);
    if (off < 0) return;

    uint8_t bmp[32];
    W25Q64_ReadData(g_fontAddr + (uint32_t)off, bmp, 32);
    draw_16x16((Line - 1) * 2, (uint8_t)px, bmp);
}

/** @brief 在指定像素 X 位置显示 8x16 半角 ASCII 字符 */
void OLED_ShowHalfWidthAtPx(uint8_t Line, uint16_t px, char ch) {
    if (Line < 1 || Line > OLED_LINES) return;
    if (px > OLED_WIDTH - 8) return;
    if (ch < 0x20 || ch > 0x7E) ch = ' ';

    uint8_t page = (Line - 1) * 2;
    uint8_t col  = (uint8_t)px;
    uint8_t idx  = (uint8_t)(ch - 0x20);

    /* OLED_F8x16 是 column-major: [0..7] = 上半 8 列, [8..15] = 下半 8 列, 直写 */
    OLED_SetCursor(page, col);
    i2c_write_bulk(OLED_F8x16[idx], 8);

    OLED_SetCursor(page + 1, col);
    i2c_write_bulk(&OLED_F8x16[idx][8], 8);
}

/* ============================================================
 * 混排字符串: Column=1~16 (半角 8px 单位)
 *   中文 → 16x16 全角 (占 2 格)
 *   英文 → 8x16  半角 (占 1 格)
 * ============================================================ */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Ch) {
    if (Line < 1 || Line > OLED_LINES) return;
    if (Column < 1 || Column > OLED_COLS) return;
    OLED_ShowHalfWidthAtPx(Line, (Column - 1) * 8, Ch);
}

void OLED_ShowString(uint8_t Line, uint8_t Column, const uint8_t *str) {
    if (Line < 1 || Line > OLED_LINES) return;
    if (Column < 1 || Column > OLED_COLS) return;

    while (*str) {
        if (Column > OLED_COLS) break;

        if (*str < 0xA1) {
            /* 半角 ASCII */
            OLED_ShowHalfWidthAtPx(Line, (Column - 1) * 8, (char)*str);
            str++;
            Column++;
        } else {
            /* 全角中文 (GB2312 双字节) */
            if (Column + 1 > OLED_COLS) break;
            OLED_ShowChinesePx(Line, (Column - 1) * 8, ((uint16_t)str[0] << 8) | str[1]);
            str += 2;
            Column += 2;
        }
    }
}

/* ============================================================
 * 数字 (全角 GB2312 显示)
 * ============================================================ */
static uint32_t pow10(uint32_t e) { uint32_t r = 1; while (e--) r *= 10; return r; }

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t N, uint8_t Len) {
    for (uint8_t i = 0; i < Len; i++) {
        if (Column + i > OLED_COLS) break;
        OLED_ShowChar(Line, Column + i, N / pow10(Len - i - 1) % 10 + '0');
    }
}

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t N, uint8_t Len) {
    if (Column > OLED_COLS) return;
    if (N >= 0) { OLED_ShowChar(Line, Column, '+'); } else { OLED_ShowChar(Line, Column, '-'); N = -N; }
    for (uint8_t i = 0; i < Len; i++) {
        if (Column + i + 1 > OLED_COLS) break;
        OLED_ShowChar(Line, Column + i + 1, (uint32_t)N / pow10(Len - i - 1) % 10 + '0');
    }
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t N, uint8_t Len) {
    for (uint8_t i = 0; i < Len; i++) {
        if (Column + i > OLED_COLS) break;
        uint8_t n = N / pow10(Len - i - 1) % 16;
        OLED_ShowChar(Line, Column + i, n < 10 ? n + '0' : n + 'A' - 10);
    }
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t N, uint8_t Len) {
    for (uint8_t i = 0; i < Len; i++) {
        if (Column + i > OLED_COLS) break;
        OLED_ShowChar(Line, Column + i, N / pow10(Len - i - 1) % 2 + '0');
    }
}

/* ============================================================
 * 菜单渲染 (混排: 英文半角 8px, 中文全角 16px)
 * ============================================================ */
void OLED_FillRow(uint8_t Line, uint8_t on) {
    if (Line < 1 || Line > OLED_LINES) return;
    uint8_t fill = on ? 0xFF : 0x00, ps = (Line - 1) * 2;
    for (uint8_t p = 0; p < 2; p++) {
        OLED_SetCursor(ps + p, 0);
        i2c_start(); i2c_send_byte(OLED_ADDR); i2c_send_byte(0x40);
        for (uint8_t c = 0; c < OLED_WIDTH; c++) i2c_send_byte(fill);
        i2c_stop();
    }
}

void OLED_MenuRow(uint8_t Line, const uint8_t *text, uint8_t selected) {
    if (Line < 1 || Line > OLED_LINES) return;
    OLED_FillRow(Line, 0);

    uint16_t px = 0;  /* 像素 X 坐标 */

    /* 选中标记: 半角 '>' 或空格 (8px) */
    OLED_ShowHalfWidthAtPx(Line, px, selected ? '>' : ' ');
    px += 8;

    /* 混排渲染: ASCII=8px 半角, 中文=16px 全角 */
    while (*text && px + 8 <= OLED_WIDTH) {
        if (*text < 0xA1) {
            /* ASCII — 半角 8px */
            OLED_ShowHalfWidthAtPx(Line, px, (char)*text);
            text++;
            px += 8;
        } else {
            /* 中文 — 全角 16px */
            if (px + 16 <= OLED_WIDTH) {
                OLED_ShowChinesePx(Line, px, ((uint16_t)text[0] << 8) | text[1]);
            }
            text += 2;
            px += 16;
        }
    }
}
