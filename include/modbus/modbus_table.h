#ifndef MODBUS_TABLE_H
#define MODBUS_TABLE_H

#include <stdint.h>
#include <string.h>

/* ── Размер таблицы ───────────────────────────────────────────────────────── */
#define MB_TABLE_SIZE       0x74    /* 120 байт */

/* ── Адреса регистров ─────────────────────────────────────────────────────── */
#define MB_ADDR_BIT_SR          0x00    /* bitfield, 1 байт,  R   */
#define MB_ADDR_BIT_CR          0x01    /* bitfield, 1 байт,  W   */
#define MB_ADDR_TIMER           0x02    /* uint32_t, 4 байта, R   */
#define MB_ADDR_VERSION         0x04    /* float,    4 байта, RW  */
#define MB_ADDR_DISPLAY_LINE_0  0x06    /* char[22],          R   */
#define MB_ADDR_DISPLAY_LINE_1  0x1C    /* char[22],          R   */
#define MB_ADDR_DISPLAY_LINE_2  0x32    /* char[22],          R   */
#define MB_ADDR_DISPLAY_LINE_3  0x48    /* char[22],          R   */
#define MB_ADDR_USR_TEXT        0x5E    /* char[22],          RW  */

#define MB_STRING_LEN           22

/* ── bit_sr биты ──────────────────────────────────────────────────────────── */
#define MB_BIT_SR_LED           (1u << 0)
#define MB_BIT_SR_TIMER         (1u << 1)

/* ── bit_cr биты ──────────────────────────────────────────────────────────── */
#define MB_BIT_CR_RESET_TIMER   (1u << 0)
#define MB_BIT_CR_LED_SET       (1u << 1)
#define MB_BIT_CR_LED_RESET     (1u << 2)

/* ── Массив-таблица ───────────────────────────────────────────────────────── */
extern uint8_t mb_table[MB_TABLE_SIZE];

/* ── Функции доступа ──────────────────────────────────────────────────────── */

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

static inline uint32_t MB_ReadUint32(uint16_t addr)
{
    uint32_t val;
    memcpy(&val, &mb_table[addr], sizeof(val));
    return val;
}

static inline void MB_WriteUint32(uint16_t addr, uint32_t val)
{
    memcpy(&mb_table[addr], &val, sizeof(val));
}

static inline float MB_ReadFloat(uint16_t addr)
{
    float val;
    memcpy(&val, &mb_table[addr], sizeof(val));
    return val;
}

static inline void MB_WriteFloat(uint16_t addr, float val)
{
    memcpy(&mb_table[addr], &val, sizeof(val));
}

static inline const char *MB_ReadString(uint16_t addr)
{
    return (const char *)&mb_table[addr];
}

static inline void MB_WriteString(uint16_t addr, const char *str)
{
    memset(&mb_table[addr], 0, MB_STRING_LEN);
    strncpy((char *)&mb_table[addr], str, MB_STRING_LEN);
}

#endif /* MODBUS_TABLE_H */
