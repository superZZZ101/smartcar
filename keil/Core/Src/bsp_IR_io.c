// bsp_IR_gpio.c
// 使用 GPIO 直接读取 8 路红外传感器状态（每路一个独立引脚）
#include "ti_msp_dl_config.h"
#include <stdint.h>

// ====================== 引脚自定义区域 ======================
// 请根据实际硬件连接修改以下端口和引脚号
// 假设传感器输出电平：低电平 = 检测到黑线，高电平 = 无黑线
// 若您的传感器极性相反，请调整 Read_IR_SensorByte 中的逻辑

// 传感器 1 引脚
#define IR_SENSOR1_PORT   IR_SENSOR_IR1_PORT
#define IR_SENSOR1_PIN    IR_SENSOR_IR1_PIN

// 传感器 2 引脚
#define IR_SENSOR2_PORT   IR_SENSOR_IR2_PORT
#define IR_SENSOR2_PIN    IR_SENSOR_IR2_PIN

// 传感器 3 引脚
#define IR_SENSOR3_PORT   IR_SENSOR_IR3_PORT
#define IR_SENSOR3_PIN    IR_SENSOR_IR3_PIN

// 传感器 4 引脚
#define IR_SENSOR4_PORT   IR_SENSOR_IR4_PORT
#define IR_SENSOR4_PIN    IR_SENSOR_IR4_PIN

// 传感器 5 引脚
#define IR_SENSOR5_PORT   IR_SENSOR_IR5_PORT
#define IR_SENSOR5_PIN    IR_SENSOR_IR5_PIN

// 传感器 6 引脚
#define IR_SENSOR6_PORT   IR_SENSOR_IR6_PORT
#define IR_SENSOR6_PIN    IR_SENSOR_IR6_PIN

// 传感器 7 引脚
#define IR_SENSOR7_PORT   IR_SENSOR_IR7_PORT
#define IR_SENSOR7_PIN    IR_SENSOR_IR7_PIN

// 传感器 8 引脚
#define IR_SENSOR8_PORT   IR_SENSOR_IR8_PORT
#define IR_SENSOR8_PIN    IR_SENSOR_IR8_PIN


// ====================== 传感器控制函数 ======================
/**
  * @brief 设置传感器校准模式（GPIO 版本无此功能，保留为空函数以兼容原有调用）
  * @param mode: 无效参数，仅用于接口兼容
  */
void set_adjust_mode(uint8_t mode) {
    // GPIO 直连传感器通常无需校准，此函数保留以兼容原有代码
    (void)mode;
}

// ====================== 读取原始数据 ======================
/**
  * @brief 读取所有 GPIO 引脚状态并组合成一个字节
  * @retval 8位状态字节，每位对应一个传感器（bit0 = 传感器1）
  *         位值含义：0 = 检测到黑线，1 = 未检测到黑线
  * @note   引脚读取使用 DL_GPIO_readPins，需根据实际 DriverLib 调整函数名
  */
static uint8_t read_IRdata(void) {
    uint8_t data = 0;

    // 读取传感器1（bit0）
    if (DL_GPIO_readPins(IR_SENSOR1_PORT, IR_SENSOR1_PIN) == 0) {
        // 低电平 -> 检测到黑线 -> 对应位设为 0
        data &= ~(1 << 0);
    } else {
        // 高电平 -> 未检测到黑线 -> 对应位设为 1
        data |= (1 << 0);
    }

    // 读取传感器2（bit1）
    if (DL_GPIO_readPins(IR_SENSOR2_PORT, IR_SENSOR2_PIN) == 0) {
        data &= ~(1 << 1);
    } else {
        data |= (1 << 1);
    }

    // 传感器3（bit2）
    if (DL_GPIO_readPins(IR_SENSOR3_PORT, IR_SENSOR3_PIN) == 0) {
        data &= ~(1 << 2);
    } else {
        data |= (1 << 2);
    }

    // 传感器4（bit3）
    if (DL_GPIO_readPins(IR_SENSOR4_PORT, IR_SENSOR4_PIN) == 0) {
        data &= ~(1 << 3);
    } else {
        data |= (1 << 3);
    }

    // 传感器5（bit4）
    if (DL_GPIO_readPins(IR_SENSOR5_PORT, IR_SENSOR5_PIN) == 0) {
        data &= ~(1 << 4);
    } else {
        data |= (1 << 4);
    }

    // 传感器6（bit5）
    if (DL_GPIO_readPins(IR_SENSOR6_PORT, IR_SENSOR6_PIN) == 0) {
        data &= ~(1 << 5);
    } else {
        data |= (1 << 5);
    }

    // 传感器7（bit6）
    if (DL_GPIO_readPins(IR_SENSOR7_PORT, IR_SENSOR7_PIN) == 0) {
        data &= ~(1 << 6);
    } else {
        data |= (1 << 6);
    }

    // 传感器8（bit7）
    if (DL_GPIO_readPins(IR_SENSOR8_PORT, IR_SENSOR8_PIN) == 0) {
        data &= ~(1 << 7);
    } else {
        data |= (1 << 7);
    }

    return data;
}

// ====================== 数据处理函数 ======================
/**
  * @brief 读取并返回传感器字节数据
  * @retval 传感器状态字节，每 bit 对应一个传感器（0=黑线，1=无黑线）
  * @note   通信失败（GPIO 读取总是成功）故无需重试
  */
uint8_t Read_IR_SensorByte(void) {
    return read_IRdata();
}

/**
  * @brief 解析传感器字节数据到数组
  * @param sensors: 存储解析结果的数组（长度8）
  * @note 最终数组中元素含义：0 = 检测到黑线，1 = 未检测到黑线
  *       与原 I2C 版本行为完全一致
  */
void deal_IRdata(uint8_t sensors[8]) {
    uint8_t IRbuf = Read_IR_SensorByte();

    // 解析每个传感器值（与原有逻辑相同，包括最后的异或翻转）
    for (int i = 0; i < 8; i++) {
        /*
         * 位解析说明：
         *   (IRbuf >> i) & 0x01 : 提取第 i 位原始值（0=黑线，1=无黑线）
         *   ^ 0x01              : 翻转值，使得最终 sensors[i]=0 表示黑线
         * 结果：
         *   0 = 检测到黑线
         *   1 = 未检测到黑线
         */
        sensors[i] = ((IRbuf >> i) & 0x01) ^ 0x01;
    }
}

// ====================== OLED显示函数 ======================
/**
  * @brief 在OLED上显示8路红外传感器状态
  * @param sensors: 传感器状态数组（长度8）
  * @note 显示格式：
  *        第2行: x1 x2 x3 x4
  *        第3行: x5 x6 x7 x8
  */
void OLED_ShowIRSensors(void) {
    uint8_t sensors[8];
    
    // 读取传感器原始数据并解析为数组 (0=黑线,1=无黑线)
    deal_IRdata(sensors);
	// 第2行: 显示传感器1-4
    OLED_ShowString(2, 1, "x1-x4:");
    OLED_ShowNum(2, 7, sensors[0], 1);
    OLED_ShowString(2, 8, ",");
    OLED_ShowNum(2, 9, sensors[1], 1);
    OLED_ShowString(2, 10, ",");
    OLED_ShowNum(2, 11, sensors[2], 1);
    OLED_ShowString(2, 12, ",");
    OLED_ShowNum(2, 13, sensors[3], 1);

    // 第3行: 显示传感器5-8
    OLED_ShowString(3, 1, "x5-x8:");
    OLED_ShowNum(3, 7, sensors[4], 1);
    OLED_ShowString(3, 8, ",");
    OLED_ShowNum(3, 9, sensors[5], 1);
    OLED_ShowString(3, 10, ",");
    OLED_ShowNum(3, 11, sensors[6], 1);
    OLED_ShowString(3, 12, ",");
    OLED_ShowNum(3, 13, sensors[7], 1);
}
