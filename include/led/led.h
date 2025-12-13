#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "..\..\GD_libraries\CMSIS\DeviceSupport\gd32f10x.h"

typedef struct Led Led;

uint8_t LED_Init(Led* _led);
void LED_On(const Led* _led);
void LED_Off(const Led* _led);
void LED_Toggle(const Led* _led);
void LED_Delete(const Led* _led);

#endif
