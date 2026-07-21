#ifndef __BSP_IR_IO_H__
#define __BSP_IR_IO_H__

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include "OLED.h"

// ��������������
#define IR_SENSOR_NUM   8   // 8·���⴫����

// ====================== ����ӿں������� ================

/**
  * @brief �����������ֽ����ݵ�����
  * @param sensors: �洢������������飨���� 8��
  * @note ����������Ԫ�غ��壺0 = ��⵽���ߣ�1 = δ��⵽����
  *       ����ǰ��ȷ�� sensors �����С����Ϊ 8
  */
void deal_IRdata(uint8_t sensors[8]);
uint8_t Read_IR_SensorByte(void);

void OLED_ShowIRSensors(void);
#endif