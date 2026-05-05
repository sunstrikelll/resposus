#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>

/* AT24C64 on I2C1 (PB6/PB7), A0=A1=A2=GND → 7-bit addr 0x50.
   Используем первые 512 байт чипа (реальный объём 8 KB).

   Раскладка:
     [0 .. 509]  — данные (EEPROM_DATA_SIZE байт)
     [510 .. 511] — CRC-16 (Modbus, полином 0xA001), little-endian           */

#define EEPROM_SIZE        512U   /* байт, используем в чипе   */
#define EEPROM_DATA_SIZE   510U   /* байт данных               */
#define EEPROM_CRC_OFFSET  510U   /* смещение CRC16 в EEPROM   */

/* Инициализация (вызывает i2c1_init внутри). */
void eeprom_init(void);

/* Записать data[0..len-1] в EEPROM + CRC16 в конце.
   len должен быть ≤ EEPROM_DATA_SIZE.
   Возвращает 0 при успехе, -1 при ошибке I2C.                              */
int  eeprom_write_regs(const uint8_t *data, uint16_t len);

/* Прочитать data[0..len-1] из EEPROM (начиная с адреса 0).
   len должен быть ≤ EEPROM_DATA_SIZE.
   Возвращает 0 при успехе, -1 при ошибке I2C.                              */
int  eeprom_read_regs(uint8_t *data, uint16_t len);

/* Прочитать данные + CRC, вычислить CRC16 по len байтам данных,
   сравнить с хранимым CRC в [510..511].
   ВАЖНО: len должен совпадать с тем, что был передан в eeprom_write_regs(),
   иначе CRC не сойдётся (это и был баг старой версии — fixed length
   EEPROM_DATA_SIZE, а писали меньше). Возвращает 0 если CRC совпадает,
   -1 при несовпадении или ошибке I2C.                                    */
int  eeprom_check_crc(uint16_t len);

#endif /* EEPROM_H */
