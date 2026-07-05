#ifndef MOTOR_VARS_H
#define MOTOR_VARS_H

#include <stdint.h>
#include <stdbool.h>

extern volatile int16_t left_speed;
extern volatile int16_t right_speed;
extern bool pwm_flag;

#endif
