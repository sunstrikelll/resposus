#include "gd32f10x.h"
#include "systick.h"
#include "..\src\led\led.c"
#include "..\src\clock\tim3_ms.c"
#include <stdint.h>
#include "..\src\PWM\PWM.c"

#define LED_GPIO_PORT          GPIOA
#define LED_PIN1                GPIO_PIN_0
#define LED_GPIO_CLK           RCU_GPIOA



int main(void) 
{

    tim_Init();
    pwm_init();

    while (1)
    {
        for(uint8_t i = 0; i <= 100; i++) 
        {
            pwm_setVoltage(i);
            tim_delay(100);
        }

        for(uint8_t i = 100; i > 0; i--) 
        {
            pwm_setVoltage(i);
            tim_delay(100);
        }
    }
    
}


