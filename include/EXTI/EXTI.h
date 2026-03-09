#ifndef EXTI_H
#define EXTI_H

#include "gd32f10x.h"

void START_EXTI(void);
void EXTI0_IRQHandler(void);
void set_flag(void);
void clear_flag(void);
uint8_t get_flag(void);

#endif