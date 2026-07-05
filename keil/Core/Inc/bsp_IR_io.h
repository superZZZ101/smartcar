#ifndef __BSP_IR_IO_H__
#define __BSP_IR_IO_H__

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include "OLED.h"

// 传感器数量定义
#define IR_SENSOR_NUM   8   // 8路红外传感器

// ====================== 对外接口函数声明 ================

/**
  * @brief 解析传感器字节数据到数组
  * @param sensors: 存储解析结果的数组（长度 8）
  * @note 最终数组中元素含义：0 = 检测到黑线，1 = 未检测到黑线
  *       调用前请确保 sensors 数组大小至少为 8
  */
void deal_IRdata(uint8_t sensors[8]);


void OLED_ShowIRSensors(void);
#endif