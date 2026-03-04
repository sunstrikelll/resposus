#include "EXTI.h"

int START_EXTI(void)
{
    rcu_periph_clock_enable(LED_GPIO_CLK);
    gpio_init(LED_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LED_PIN);

    GPIO_BC(LED_GPIO_PORT) = LED_PIN;

    rcu_periph_clock_enable(BUTTON_GPIO_CLK);
    rcu_periph_clock_enable(RCU_AF);

    gpio_init(BUTTON_GPIO_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, BUTTON_PIN);
}