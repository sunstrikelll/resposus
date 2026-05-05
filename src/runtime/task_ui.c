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
#include "watchdog.h"

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
        /* Кикаем сторож на каждом цикле UI — если задача зависнет,
           FWDGT перезагрузит контроллер через WDT_TIMEOUT_MS.            */
        wdt_kick();

        /* Объединённый опрос: физические кнопки + виртуальная кнопка
           (MB_ADDR_BTN_CMD). Регистр обнуляется внутри при срабатывании. */
        BtnEvent_t ev = btn_scan_with_cmd();
        menu_process(ev);
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(BTN_SCAN_MS));
    }
}

void task_ui_start(void)
{
    /* Стек 512 слов — нужен для menu_update_display (LCD + форматирование).
       Приоритет 2 — ниже task_modbus (3), чтобы busy-wait внутри LCD-драйвера
       не блокировал приём/ответ RTU-фреймов.                               */
    xTaskCreate(task_ui, "ui", 512, NULL, 2, NULL);
}
