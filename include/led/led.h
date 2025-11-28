#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef struct Led Led;

int LED_Init(Led* _led, uint32_t _capacity);
void LED_On(Led* _led);
void LED_Off(Led* _led);
void LED_Toggle(Led* _led);

#endif
