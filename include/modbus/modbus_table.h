#ifndef MODBUS_TABLE_H
#define MODBUS_TABLE_H

#include <stdint.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════════
   МКВП-02: карта Modbus-регистров (v2)
   ══════════════════════════════════════════════════════════════════════════
   Структура карты повторяет SMKK_RegisterMap_v2:

     • Runtime  (0 .. 99)   — RAM, обнуляется при сбросе.
     • EEPROM   (100 .. 149)— энергонезависимая зона, грузится из AT24C64.
     • [150 .. 199]         — резерв под будущее расширение EEPROM-зоны.

   Все адреса — Modbus-регистровые (десятичные).  Байтовое смещение в
   mb_table = 2·reg, для u8 — 2·reg+1 (Modbus читает 16-бит big-endian).

   ┌──────────────────────── Runtime (RAM, R/W) ──────────────────────────┐

   Группа A — Зеркало LCD (R)
     0   DISPLAY_LINE_0    string 22 (11 регистров)
     11  DISPLAY_LINE_1    string 22
     22  spare

   Группа B — Тестовые/пользовательские строки (RW)
     23  TEST_LINE_0       string 22
     34  TEST_LINE_1       string 22
     45  USR_TEXT          string 22

   Группа C — Состояние и команды
     56  SYS_SR            u8 R   статус FSM
                                  b0 POWER  b1 WORK  b2 ALARM  b3 MANUAL
                                  b4 STDBY  b5 REPEAT b6 NIGHT b7 EMERGENCY
     57  SYS_CR            u8 RW  команда (auto-clear)
                                  1 PWR_ON  2 STBY  3 START  4 STOP  5 AUTO
                                  6 MANUAL  7 ACK_ALM  8 NIGHT  9 LAMP
                                  10 SOCKET 11 EMERG  12 BUZ_MUTE
     58  ALARM_FL          u8 R   биты аварий: FLOW_LOW INVERTER DOOR_OPEN
     59  LED_SR            u8 R   зеркало 4 LED: POWER WORK ALARM MANUAL
     60  HW_ER             u8 R   аппаратные ошибки: EE_ER I2C_ER LSE_ER FWDT
     61  SW_CR             u8 RW  системное управление (auto-clear)
                                  b0 RESET  b1 FACT_RST  b2 DBG_ON  b3 DBG_OFF
     62  SYS_EV            u8 R   события:  EE_ERR EE_RST EE_WST WDTF
     63  SYS_EVR           u8 W   сброс битов SYS_EV (auto-clear)
     64  SYS_MODE          u8 R   режим: 0 PRODUCTION, 1 TEST

   Группа D — Кнопки
     65  BTN_PR_SR         u8 R   биты короткого нажатия: PRG ONOFF AM LMP RB E
     66  BTN_LPR_SR        u8 R   биты длинного нажатия (то же расположение)
     67  BTN_PR_IR         u8 W   имитация короткого (auto-clear)
     68  BTN_LPR_IR        u8 W   имитация длинного (auto-clear)
     69  BTN_EVENT         u8 R   последнее событие (legacy hex-код)
     70  BTN_CMD           u8 RW  виртуальная кнопка (auto-clear)
     71  MENU_GOTO         u8 RW  прыжок в состояние меню (auto-clear)

   Группа E — Живые значения процесса
     73  FLOW              float R   текущий поток, м/с (73-74)
     75  EXT_TEMP          float R   внешняя температура, °С (75-76)
     77  MANUAL_OUT        u8 R/W    выход ручного режима, 0-100 %
     78  OUT_SR            u8 R      биты дискр. выходов: LAMP SOCKET
     79  BUZZER_SR         u8 R      зуммер: 0=OFF 1=ON

     80-99 spare

   └──────────────────────────────────────────────────────────────────────┘

   ┌─────────────────────── EEPROM (persist) ─────────────────────────────┐

   Группа F — Уставки потока
     100 SETPOINT          float    уставка потока (АВТО), м/с
     102 FLOW_SP_R         float    уставка повтора, м/с

   Группа G — Пороги аварии
     104 ALARM_LOW         float    порог «мало потока», м/с
     106 ALARM_LOW_R       float    порог сброса аварии, м/с
     108 ALARM_DELAY       u8       задержка аварии, 0-180 с

   Группа H — Режим ручного
     109 MANUAL_MEM        u8       0 НОРМ, 1 ПАМЯТЬ

   Группа I — Калибровка
     110 SENSOR_ZERO       float    калибровка нуля датчика, м/с
     112 SENSOR_SPAN       float    калибровка макс. датчика, м/с
     114 OUT_ZERO_PCT      u8       калибровка нуля выхода, %
     115 OUT_SPAN_PCT      u8       калибровка макс. выхода, %

   Группа J — ПИД
     116 PID_TI            float    постоянная интегрирования, с
     118 PID_BAND          float    пропорциональная полоса, см/с

   Группа K — Обслуживание
     120 MAINT_HOURS       float    счётчик моточасов, ч
     122 COUNT_MAX         float    лимит до ТО, ч

   Группа L — Опции
     124 BLACKOUT_EN       u8       Blackout: 0 OFF, 1 ON
     125 DATALOG_EN        u8       логгер:   0 OFF, 1 ON
     126 DATALOG_SEC       u8       период логгера, с

   Группа M — Тайминги кнопок
     128 BTN_DEBOUNCE_MS   u16      длительность антидребезга, мс
     129 BTN_LONG_MS       u16      порог длинного нажатия, мс
     130 BTN_MID_MS        u16      порог среднего нажатия, мс

     131-149 spare

   └──────────────────────────────────────────────────────────────────────┘

   КОНЕЦ КАРТЫ: reg 149, byte 0x12C.                                      */

#define MB_TABLE_SIZE           0x12C   /* 300 байт = 150 регистров */

/* ── Хелперы перевода «регистр → байтовое смещение» ────────────────────── */
#define MB_REG(n)        ((uint16_t)((n) * 2u))
#define MB_REG_U8(n)     ((uint16_t)((n) * 2u + 1u))

/* ═══════════════════════ Группа A — Дисплей ═════════════════════════════ */
#define MB_ADDR_DISPLAY_LINE_0   MB_REG(0)        /* string 22 */
#define MB_ADDR_DISPLAY_LINE_1   MB_REG(11)

/* ═══════════════════════ Группа B — Тест-строки/пользователь ════════════ */
#define MB_ADDR_TEST_LINE_0      MB_REG(23)
#define MB_ADDR_TEST_LINE_1      MB_REG(34)
#define MB_ADDR_USR_TEXT         MB_REG(45)

#define MB_STRING_LEN            22u

/* ═══════════════════════ Группа C — Статус и команды ════════════════════ */
#define MB_ADDR_SYS_SR           MB_REG_U8(56)
#define MB_ADDR_SYS_CR           MB_REG_U8(57)
#define MB_ADDR_ALARM_FL         MB_REG_U8(58)
#define MB_ADDR_LED_SR           MB_REG_U8(59)
#define MB_ADDR_HW_ER            MB_REG_U8(60)
#define MB_ADDR_SW_CR            MB_REG_U8(61)
#define MB_ADDR_SYS_EV           MB_REG_U8(62)
#define MB_ADDR_SYS_EVR          MB_REG_U8(63)
#define MB_ADDR_SYS_MODE         MB_REG_U8(64)

/* Старые имена (back-compat) */
#define MB_ADDR_MODE_SR          MB_ADDR_SYS_SR
#define MB_ADDR_MODE_CR          MB_ADDR_SYS_CR
#define MB_ADDR_ALARM_FLAGS      MB_ADDR_ALARM_FL
#define MB_ADDR_LED_STATE        MB_ADDR_LED_SR
#define MB_ADDR_RUNTIME_MODE     MB_ADDR_SYS_MODE

/* ═══════════════════════ Группа D — Кнопки ══════════════════════════════ */
#define MB_ADDR_BTN_PR_SR        MB_REG_U8(65)
#define MB_ADDR_BTN_LPR_SR       MB_REG_U8(66)
#define MB_ADDR_BTN_PR_IR        MB_REG_U8(67)
#define MB_ADDR_BTN_LPR_IR       MB_REG_U8(68)
#define MB_ADDR_BTN_EVENT        MB_REG_U8(69)
#define MB_ADDR_BTN_CMD          MB_REG_U8(70)
#define MB_ADDR_MENU_GOTO        MB_REG_U8(71)

/* ═══════════════════════ Группа E — Живые значения процесса ═════════════ */
#define MB_ADDR_FLOW             MB_REG(73)
#define MB_ADDR_EXT_TEMP         MB_REG(75)
#define MB_ADDR_MANUAL_OUT       MB_REG_U8(77)
#define MB_ADDR_OUT_SR           MB_REG_U8(78)
#define MB_ADDR_BUZZER_SR        MB_REG_U8(79)

/* Старые имена (back-compat) */
#define MB_ADDR_OUTPUT_STATE     MB_ADDR_OUT_SR
#define MB_ADDR_BUZZER_STATE     MB_ADDR_BUZZER_SR

/* buzzer_state значения */
#define MB_BUZZER_OFF            0x00u
#define MB_BUZZER_ON             0x01u

/* ═══════════════════════ Граница EEPROM-зоны ════════════════════════════ */
#define MB_EEPROM_REG_BASE       100u
#define MB_EEPROM_REG_END        150u
#define MB_EEPROM_BYTE_BASE      MB_REG(MB_EEPROM_REG_BASE)   /* 0x0C8 */
#define MB_EEPROM_BYTE_END       MB_REG(MB_EEPROM_REG_END)    /* 0x12C */
#define MB_EEPROM_BYTE_SIZE      (MB_EEPROM_BYTE_END - MB_EEPROM_BYTE_BASE)
                                                     /* 100 байт */

/* ═══════════════════════ Группа F — Уставки потока ══════════════════════ */
#define MB_ADDR_SETPOINT         MB_REG(100)      /* float */
#define MB_ADDR_FLOW_SP_R        MB_REG(102)      /* float */

/* ═══════════════════════ Группа G — Пороги аварии ═══════════════════════ */
#define MB_ADDR_ALARM_LOW        MB_REG(104)      /* float */
#define MB_ADDR_ALARM_LOW_R      MB_REG(106)      /* float */
#define MB_ADDR_ALARM_DELAY      MB_REG_U8(108)   /* u8 */

/* ═══════════════════════ Группа H — Режим ручного ═══════════════════════ */
#define MB_ADDR_MANUAL_MEM       MB_REG_U8(109)   /* u8 */

/* ═══════════════════════ Группа I — Калибровка ══════════════════════════ */
#define MB_ADDR_SENSOR_ZERO      MB_REG(110)      /* float */
#define MB_ADDR_SENSOR_SPAN      MB_REG(112)      /* float */
#define MB_ADDR_OUT_ZERO_PCT     MB_REG_U8(114)   /* u8 */
#define MB_ADDR_OUT_SPAN_PCT     MB_REG_U8(115)   /* u8 */

/* ═══════════════════════ Группа J — ПИД ═════════════════════════════════ */
#define MB_ADDR_PID_TI           MB_REG(116)      /* float */
#define MB_ADDR_PID_BAND         MB_REG(118)      /* float */

/* ═══════════════════════ Группа K — Обслуживание ════════════════════════ */
#define MB_ADDR_MAINT_HOURS      MB_REG(120)      /* float */
#define MB_ADDR_COUNT_MAX        MB_REG(122)      /* float */

/* ═══════════════════════ Группа L — Опции ═══════════════════════════════ */
#define MB_ADDR_BLACKOUT_EN      MB_REG_U8(124)   /* u8 */
#define MB_ADDR_DATALOG_EN       MB_REG_U8(125)   /* u8 */
#define MB_ADDR_DATALOG_SEC      MB_REG_U8(126)   /* u8 */

/* ═══════════════════════ Группа M — Тайминги кнопок ═════════════════════ */
#define MB_ADDR_BTN_DEBOUNCE_MS  MB_REG(128)      /* u16 (1 рег.) */
#define MB_ADDR_BTN_LONG_MS      MB_REG(129)      /* u16 (1 рег.) */
#define MB_ADDR_BTN_MID_MS       MB_REG(130)      /* u16 (1 рег.) */

/* ══════════════════════════════════════════════════════════════════════════
   Битовые константы
   ══════════════════════════════════════════════════════════════════════════ */

/* sys_mode значения */
#define MB_RUNTIME_PRODUCTION   0x00u
#define MB_RUNTIME_TEST         0x01u

/* sys_sr биты (FSM-статус) */
#define MB_BIT_MODE_POWER       (1u << 0)
#define MB_BIT_MODE_WORK        (1u << 1)
#define MB_BIT_MODE_ALARM       (1u << 2)
#define MB_BIT_MODE_MANUAL      (1u << 3)
#define MB_BIT_MODE_STANDBY     (1u << 4)
#define MB_BIT_MODE_REPEAT      (1u << 5)
#define MB_BIT_MODE_NIGHT       (1u << 6)
#define MB_BIT_MODE_EMERGENCY   (1u << 7)

/* out_sr биты (дискретные выходы) */
#define MB_OUT_LAMP             (1u << 0)
#define MB_OUT_SOCKET           (1u << 1)

/* sys_cr команды */
#define MB_CMD_NONE             0x00u
#define MB_CMD_POWER_ON         0x01u
#define MB_CMD_STANDBY          0x02u
#define MB_CMD_START            0x03u
#define MB_CMD_STOP             0x04u
#define MB_CMD_SET_AUTO         0x05u
#define MB_CMD_SET_MANUAL       0x06u
#define MB_CMD_ACK_ALARM        0x07u
#define MB_CMD_NIGHT_TOGGLE     0x08u
#define MB_CMD_LAMP_TOGGLE      0x09u
#define MB_CMD_SOCKET_TOGGLE    0x0Au
#define MB_CMD_EMERGENCY_TOGGLE 0x0Bu
#define MB_CMD_BUZZER_MUTE      0x0Cu

/* alarm_fl биты */
#define MB_ALARM_FLOW_LOW       (1u << 0)
#define MB_ALARM_INVERTER       (1u << 1)
#define MB_ALARM_DOOR_OPEN      (1u << 2)

/* led_sr биты */
#define MB_LED_POWER            (1u << 0)
#define MB_LED_WORK             (1u << 1)
#define MB_LED_ALARM            (1u << 2)
#define MB_LED_MANUAL           (1u << 3)

/* btn_*_sr / btn_*_ir биты — порядок §5.1..§5.6 */
#define MB_BTN_PRG              (1u << 0)
#define MB_BTN_ONOFF            (1u << 1)
#define MB_BTN_LAMP             (1u << 2)
#define MB_BTN_AUTO_MAN         (1u << 3)
#define MB_BTN_RB               (1u << 4)
#define MB_BTN_E                (1u << 5)

/* hw_er биты — аппаратные ошибки */
#define MB_HW_EE_ER             (1u << 0)   /* отказ EEPROM (I2C/CRC)        */
#define MB_HW_I2C_ER            (1u << 1)   /* шина I2C (NACK/timeout)       */
#define MB_HW_LSE_ER            (1u << 2)   /* кварц LSE не запустился       */
#define MB_HW_FWDT_ER           (1u << 3)   /* старт по сбросу FWDGT         */

/* sw_cr биты — системное управление */
#define MB_SW_RESET             (1u << 0)   /* программный сброс             */
#define MB_SW_FACT_RST          (1u << 1)   /* сброс к заводским             */
#define MB_SW_DEBUG_ON          (1u << 2)
#define MB_SW_DEBUG_OFF         (1u << 3)

/* sys_ev / sys_evr биты — системные события */
#define MB_SYS_EV_EE_ERR        (1u << 0)   /* ошибка чтения/записи EEPROM   */
#define MB_SYS_EV_EE_RST        (1u << 1)   /* успешное чтение EEPROM        */
#define MB_SYS_EV_EE_WST        (1u << 2)   /* успешная запись EEPROM        */
#define MB_SYS_EV_WDTF          (1u << 3)   /* был сброс по сторожу          */

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

static inline uint16_t MB_ReadU16(uint16_t addr)
{
    return (uint16_t)(((uint16_t)mb_table[addr] << 8) | (uint16_t)mb_table[addr + 1]);
}

static inline void MB_WriteU16(uint16_t addr, uint16_t val)
{
    mb_table[addr]     = (uint8_t)(val >> 8);
    mb_table[addr + 1] = (uint8_t)(val & 0xFFu);
}

/* uint32/float — порядок CDAB. */
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

void MB_ReadString (uint16_t addr, char *out, uint8_t max_len);
void MB_WriteString(uint16_t addr, const char *str);

#endif /* MODBUS_TABLE_H */
