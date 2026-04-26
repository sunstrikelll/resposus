#ifndef SETTINGS_H
#define SETTINGS_H

/*  settings — загрузка/сохранение EEPROM-зоны Modbus-таблицы.
 *
 *  Зона в mb_table: [MB_EEPROM_BYTE_BASE .. MB_EEPROM_BYTE_END) — 90 байт.
 *  В AT24C64 они лежат тем же байтовым порядком по смещению 0.
 *
 *  Последовательность boot:
 *      main() → eeprom_init() → settings_load()
 *      settings_load()  ── CRC OK  → читает EEPROM в mb_table
 *                      ── CRC BAD → пишет «заводские» дефолты в mb_table
 *                                   и в EEPROM (чтобы при след. старте CRC OK)
 *
 *  settings_save() — вызывается после любого изменения параметра через
 *  меню или по внешней команде. Пишет EEPROM-зону mb_table обратно в чип.
 */

#include <stdint.h>

/* Прочитать EEPROM в mb_table (EEPROM-зона). Если CRC плохой — записать
   дефолты и вернуть -1 (вызывающий может залогировать / индицировать).
   При успехе — 0.                                                          */
int  settings_load(void);

/* Сохранить текущую EEPROM-зону mb_table в AT24C64. Возвращает 0/-1.      */
int  settings_save(void);

/* Записать дефолты в mb_table (без обращения к EEPROM).
   Вызывается из settings_load() при CRC mismatch. Публично — для тестов.  */
void settings_set_defaults(void);

#endif /* SETTINGS_H */
