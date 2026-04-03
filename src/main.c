#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tim3_ms.h"
#include "UART.h"
#include "modbus.h"
#include "led.h"

static Led led1 = { .pin = GPIO_PIN_6, .port = GPIOC, .rcu_periph = RCU_GPIOC };

static void task_modbus(void *arg)
{
    static uint8_t rx_data[256];
    static uint8_t tx_data[260];

    for (;;)
    {
        if (uart_getReadyFlag())
        {
            uint16_t rx_len = UART_Receive(rx_data);
            uint16_t tx_len = modbus_process(rx_data, rx_len, tx_data);
            if (tx_len > 0)
                UART_Transmit(tx_data, tx_len);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void task_led2(void *arg)
{
    for (;;)
    {
        LED_Toggle(&led1);
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

    xTaskCreate(task_modbus,   "modbus",   configMINIMAL_STACK_SIZE,     NULL, 1, NULL);
    xTaskCreate(task_led2,   "led2",   configMINIMAL_STACK_SIZE,     NULL, 1, NULL);

    vTaskStartScheduler();

    for (;;) {}
}
