#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "..\..\lib\CMSIS\DeviceSupport\gd32f10x.h"

typedef struct Led Led;

int LED_Init(const Led* _led);
void LED_On(const Led* _led);
void LED_Off(const Led* _led);
void LED_Toggle(const Led* _led);

#endif
