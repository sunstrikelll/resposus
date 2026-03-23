#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tim3_ms.h"
#include "UART.h"
#include "modbus.h"
<<<<<<< Updated upstream

=======
#include "led.h"

/* === Настройка пинов LED ===
   Поменяй под свою плату. Пример: PC13 — встроенный LED Blue Pill */
static Led led1 = { .pin = GPIO_PIN_13, .port = GPIOC, .rcu_periph = RCU_GPIOC };
static Led led2 = { .pin = GPIO_PIN_0,  .port = GPIOA, .rcu_periph = RCU_GPIOA };

/* --- Задача: мигать LED1 каждые 500 мс --- */
static void task_led1(void *arg)
{
    (void)arg;
    for (;;)
    {
        LED_Toggle(&led1);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* --- Задача: мигать LED2 каждые 200 мс --- */
static void task_led2(void *arg)
{
    (void)arg;
    for (;;)
    {
        LED_Toggle(&led2);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* --- Задача: обработка Modbus по UART --- */
static void task_modbus(void *arg)
{
    (void)arg;
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

>>>>>>> Stashed changes
int main(void)
{
    tim_Init();
    uart_init();
    modbus_init();

<<<<<<< Updated upstream
    /* Максимальный ответ FC03/FC04: 3 + 125*2 + 2 = 255 байт */
    uint8_t rx_data[256];
    uint8_t tx_data[260];

    while (1)
    {
        if (uart_getReadyFlag())
        {
            uint16_t rx_len = UART_Receive(rx_data);
            uint16_t tx_len = modbus_process(rx_data, rx_len, tx_data);

            if (tx_len > 0)
                UART_Transmit(tx_data, tx_len);
        }
    }
=======
    LED_Init(&led1);
    LED_Init(&led2);

    xTaskCreate(task_led1,   "led1",   configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(task_led2,   "led2",   configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(task_modbus, "modbus", configMINIMAL_STACK_SIZE * 3, NULL, 2, NULL);

    vTaskStartScheduler();

    /* сюда не доходим */
    for (;;) {}
>>>>>>> Stashed changes
}
