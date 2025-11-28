#include "../include/led/led.h"

typedef struct Led 
{
    uint8_t pin;
    unsigned char power:1;
} Led;

int LED_Init(Led* _led, uint8_t _pin)
{
    _led->power = 0;
    _led->pin = _pin;
    PinMode(_pin, output);
    return 0;
}

void LED_On(Led* _led, uint8_t _pin)
{
    _led->power = 1;
    digitalwright(_pin, high);
}

void LED_Off(Led* _led, uint8_t _pin)
{
    _led->power = 0;
    digitalwright(_pin, low);
}

void LED_Toggle(Led* _led, uint8_t _pin, unsigned char _power)
{
    if (_power == 1) digitalwright(_pin, high);
    else digitalwright(_pin, low);
}
