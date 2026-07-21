// line_following.h
#ifndef __LINE_FOLLOWING_H
#define __LINE_FOLLOWING_H
#include "ti_msp_dl_config.h"
//#include "stm32f1xx_hal.h"  // 引入STM32 HAL库
#include "PWM.h"
// ====================== 系统配置 ======================
#define SENSOR_SPACING_MM 11.0f    // 相邻红外传感器间距（单位：毫米）
#define SENSOR_COUNT      8        // 红外传感器总数

#define WHEELBASE_MM      135.0f   // 轮距（左右轮间距，单位：毫米）
#define BASE_SPEED        250      // 电机基准转速（PWM值, ≤ MAX_SPEED=1000）

#define MAX_SPEED 1000  // 最大电机PWM值
#define LINE_SPEED_LIMIT  600  // 循迹速度上限
#define CTRL_INTERVAL_MS  10   // 控制周期（毫秒）
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

// 速度闭环控制（供 PI 调参单独调用）
void Wheelbase_Control(float pid_output, int16_t base_speed,
                        int16_t enc_l, int16_t enc_r);
void SpeedPI_Reset(void);
void SpeedTune_Step(int16_t tgtL, int16_t tgtR);

extern float current_line_position;
extern float current_pid_output;

// 调试: 当前左右轮 PWM 值 (由 Wheelbase_Control 更新)
extern int16_t debug_pwm_left;
extern int16_t debug_pwm_right;

// 编码器缓存值 (速度环更新, 供显示用)
extern int16_t last_enc_l;
extern int16_t last_enc_r;

// 丢线逻辑开关 — 1=执行丢线找回, 0=不执行(直接停)
extern volatile uint8_t lost_line_flag;

#endif
