#include "gd32f10x.h"
#include "systick.h"
#include "..\src\led\led.c"

#define LED_GPIO_PORT          GPIOB
#define LED_PIN                GPIO_PIN_12
#define LED_GPIO_CLK           RCU_GPIOB


int main(void) {
    systick_config();
    Led led1 = {LED_PIN, LED_GPIO_PORT, LED_GPIO_CLK};
    LED_Init(&led1);

    while(1){
        LED_On(&led1);
        delay_1ms(12000);
        LED_Off(&led1);
    }
}
