#ifndef EXTI_H
#define EXTI_H

#include "gd32f10x.h"
#include "tim3_ms.h"

#define LED_GPIO_PORT          GPIOB
#define LED_PIN                GPIO_PIN_12
#define LED_GPIO_CLK           RCU_GPIOB
#define BUTTON_GPIO_CLK        RCU_GPIOB
#define BUTTON_GPIO_PORT          GPIOB
#define BUTTON_PIN                GPIO_PIN_11
void EXTI_init(void);

#endif