#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "..\..\GD_libraries\CMSIS\DeviceSupport\gd32f10x.h"

typedef struct Led
{
    uint32_t pin;
    uint32_t port;
    uint32_t rcu_periph;
} Led;

uint8_t LED_Init(Led* _led)
{
    rcu_periph_clock_enable(_led->rcu_periph);
    gpio_init(_led->port, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, _led->pin);
    return 0;
}

void LED_On(const Led* _led)
{
    gpio_bit_set(_led->port, _led->pin);
}

void LED_Off(const Led* _led)
{
    gpio_bit_reset(_led->port, _led->pin);
}

void LED_Toggle(const Led* _led)
{
    uint8_t state = gpio_output_bit_get(_led->port, _led->pin);
    if (state == SET)
    {
        gpio_bit_reset(_led->port, _led->pin);
    }
    else
    {
        gpio_bit_set(_led->port, _led->pin);
    }
}


#endif
