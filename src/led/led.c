#include "../include/led/led.h"

typedef struct Led 
{
    uint8_t pin;
    uint8_t port;
    uint32_t rcu_periph;
} Led;

int LED_Init(const Led* _led)
{
    _rcu_periph_clock_enable(_led->rcu_periph);
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
    gpio_bit_toggle(_led->port, _led->pin);
}

void LED_Delete(const Led* _led)
{
    gpio_bit_reset(_led->port, _led->pin);
    free(_led);
}
