#include "gd32f10x.h"
#include "systick.h"
#include "..\src\led\led.c"

#define LED_GPIO_PORT          GPIOB
#define LED_PIN1                GPIO_PIN_12
#define LED_PIN2                GPIO_PIN_13
#define LED_PIN3                GPIO_PIN_14
#define LED_PIN4                GPIO_PIN_15
#define LED_PIN5                GPIO_PIN_11
#define LED_GPIO_CLK           RCU_GPIOB
#define MAX_D_ACC               9

struct Counter 
{
    uint8_t max;
    uint8_t current;
    uint8_t led;
};

  struct Led 
  {
    _Led *arr[MAX_D_ACC];
  };

int main(void) {
    systick_config();
    Led led1 = {LED_PIN1, LED_GPIO_PORT, LED_GPIO_CLK};
    Led led2 = {LED_PIN2, LED_GPIO_PORT, LED_GPIO_CLK};
    Led led3 = {LED_PIN3, LED_GPIO_PORT, LED_GPIO_CLK};
    Led led4 = {LED_PIN4, LED_GPIO_PORT, LED_GPIO_CLK};
    Led led5 = {LED_PIN5, LED_GPIO_PORT, LED_GPIO_CLK};
    struct Counter ledacc = {MAX_D_ACC, 0, &led1};
    uint8_t d;
   
    LED_Init(&led1);
    LED_Init(&led2);
    LED_Init(&led3);
    LED_Init(&led4);
    LED_Init(&led5);

    struct Led *arr[MAX_D_ACC];
    arr[0] = &led1;
    arr[1] = &led1;
    arr[2] = &led2;
    arr[3] = &led2;
    arr[4] = &led3;
    arr[5] = &led3;
    arr[6] = &led4;
    arr[7] = &led4;
    arr[8] = &led5;
    arr[9] = &led5;

    while(1){
        dioAcc();
        d = Counter->led
        LED_Toggle(d);
        delay_1ms(600);
    }
}

void dioAcc(void)
{
    Counter->led = arr[Counter->current];
    Counter->current += 1;
    if (Counter->current == Counter->max && Counter->max > 1)
    {
        Counter->max -= 2;
        Counter->current = 0;
    }
    elseif (Counter->max = 0)
    {
        Counter->current = (Counter->current * 2) - 1;
        if (Counter->current > 8)
        {
            Counter->max = MAX_D_ACC;
        }
    }
}
