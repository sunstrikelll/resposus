#include "gd32f10x.h"
#include "tim3_ms.h"
#include <stdint.h>
#include "PWM.h"
#include "dma.h"
#include "led.h"

#define LED_GPIO_PORT          GPIOA
#define LED_PIN                GPIO_PIN_0
#define LED_GPIO_CLK           RCU_GPIOA

Led led1 = {LED_PIN, LED_GPIO_PORT, LED_GPIO_CLK};

void EXTI10_15_IRQHandler(void)
{
    
}

int main(void) 
{
    tim_Init();
    START_EXTI();
    while (1)
    {
        if(RESET != exti_interrupt_flag_get(EXTI_11)) 
        {
            LED_Toggle(&led1);
            tim_delay(3000);
            LED_Toggle(&led1);
            exti_interrupt_flag_clear(EXTI_11);
        }
    }
    
}


