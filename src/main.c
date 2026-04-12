#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tim3_ms.h"
#include "usb_cdc.h"
#include "modbus.h"
#include "modbus_table.h"
#include "led.h"
#include "lcd_hd44780.h"

static Led led1 = { .pin = GPIO_PIN_6, .port = GPIOC, .rcu_periph = RCU_GPIOC };

static void task_modbus(void *arg)
{
    static uint8_t rx_data[256];
    static uint8_t tx_data[260];

    for (;;)
    {
        if (usb_cdc_getReadyFlag())
        {
            uint16_t rx_len = usb_cdc_receive(rx_data);
            uint16_t tx_len = modbus_process(rx_data, rx_len, tx_data);
            if (tx_len > 0)
                usb_cdc_transmit(tx_data, tx_len);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void task_timer(void *arg)
{
    TickType_t xLastWake = xTaskGetTickCount();
    uint8_t tick_count = 0;

    for (;;)
    {
        /* Каждые 50 мс: проверить RESET_TIMER */
        if (MB_ReadBits(MB_ADDR_BIT_CR) & MB_BIT_CR_RESET_TIMER)
        {
            MB_WriteUint32(MB_ADDR_TIMER, 0);
            MB_ClearBit(MB_ADDR_BIT_CR, MB_BIT_CR_RESET_TIMER);
        }

        tick_count++;
        /* 20 * 50 мс = 1000 мс — инкремент таймера */
        if (tick_count >= 20)
        {
            tick_count = 0;
            uint32_t t = MB_ReadUint32(MB_ADDR_TIMER);
            MB_WriteUint32(MB_ADDR_TIMER, t + 1);
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(50));
    }
}

static void task_lcd_test(void *arg)
{
    lcd_init();

    lcd_print_at(0, 0, "MT-20S4M  LCD Test");
    lcd_print_at(1, 0, "ABCabc 0123 !@#$%");
    lcd_print_at(2, 0, "Привет Мир!");
    lcd_print_at(3, 0, "Тест кириллицы");

    vTaskDelete(NULL);
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
    usb_cdc_init();
    modbus_init();

    LED_Init(&led1);

    xTaskCreate(task_modbus,    "modbus",    configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(task_timer,    "timer",     configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(task_lcd_test, "lcd_test",  256,                      NULL, 2, NULL);
    xTaskCreate(task_led2,     "led2",      configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    vTaskStartScheduler();

    for (;;) {}
}
