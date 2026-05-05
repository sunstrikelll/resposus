/*  watchdog.c — драйвер FWDGT (free watchdog timer) для GD32F10x
 *
 *  IRC40K → /256 → 156.25 Hz (~6.4 мс/тик).
 *  Reload до 0xFFF (4095) → максимально ~26.2 с.
 *
 *  Защита от зависания: если основной цикл UI/Modbus не вызывает
 *  wdt_kick() в течение timeout_ms — контроллер аппаратно ресетится.
 */

#include "watchdog.h"
#include "gd32f10x.h"
#include "gd32f10x_fwdgt.h"
#include "gd32f10x_rcu.h"

void wdt_init(uint32_t timeout_ms)
{
    /* Запускаем встроенный 40-кГц RC-генератор для FWDGT. */
    rcu_osci_on(RCU_IRC40K);
    while (rcu_flag_get(RCU_FLAG_IRC40KSTB) == RESET) { /* wait stable */ }

    /* Вычисляем reload-значение для нужного таймаута:
         tick_period = prescaler / 40000 секунд
         reload      = timeout_ms / 1000 / tick_period
                     = timeout_ms * 40 / prescaler                          */
    const uint16_t prescaler = 256u;       /* /256 — самый медленный */
    uint32_t reload = (timeout_ms * 40u) / prescaler;
    if (reload < 1u)    reload = 1u;
    if (reload > 0xFFFu) reload = 0xFFFu;

    /* Разрешаем запись регистров FWDGT и конфигурируем. */
    fwdgt_write_enable();
    fwdgt_prescaler_value_config(FWDGT_PSC_DIV256);
    fwdgt_reload_value_config((uint16_t)reload);
    fwdgt_counter_reload();

    /* Старт сторожа.  После этого выключить уже нельзя до следующего сброса. */
    fwdgt_enable();
}

void wdt_kick(void)
{
    fwdgt_counter_reload();
}

uint8_t wdt_was_reset_cause(void)
{
    uint8_t was_wdt = (rcu_flag_get(RCU_FLAG_FWDGTRST) == SET) ? 1u : 0u;
    /* Очистить все флаги источников сброса, иначе они останутся до
       следующего полного сброса по питанию.                                 */
    rcu_all_reset_flag_clear();
    return was_wdt;
}
