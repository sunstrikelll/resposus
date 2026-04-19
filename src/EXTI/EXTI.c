#include "EXTI.h"

static volatile uint8_t exti_flag;

void START_EXTI(void)
{   
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_AF);

    gpio_init(GPIOB, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, GPIO_PIN_2);
    gpio_init(GPIOB, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, GPIO_PIN_3);
    gpio_init(GPIOB, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, GPIO_PIN_4);
    gpio_init(GPIOB, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, GPIO_PIN_5);
    gpio_init(GPIOB, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, GPIO_PIN_15);
    gpio_init(GPIOD, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, GPIO_PIN_2);

}


void Button_Task(void)
{
    if(gpio_input_bit_get(GPIOB, GPIO_PIN_2) == RESET)
    {
        tim_delay(50);

        if(gpio_input_bit_get(GPIOB, GPIO_PIN_2) == RESET)
        {
            MB_WriteString(MB_ADDR_TEST_LINE_0, "1");
        }
    }

    if(gpio_input_bit_get(GPIOB, GPIO_PIN_3) == RESET)
    {
        tim_delay(50);

        if(gpio_input_bit_get(GPIOB, GPIO_PIN_3) == RESET)
        {
            MB_WriteString(MB_ADDR_TEST_LINE_0, "2");
        }
    }

    if(gpio_input_bit_get(GPIOB, GPIO_PIN_4) == RESET)
    {
        tim_delay(50);

        if(gpio_input_bit_get(GPIOB, GPIO_PIN_4) == RESET)
        {
            MB_WriteString(MB_ADDR_TEST_LINE_0, "3");
        }
    }

    if(gpio_input_bit_get(GPIOB, GPIO_PIN_5) == RESET)
    {
        tim_delay(50);

        if(gpio_input_bit_get(GPIOB, GPIO_PIN_5) == RESET)
        {
            MB_WriteString(MB_ADDR_TEST_LINE_0, "4");
        }
    }

    if(gpio_input_bit_get(GPIOB, GPIO_PIN_2) == RESET)
    {
        tim_delay(50);

        if(gpio_input_bit_get(GPIOB, GPIO_PIN_15) == RESET)
        {
            MB_WriteString(MB_ADDR_TEST_LINE_0, "5");
        }
    }

    if(gpio_input_bit_get(GPIOD, GPIO_PIN_2) == RESET)
    {
        tim_delay(50);

        if(gpio_input_bit_get(GPIOD, GPIO_PIN_2) == RESET)
        {
            MB_WriteString(MB_ADDR_TEST_LINE_0, "6");
        }
    }
}
