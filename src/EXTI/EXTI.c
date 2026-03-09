#include "EXTI.h"

static volatile uint8_t exti_flag;

void START_EXTI(void)
{   
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_AF);

    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_0);

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

void Button_Task(void)
{
    if(gpio_input_bit_get(GPIOA, GPIO_PIN_0) == RESET)
    {
        tim_delay(50);

        if(gpio_input_bit_get(GPIOA, GPIO_PIN_0) == RESET)
        {
            set_flag();
            while(gpio_input_bit_get(GPIOA, GPIO_PIN_0) == RESET);
        }
    }
}
