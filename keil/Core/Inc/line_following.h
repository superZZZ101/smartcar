// line_following.h
#ifndef __LINE_FOLLOWING_H
#define __LINE_FOLLOWING_H
#include "ti_msp_dl_config.h"
//#include "stm32f1xx_hal.h"  // 引入STM32 HAL库
#include "PWM.h"
// ====================== 系统配置 ======================
#define SENSOR_SPACING_MM 11.0f    // 相邻红外传感器间距（单位：毫米）
#define SENSOR_COUNT      8        // 红外传感器总数
#define CONTROL_FREQ      50       // 控制频率（单位：Hz）
#define BASE_SPEED        2200     // 电机基准转速（PWM值）
#define WHEELBASE_MM      171.0f   // 轮距（左右轮间距，单位：毫米）

// 传感器位置数组（从左到右排列，以中心点为原点对称分布）
// 示例：8个传感器时位置 = [-38.5, -27.5, -16.5, -5.5, 5.5, 16.5, 27.5, 38.5]
extern const float SENSOR_POSITIONS[SENSOR_COUNT];

// ====================== PID控制器结构体 ======================
typedef struct {
    float Kp, Ki, Kd;       // PID参数：比例、积分、微分系数
    float integral;          // 积分项累计值
    float prev_error;        // 上一次误差（用于计算微分）
    float derivative;        // 微分项缓存（最近一次计算的微分值）
    float max_integral;      // 积分项限幅（防饱和）
    float max_output;        // PID输出限幅
} PID_Controller;

// 调试结构体
typedef struct {
    float position;   // 当前黑线位置
    float error;      // 当前误差
    float p_term;     // P项输出
    float i_term;     // I项输出
    float d_term;     // D项输出
    float output;     // PID总输出
    uint8_t sensors;  // 传感器状态字节
} PID_DebugData;
// 函数声明
void PID_Init(PID_Controller *pid, float Kp, float Ki, float Kd, 
             float max_i, float max_out);  // PID控制器初始化
             
float PID_Calculate(PID_Controller *pid, float setpoint,float current_value, PID_DebugData *debug);

void Line_Following_Init(void);          // 循迹模块初始化
void Line_Following_Update(void);        // 循迹主控制循环（需周期性调用）

uint8_t Read_IR_SensorByte(void);        // 读取所有红外传感器状态（1

extern float current_line_position;
extern float current_pid_output;

#endif
