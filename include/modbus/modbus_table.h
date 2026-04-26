#ifndef MODBUS_TABLE_H
#define MODBUS_TABLE_H

#include <stdint.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════════
   МКВП-02: карта Modbus-регистров
   ══════════════════════════════════════════════════════════════════════════
   Пространство регистров РАЗДЕЛЕНО на две зоны:

     • Runtime  (регистры 0 … 121)   — RAM, содержимое теряется при сбросе.
     • EEPROM   (регистры 200 … 244) — энергонезависимо, загружается при
                                       старте, сохраняется при изменении
                                       пользователем.

   Между регистрами 122 … 199 — «мёртвая» зона (чтение = 0).

   В каждой группе оставлено 2 свободных регистра (<spare>) — при необходимости
   добавить новое поле, использовать их, НЕ сдвигая существующую карту.

   Все адреса в таблице ниже — Modbus-регистровые (десятичн.).
   Байтовое смещение внутри mb_table = 2·reg (для 1-байтовых полей — 2·reg+1,
   т. к. Modbus читает 16-битный регистр big-endian и u8 живёт в low-byte).

   ┌───────────────────────── Runtime (RAM) ──────────────────────────────┐

   Группа A — Зеркало LCD (R для мастера)
     0   DISPLAY_LINE_0     string 22 байта (11 регистров)
     11  DISPLAY_LINE_1     string 22 байта
     22  DISPLAY_LINE_2     string 22 байта
     33  DISPLAY_LINE_3     string 22 байта
     44-45                  <spare>

   Группа B — Тестовые строки (RW для мастера, выводятся на LCD в TEST)
     46  TEST_LINE_0        string 22 байта
     57  TEST_LINE_1        string 22 байта
     68  TEST_LINE_2        string 22 байта
     79  TEST_LINE_3        string 22 байта
     90  USR_TEXT           string 22 байта (произвольный текст мастера)
     101-102                <spare>

   Группа C — Статус и команды FSM
     103 MODE_SR            u8  R   биты статуса (POWER/WORK/ALARM/…)
     104 MODE_CR            u8  RW  команда (1=PwrOn,2=Stby,…,7=AckAlm)
     105 ALARM_FLAGS        u8  R   биты аварий
     106 LED_STATE          u8  R   зеркало 4 LED
     107 RUNTIME_MODE       u8  R   0=PRODUCTION, 1=TEST
     108-109                <spare>

   Группа D — Кнопки
     110 BTN_EVENT          u8  R   последнее debounced-событие
     111 BTN_CMD            u8  RW  виртуальная кнопка (auto-clear)
     112 MENU_GOTO          u8  RW  прыжок в состояние меню (auto-clear)
     113-114                <spare>

   Группа E — Живые значения процесса (R)
     115 FLOW               float   текущий поток, м/с
     117 EXT_TEMP           float   внешняя температура, °С
     119 MANUAL_OUT         u8      выход ручного режима, 0-100 %
     120 OUTPUT_STATE       u8      дискретные выходы: биты LAMP/SOCKET
     121 BUZZER_STATE       u8      зуммер тревоги: 0=OFF, 1=ON (R)
                                    Цикл §8: 10 с / 2 с при активной тревоге;
                                    PRG-short (Mute) глушит до новой тревоги.

   └──────────────────────────────────────────────────────────────────────┘

   ┌─────────────────────── EEPROM (persist) ─────────────────────────────┐

   Группа F — Уставки потока
     200 SETPOINT           float   уставка потока, м/с
     202 FLOW_SP_R          float   уставка повтора, м/с
     204-205                <spare>

   Группа G — Пороги аварии
     206 ALARM_LOW          float   порог «мало потока», м/с
     208 ALARM_LOW_R        float   порог сброса аварии, м/с
     210 ALARM_DELAY        u8      задержка аварии, 0-180 с
     211-212                <spare>

   Группа H — Режим ручного
     213 MANUAL_MEM         u8      0=НОРМ, 1=ПАМЯТЬ
     214-215                <spare>

   Группа I — Калибровка датчика и выхода
     216 SENSOR_ZERO        float   калибровка нуля датчика, м/с
     218 SENSOR_SPAN        float   калибровка макс. датчика, м/с
     220 OUT_ZERO_PCT       u8      калибровка нуля аналог. выхода, %
     221 OUT_SPAN_PCT       u8      калибровка макс. выхода, %
     222-223                <spare>

   Группа J — ПИД-регулятор
     224 PID_TI             float   постоянная интегрирования, с
     226 PID_BAND           float   пропорциональная полоса, см/с
     228-229                <spare>

   Группа K — Обслуживание
     230 MAINT_HOURS        float   счётчик моточасов, ч
     232 COUNT_MAX          float   лимит моточасов (порог ТО), ч
     234-235                <spare>

   Группа L — Опции
     236 BLACKOUT_EN        u8      0=OFF, 1=ON
     237 DATALOG_EN         u8      0=OFF, 1=ON
     238 DATALOG_SEC        u8      период datalogger, с
     239-240                <spare>

   Группа M — Тайминги кнопок
     241 BTN_DEBOUNCE_MS    u16     длительность debounce, мс (default 30)
     242 BTN_LONG_MS        u16     порог «длинного» нажатия, мс (def 1500)
     243-244                <spare>

   └──────────────────────────────────────────────────────────────────────┘

   КОНЕЦ КАРТЫ: reg 244, byte 0x1E9.                                      */

#define MB_TABLE_SIZE           0x1EA   /* 490 байт = 245 регистров */

/* ── Хелперы перевода «регистр → байтовое смещение» ──────────────────────
   Modbus-регистр = 16 бит, в mb_table хранится big-endian (MSB, LSB).
     MB_REG(n)     — offset для многобайтных полей (float/u16/строки).
     MB_REG_U8(n)  — offset для 1-байтного поля (u8 живёт в low-byte).    */
#define MB_REG(n)        ((uint16_t)((n) * 2u))
#define MB_REG_U8(n)     ((uint16_t)((n) * 2u + 1u))

/* ═══════════════════════ Группа A — Дисплей ═════════════════════════════ */
#define MB_ADDR_DISPLAY_LINE_0   MB_REG(0)    /* reg 0,  byte 0x000 */
#define MB_ADDR_DISPLAY_LINE_1   MB_REG(11)   /* reg 11, byte 0x016 */
#define MB_ADDR_DISPLAY_LINE_2   MB_REG(22)   /* reg 22, byte 0x02C */
#define MB_ADDR_DISPLAY_LINE_3   MB_REG(33)   /* reg 33, byte 0x042 */

/* ═══════════════════════ Группа B — Тест-строки ═════════════════════════ */
#define MB_ADDR_TEST_LINE_0      MB_REG(46)   /* reg 46, byte 0x05C */
#define MB_ADDR_TEST_LINE_1      MB_REG(57)   /* reg 57, byte 0x072 */
#define MB_ADDR_TEST_LINE_2      MB_REG(68)   /* reg 68, byte 0x088 */
#define MB_ADDR_TEST_LINE_3      MB_REG(79)   /* reg 79, byte 0x09E */
#define MB_ADDR_USR_TEXT         MB_REG(90)   /* reg 90, byte 0x0B4 */

#define MB_STRING_LEN            22u

/* ═══════════════════════ Группа C — FSM статус/команды ══════════════════ */
#define MB_ADDR_MODE_SR          MB_REG_U8(103)   /* byte 0x0CF */
#define MB_ADDR_MODE_CR          MB_REG_U8(104)   /* byte 0x0D1 */
#define MB_ADDR_ALARM_FLAGS      MB_REG_U8(105)   /* byte 0x0D3 */
#define MB_ADDR_LED_STATE        MB_REG_U8(106)   /* byte 0x0D5 */
#define MB_ADDR_RUNTIME_MODE     MB_REG_U8(107)   /* byte 0x0D7 */

/* ═══════════════════════ Группа D — Кнопки ══════════════════════════════ */
#define MB_ADDR_BTN_EVENT        MB_REG_U8(110)   /* byte 0x0DD */
#define MB_ADDR_BTN_CMD          MB_REG_U8(111)   /* byte 0x0DF */
#define MB_ADDR_MENU_GOTO        MB_REG_U8(112)   /* byte 0x0E1 */

/* ═══════════════════════ Группа E — Живые значения процесса ═════════════ */
#define MB_ADDR_FLOW             MB_REG(115)      /* byte 0x0E6, float */
#define MB_ADDR_EXT_TEMP         MB_REG(117)      /* byte 0x0EA, float */
#define MB_ADDR_MANUAL_OUT       MB_REG_U8(119)   /* byte 0x0EF, u8    */
#define MB_ADDR_OUTPUT_STATE     MB_REG_U8(120)   /* byte 0x0F1, u8    */
#define MB_ADDR_BUZZER_STATE     MB_REG_U8(121)   /* byte 0x0F3, u8    */

/* buzzer_state значения */
#define MB_BUZZER_OFF            0x00u
#define MB_BUZZER_ON             0x01u

/* ═══════════════════════ Граница EEPROM-зоны ════════════════════════════ */
#define MB_EEPROM_REG_BASE       200u
#define MB_EEPROM_BYTE_BASE      MB_REG(MB_EEPROM_REG_BASE)   /* 0x190 */
#define MB_EEPROM_BYTE_END       MB_REG(245u)                 /* 0x1EA */
#define MB_EEPROM_BYTE_SIZE      (MB_EEPROM_BYTE_END - MB_EEPROM_BYTE_BASE)
                                                     /* 90 байт = 45 регистров */

/* ═══════════════════════ Группа F — Уставки потока ══════════════════════ */
#define MB_ADDR_SETPOINT         MB_REG(200)      /* float */
#define MB_ADDR_FLOW_SP_R        MB_REG(202)      /* float */

/* ═══════════════════════ Группа G — Пороги аварии ═══════════════════════ */
#define MB_ADDR_ALARM_LOW        MB_REG(206)      /* float */
#define MB_ADDR_ALARM_LOW_R      MB_REG(208)      /* float */
#define MB_ADDR_ALARM_DELAY      MB_REG_U8(210)   /* u8    */

/* ═══════════════════════ Группа H — Режим ручного ═══════════════════════ */
#define MB_ADDR_MANUAL_MEM       MB_REG_U8(213)   /* u8 */

/* ═══════════════════════ Группа I — Калибровка ══════════════════════════ */
#define MB_ADDR_SENSOR_ZERO      MB_REG(216)      /* float */
#define MB_ADDR_SENSOR_SPAN      MB_REG(218)      /* float */
#define MB_ADDR_OUT_ZERO_PCT     MB_REG_U8(220)   /* u8    */
#define MB_ADDR_OUT_SPAN_PCT     MB_REG_U8(221)   /* u8    */

/* ═══════════════════════ Группа J — ПИД ═════════════════════════════════ */
#define MB_ADDR_PID_TI           MB_REG(224)      /* float */
#define MB_ADDR_PID_BAND         MB_REG(226)      /* float */

/* ═══════════════════════ Группа K — Обслуживание ════════════════════════ */
#define MB_ADDR_MAINT_HOURS      MB_REG(230)      /* float */
#define MB_ADDR_COUNT_MAX        MB_REG(232)      /* float */

/* ═══════════════════════ Группа L — Опции ═══════════════════════════════ */
#define MB_ADDR_BLACKOUT_EN      MB_REG_U8(236)   /* u8 */
#define MB_ADDR_DATALOG_EN       MB_REG_U8(237)   /* u8 */
#define MB_ADDR_DATALOG_SEC      MB_REG_U8(238)   /* u8 */

/* ═══════════════════════ Группа M — Тайминги кнопок ═════════════════════ */
#define MB_ADDR_BTN_DEBOUNCE_MS  MB_REG(241)      /* u16 */
#define MB_ADDR_BTN_LONG_MS      MB_REG(242)      /* u16 */

/* ══════════════════════════════════════════════════════════════════════════
   Константы битов и команд (значения не изменились по сравнению с прошлой
   версией карты — меняется только расположение регистров)                 */

/* runtime_mode */
#define MB_RUNTIME_PRODUCTION   0x00u
#define MB_RUNTIME_TEST         0x01u

/* mode_sr биты */
#define MB_BIT_MODE_POWER       (1u << 0)
#define MB_BIT_MODE_WORK        (1u << 1)
#define MB_BIT_MODE_ALARM       (1u << 2)
#define MB_BIT_MODE_MANUAL      (1u << 3)
#define MB_BIT_MODE_STANDBY     (1u << 4)
#define MB_BIT_MODE_REPEAT      (1u << 5)
#define MB_BIT_MODE_NIGHT       (1u << 6)  /* Ночной/пониженный режим       */
#define MB_BIT_MODE_EMERGENCY   (1u << 7)  /* Аварийный режим (кнопка E)    */

/* output_state биты (дискретные выходы, управляются кнопками Lamp/RB) */
#define MB_OUT_LAMP             (1u << 0)  /* реле освещения                */
#define MB_OUT_SOCKET           (1u << 1)  /* реле розеток (Выбор/RB)       */

/* mode_cr команды */
#define MB_CMD_NONE             0x00u
#define MB_CMD_POWER_ON         0x01u
#define MB_CMD_STANDBY          0x02u
#define MB_CMD_START            0x03u
#define MB_CMD_STOP             0x04u
#define MB_CMD_SET_AUTO         0x05u
#define MB_CMD_SET_MANUAL       0x06u
#define MB_CMD_ACK_ALARM        0x07u
#define MB_CMD_NIGHT_TOGGLE     0x08u  /* переключить Ночной режим          */
#define MB_CMD_LAMP_TOGGLE      0x09u  /* переключить реле лампы            */
#define MB_CMD_SOCKET_TOGGLE    0x0Au  /* переключить реле розеток          */
#define MB_CMD_EMERGENCY_TOGGLE 0x0Bu  /* переключить аварийный режим       */
#define MB_CMD_BUZZER_MUTE      0x0Cu  /* заглушить зуммер до нов. тревоги  */

/* alarm_flags биты */
#define MB_ALARM_FLOW_LOW       (1u << 0)
#define MB_ALARM_INVERTER       (1u << 1)
#define MB_ALARM_DOOR_OPEN      (1u << 2)

/* led_state биты */
#define MB_LED_POWER            (1u << 0)
#define MB_LED_WORK             (1u << 1)
#define MB_LED_ALARM            (1u << 2)
#define MB_LED_MANUAL           (1u << 3)

/* manual_mem значения */
#define MB_MANUAL_NOR           0x00u
#define MB_MANUAL_MEM           0x01u

/* ══════════════════════════════════════════════════════════════════════════
   Таблица
   ══════════════════════════════════════════════════════════════════════════ */
extern uint8_t mb_table[MB_TABLE_SIZE];

/* ══════════════════════════════════════════════════════════════════════════
   Функции доступа
   ══════════════════════════════════════════════════════════════════════════ */

static inline uint8_t MB_ReadBits(uint16_t addr)
{
    return mb_table[addr];
}

static inline void MB_WriteBits(uint16_t addr, uint8_t val)
{
    mb_table[addr] = val;
}

static inline void MB_SetBit(uint16_t addr, uint8_t bit)
{
    mb_table[addr] |= bit;
}

static inline void MB_ClearBit(uint16_t addr, uint8_t bit)
{
    mb_table[addr] &= (uint8_t)~bit;
}

/* uint16 в одном регистре (big-endian, Modbus-стандарт). */
static inline uint16_t MB_ReadU16(uint16_t addr)
{
    return (uint16_t)(((uint16_t)mb_table[addr] << 8) | (uint16_t)mb_table[addr + 1]);
}

static inline void MB_WriteU16(uint16_t addr, uint16_t val)
{
    mb_table[addr]     = (uint8_t)(val >> 8);
    mb_table[addr + 1] = (uint8_t)(val & 0xFFu);
}

/* uint32/float — порядок CDAB (ModbusUtility CDAB-режим):
   байты [CC DD AA BB] ↔ значение 0xAABBCCDD                              */
static inline uint32_t MB_ReadUint32(uint16_t addr)
{
    return ((uint32_t)mb_table[addr + 2] << 24) |
           ((uint32_t)mb_table[addr + 3] << 16) |
           ((uint32_t)mb_table[addr]     <<  8) |
            (uint32_t)mb_table[addr + 1];
}

static inline void MB_WriteUint32(uint16_t addr, uint32_t val)
{
    mb_table[addr]     = (uint8_t)((val >>  8) & 0xFFu);
    mb_table[addr + 1] = (uint8_t)(val & 0xFFu);
    mb_table[addr + 2] = (uint8_t)(val >> 24);
    mb_table[addr + 3] = (uint8_t)((val >> 16) & 0xFFu);
}

static inline float MB_ReadFloat(uint16_t addr)
{
    uint32_t bits = MB_ReadUint32(addr);
    float val;
    memcpy(&val, &bits, sizeof(val));
    return val;
}

static inline void MB_WriteFloat(uint16_t addr, float val)
{
    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));
    MB_WriteUint32(addr, bits);
}

/* ── Строковые операции ──────────────────────────────────────────────────
   Подробности (swap пар, критическая секция) — см. modbus_table.c.        */
void MB_ReadString (uint16_t addr, char *out, uint8_t max_len);
void MB_WriteString(uint16_t addr, const char *str);

#endif /* MODBUS_TABLE_H */
