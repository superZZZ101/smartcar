// line_following.c
#include "line_following.h"
#include "bsp_IR_io.h"  // 包含红外传感器驱动

#include <stdio.h>  // 用于调试输出
#define HAL_Delay(X)		delay_cycles( (CPUCLK_FREQ/1000) * (X) )
// 外部依赖声明
//extern I2C_HandleTypeDef hi2c2;    // I2C2外设句柄（用于传感器通信）
//extern TIM_HandleTypeDef htim2;    // TIM2定时器句柄（用于PWM输出）

// 传感器物理位置数组（以中心点为原点，单位：毫米）
const float SENSOR_POSITIONS[SENSOR_COUNT] = {
    -38.5f, -27.5f, -16.5f, -5.5f, 
    5.5f, 16.5f, 27.5f, 38.5f
};
#define MAX_SPEED 1000  // 最大电机PWM值

// ====================== 全局状态变量 ======================
static PID_Controller line_pid;         // 循迹PID控制器实例
static uint8_t lost_count = 0;          // 连续丢线计数器
static float last_valid_position = 0.0f; // 最后有效位置记录
static uint32_t last_control_time = 0;  // 上次控制时间戳

uint32_t HAL_GetTick(void)
{
    return DL_SYSTICK_getValue();
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
//float PID_Calculate(PID_Controller *pid, float setpoint, float current_value) {
//    // 计算当前误差
//    float error = setpoint - current_value ;
//    
//    // 积分项处理（带限幅防饱和）
//    pid->integral += error;
//    if (pid->integral > pid->max_integral) pid->integral = pid->max_integral;
//    if (pid->integral < -pid->max_integral) pid->integral = -pid->max_integral;
//    
//    // 微分项处理（带一阶低通滤波）
//    float derivative = (error - pid->prev_error) * 0.6f + pid->derivative * 0.4f;
//    pid->derivative = derivative;  // 存储滤波后的微分值
//    pid->prev_error = error;       // 存储当前误差供下次使用
//    
//    // PID输出计算: 比例项 + 积分项 + 微分项
//    float output = (pid->Kp * error) + 
//                  (pid->Ki * pid->integral) + 
//                  (pid->Kd * derivative);
//    
//    // 输出限幅
//    if (output > pid->max_output) output = pid->max_output;
//    else if (output < -pid->max_output) output = -pid->max_output;
//    
//    return output;
//}
// 修改后的PID计算函数（增加调试输出）
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
    
    // 遍历所有传感器
    for (int i = 0; i < SENSOR_COUNT; i++) {
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
  * @brief 根据PID输出和轮距计算电机速度
  * @param pid_output: PID控制器输出值
  * @param base_speed: 基准速度值
  */
void Wheelbase_Control(float pid_output, int16_t base_speed) {
    // 计算路径曲率（归一化处理）
    float curvature = pid_output / 1000.0f;
    
    // 计算速度衰减因子（转弯时适当减速）
    float speed_factor = 1.0f - fminf(0.5f, fabsf(curvature) * 1.2f);
    
    // 计算左右轮速差（基于轮距和曲率）
    float speed_diff = curvature * base_speed * (WHEELBASE_MM / 100.0f);
    
    // 计算最终左右轮速
    int16_t left_speed = (int16_t)(base_speed * speed_factor - speed_diff);
    int16_t right_speed = (int16_t)(base_speed * speed_factor + speed_diff);
    
    // 设置电机速度
    PWM_Run(left_speed, right_speed);
}

// ====================== 主控制逻辑 ======================
/**
  * @brief 循迹系统初始化
  */
void Line_Following_Init(void) {
    // 初始化PID控制器（参数根据实际调试确定）
//    PID_Init(&line_pid, 3.8f, 0.04f, 1.6f, 100.0f, 800.0f);
    PID_Init(&line_pid, 1.0f, 0.0f, 0.0f, 100.0f, 800.0f);
    // 初始化状态变量
    lost_count = 0;
    last_valid_position = 0.0f;
    last_control_time = HAL_GetTick();  // 获取当前系统时间
   
}

/**
  * @brief 循迹主控制循环（需周期性调用，20ms一次）
  */
void Line_Following_Update(void) {
    uint32_t current_time = HAL_GetTick();  // 获取当前时间
    
    // 确保20ms控制周期（约50Hz）
    if (current_time - last_control_time < 20) {
        return;  // 未到执行时间，直接返回
    }
    last_control_time = current_time;  // 更新最后控制时间

    // 1. 读取传感器数据到数组
    uint8_t sensor_array[8];
    deal_IRdata(sensor_array);  // 将传感器数据解析到数组
    
    // 2. 计算黑线位置（单位：毫米）
    float position_mm = Calculate_Line_Position(sensor_array);

    // 3. 丢线处理逻辑
    if (position_mm < -900.0f) {  // 丢线状态
        lost_count++;  // 增加连续丢线计数
        
        if (lost_count < 5) {
            // 短暂丢线：使用最后有效位置
            position_mm = last_valid_position;
            
            // 计算恢复速度（随丢线时间增加而减速）
            int16_t recovery_speed = BASE_SPEED * (1.0f - lost_count * 0.15f);
            
            // 根据最后位置方向设置转向偏置
            int16_t turn_bias = (last_valid_position > 0) ? -400 : 400;
            
            // 设置差速转向
            PWM_Run(recovery_speed + turn_bias, recovery_speed - turn_bias);
        } else {
            // 持续丢线：完全停止
            PWM_Run(0, 0);
        }
        return;  // 丢线状态下跳过正常控制
    }

    // 4. 正常循线处理
    lost_count = 0;  // 重置连续丢线计数
    last_valid_position = position_mm;  // 更新最后有效位置
    
    // 5. PID计算（声明调试结构体并传递指针）
    PID_DebugData pid_debug = {0};
    float pid_output = PID_Calculate(&line_pid, 0.0f, position_mm, &pid_debug);
		// 在PID计算后添加输出限幅
	if (pid_output > MAX_SPEED) pid_output = MAX_SPEED;
	else if (pid_output < -MAX_SPEED) pid_output = -MAX_SPEED;
    
    // 获取传感器字节用于调试
    uint8_t sensor_byte = 0;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensor_byte |= (sensor_array[i] << i);
    }
    pid_debug.sensors = sensor_byte;
    
    // 6. 轮距自适应控制
    Wheelbase_Control(pid_output, BASE_SPEED);

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
