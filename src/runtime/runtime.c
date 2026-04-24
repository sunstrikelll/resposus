/*  runtime.c — диспетчер режимов.
 *
 *  Сами задачи живут в отдельных файлах:
 *      task_modbus.c         — общая для обоих режимов
 *      task_timer.c          — PRODUCTION (1-Гц счётчик)
 *      task_ui.c             — PRODUCTION (кнопки + меню + LCD + LEDs)
 *      task_test_display.c   — TEST (вывод 4 test_line_N на LCD)
 *      task_test_btn.c       — TEST (btn → btn_event)
 *
 *  Здесь только:
 *      • запись выбранного режима в MB_ADDR_RUNTIME_MODE;
 *      • инициализация «железа», специфичного для режима
 *        (buttons, menu — чтобы не делать это в main.c);
 *      • создание нужного набора задач.
 */

#include "runtime.h"

#include "modbus_table.h"
#include "buttons.h"
#include "menu.h"

#include "task_modbus.h"
#include "task_timer.h"
#include "task_ui.h"
#include "task_test_display.h"
#include "task_test_btn.h"

void runtime_start(RuntimeMode_t mode)
{
    /* Примечание: запись MB_ADDR_RUNTIME_MODE делается в main() ДО
       вызова этой функции — держим единственную точку публикации
       режима, чтобы не было расхождений между регистром и фактически
       стартанувшими задачами.                                          */

    /* Общая задача Modbus RTU / USB CDC */
    task_modbus_start();

    if (mode == RUNTIME_TEST) {
        /* ── Тестовый режим ──
           LCD инициализирует сама task_test_display, меню/LEDs не нужны. */
        btn_init();

        task_test_display_start();
        task_test_btn_start();
    } else {
        /* ── Штатный режим (PRODUCTION) ── */
        btn_init();
        /* menu_init() НЕ вызываем здесь — он зовёт lcd_init(),
           который использует vTaskDelay(). Сцедулер ещё не запущен,
           это приводило бы к HardFault. menu_init() теперь вызывает
           task_ui() внутри своего контекста (после старта сцедулера).  */

        task_timer_start();
        task_ui_start();
    }
}
