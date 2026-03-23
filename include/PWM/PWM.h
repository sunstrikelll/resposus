#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include "..\..\GD_libraries\CMSIS\DeviceSupport\gd32f10x.h"

#define PWM_PORT           GPIOA
#define PWM_PIN            GPIO_PIN_8
#define PWM_FREQ           GPIO_OSPEED_50MHZ
#define PWM_MODE           GPIO_MODE_AF_PP
#define PWM_TIMER          TIMER0
#define PWM_CHANNEL        TIMER_CH_0
#define PROCPERIOD         1000
#define PROCMGFREQ         72
#define GPIO_CLK           RCU_GPIOA
#define TIMER_GPIO_CLK     RCU_TIMER0
#define FOR_TO_PRECENT     100

void pwm_init(void);

void pwm_setVoltage(uint8_t percent);

#endif