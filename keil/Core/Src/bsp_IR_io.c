/*
 * bsp_IR_io.c — 多路复用型灰度传感器驱动
 *
 * 传感器: 3 位地址线 (MCU 输出) + 1 位数据线 (MCU 输入)
 *   设置地址 → 读取数据 → 得到 1 个传感器位, 循环 8 次得到完整字节
 *
 * 引脚映射 (用 SysConfig 已定义的 GPIO):
 *   GRAY_ADDR0 = IR_SENSOR_IR1   ← SysConfig 中需设为输出
 *   GRAY_ADDR1 = IR_SENSOR_IR2   ← SysConfig 中需设为输出
 *   GRAY_ADDR2 = IR_SENSOR_IR3   ← SysConfig 中需设为输出
 *   GRAY_DATA  = IR_SENSOR_IR4   ← SysConfig 中保持输入
 */

#include "bsp_IR_io.h"
#include "ti_msp_dl_config.h"
#include "delay.h"

/* ============================================================
 * 引脚映射 (对应 SysConfig 中的 IR_SENSOR)
 * ============================================================ */
/* 3 位地址线 — 在 SysConfig 中将 AD0/AD1/AD2 设为 输出 */
#define GRAY_ADDR0_PORT   IR_SENSOR_AD0_PORT
#define GRAY_ADDR0_PIN    IR_SENSOR_AD0_PIN

#define GRAY_ADDR1_PORT   IR_SENSOR_AD1_PORT
#define GRAY_ADDR1_PIN    IR_SENSOR_AD1_PIN

#define GRAY_ADDR2_PORT   IR_SENSOR_AD2_PORT
#define GRAY_ADDR2_PIN    IR_SENSOR_AD2_PIN

/* 1 位数据输入 — 在 SysConfig 中将 OUT 保持 输入 */
#define GRAY_DATA_PORT    IR_SENSOR_OUT_PORT
#define GRAY_DATA_PIN     IR_SENSOR_OUT_PIN

/* 传感器极性: 0 = 检测到黑线, 1 = 无黑线 (若相反就交换) */
#define GRAY_ACTIVE_LEVEL  0   /* 0: 低电平=黑线, 1: 高电平=黑线 */

/* 屏蔽故障传感器: 第 1 路 (addr=0) 硬件损坏, 常亮, 忽略它 */
#define BROKEN_SENSOR_MASK  ~(1 << 0)

/* ============================================================
 * 设置 3 位地址
 * ============================================================ */
static void gray_set_addr(uint8_t addr) {
    /* ADDR0 = bit0, ADDR1 = bit1, ADDR2 = bit2 */
    if (addr & 0x01)
        DL_GPIO_setPins(GRAY_ADDR0_PORT, GRAY_ADDR0_PIN);
    else
        DL_GPIO_clearPins(GRAY_ADDR0_PORT, GRAY_ADDR0_PIN);

    if (addr & 0x02)
        DL_GPIO_setPins(GRAY_ADDR1_PORT, GRAY_ADDR1_PIN);
    else
        DL_GPIO_clearPins(GRAY_ADDR1_PORT, GRAY_ADDR1_PIN);

    if (addr & 0x04)
        DL_GPIO_setPins(GRAY_ADDR2_PORT, GRAY_ADDR2_PIN);
    else
        DL_GPIO_clearPins(GRAY_ADDR2_PORT, GRAY_ADDR2_PIN);

    /* 等待地址稳定 ~50us @ 32MHz */
    for (volatile uint32_t _d = 0; _d < 400; _d++);
}

/* ============================================================
 * 读取数据引脚
 * ============================================================ */
static uint8_t gray_read_data(void) {
    uint32_t val = DL_GPIO_readPins(GRAY_DATA_PORT, GRAY_DATA_PIN);
#if GRAY_ACTIVE_LEVEL == 0
    return (val == 0) ? 0 : 1;   /* 低=黑线(0), 高=无线(1) */
#else
    return (val != 0) ? 0 : 1;   /* 高=黑线(0), 低=无线(1) */
#endif
}

/* ============================================================
 * 读取 8 位传感器数据 (地址 0~7)
 * ============================================================ */
static uint8_t read_IRdata(void) {
    uint8_t data = 0;
    for (uint8_t addr = 0; addr < 8; addr++) {
        gray_set_addr(addr);
        if (gray_read_data() == 0)
            data |= (1 << addr);   /* 检测到黑线时该位置 1 */
    }
    return data;
}

/* ============================================================
 * 对外接口: 读取传感器字节 (兼容原有调用方)
 * ============================================================ */
uint8_t Read_IR_SensorByte(void) {
    return read_IRdata() & BROKEN_SENSOR_MASK;
}

/* ============================================================
 * 空校准函数 (复用接口兼容)
 * ============================================================ */
void set_adjust_mode(uint8_t mode) {
    (void)mode;
}

/* ============================================================
 * 解析字节到数组: sensors[i] = 0 表示黑线, 1 表示无线
 * ============================================================ */
void deal_IRdata(uint8_t sensors[8]) {
    uint8_t IRbuf = Read_IR_SensorByte();
    for (int i = 0; i < 8; i++) {
        sensors[i] = ((IRbuf >> i) & 0x01) ^ 0x01;
    }
}

/* ============================================================
 * OLED 显示 8 路传感器状态 (混排: 半角)
 * ============================================================ */
void OLED_ShowIRSensors(void) {
    uint8_t sensors[8];
    char buf[2];
    uint8_t i;

    deal_IRdata(sensors);

    OLED_ShowString(2, 1, (const uint8_t*)"x1-x4:");
    for (i = 0; i < 4; i++) {
        if (i) OLED_ShowString(2, 7 + 2 * i, (const uint8_t*)",");
        buf[0] = (char)(sensors[i] + '0'); buf[1] = '\0';
        OLED_ShowString(2, 8 + 2 * i, (const uint8_t*)buf);
    }

    OLED_ShowString(3, 1, (const uint8_t*)"x5-x8:");
    for (i = 0; i < 4; i++) {
        if (i) OLED_ShowString(3, 7 + 2 * i, (const uint8_t*)",");
        buf[0] = (char)(sensors[4 + i] + '0'); buf[1] = '\0';
        OLED_ShowString(3, 8 + 2 * i, (const uint8_t*)buf);
    }
}
