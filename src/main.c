/*  main.c — точка входа
 *
 *  Последовательность:
 *    1) Запоминаем причину последнего сброса (FWDGT?) до перезаписи флагов.
 *    2) Инициализация периферии (NVIC, TIMER3, USB CDC, Modbus, EEPROM).
 *    3) Старт сторожевого таймера (FWDGT).
 *    4) settings_load() — загрузка EEPROM-зоны mb_table.
 *    5) Публикация runtime-режима в MB_ADDR_SYS_MODE.
 *    6) runtime_start(mode) → vTaskStartScheduler().
 */

#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tim3_ms.h"
#include "usb_cdc.h"
#include "modbus.h"
#include "modbus_table.h"
#include "eeprom.h"
#include "settings.h"
#include "runtime.h"
#include "watchdog.h"
#include "buttons.h"

/* ══════════════════════════════════════════════════════════════════════════
   ВЫБОР РЕЖИМА РАНТАЙМА
   ══════════════════════════════════════════════════════════════════════════ */
#define RUNTIME_MODE   RUNTIME_PRODUCTION
/* #define RUNTIME_MODE   RUNTIME_TEST */

/* Таймаут сторожа: при 6.4 мс/тик и максимуме 4095 тиков = ~26 с.
   Берём 2 с — достаточно с учётом 10-мс цикла task_ui (200× запас).      */
#define WDT_TIMEOUT_MS  2000u

int main(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    /* (1) Сохраняем причину сброса ДО любых других действий с RCU.
       Если последний сброс был по сторожу — пометим бит WDTF в SYS_EV.   */
    uint8_t wdt_reset = wdt_was_reset_cause();

    tim_Init();
    usb_cdc_init();
    modbus_init();
    eeprom_init();

    /* (3) Запускаем FWDGT сразу — даже если что-то ниже зависнет, ресет
           гарантирован.                                                    */
    wdt_init(WDT_TIMEOUT_MS);

    /* После modbus_init() таблица mb_table очищена. Публикуем флаги. */
    if (wdt_reset) {
        MB_SetBit(MB_ADDR_SYS_EV, MB_SYS_EV_WDTF);
        MB_SetBit(MB_ADDR_HW_ER,  MB_HW_FWDT_ER);
    }

    /* passport_v1.5.md §11: factory-reset — старт с удержанием A+B+E. */
    if (btn_factory_reset_combo_held()) {
        settings_set_defaults();
        (void)settings_save();
    } else {
        (void)settings_load();
    }

    /* Публикуем рантайм-режим в Modbus-регистр. */
    const RuntimeMode_t mode = (RuntimeMode_t)RUNTIME_MODE;
    MB_WriteBits(MB_ADDR_SYS_MODE, (uint8_t)mode);

    runtime_start(mode);
    vTaskStartScheduler();

    for (;;) { wdt_kick(); }
}
