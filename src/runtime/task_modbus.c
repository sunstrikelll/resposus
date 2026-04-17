/*  task_modbus.c — задача Modbus RTU / USB CDC
 *
 *  Опрашивает флаг готовности USB CDC, при наличии фрейма — вызывает
 *  modbus_process() и отправляет ответ обратно в хост.
 *
 *  Буферы rx/tx — static (не на стеке задачи), чтобы не раздувать стек.
 */

#include "task_modbus.h"

#include "FreeRTOS.h"
#include "task.h"

#include "usb_cdc.h"
#include "modbus.h"

static void task_modbus(void *arg)
{
    static uint8_t rx_data[256];
    static uint8_t tx_data[260];
    (void)arg;

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

void task_modbus_start(void)
{
    xTaskCreate(task_modbus, "modbus", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}
