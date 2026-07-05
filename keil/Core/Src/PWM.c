//PWM.c
#include "PWM.h"
#include "oled.h"
//4096*0.6=2457.6
#define MAX_SPEED 1001  // 最大电机PWM值
// 电机方向控制引脚定义（根据实际硬件连接修改）
#define MOTOR1_IN1_PIN       	WHEEL_CTRL_IN1_1_PIN
#define MOTOR1_IN2_PIN       	WHEEL_CTRL_IN1_2_PIN
#define MOTOR2_IN1_PIN        WHEEL_CTRL_IN2_1_PIN
#define MOTOR2_IN2_PIN        WHEEL_CTRL_IN2_2_PIN

#define TIM_CHANNEL_1		0
#define TIM_CHANNEL_2		1
#define GPIO_PIN_RESET	0
#define GPIO_PIN_SET		1
#define LF_PIN		DL_GPIO_PIN_21
#define RF_PIN		DL_GPIO_PIN_21
#define LR_PIN		DL_GPIO_PIN_21
#define RR_PIN		DL_GPIO_PIN_21
void  PWM_SetCompare(uint8_t channel,uint16_t dutyCycle){//0-1000
	switch(channel){
		case 0:DL_TimerG_setCaptureCompareValue(WHEEL_INST,dutyCycle,GPIO_WHEEL_C0_IDX);break;
		case 1:DL_TimerG_setCaptureCompareValue(WHEEL_INST,dutyCycle,GPIO_WHEEL_C1_IDX);break;
	}
}
void GPIO_WritePin(	uint32_t gpio_pin,uint8_t i){
	
	if(i)DL_GPIO_setPins(WHEEL_CTRL_PORT,gpio_pin);  //输出高电平
	else DL_GPIO_clearPins(GPIOB,gpio_pin);//输出低电平
}

	/**
	  * @brief PWM初始化
	  * @note 启动TIM2的PWM通道1和2
	  */
//	void PWM_init() {
//		PWM_sleep();  // 初始状态设为休眠
//		DL_TimerG_startCounter(WHEEL_INST);
//		
//	}
void PWM_init() {
    PWM_sleep();
   
    DL_TimerG_startCounter(WHEEL_INST);
}

	/**
	  * @brief 电机休眠
	  * @note 设置PWM为0引脚为1模式
	  */
	void PWM_sleep() {
		// PWM输出置高
		PWM_SetCompare(TIM_CHANNEL_1, 1000);
		PWM_SetCompare(TIM_CHANNEL_2, 1000);
		
		// （所有控制引脚低电平）
		GPIO_WritePin(MOTOR1_IN1_PIN, GPIO_PIN_RESET);
		GPIO_WritePin(MOTOR1_IN2_PIN, GPIO_PIN_RESET);
		
		GPIO_WritePin( MOTOR2_IN1_PIN, GPIO_PIN_RESET);
		GPIO_WritePin(MOTOR2_IN2_PIN, GPIO_PIN_RESET);
		
//		OLED_ShowString(4,11, "Sleep");
	}

	// ====================== 电机控制函数 ======================
	/**
	  * @brief 设置左右电机PWM输出
	  * @param left_speed: 左轮速度（-MAX_SPEED到+MAX_SPEED）
	  * @param right_speed: 右轮速度（-MAX_SPEED到+MAX_SPEED）
	  */
	void PWM_Run(int16_t left_speed, int16_t right_speed) {
		// 1. 限幅保护（确保在安全范围内）
//		left_speed = constrain(left_speed, -MAX_SPEED, MAX_SPEED);
//		right_speed = constrain(right_speed, -MAX_SPEED, MAX_SPEED);
		
		// 2. 设置左轮方向和PWM
		if (left_speed>= 0) {
			// 正向旋转
			GPIO_WritePin(MOTOR1_IN1_PIN, GPIO_PIN_SET);
			GPIO_WritePin(MOTOR1_IN2_PIN, GPIO_PIN_RESET);
			PWM_SetCompare(TIM_CHANNEL_1, MAX_SPEED-abs(left_speed));
			
		} else {
			// 反向旋转
			 GPIO_WritePin(MOTOR1_IN1_PIN, GPIO_PIN_RESET);
			GPIO_WritePin(MOTOR1_IN2_PIN, GPIO_PIN_SET);
			PWM_SetCompare(TIM_CHANNEL_1, MAX_SPEED-abs(left_speed));
		}
		
		// 3. 设置右轮方向和PWM
		if ( right_speed >= 0) {
			// 正向旋转
			GPIO_WritePin( MOTOR2_IN1_PIN, GPIO_PIN_RESET);
			GPIO_WritePin(MOTOR2_IN2_PIN, GPIO_PIN_SET);
			PWM_SetCompare(TIM_CHANNEL_2, MAX_SPEED-abs(right_speed));
		} else {
			// 反向旋转
			GPIO_WritePin( MOTOR2_IN1_PIN, GPIO_PIN_SET);
			GPIO_WritePin(MOTOR2_IN2_PIN, GPIO_PIN_RESET);
			PWM_SetCompare(TIM_CHANNEL_2, MAX_SPEED-abs(right_speed));
		}
		  OLED_ShowString(4,11, "Run!");

	}


	/**
	  * @brief 紧急停止
	  * @note 设置PWM为0刹车
	  */
	void PWM_stop() {
		// PWM输出0（根据驱动器特性设置）
		PWM_Run(0,0);
		GPIO_WritePin(MOTOR1_IN1_PIN, GPIO_PIN_RESET);
			GPIO_WritePin(MOTOR1_IN2_PIN, GPIO_PIN_RESET);
		GPIO_WritePin( MOTOR2_IN1_PIN, GPIO_PIN_RESET);
			GPIO_WritePin(MOTOR2_IN2_PIN, GPIO_PIN_RESET);
		
		OLED_ShowString(4,11, "Stop!");
	}

