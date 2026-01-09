#include "gd32f10x.h"
#include "systick.h"
#include "..\src\led\led.c"
#include "..\src\clock\tim3_sec.c"
#include <stdint.h>

#define LED_GPIO_PORT          GPIOB
#define LED_PIN1                GPIO_PIN_12
#define LED_GPIO_CLK           RCU_GPIOB



int main(void) {
    systick_config();
    Led led1 = {LED_PIN1, LED_GPIO_PORT, LED_GPIO_CLK};
   
    LED_Init(&led1);

    uint32_t temp = 0;
    uint32_t init_time = 0;
    tim_Init();

    while (1)
    {
        temp = tim_getTime() - init_time;
        init_time = tim_getTime();
        LED_Toggle(&led1);
        tim_delay(1000);
        LED_Toggle(&led1);
        tim_delay(1000);
    }
    
}


