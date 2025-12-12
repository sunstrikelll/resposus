#include "..\include\led\led.h"

typedef struct Led 
{
    uint32_t pin;
    uint32_t port;
    uint32_t rcu_periph;
} Led;

uint8_t LED_Init(Led* _led, uint8_t _pin, uint8_t _port, uint32_t _rcu_periph)
{
    _led->pin = _pin;
    _led->port = _port;
    _led->rcu_periph = _rcu_periph;
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
