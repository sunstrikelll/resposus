/*  main.c — точка входа БУСШ 02.101.01
 *
 *  Последовательность:
 *    1) Инициализация периферии (NVIC, таймеры, USB CDC, Modbus, EEPROM)
 *    2) Выбор режима рантайма:
 *         • RUNTIME_PRODUCTION — штатный режим (меню, кнопки, светодиоды)
 *         • RUNTIME_TEST       — тест вывода LCD (4 регистра test_line_N)
 *       Значение берётся из регистра MB_ADDR_RUNTIME_MODE, если оно
 *       валидно (0 или 1), иначе используется RUNTIME_MODE_DEFAULT.
 *    3) runtime_start(mode)  — создаёт набор FreeRTOS-задач
 *    4) vTaskStartScheduler()
 */

#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tim3_ms.h"
#include "usb_cdc.h"
#include "modbus.h"
#include "modbus_table.h"
#include "eeprom.h"
#include "runtime.h"

/* ── Режим по умолчанию, если в регистре RUNTIME_MODE что-то «не то» ──
   Чтобы переключиться на тест вывода, достаточно раскомментировать
   вторую строку и закомментировать первую.                              */
#define RUNTIME_MODE_DEFAULT  RUNTIME_PRODUCTION
/* #define RUNTIME_MODE_DEFAULT  RUNTIME_TEST */

/* ─── resolve_runtime_mode ──────────────────────────────────────────────────
   Читает MB_ADDR_RUNTIME_MODE. Если в регистре уже лежит 0 или 1 — берём
   его (так внешний мастер может заранее «попросить» конкретный режим).
   Иначе ставим дефолт.                                                      */
static RuntimeMode_t resolve_runtime_mode(void)
{
    uint8_t m = MB_ReadBits(MB_ADDR_RUNTIME_MODE);
    if (m == (uint8_t)RUNTIME_PRODUCTION) return RUNTIME_PRODUCTION;
    if (m == (uint8_t)RUNTIME_TEST)       return RUNTIME_TEST;
    return RUNTIME_MODE_DEFAULT;
}

/* ─── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    tim_Init();
    usb_cdc_init();
    modbus_init();
    eeprom_init();

    /* Начальное значение version = 1.0 */
    MB_WriteFloat(MB_ADDR_VERSION, 1.0f);

    /* Выбор режима и запуск задач */
    RuntimeMode_t mode = resolve_runtime_mode();
    runtime_start(mode);

    vTaskStartScheduler();

    for (;;) {}
}
