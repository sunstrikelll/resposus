#include "gd32f10x.h"
#include "tim3_ms.h"
#include <stdint.h>
#include "PWM.h"
#include "dma.h"
#include "led.h"
#include "EXTI.h"

#define LED_GPIO_PORT          GPIOB
#define LED_PIN                GPIO_PIN_13
#define LED_GPIO_CLK           RCU_GPIOB

Led led1 = {LED_PIN, LED_GPIO_PORT, LED_GPIO_CLK};


int main(void) 
{
    Button_Task();
    LED_Init(&led1);
    tim_Init();
    START_EXTI();
    while (1)
    {
        if(get_flag()) 
        {
            LED_Toggle(&led1);
            tim_delay(3000);
            LED_Toggle(&led1);
            clear_flag();
        }
    }
    
}


