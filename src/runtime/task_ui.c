/*  task_ui.c — PRODUCTION-задача HMI
 *
 *  Опрос кнопок + обработка меню (дисплей + светодиоды).
 *  menu_process() внутри себя вызывает menu_update_display() при трёх
 *  условиях: таймер (1500 мс), событие кнопки, переход состояния.
 */

#include "task_ui.h"

#include "FreeRTOS.h"
#include "task.h"

#include "buttons.h"
#include "menu.h"

static void task_ui(void *arg)
{
    (void)arg;

    /* menu_init() вызывает lcd_init(), который использует vTaskDelay().
       Поэтому обязан выполняться внутри задачи (после старта сцедулера),
       а не в runtime_start() до vTaskStartScheduler().                    */
    menu_init();

    TickType_t xLastWake = xTaskGetTickCount();

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
