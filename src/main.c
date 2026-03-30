#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tim3_ms.h"
#include "UART.h"
#include "modbus.h"
#include "led.h"

static Led led1 = { .pin = GPIO_PIN_13, .port = GPIOC, .rcu_periph = RCU_GPIOC };
static Led led2 = { .pin = GPIO_PIN_0,  .port = GPIOA, .rcu_periph = RCU_GPIOA };

static void task_led1(void *arg)
{
    for (;;)
    {
        LED_Toggle(&led1);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void task_led2(void *arg)
{
    for (;;)
    {
        LED_Toggle(&led2);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}


int main(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    tim_Init();
    uart_init();
    modbus_init();

    LED_Init(&led1);
    LED_Init(&led2);

    xTaskCreate(task_led1,   "led1",   configMINIMAL_STACK_SIZE,     NULL, 1, NULL);
    xTaskCreate(task_led2,   "led2",   configMINIMAL_STACK_SIZE,     NULL, 1, NULL);

    vTaskStartScheduler();

    for (;;) {}
}
