/**
 * @file main.c
 * @brief PID 调参专用 — 仅保留 PID 菜单 + 循迹运行
 */
#include "ti_msp_dl_config.h"
#include "OLED.h"
#include "delay.h"
#include "key.h"
#include "led.h"
#include "oled_menu.h"
#include "w25q64.h"
#include "bsp_IR_io.h"
#include "Encode.h"
#include "line_following.h"
#include "PWM.h"
#include <stdbool.h>
#include <stdio.h>     /* snprintf */
#include "param.h"

/* PID 限幅  */
#define PID_P_MAX   20.0f
#define PID_I_MAX   20.0f
#define PID_D_MAX   20.0f
#define SPD_P_MAX   20.0f
#define SPD_I_MAX   20.0f

/* 版本存储地址 (不同扇区) */
#define VER_A_ADDR  (PARAM_FLASH_ADDR + 0x1000)
#define VER_B_ADDR  (PARAM_FLASH_ADDR + 0x2000)

static char LText[12];
static char RText[12];
static char PWMFlagText[12];
static char PText[16];
static char IText[16];
static char DText[16];
static char SPD_PText[16];
static char SPD_IText[16];
static char PID_stepText[16];
static char SPD_stepText[16];
static char LostFlagText[12];

int8_t pwm_step = 100;

void pwm_setleftspeed(void);
void pwm_setrightspeed(void);
void pwm_setflag(void);
void pwm_back(void);
void pwm_updateText(void);

void PID_SetP(void);
void PID_SetI(void);
void PID_SetD(void);
void PID_SetSpdP(void);
void PID_SetSpdI(void);
void PID_switchstep(void);
void SPD_switchstep(void);
void PID_clear(void);
void lost_flag_toggle(void);
void ver_save_a(void);
void ver_save_b(void);
void ver_load_a(void);
void ver_load_b(void);
void SPD_clear(void);
void PID_save(void);
void PID_updateText(void);
void menu_back(void);

/* 循迹运行 */
void run_line_following(void);

/* IR 传感器显示 */
void ir_sensor_show(void);

/* 编码器显示 */
void encode_show(void);

/* PI 调参 — 固定目标速度跑 */
void run_pi_tune(void);

static const OLED_MenuItem mainItems[9];

void pwm_clear(void);

static const OLED_MenuItem subMenu1[] = {
    {(uint8_t*)LText, NULL, 0, pwm_setleftspeed},
    {(uint8_t*)RText, NULL, 0, pwm_setrightspeed},
    {(uint8_t*)PWMFlagText, NULL, 0, pwm_setflag},
    {(uint8_t*)"CLEAR", NULL, 0, pwm_clear},
    {(uint8_t*)"BACK", NULL, 0, pwm_back}
};

static const OLED_MenuItem subMenu2[] = {
    {(uint8_t*)PText, NULL, 0, PID_SetP},
    {(uint8_t*)IText, NULL, 0, PID_SetI},
    {(uint8_t*)DText, NULL, 0, PID_SetD},
    {(uint8_t*)PID_stepText, NULL, 0, PID_switchstep},
    {(uint8_t*)"CLEAR", NULL, 0, PID_clear},
    {(uint8_t*)"back",  NULL, 0, menu_back}
};

static const OLED_MenuItem subMenuVer[] = {
    {(uint8_t*)"save A", NULL, 0, ver_save_a},
    {(uint8_t*)"save B", NULL, 0, ver_save_b},
    {(uint8_t*)"load A", NULL, 0, ver_load_a},
    {(uint8_t*)"load B", NULL, 0, ver_load_b},
    {(uint8_t*)"back",  NULL, 0, menu_back}
};

static const OLED_MenuItem subMenuSpd[] = {
    {(uint8_t*)SPD_PText, NULL, 0, PID_SetSpdP},
    {(uint8_t*)SPD_IText, NULL, 0, PID_SetSpdI},
    {(uint8_t*)SPD_stepText, NULL, 0, SPD_switchstep},
    {(uint8_t*)"CLEAR", NULL, 0, SPD_clear},
    {(uint8_t*)"back",  NULL, 0, menu_back}
};

static const OLED_MenuItem mainItems[9] = {
    {(uint8_t*)"run",    NULL,        0, run_line_following},
    {(uint8_t*)"PID",    subMenu2,    OLED_MENU_COUNT(subMenu2)},
    {(uint8_t*)"spdPI",  subMenuSpd,  OLED_MENU_COUNT(subMenuSpd)},
    {(uint8_t*)"PI tune",NULL,        0, run_pi_tune},
    {(uint8_t*)"IR",     NULL,        0, ir_sensor_show},
    {(uint8_t*)"encode", NULL,        0, encode_show},
    {(uint8_t*)"pwm",     subMenu1,   OLED_MENU_COUNT(subMenu1)},
    {(uint8_t*)LostFlagText, NULL,       0, lost_flag_toggle},
    {(uint8_t*)"ver",       subMenuVer, OLED_MENU_COUNT(subMenuVer)},
};

OLED_Menu menu;

int main(void) {
    SYSCFG_DL_init();
    NVIC_EnableIRQ(encode_INT_IRQN);
    
    
    W25Q64_Init(); delay(5);
    W25Q64_ReadID(); 
    PARAM_Load();
    KEY_Init();
    PWM_init();
    OLED_Init();
    pwm_updateText();
    PID_updateText();
    snprintf(LostFlagText, sizeof(LostFlagText), "lost:%d", lost_line_flag);
    OLED_SetFontFlashBase(GB2312_FONT_FLASH_ADDR);
    OLED_Menu_Init(&menu);
    OLED_Menu_SetRoot(&menu, mainItems, 9);
    OLED_Menu_Draw(&menu);

 

    while (1) { //OLED_ShowIRSensors();
        KeyEvent key = KEY_Scan();
			led_on();
        switch (key) {
					
            case KEY_UP:    OLED_Menu_Prev(&menu); break;
            case KEY_DOWN:  OLED_Menu_Next(&menu); break;
            case KEY_ENTER: OLED_Menu_Enter(&menu); break;
            default: break;
        }
    }

}

void menu_back(void) {
    OLED_Menu_Back(&menu);
}

void PID_SetP(void){
    kp += PID_step;
    if(kp > PID_P_MAX) kp = PID_P_MAX;
    else if(kp < 0) kp = 0;
    PARAM_Save(); PID_updateText();
}

void PID_SetI(void){
    ki += PID_step;
    if(ki > PID_I_MAX) ki = PID_I_MAX;
    else if(ki < 0) ki = 0;
    PARAM_Save(); PID_updateText();
}



void PID_switchstep(void){
    static uint8_t idx = 0;
    const float steps[] = {0.0025f,-0.0025f,0.02f, -0.02f, 0.1f, -0.1f, 1.0f, -1.0f};
    idx = (idx + 1) % (sizeof(steps)/sizeof(steps[0]));
    PID_step = steps[idx];
    PID_updateText();
}

void SPD_switchstep(void){
    static uint8_t idx = 0;
    const float steps[] = {0.005f,-0.005f,0.02f, -0.02f, 0.1f, -0.1f,1.0f, -1.0f};
    idx = (idx + 1) % (sizeof(steps)/sizeof(steps[0]));
    spd_step = steps[idx];
    PID_updateText();
}

void PID_SetSpdP(void) {
    kp_spd += spd_step;
    if (kp_spd > SPD_P_MAX) kp_spd = SPD_P_MAX;
    else if (kp_spd < 0) kp_spd = 0;
    PARAM_Save(); PID_updateText();
}

void PID_SetSpdI(void) {
    ki_spd += spd_step;
    if (ki_spd > SPD_I_MAX) ki_spd = SPD_I_MAX;
    else if (ki_spd < 0) ki_spd = 0;
    PARAM_Save(); PID_updateText();
}

void PID_SetD(void){
    kd += PID_step;
    if(kd > PID_D_MAX) kd = PID_D_MAX;
    else if(kd < 0) kd = 0;
    PARAM_Save(); PID_updateText();
}
void PID_clear(void){
    kp = 0.0f;
    ki = 0.0f;
    kd = 0.0f;
    PARAM_Save(); PID_updateText();
}

void lost_flag_toggle(void) {
    lost_line_flag = !lost_line_flag;
    snprintf(LostFlagText, sizeof(LostFlagText), "lost:%d", lost_line_flag);
}

void ver_save_a(void) {
    ParamBlock pb;
    pb.magic = PARAM_MAGIC;
    pb.kp = kp; pb.ki = ki; pb.kd = kd;
    pb.kp_spd = kp_spd; pb.ki_spd = ki_spd;
    W25Q64_Write(VER_A_ADDR, (uint8_t*)&pb, sizeof(pb), 1);
}

void ver_save_b(void) {
    ParamBlock pb;
    pb.magic = PARAM_MAGIC;
    pb.kp = kp; pb.ki = ki; pb.kd = kd;
    pb.kp_spd = kp_spd; pb.ki_spd = ki_spd;
    W25Q64_Write(VER_B_ADDR, (uint8_t*)&pb, sizeof(pb), 1);
}

void ver_load_a(void) {
    ParamBlock pb;
    W25Q64_ReadData(VER_A_ADDR, (uint8_t*)&pb, sizeof(pb));
    if (pb.magic != PARAM_MAGIC) return;
    kp = pb.kp; ki = pb.ki; kd = pb.kd;
    kp_spd = pb.kp_spd; ki_spd = pb.ki_spd;
    PARAM_Save(); PID_updateText();
}

void ver_load_b(void) {
    ParamBlock pb;
    W25Q64_ReadData(VER_B_ADDR, (uint8_t*)&pb, sizeof(pb));
    if (pb.magic != PARAM_MAGIC) return;
    kp = pb.kp; ki = pb.ki; kd = pb.kd;
    kp_spd = pb.kp_spd; ki_spd = pb.ki_spd;
    PARAM_Save(); PID_updateText();
}

void SPD_clear(void){
    kp_spd = 0.0f;
    ki_spd = 0.0f;
    PARAM_Save(); PID_updateText();
}

void PID_save(){ PARAM_Save(); PID_updateText(); }

void pwm_setleftspeed(void) {
    if (pwm_flag) left_speed += pwm_step; else left_speed -= pwm_step;
    if (left_speed >= 1000) left_speed = 1000;
    else if (left_speed <= -1000) left_speed = -1000;
    pwm_updateText();
    PWM_Run(left_speed, right_speed);
}

void pwm_setrightspeed(void) {
    if (pwm_flag) right_speed += pwm_step; else right_speed -= pwm_step;
    if (right_speed >= 1000) right_speed = 1000;
    else if (right_speed <= -1000) right_speed = -1000;
    pwm_updateText();
    PWM_Run(left_speed, right_speed);
}

void pwm_setflag(void) {
    pwm_flag = !pwm_flag;
    if (pwm_flag)
        snprintf(PWMFlagText, sizeof(PWMFlagText), "step:+%d", pwm_step);
    else
        snprintf(PWMFlagText, sizeof(PWMFlagText), "step:-%d", pwm_step);
}

void pwm_clear(void) {
    left_speed = 0;
    right_speed = 0;
    pwm_updateText();
    PWM_Run(0, 0);
}

void pwm_back(void) {
    // 退出不清空，保留当前 PWM 输出
    OLED_Menu_Back(&menu);
}

void pwm_updateText(void) {
    snprintf(LText, sizeof(LText), "L:%d", left_speed);
    snprintf(RText, sizeof(RText), "R:%d", right_speed);
    if (pwm_flag)
        snprintf(PWMFlagText, sizeof(PWMFlagText), "step:+%d", pwm_step);
    else
        snprintf(PWMFlagText, sizeof(PWMFlagText), "step:-%d", pwm_step);
}

void PID_updateText(void){
    snprintf(PText, sizeof(PText), "P:%.4f", kp);
    snprintf(IText, sizeof(IText), "I:%.4f", ki);
    snprintf(DText, sizeof(DText), "D:%.4f", kd);
    snprintf(SPD_PText, sizeof(SPD_PText), "Sp:%+.3f", kp_spd);
    snprintf(SPD_IText, sizeof(SPD_IText), "Si:%+.3f", ki_spd);
    snprintf(PID_stepText, sizeof(PID_stepText), "step:%+.4f", PID_step);
    snprintf(SPD_stepText, sizeof(SPD_stepText), "step:%+.3f", spd_step);
}

void ir_sensor_show(void) {
    OLED_Clear();
    OLED_ShowString(1, 1, (const uint8_t*)"IR Sensors");
    OLED_ShowIRSensors();
    OLED_ShowString(4, 5, (const uint8_t*)"> Back");

    while (1) {
        KeyEvent key = KEY_Scan();
        if (key == KEY_ENTER) break;
        OLED_ShowIRSensors();
    }
}

void encode_show(void) {
    OLED_Clear();
    OLED_ShowString(1, 1, (const uint8_t*)"Encoder");
    OLED_ShowString(2, 1, (const uint8_t*)"L:");
    OLED_ShowString(3, 1, (const uint8_t*)"R:");
    OLED_ShowString(4, 5, (const uint8_t*)"> Back");

    while (1) {
        KeyEvent key = KEY_Scan();
        if (key == KEY_ENTER) break;

        int16_t l = Encode_GetspeedL();
        int16_t r = Encode_GetspeedR();
        OLED_ShowSignedNum(2, 3, l, 6);
        OLED_ShowSignedNum(3, 3, r, 6);
    }
}

void run_pi_tune(void) {
    OLED_Clear();
    OLED_ShowString(1, 1, (const uint8_t*)"<EN=Ext");
	
    SpeedPI_Reset();
    int16_t tgtL = 300, tgtR = 300;
    // state: 0=调L, 1=调R, 2=待退出
    uint8_t state = 0;

    while (1) {
        KeyEvent key = KEY_Scan();
        if (key == KEY_ENTER) {
            state++;
            if (state > 2) state = 0;
        }
        if (state == 2) {
            if (key == KEY_ENTER) break;
            if (key == KEY_UP || key == KEY_DOWN) state = 0;  // 按方向切回调节
        } else if (state == 0) {
            if (key == KEY_UP)   { tgtL += 50; if (tgtL > 1000) tgtL = 1000; }
            if (key == KEY_DOWN) { tgtL -= 50; if (tgtL < 0) tgtL = 0; }
        } else { // state == 1
            if (key == KEY_UP)   { tgtR += 50; if (tgtR > 1000) tgtR = 1000; }
            if (key == KEY_DOWN) { tgtR -= 50; if (tgtR < 0) tgtR = 0; }
        }

        SpeedTune_Step(tgtL, tgtR);

        // 第2行: 左轮目标
        OLED_ShowChar(2, 1, (state == 0) ? '>' : ' ');
        OLED_ShowChar(2, 2, 'L');
        OLED_ShowSignedNum(2, 4, tgtL, 4);
        // 第3行: 右轮目标
        OLED_ShowChar(3, 1, (state == 1) ? '>' : ' ');
        OLED_ShowChar(3, 2, 'R');
        OLED_ShowSignedNum(3, 4, tgtR, 4);
        // 第4行: 编码器实际值
        OLED_ShowChar(4, 1, 'L');
        OLED_ShowSignedNum(4, 3, last_enc_l, 4);
        OLED_ShowChar(4, 9, 'R');
        OLED_ShowSignedNum(4, 11, last_enc_r, 4);
    }

    PWM_Run(0, 0);
}

void run_line_following(void) {
    OLED_Clear();
  

    Line_Following_Init();

    while (1) {
        /* Line 1: 编码器速度 L / R (由速度环缓存) */
        OLED_ShowChar(1, 1, 'L');
        OLED_ShowSignedNum(1, 3, last_enc_l, 4);
        OLED_ShowChar(1, 9, 'R');
        OLED_ShowSignedNum(1, 10, last_enc_r, 4);

        /* Line 2-3: IR 传感器状态 */
        OLED_ShowIRSensors();

        /* Line 4: 左右轮 PWM 值 */
        OLED_ShowChar(4, 1, 'L');
        OLED_ShowSignedNum(4, 3, 1001-debug_pwm_left, 5);
        OLED_ShowChar(4, 9, 'R');
        OLED_ShowSignedNum(4, 11, 1001-debug_pwm_right, 5);

        KeyEvent key = KEY_Scan();
        if (key == KEY_ENTER) break;
        Line_Following_Update();
    }

    PWM_stop();
}

void GROUP1_IRQHandler(void){
    switch( DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1) )
    {
        case DL_INTERRUPT_GROUP1_IIDX_GPIOA:
            encode_getcount();
        break;
    }
}

