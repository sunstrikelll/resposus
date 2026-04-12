#ifndef MODBUS_TABLE_H
#define MODBUS_TABLE_H

#include <stdint.h>
#include <string.h>

/* ── Размер таблицы ─────────────────────────────────────────────────────────
   122 байта = 61 Modbus-регистр (16-бит каждый).
   Данные хранятся в big-endian порядке (старший байт по меньшему адресу),
   чтобы Modbus-мастер (ModbusUtility) читал их без преобразований.

   Адресная карта (байтовые смещения → номера Modbus-регистров):
   ┌─────────────────────────┬──────┬──────┬───────────────────────────────┐
   │ Поле                    │ Байт │ Рег. │ Примечание                    │
   ├─────────────────────────┼──────┼──────┼───────────────────────────────┤
   │ (pad)                   │ 0x00 │  0↑  │                               │
   │ bit_sr                  │ 0x01 │  0↓  │ R,  bitfield: LED(0) TIMER(1) │
   │ (pad)                   │ 0x02 │  1↑  │                               │
   │ bit_cr                  │ 0x03 │  1↓  │ W,  RESET_TIMER(0) LED_SET(1) │
   │                         │      │      │     LED_RESET(2)               │
   │ timer[3..0]             │ 0x04 │ 2-3  │ R,  uint32 big-endian         │
   │ version[3..0]           │ 0x08 │ 4-5  │ RW, float  big-endian         │
   │ display_line_0[0..21]   │ 0x0C │ 6-16 │ R,  string 22 байта           │
   │ display_line_1[0..21]   │ 0x22 │17-27 │ R,  string 22 байта           │
   │ display_line_2[0..21]   │ 0x38 │28-38 │ R,  string 22 байта           │
   │ display_line_3[0..21]   │ 0x4E │39-49 │ R,  string 22 байта           │
   │ usr_text[0..21]         │ 0x64 │50-60 │ RW, string 22 байта           │
   └─────────────────────────┴──────┴──────┴───────────────────────────────┘  */

#define MB_TABLE_SIZE           0x7A    /* 122 байта = 61 регистр */

/* ── Байтовые адреса ─────────────────────────────────────────────────────── */
#define MB_ADDR_BIT_SR          0x01    /* R   bitfield, 1 байт  */
#define MB_ADDR_BIT_CR          0x03    /* W   bitfield, 1 байт  */
#define MB_ADDR_TIMER           0x04    /* R   uint32,   4 байта */
#define MB_ADDR_VERSION         0x08    /* RW  float,    4 байта */
#define MB_ADDR_DISPLAY_LINE_0  0x0C    /* R   string,  22 байта */
#define MB_ADDR_DISPLAY_LINE_1  0x22    /* R   string,  22 байта */
#define MB_ADDR_DISPLAY_LINE_2  0x38    /* R   string,  22 байта */
#define MB_ADDR_DISPLAY_LINE_3  0x4E    /* R   string,  22 байта */
#define MB_ADDR_USR_TEXT        0x64    /* RW  string,  22 байта */

#define MB_STRING_LEN           22

/* ── bit_sr биты ─────────────────────────────────────────────────────────── */
#define MB_BIT_SR_LED           (1u << 0)
#define MB_BIT_SR_TIMER         (1u << 1)

/* ── bit_cr биты ─────────────────────────────────────────────────────────── */
#define MB_BIT_CR_RESET_TIMER   (1u << 0)
#define MB_BIT_CR_LED_SET       (1u << 1)
#define MB_BIT_CR_LED_RESET     (1u << 2)

/* ── Массив-таблица ──────────────────────────────────────────────────────── */
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

/* uint32 и float хранятся big-endian — совпадает с Modbus-представлением.
   ModbusUtility: тип "uint32" или "float", порядок байт "Big-endian (ABCD)". */

static inline uint32_t MB_ReadUint32(uint16_t addr)
{
    return ((uint32_t)mb_table[addr]     << 24) |
           ((uint32_t)mb_table[addr + 1] << 16) |
           ((uint32_t)mb_table[addr + 2] <<  8) |
            (uint32_t)mb_table[addr + 3];
}

static inline void MB_WriteUint32(uint16_t addr, uint32_t val)
{
    mb_table[addr]     = (uint8_t)(val >> 24);
    mb_table[addr + 1] = (uint8_t)(val >> 16);
    mb_table[addr + 2] = (uint8_t)(val >>  8);
    mb_table[addr + 3] = (uint8_t)(val);
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

/* Строки хранятся как есть (байт N = старший байт регистра N/2).
   ModbusUtility: тип "string", big-endian — даст правильный порядок символов. */

static inline const char *MB_ReadString(uint16_t addr)
{
    return (const char *)&mb_table[addr];
}

static inline void MB_WriteString(uint16_t addr, const char *str)
{
    memset(&mb_table[addr], 0, MB_STRING_LEN);
    strncpy((char *)&mb_table[addr], str, MB_STRING_LEN - 1);
}

#endif /* MODBUS_TABLE_H */
