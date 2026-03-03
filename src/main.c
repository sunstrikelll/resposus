#include "gd32f10x.h"
#include "systick.h"
#include "tim3_ms.h"
#include <stdint.h>
#include "PWM.h"
#include "dma.h"

#define LED_GPIO_PORT          GPIOA
#define LED_PIN1                GPIO_PIN_0
#define LED_GPIO_CLK           RCU_GPIOA



int main(void) 
{
    adc_init();
    tim_Init();
    pwm_init();
    int a = 0;

    while (1)
    {
        a = adc_getValue();
        tim_delay(100);
        pwm_setVoltage(a);
    }
    
}


