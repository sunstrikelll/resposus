/*  task_test_btn.c — TEST-режим: запись кода кнопки в btn_event
 *
 *  Никакого меню и светодиодов — просто переводит события кнопок
 *  в Modbus-регистр для внешнего тестирования.
 */

#include "task_test_btn.h"

#include "FreeRTOS.h"
#include "task.h"

#include "buttons.h"
#include "modbus_table.h"

static void task_test_btn(void *arg)
{
    TickType_t xLastWake = xTaskGetTickCount();
    (void)arg;

    for (;;)
    {
        BtnEvent_t ev = btn_scan();
        if (ev != BTN_EV_NONE)
            MB_WriteBits(MB_ADDR_BTN_EVENT, (uint8_t)ev);

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(BTN_SCAN_MS));
    }
}

void task_test_btn_start(void)
{
    xTaskCreate(task_test_btn, "tst_btn", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
}
