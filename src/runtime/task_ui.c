/*  task_ui.c — PRODUCTION-задача HMI
 *
 *  Опрос кнопок + обработка меню (дисплей + светодиоды).
 *  Сам menu_process() каждые 200 мс внутри себя вызывает
 *  menu_update_display() для обновления LCD.
 */

#include "task_ui.h"

#include "FreeRTOS.h"
#include "task.h"

#include "buttons.h"
#include "menu.h"

static void task_ui(void *arg)
{
    TickType_t xLastWake = xTaskGetTickCount();
    (void)arg;

    for (;;)
    {
        BtnEvent_t ev = btn_scan();
        menu_process(ev);
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(BTN_SCAN_MS));
    }
}

void task_ui_start(void)
{
    /* Стек 512 слов — нужен для menu_update_display (LCD + форматирование) */
    xTaskCreate(task_ui, "ui", 512, NULL, 3, NULL);
}
