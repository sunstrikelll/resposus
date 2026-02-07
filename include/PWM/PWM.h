#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include "..\..\GD_libraries\CMSIS\DeviceSupport\gd32f10x.h"

#define PWM_PORT        GPIOA
#define PWM_PIN         GPIO_PIN_0
#define PWM_TIMER       TIMER2
#define PWM_CHANNEL     TIMER_CH_1

void pwm_init(void);

void pwm_setVoltage(uint8_t percent);

#endif