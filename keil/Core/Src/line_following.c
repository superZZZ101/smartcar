// line_following.c
#include "line_following.h"
#include "bsp_IR_io.h"
#include "param.h"     // 使用菜单修改后的 kp, ki, kd
#include "Encode.h"    // 编码器速度反馈

#include <stdio.h>
#include <math.h>      // fminf / fabsf
#define HAL_Delay(X)		delay_cycles( (CPUCLK_FREQ/1000) * (X) )
// 外部依赖声明
//extern I2C_HandleTypeDef hi2c2;    // I2C2外设句柄（用于传感器通信）
//extern TIM_HandleTypeDef htim2;    // TIM2定时器句柄（用于PWM输出）

// 传感器物理位置数组（以中心点为原点，单位：毫米）
const float SENSOR_POSITIONS[SENSOR_COUNT] = {
    -38.5f, -27.5f, -16.5f, -5.5f, 
    5.5f, 16.5f, 27.5f, 38.5f
};


// ====================== 全局状态变量 ======================
static PID_Controller line_pid;         // 循迹PID控制器实例
static uint8_t lost_count = 0;          // 连续丢线计数器
static float last_valid_position = 0.0f; // 最后有效位置记录
static uint32_t last_control_time = 0;  // 上次控制时间戳

// 调试用：当前左右轮 PWM 值
int16_t debug_pwm_left = 0;
int16_t debug_pwm_right = 0;

// 编码器缓存值（供显示用，每控制周期更新一次）
int16_t last_enc_l = 0;
int16_t last_enc_r = 0;

// 丢线逻辑开关 — 默认开启
volatile uint8_t lost_line_flag = 1;

// 速度环 PI 状态
static float spd_integral_l = 0.0f;
static float spd_integral_r = 0.0f;

void SpeedPI_Reset(void) {
    spd_integral_l = 0.0f;
    spd_integral_r = 0.0f;
}

/**
  * @brief 独立速度 PI 调参 — 左右轮各自设目标值
  * @param tgtL, tgtR: 左右目标编码器速度
  */
void SpeedTune_Step(int16_t tgtL, int16_t tgtR) {
    int16_t enc_l = Encode_GetspeedL();
    int16_t enc_r = Encode_GetspeedR();

    last_enc_l = enc_l;
    last_enc_r = enc_r;

    float err_l = (float)tgtL - (float)enc_l;
    float err_r = (float)tgtR - (float)enc_r;

    spd_integral_l += err_l;
    spd_integral_r += err_r;
    if (spd_integral_l >  100000.0f) spd_integral_l =  100000.0f;
    if (spd_integral_l < -100000.0f) spd_integral_l = -100000.0f;
    if (spd_integral_r >  100000.0f) spd_integral_r =  100000.0f;
    if (spd_integral_r < -100000.0f) spd_integral_r = -100000.0f;

    float corr_l = kp_spd * err_l + ki_spd * spd_integral_l;
    float corr_r = kp_spd * err_r + ki_spd * spd_integral_r;

    int16_t outL = (int16_t)((float)tgtL + corr_l);
    int16_t outR = (int16_t)((float)tgtR + corr_r);

    if (outL < 0) outL = 0;
    if (outR < 0) outR = 0;
    debug_pwm_left  = outL;
    debug_pwm_right = outR;

    PWM_Run(outL, outR);
}

// ====================== SysTick 毫秒计数器 ======================
// SysTick 已初始化为 1ms 周期 (32MHz / 32000), 但中断默认未启用
// 这里自行接管 SysTick_Handler 并提供 HAL_GetTick

static volatile uint32_t g_sysTickCount = 0;

void SysTick_Handler(void) {
    g_sysTickCount++;
}

uint32_t HAL_GetTick(void) {
    return g_sysTickCount;
}
// ====================== PID控制器实现 ======================
/**
  * @brief 初始化PID控制器参数
  * @param pid: PID控制器实例指针
  * @param Kp, Ki, Kd: PID参数
  * @param max_i: 积分项限幅值
  * @param max_out: 输出限幅值
  */
void PID_Init(PID_Controller *pid, float Kp, float Ki, float Kd, 
             float max_i, float max_out) {
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;
    pid->integral = 0.0f;        // 积分项清零
    pid->prev_error = 0.0f;      // 前次误差清零
    pid->derivative = 0.0f;      // 微分项清零
    pid->max_integral = max_i;   // 设置积分限幅
    pid->max_output = max_out;   // 设置输出限幅
}

/**
  * @brief 执行PID计算
  * @param pid: PID控制器实例指针
  * @param setpoint: 目标设定值
  * @param current_value: 当前测量值
  * @retval PID输出值
  */

float PID_Calculate(PID_Controller *pid, float setpoint, 
	float current_value, PID_DebugData *debug) {
    float error = setpoint - current_value;
    
    // P项
    float p_term = pid->Kp * error;
    
    // I项
    pid->integral += error;
    if (pid->integral > pid->max_integral) pid->integral = pid->max_integral;
    if (pid->integral < -pid->max_integral) pid->integral = -pid->max_integral;
    float i_term = pid->Ki * pid->integral;
    
    // D项
    float derivative = (error - pid->prev_error) * 0.6f + pid->derivative * 0.4f;
    pid->derivative = derivative;
    pid->prev_error = error;
    float d_term = pid->Kd * derivative;
    
    // 总输出
    float output = p_term + i_term + d_term;
    if (output > pid->max_output) output = pid->max_output;
    else if (output < -pid->max_output) output = -pid->max_output;
    
    // 填充调试数据
    if (debug) {
        debug->position = current_value;
        debug->error = error;
        debug->p_term = p_term;
        debug->i_term = i_term;
        debug->d_term = d_term;
        debug->output = output;
    }
    
    return output;
}




// ====================== 传感器数据处理 ======================
/**
  * @brief 根据传感器数据计算黑线位置
  * @param sensor_values: 传感器状态数组（每个元素代表一个传感器）
  * @retval 黑线中心位置（单位：毫米），-1000表示丢线
  */ 
float Calculate_Line_Position (uint8_t sensor_values []) {
    uint8_t active_count = 0;    // 检测到黑线的传感器数量
    float position_sum = 0.0f;   // 位置加权和
    uint8_t leftmost = 0xFF;     // 最左激活传感器索引
    uint8_t rightmost = 0;       // 最右激活传感器索引
    
    // 只用中间 6 路 (index 1~6), 头尾两路废弃
    for (int i = 1; i < SENSOR_COUNT - 1; i++) {
        if (sensor_values[i]) {  // 传感器i检测到黑线
            position_sum += SENSOR_POSITIONS[i];  // 累加位置
            active_count++;
            
            // 更新左右边界
            if (i < leftmost) leftmost = i;
            if (i > rightmost) rightmost = i;
        }
    }
    
    // 丢线检测（没有传感器检测到黑线）
    if (active_count == 0) return -1000.0f;
    
    // 计算加权位置（基础位置+边界加权）
    float base_position = position_sum / active_count;  // 平均位置
    float boundary_weight = 0.3f;  // 边界权重系数
    
    return base_position * (1 - boundary_weight) + 
          (SENSOR_POSITIONS[leftmost] + SENSOR_POSITIONS[rightmost]) * 
          boundary_weight / 2;
}

// ====================== 轮距自适应控制 ======================
/**
  * @brief 根据PID输出、轮距和编码器反馈计算电机速度
  * @param pid_output: PID控制器输出值
  * @param base_speed: 基准速度值
  * @param enc_l, enc_r: 实际编码器速度（用于速度闭环）
  */
void Wheelbase_Control(float pid_output, int16_t base_speed,
                        int16_t enc_l, int16_t enc_r) {
    // 计算路径曲率（归一化处理）
    float curvature = pid_output / 1000.0f;
    
    // 计算速度衰减因子（转弯时适当减速）
    float speed_factor = 1.0f - fminf(0.5f, fabsf(curvature) * 1.2f);
    
    // 计算左右轮速差（基于轮距和曲率）
    float speed_diff = curvature * base_speed * (WHEELBASE_MM / 100.0f);
    
    // 计算目标左右轮速（抽象值 0~1000）
    float target_left  = base_speed * speed_factor - speed_diff;
    float target_right = base_speed * speed_factor + speed_diff;
    
    // ========== 编码器速度闭环 (PI) ==========
    float err_l = target_left  - (float)enc_l;
    float err_r = target_right - (float)enc_r;
    
    spd_integral_l += err_l;
    spd_integral_r += err_r;
    // 积分限幅 (编码器最大约 1000, ki_spd~0.001, 允许 I 贡献 ~100)
    if (spd_integral_l >  100000.0f) spd_integral_l =  100000.0f;
    if (spd_integral_l < -100000.0f) spd_integral_l = -100000.0f;
    if (spd_integral_r >  100000.0f) spd_integral_r =  100000.0f;
    if (spd_integral_r < -100000.0f) spd_integral_r = -100000.0f;
    
    float corr_l = kp_spd * err_l + ki_spd * spd_integral_l;
    float corr_r = kp_spd * err_r + ki_spd * spd_integral_r;
    
    // 目标 + 修正 = 最终输出
    int16_t left_speed  = (int16_t)(target_left  + corr_l);
    int16_t right_speed = (int16_t)(target_right + corr_r);
    // ========================================
    
    // 编码器速度限速 (超了主动减速, 不超不干预)
    // 编码器速度限速 (超了主动减速, 不超不干预)
    if (enc_l >  LINE_SPEED_LIMIT) left_speed  -= (enc_l - LINE_SPEED_LIMIT) * 3;
    if (enc_r >  LINE_SPEED_LIMIT) right_speed -= (enc_r - LINE_SPEED_LIMIT) * 3;
    
    // 仅正转，负值夹到 0
    if (left_speed  < 0) left_speed  = 0;
    if (right_speed < 0) right_speed = 0;
    
    // 保存实际指令供显示
    debug_pwm_left  = left_speed;
    debug_pwm_right = right_speed;
    
    // 设置电机速度
    PWM_Run(left_speed, right_speed);
}

// ====================== 主控制逻辑 ======================
/**
  * @brief 循迹系统初始化
  */
void Line_Following_Init(void) {
    // 初始化PID控制器 — 使用菜单里修改的 kp, ki, kd
    // 原硬编码参数保留如下：
    //   PID_Init(&line_pid, 3.8f, 0.04f, 1.6f, 100.0f, 800.0f);  // 第一版

    PID_Init(&line_pid, kp, ki, kd, 100.0f, 800.0f);
    // 初始化状态变量
    lost_count = 0;
    last_valid_position = 0.0f;
    last_control_time = HAL_GetTick();
    
    // 清零速度环积分，避免循迹起步时抽搐
    SpeedPI_Reset();
}

/**
  * @brief 循迹主控制循环（需周期性调用，20ms一次）
  */
void Line_Following_Update(void) {
    uint32_t current_time = HAL_GetTick();  // 获取当前时间
  
    // 确保20ms控制周期（约50Hz）
    if (current_time - last_control_time < CTRL_INTERVAL_MS) {
        return;  // 未到执行时间，直接返回
    }
    last_control_time = current_time;  // 更新最后控制时间

    // 1. 读取传感器数据到数组
    uint8_t sensor_array[8];
    deal_IRdata(sensor_array);  // 将传感器数据解析到数组
    
    // 2. 计算黑线位置（单位：毫米）
    float position_mm = Calculate_Line_Position(sensor_array);

    // 3. 丢线 — 朝最后位置方向转圈找 (限 2 秒)
    if (position_mm < -900.0f) {
        if (lost_line_flag) {
            static uint32_t lost_start = 0;
            if (lost_count == 0) lost_start = current_time;
            lost_count++;
            if (current_time - lost_start > 2000) {
                PWM_Run(0, 0);
                return;
            }
            int16_t turn_bias = (last_valid_position > 0) ? 100 : -100;
            if (turn_bias > 0) {
                // 线在右，减速右轮以向右转
                PWM_Run(BASE_SPEED*0.7, BASE_SPEED*0.1);
            } else {
                // 线在左，减速左轮以向左转
                PWM_Run(BASE_SPEED*0.1, BASE_SPEED*0.7);
            }
        } else {
            PWM_Run(0, 0);
        }
        return;
    } else {
        lost_count = 0;
    }

    // 4. 正常循线处理
    last_valid_position = position_mm;  // 更新最后有效位置
    
    // 5. PID计算（声明调试结构体并传递指针）
    PID_DebugData pid_debug = {0};
    float pid_output = PID_Calculate(&line_pid, 0.0f, position_mm, &pid_debug);
    // 输出限幅
    if (pid_output > MAX_SPEED) pid_output = MAX_SPEED;
    else if (pid_output < -MAX_SPEED) pid_output = -MAX_SPEED;
    
    // 获取传感器字节用于调试
    uint8_t sensor_byte = 0;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensor_byte |= (sensor_array[i] << i);
    }
    pid_debug.sensors = sensor_byte;
    
    // 5.5. 读取编码器实际速度
    last_enc_l = Encode_GetspeedL();
    last_enc_r = Encode_GetspeedR();
    
    // 6. 轮距自适应控制 + 速度闭环
    Wheelbase_Control(pid_output, BASE_SPEED, last_enc_l, last_enc_r);

    // 7. 调试输出（通过串口打印信息）
    #ifdef PID_DEBUG
    printf("Pos:%.1f|Err:%.1f|P:%.1f|I:%.1f|D:%.1f|Out:%.1f|Sens:0x%02X\n", 
           pid_debug.position,
           pid_debug.error,
           pid_debug.p_term,
           pid_debug.i_term,
           pid_debug.d_term,
           pid_debug.output,
           pid_debug.sensors);
    #endif
}
