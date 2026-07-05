//PWM.h
#ifndef __PWM_H
#define __PWM_H
#include <stdint.h> // 必须包含此文件
#include <ti_msp_dl_config.h>
//#include "tim.h"
#include<stdlib.h>
// 电机控制函数声明
void PWM_init(void);
void PWM_Run(int16_t left_speed, int16_t right_speed);
void PWM_sleep(void);
void PWM_stop(void);
//extern int16_t constrain(int16_t value, int16_t min_val, int16_t max_val);

#endif
