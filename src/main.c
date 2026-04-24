/*  main.c — точка входа
 *
 *  Последовательность:
 *    1) Инициализация периферии (NVIC, TIMER3, USB CDC, Modbus, EEPROM)
 *    2) Запись текущего режима в MB_ADDR_RUNTIME_MODE (чтобы мастер
 *       через Modbus мог прочитать, в каком режиме работает прошивка).
 *    3) runtime_start(mode) — создаёт набор FreeRTOS-задач.
 *    4) vTaskStartScheduler().
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

/* ══════════════════════════════════════════════════════════════════════════
   ВЫБОР РЕЖИМА РАНТАЙМА  ← редактируй ТОЛЬКО одну строку ниже
   ══════════════════════════════════════════════════════════════════════════
     RUNTIME_PRODUCTION — штатная работа
     RUNTIME_TEST       — тест LCD и кнопок через Modbus

   Режим фиксируется на этапе компиляции. Выбранное значение дублируется
   в регистр MB_ADDR_RUNTIME_MODE (R), чтобы мастер через Modbus мог
   увидеть, с какой прошивкой он разговаривает.
   ─────────────────────────────────────────────────────────────────────── */
#define RUNTIME_MODE   RUNTIME_PRODUCTION
/* #define RUNTIME_MODE   RUNTIME_TEST */

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

    /* Публикуем текущий режим в Modbus-регистр (R для мастера). */
    const RuntimeMode_t mode = (RuntimeMode_t)RUNTIME_MODE;
    MB_WriteBits(MB_ADDR_RUNTIME_MODE, (uint8_t)mode);

    /* Создаём задачи под выбранный режим и запускаем планировщик. */
    runtime_start(mode);
    vTaskStartScheduler();

    /* Сюда управление не должно попадать — vTaskStartScheduler() не
       возвращается при корректной настройке FreeRTOS. Спин — чтобы
       линкер не оптимизировал хвост main(), и на случай HardFault-логики. */
    for (;;) {}
}
