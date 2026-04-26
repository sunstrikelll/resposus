/*  settings.c — загрузка/сохранение EEPROM-зоны Modbus-таблицы.          */

#include "settings.h"
#include "modbus_table.h"
#include "eeprom.h"

#include <string.h>

/* ── Заводские дефолты ────────────────────────────────────────────────────
   Применяются при несовпадении CRC EEPROM (первое включение, повреждение
   микросхемы, прошивка с изменённой раскладкой).                         */
static void apply_defaults_to_mb_table(void)
{
    /* Группа F — Уставки потока */
    MB_WriteFloat(MB_ADDR_SETPOINT,         0.50f);
    MB_WriteFloat(MB_ADDR_FLOW_SP_R,        0.30f);

    /* Группа G — Пороги аварии */
    MB_WriteFloat(MB_ADDR_ALARM_LOW,        0.20f);
    MB_WriteFloat(MB_ADDR_ALARM_LOW_R,      0.25f);
    MB_WriteBits (MB_ADDR_ALARM_DELAY,      5u);

    /* Группа H — Режим ручного */
    MB_WriteBits (MB_ADDR_MANUAL_MEM,       MB_MANUAL_NOR);

    /* Группа I — Калибровка */
    MB_WriteFloat(MB_ADDR_SENSOR_ZERO,      0.00f);
    MB_WriteFloat(MB_ADDR_SENSOR_SPAN,      5.00f);
    MB_WriteBits (MB_ADDR_OUT_ZERO_PCT,     0u);
    MB_WriteBits (MB_ADDR_OUT_SPAN_PCT,     100u);

    /* Группа J — ПИД */
    MB_WriteFloat(MB_ADDR_PID_TI,           2.7f);
    MB_WriteFloat(MB_ADDR_PID_BAND,         27.0f);

    /* Группа K — Обслуживание */
    MB_WriteFloat(MB_ADDR_MAINT_HOURS,      0.0f);
    MB_WriteFloat(MB_ADDR_COUNT_MAX,        9999.0f);

    /* Группа L — Опции */
    MB_WriteBits (MB_ADDR_BLACKOUT_EN,      1u);
    MB_WriteBits (MB_ADDR_DATALOG_EN,       0u);
    MB_WriteBits (MB_ADDR_DATALOG_SEC,      60u);

    /* Группа M — Тайминги кнопок (мс).
       BTN_LONG_MS = 3000: по документации «Включение и работа прибора»
       удержание кнопки ВКЛ/ВЫКЛ ≥ 3 с переводит прибор в Ночной режим.
       Один и тот же порог применяется ко всем кнопкам с длинным
       событием (PRG, ONOFF, LAMP, E) — настраивается через Modbus.    */
    MB_WriteU16  (MB_ADDR_BTN_DEBOUNCE_MS,  30u);
    MB_WriteU16  (MB_ADDR_BTN_LONG_MS,      3000u);
}

void settings_set_defaults(void)
{
    apply_defaults_to_mb_table();
}

int settings_load(void)
{
    /* Проверить CRC в EEPROM. При сбое — записать дефолты. */
    if (eeprom_check_crc() != 0) {
        apply_defaults_to_mb_table();
        /* Пытаемся зафиксировать дефолты в EEPROM. Если I2C мёртв —
           вернём -1, но прошивка продолжит работать с RAM-дефолтами. */
        return (settings_save() == 0) ? -1 : -1;
    }

    /* CRC OK — читаем 90 байт EEPROM прямо в EEPROM-зону mb_table. */
    if (eeprom_read_regs(&mb_table[MB_EEPROM_BYTE_BASE],
                         MB_EEPROM_BYTE_SIZE) != 0) {
        /* Физическая ошибка I2C — fallback на дефолты без записи. */
        apply_defaults_to_mb_table();
        return -1;
    }
    return 0;
}

int settings_save(void)
{
    /* Пишем ровно EEPROM-зону mb_table. eeprom_write_regs сам считает CRC
       по переданному буферу и кладёт его в хвост AT24C64 (поэтому у нас
       единый CRC на ВСЕ настройки — достаточно для фиксации целостности). */
    return eeprom_write_regs(&mb_table[MB_EEPROM_BYTE_BASE],
                             MB_EEPROM_BYTE_SIZE);
}
