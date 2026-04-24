#ifndef MODBUS_TABLE_H
#define MODBUS_TABLE_H

#include <stdint.h>
#include <string.h>

/* ── Размер таблицы ─────────────────────────────────────────────────────────
   352 байта = 176 Modbus-регистров (16-бит каждый).

   Адресная карта (байтовое смещение в mb_table):
   ┌──────────────────────────┬──────────┬────────────┬──────────────────────────────────────┐
   │ Поле                     │ MB-адрес │ Байт       │ Примечание                           │
   ├──────────────────────────┼──────────┼────────────┼──────────────────────────────────────┤
   │ bit_sr                   │ 0x0001   │ 0x03       │ R   bitfield: LED(0) TIMER(1)        │
   │ bit_cr                   │ 0x0003   │ 0x07       │ W   RESET_TIMER(0)                   │
   │ timer[3..0]              │ 0x0004   │ 0x08-0x0B  │ R   uint32 big-endian (секунды)      │
   │ version[3..0]            │ 0x0008   │ 0x10-0x13  │ RW  float  big-endian                │
   │ display_line_0[0..21]    │ 0x000C   │ 0x18-0x2D  │ R   string 22 байта                  │
   │ display_line_1[0..21]    │ 0x0022   │ 0x44-0x59  │ R   string 22 байта                  │
   │ display_line_2[0..21]    │ 0x0038   │ 0x70-0x85  │ R   string 22 байта                  │
   │ display_line_3[0..21]    │ 0x004E   │ 0x9C-0xB1  │ R   string 22 байта                  │
   │ usr_text[0..21]          │ 0x0064   │ 0xC8-0xDD  │ RW  string 22 байта                  │
   ├──────────────────────────┼──────────┼────────────┼──────────────────────────────────────┤
   │ mode_sr                  │ 0x006F   │ 0xDF       │ R   режим устройства (биты ниже)     │
   │ mode_cr                  │ 0x0070   │ 0xE1       │ RW  команда управления               │
   │ manual_out               │ 0x0071   │ 0xE3       │ RW  выход ручного режима 0-100%      │
   │ setpoint[3..0]           │ 0x0072   │ 0xE4-0xE7  │ RW  уставка потока м/с (float)       │
   │ flow[3..0]               │ 0x0074   │ 0xE8-0xEB  │ R   текущий поток м/с (float)        │
   │ alarm_flags              │ 0x0076   │ 0xED       │ R   биты аварий (см. ниже)           │
   │ led_state                │ 0x0077   │ 0xEF       │ R   состояние светодиодов (зеркало)  │
   │ btn_event                │ 0x0078   │ 0xF1       │ R   последнее событие кнопки         │
   ├──────────────────────────┼──────────┼────────────┼──────────────────────────────────────┤
   │ runtime_mode             │ 0x0079   │ 0xF3       │ R   0=PRODUCTION, 1=TEST             │
   │ test_line_0[0..21]       │ 0x007A   │ 0xF4-0x109 │ RW  строка 22 байта                  │
   │ test_line_1[0..21]       │ 0x0085   │ 0x10A-0x11F│ RW  строка 22 байта                  │
   │ test_line_2[0..21]       │ 0x0090   │ 0x120-0x135│ RW  строка 22 байта                  │
   │ test_line_3[0..21]       │ 0x009B   │ 0x136-0x14B│ RW  строка 22 байта                  │
   ├──────────────────────────┼──────────┼────────────┼──────────────────────────────────────┤
   │ ext_temp[3..0]           │ 0x00A6   │ 0x14C-0x14F│ R   внешняя температура °С (float)   │
   │ flow_sp_r[3..0]          │ 0x00A8   │ 0x150-0x153│ RW  уставка повтора м/с (float)      │
   │ alarm_low[3..0]          │ 0x00AA   │ 0x154-0x157│ RW  порог аварии «мало потока» м/с   │
   │ alarm_low_r[3..0]        │ 0x00AC   │ 0x158-0x15B│ RW  порог сброса аварии м/с (float)  │
   │ alarm_delay              │ 0x00AE   │ 0x15D      │ RW  задержка аварии 0-180 с (uint8)  │
   │ manual_mem               │ 0x00AF   │ 0x15F      │ RW  0=НОРМ (NOR), 1=ПАМЯТЬ (MEM)     │
   ├──────────────────────────┼──────────┼────────────┼──────────────────────────────────────┤
   │ sensor_zero[3..0]        │ 0x00B0   │ 0x160-0x163│ RW  float, калибровка 0 датчика, м/с │
   │ sensor_span[3..0]        │ 0x00B2   │ 0x164-0x167│ RW  float, макс. датчика, м/с        │
   │ pid_ti[3..0]             │ 0x00B4   │ 0x168-0x16B│ RW  float, ПИД TI 0-9.9 с            │
   │ pid_band[3..0]           │ 0x00B6   │ 0x16C-0x16F│ RW  float, ПИД Band 1-999 см/с       │
   │ maint_hours[3..0]        │ 0x00B8   │ 0x170-0x173│ RW  float, счётчик ТО, ч             │
   │ count_max[3..0]          │ 0x00BA   │ 0x174-0x177│ RW  float, лимит ТО, ч               │
   │ out_zero_pct             │ 0x00BC   │ 0x179      │ RW  uint8, калибровка 0 выхода, %    │
   │ out_span_pct             │ 0x00BD   │ 0x17B      │ RW  uint8, калибровка макс. выхода, %│
   │ blackout_en              │ 0x00BE   │ 0x17D      │ RW  uint8, 0=OFF, 1=ON               │
   │ datalog_en               │ 0x00BF   │ 0x17F      │ RW  uint8, 0=OFF, 1=ON               │
   │ datalog_sec              │ 0x00C0   │ 0x181      │ RW  uint8, период datalogger, с      │
   │ btn_cmd                  │ 0x00C1   │ 0x183      │ RW  uint8, виртуальная кнопка        │
   └──────────────────────────┴──────────┴────────────┴──────────────────────────────────────┘ */

#define MB_TABLE_SIZE           0x186   /* 390 байт = 195 регистров */

/* ── Байтовые смещения ──────────────────────────────────────────────────── */
#define MB_ADDR_BIT_SR          0x03
#define MB_ADDR_BIT_CR          0x07
#define MB_ADDR_TIMER           0x08    /* R   uint32, секунды с запуска        */
#define MB_ADDR_VERSION         0x10    /* RW  float                            */
#define MB_ADDR_DISPLAY_LINE_0  0x18    /* R   string 22 байта                  */
#define MB_ADDR_DISPLAY_LINE_1  0x44
#define MB_ADDR_DISPLAY_LINE_2  0x70
#define MB_ADDR_DISPLAY_LINE_3  0x9C
#define MB_ADDR_USR_TEXT        0xC8    /* RW  string 22 байта                  */

#define MB_STRING_LEN           22

/* ── HMI-регистры ───────────────────────────────────────────────────────── */
#define MB_ADDR_MODE_SR         0xDF    /* R   битовый статус устройства        */
#define MB_ADDR_MODE_CR         0xE1    /* RW  команда (очищается БУСШ после)   */
#define MB_ADDR_MANUAL_OUT      0xE3    /* RW  выход ручного режима 0-100 %     */
#define MB_ADDR_SETPOINT        0xE4    /* RW  уставка потока, м/с (float)      */
#define MB_ADDR_FLOW            0xE8    /* R   текущий поток, м/с (float)       */
#define MB_ADDR_ALARM_FLAGS     0xED    /* R   флаги аварий                     */
#define MB_ADDR_LED_STATE       0xEF    /* R   зеркало светодиодов              */
#define MB_ADDR_BTN_EVENT       0xF1    /* R   последнее событие кнопки         */

/* ── Регистры рантайма и теста ──────────────────────────────────────────── */
#define MB_ADDR_RUNTIME_MODE    0xF3    /* R   0=PRODUCTION, 1=TEST             */
#define MB_ADDR_TEST_LINE_0     0xF4    /* RW  string 22 байта                  */
#define MB_ADDR_TEST_LINE_1     0x10A
#define MB_ADDR_TEST_LINE_2     0x120
#define MB_ADDR_TEST_LINE_3     0x136

/* ── Параметры МКВП-02 ──────────────────────────────────────────────────── */
#define MB_ADDR_EXT_TEMP        0x14C   /* R   внешняя температура °С (float)   */
#define MB_ADDR_FLOW_SP_R       0x150   /* RW  уставка повтора м/с (float)      */
#define MB_ADDR_ALARM_LOW       0x154   /* RW  порог «мало потока» м/с (float)  */
#define MB_ADDR_ALARM_LOW_R     0x158   /* RW  порог сброса аварии м/с (float)  */
#define MB_ADDR_ALARM_DELAY     0x15D   /* RW  задержка аварии 0-180 с (uint8)  */
#define MB_ADDR_MANUAL_MEM      0x15F   /* RW  0=НОРМ, 1=ПАМЯТЬ (uint8)         */

/* ── Параметры PASS2 (калибровка и ПИД) ────────────────────────────────── */
#define MB_ADDR_SENSOR_ZERO     0x160   /* RW  float, калибровка 0 датчика, м/с */
#define MB_ADDR_SENSOR_SPAN     0x164   /* RW  float, макс. датчика, м/с        */
#define MB_ADDR_PID_TI          0x168   /* RW  float, ПИД TI 0.0-9.9 с          */
#define MB_ADDR_PID_BAND        0x16C   /* RW  float, ПИД Band 1-999 см/с       */
#define MB_ADDR_OUT_ZERO_PCT    0x179   /* RW  uint8, калибр. 0 аналог.выхода,% */
#define MB_ADDR_OUT_SPAN_PCT    0x17B   /* RW  uint8, калибр. макс. выхода, %   */
#define MB_ADDR_BLACKOUT_EN     0x17D   /* RW  uint8, 0=OFF, 1=ON               */

/* ── Параметры PASS3 (обслуживание) ───────────────────────────────────── */
#define MB_ADDR_MAINT_HOURS     0x170   /* RW  float, счётчик моточасов, ч      */

/* ── Параметры PASS4 (счётчики и datalogger) ──────────────────────────── */
#define MB_ADDR_COUNT_MAX       0x174   /* RW  float, макс. значение счётчика   */
#define MB_ADDR_DATALOG_EN      0x17F   /* RW  uint8, 0=OFF, 1=ON               */
#define MB_ADDR_DATALOG_SEC     0x181   /* RW  uint8, период datalogger, с      */

/* ── Виртуальная кнопка ──────────────────────────────────────────────────
   Мастер пишет код события (см. BtnEvent_t в buttons.h): напр. 0x01=PRG,
   0x81=PRG_LONG, 0x06=MUTE, 0x86=MUTE_LONG. Прошивка читает регистр в
   btn_scan_with_cmd() → возвращает событие в FSM → обнуляет регистр
   (auto-clear). Так можно тестировать логику без физических кнопок.     */
#define MB_ADDR_BTN_CMD         0x183   /* RW  uint8, виртуальная кнопка       */

/* ── runtime_mode ───────────────────────────────────────────────────────── */
#define MB_RUNTIME_PRODUCTION   0x00u
#define MB_RUNTIME_TEST         0x01u

/* ── mode_sr биты ───────────────────────────────────────────────────────── */
#define MB_BIT_MODE_POWER       (1u << 0)  /* питание включено, устройство активно */
#define MB_BIT_MODE_WORK        (1u << 1)  /* рабочий процесс (вентилятор) запущен */
#define MB_BIT_MODE_ALARM       (1u << 2)  /* активна авария                       */
#define MB_BIT_MODE_MANUAL      (1u << 3)  /* ручной режим (0 = авто)              */
#define MB_BIT_MODE_STANDBY     (1u << 4)  /* ожидание                             */
#define MB_BIT_MODE_REPEAT      (1u << 5)  /* активна уставка повтора (AUTOR/РУЧR) */

/* ── Команды mode_cr ────────────────────────────────────────────────────── */
#define MB_CMD_NONE             0x00u
#define MB_CMD_POWER_ON         0x01u   /* включить (из STANDBY)                */
#define MB_CMD_STANDBY          0x02u   /* перейти в ожидание                   */
#define MB_CMD_START            0x03u   /* запустить (из MAIN)                  */
#define MB_CMD_STOP             0x04u   /* остановить                           */
#define MB_CMD_SET_AUTO         0x05u   /* переключить в авто                   */
#define MB_CMD_SET_MANUAL       0x06u   /* переключить в ручной                 */
#define MB_CMD_ACK_ALARM        0x07u   /* квитировать аварию                   */

/* ── alarm_flags биты (МКВП-02) ────────────────────────────────────────── */
#define MB_ALARM_FLOW_LOW       (1u << 0)  /* поток ниже порога (Low Speed)    */
#define MB_ALARM_INVERTER       (1u << 1)  /* авария инвертора (Inverter Fault) */
#define MB_ALARM_DOOR_OPEN      (1u << 2)  /* дверь/стекло открыто (Glass Open) */

/* ── led_state биты ─────────────────────────────────────────────────────── */
#define MB_LED_POWER            (1u << 0)
#define MB_LED_WORK             (1u << 1)
#define MB_LED_ALARM            (1u << 2)
#define MB_LED_MANUAL           (1u << 3)

/* ── manual_mem значения ────────────────────────────────────────────────── */
#define MB_MANUAL_NOR           0x00u   /* при переходе AUTO→РУЧ выход = 0    */
#define MB_MANUAL_MEM           0x01u   /* при переходе AUTO→РУЧ выход сохр.  */

/* ── bit_sr биты ─────────────────────────────────────────────────────────── */
#define MB_BIT_SR_LED           (1u << 0)
#define MB_BIT_SR_TIMER         (1u << 1)

/* ── bit_cr биты ─────────────────────────────────────────────────────────── */
#define MB_BIT_CR_RESET_TIMER   (1u << 0)
#define MB_BIT_CR_LED_SET       (1u << 1)
#define MB_BIT_CR_LED_RESET     (1u << 2)

/* ── Таблица ────────────────────────────────────────────────────────────── */
extern uint8_t mb_table[MB_TABLE_SIZE];

/* ── Функции доступа ─────────────────────────────────────────────────────── */

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

/* Внимание: строковые операции — НЕ inline, реализация в modbus_table.c,
   чтобы взять taskENTER_CRITICAL/taskEXIT_CRITICAL. Без критической секции
   Modbus-задача (higher prio) может препятствовать записи строки между
   memset() и strncpy() → master читает «разорванный» буфер: первые N байт
   нулевые, остальные — старые. Визуально выглядит как «строка с
   поломанным порядком символов».                                         */
const char *MB_ReadString(uint16_t addr);
void        MB_WriteString(uint16_t addr, const char *str);

#endif /* MODBUS_TABLE_H */
