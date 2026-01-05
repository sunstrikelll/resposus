#include "../include/led/led.h"

#define LED_GPIO_PORT          GPIOB
#define LED_PIN1                GPIO_PIN_12
#define LED_PIN2                GPIO_PIN_13
#define LED_PIN3                GPIO_PIN_14
#define LED_PIN4                GPIO_PIN_15
#define LED_PIN5                GPIO_PIN_11
#define LED_GPIO_CLK           RCU_GPIOB
#define MAX_D_ACC               5



int main(void) {
    systick_config();
    Led led1 = {LED_PIN1, LED_GPIO_PORT, LED_GPIO_CLK};
    Led led2 = {LED_PIN2, LED_GPIO_PORT, LED_GPIO_CLK};
    Led led3 = {LED_PIN3, LED_GPIO_PORT, LED_GPIO_CLK};
    Led led4 = {LED_PIN4, LED_GPIO_PORT, LED_GPIO_CLK};
    Led led5 = {LED_PIN5, LED_GPIO_PORT, LED_GPIO_CLK};
   
    LED_Init(&led1);
    LED_Init(&led2);
    LED_Init(&led3);
    LED_Init(&led4);
    LED_Init(&led5);

    Led *arr[MAX_D_ACC];
    arr[0] = &led1;
    arr[1] = &led2;
    arr[2] = &led3;
    arr[3] = &led4;
    arr[4] = &led5;

    while (1)
    {
        for (int i = MAX_D_ACC - 1; i >= 0 ; i--)
        {
            for (int j = 0; j < i; i++)
            {
                LED_Toggle(arr[j]);
                delay_1ms(600);
                LED_Toggle(arr[j]);
            }
            LED_On(arr[i]);
        }
        for (int i = MAX_D_ACC - 1; i >= 0 ; i--)
        {
            LED_Off(arr[i]);
        }
    }
    
}

