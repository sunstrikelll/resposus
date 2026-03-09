#include "EXTI.h"

static volatile uint8_t exti_flag;

void START_EXTI(void)
{   
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_AF);

    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_0);

    nvic_irq_enable(EXTI0_IRQn, 2U, 0U);
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOA, GPIO_PIN_SOURCE_0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(EXTI_0);
}

void EXTI0_IRQHandler(void)
{
    if(exti_interrupt_flag_get(EXTI_0) != RESET) 
    {
        set_flag();
        exti_interrupt_flag_clear(EXTI_0);
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
