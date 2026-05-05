/*  settings.c — загрузка/сохранение EEPROM-зоны Modbus-таблицы.          */

#include "settings.h"
#include "modbus_table.h"
#include "eeprom.h"

#include <string.h>

/* ── Заводские дефолты ───────────────────────────────────────────────────── */
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

    /* Группа M — Тайминги кнопок */
    MB_WriteU16  (MB_ADDR_BTN_DEBOUNCE_MS,  30u);
    MB_WriteU16  (MB_ADDR_BTN_LONG_MS,      3000u);
    MB_WriteU16  (MB_ADDR_BTN_MID_MS,       800u);
}

void settings_set_defaults(void)
{
    apply_defaults_to_mb_table();
}

int settings_load(void)
{
    /* CRC проверяется по тому же объёму, который был записан (фикс старого
       бага: CRC считался по 510 байтам, а писалось ~90 — не сходилось,
       и при каждом старте откатывались дефолты).                         */
    if (eeprom_check_crc(MB_EEPROM_BYTE_SIZE) != 0) {
        apply_defaults_to_mb_table();
        MB_SetBit(MB_ADDR_SYS_EV, MB_SYS_EV_EE_ERR);
        MB_SetBit(MB_ADDR_HW_ER,  MB_HW_EE_ER);
        /* Запишем дефолты в EEPROM, чтобы следующий старт прошёл по чтению. */
        if (settings_save() == 0) {
            /* save отчистит EE_ERR при успехе — оставим как есть. */
        }
        return -1;
    }

    if (eeprom_read_regs(&mb_table[MB_EEPROM_BYTE_BASE],
                         MB_EEPROM_BYTE_SIZE) != 0) {
        apply_defaults_to_mb_table();
        MB_SetBit(MB_ADDR_SYS_EV, MB_SYS_EV_EE_ERR);
        MB_SetBit(MB_ADDR_HW_ER,  MB_HW_EE_ER);
        return -1;
    }

    MB_SetBit(MB_ADDR_SYS_EV, MB_SYS_EV_EE_RST);
    return 0;
}

int settings_save(void)
{
    int rc = eeprom_write_regs(&mb_table[MB_EEPROM_BYTE_BASE],
                               MB_EEPROM_BYTE_SIZE);
    if (rc == 0) {
        MB_SetBit(MB_ADDR_SYS_EV, MB_SYS_EV_EE_WST);
        MB_ClearBit(MB_ADDR_HW_ER, MB_HW_EE_ER);
    } else {
        MB_SetBit(MB_ADDR_SYS_EV, MB_SYS_EV_EE_ERR);
        MB_SetBit(MB_ADDR_HW_ER,  MB_HW_EE_ER);
    }
    return rc;
}
