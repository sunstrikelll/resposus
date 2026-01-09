#ifndef TIM3_SEC
#define TIM3_SEC

#include <stdint.h>
#include "..\..\GD_libraries\CMSIS\DeviceSupport\gd32f10x.h"
#include "..\..\GD_libraries\GD32F10x_standard_peripheral\Include\gd32f10x_timer.h"

typedef struct Led Led;

void tim_Init(void);
void tim_delay(uint32_t sec);
uint32_t tim_getTime(void);
void TIMER3_IRQHandler(void);
#endif