#include "EXTI.h"

static volatile uint8_t exti_flag;

int START_EXTI(void)
{
    rcu_periph_clock_enable(GPIO_CLK);
    gpio_init(GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, PIN);

    GPIO_BC(GPIO_PORT) = PIN;

    rcu_periph_clock_enable(BUTTON_GPIO_CLK);
    rcu_periph_clock_enable(RCU_AF);

    gpio_init(BUTTON_GPIO_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, BUTTON_PIN);
}

void EXTI10_15_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(EXTI_11)) 
    {
        set_flag();
        exti_interrupt_flag_clear(EXTI_11);
    }
}

void set_flag(void)
{
    exti_flag = 1;
}

void clear_flag(void)
{
    exti_flag = 0;
}

uint8_t get_flag(void)
{
    return exti_flag;
}
