#ifndef MODBUS_TABLE_H
#define MODBUS_TABLE_H

#include <stdint.h>
#include <string.h>

/* ── Размер таблицы ─────────────────────────────────────────────────────────
   222 байта = 111 Modbus-регистров (16-бит каждый).
   Данные хранятся в big-endian порядке (старший байт по меньшему адресу),
   чтобы Modbus-мастер (ModbusUtility) читал их без преобразований.

   reg_read(n) читает байты [n*2, n*2+1], поэтому байтовое смещение поля
   = 2 × Modbus-адрес регистра. Нечётные Modbus-адреса (bit_sr, bit_cr)
   хранятся в младшем байте регистра (высокий байт = 0).

   Адресная карта (Modbus-адрес регистра → байтовое смещение в mb_table):
   ┌─────────────────────────┬──────────┬────────────┬─────────────────────────────────┐
   │ Поле                    │ MB-адрес │ Байт       │ Примечание                      │
   ├─────────────────────────┼──────────┼────────────┼─────────────────────────────────┤
   │ bit_sr                  │ 0x0001   │ 0x03       │ R,  bitfield: LED(0) TIMER(1)   │
   │ bit_cr                  │ 0x0003   │ 0x07       │ W,  RESET_TIMER(0) LED_SET(1)   │
   │                         │          │            │     LED_RESET(2)                 │
   │ timer[3..0]             │ 0x0004   │ 0x08-0x0B  │ R,  uint32 big-endian           │
   │ version[3..0]           │ 0x0008   │ 0x10-0x13  │ RW, float  big-endian           │
   │ display_line_0[0..21]   │ 0x000C   │ 0x18-0x2D  │ R,  string 22 байта             │
   │ display_line_1[0..21]   │ 0x0022   │ 0x44-0x59  │ R,  string 22 байта             │
   │ display_line_2[0..21]   │ 0x0038   │ 0x70-0x85  │ R,  string 22 байта             │
   │ display_line_3[0..21]   │ 0x004E   │ 0x9C-0xB1  │ R,  string 22 байта             │
   │ usr_text[0..21]         │ 0x0064   │ 0xC8-0xDD  │ RW, string 22 байта             │
   └─────────────────────────┴──────────┴────────────┴─────────────────────────────────┘  */

#define MB_TABLE_SIZE           0xDE    /* 222 байта = 111 регистров */

/* ── Байтовые смещения в mb_table (= 2 × Modbus-адрес регистра) ──────────── */
#define MB_ADDR_BIT_SR          0x03    /* R   bitfield, 1 байт  (MB reg 0x0001 low) */
#define MB_ADDR_BIT_CR          0x07    /* W   bitfield, 1 байт  (MB reg 0x0003 low) */
#define MB_ADDR_TIMER           0x08    /* R   uint32,   4 байта (MB regs 0x0004-0x0005) */
#define MB_ADDR_VERSION         0x10    /* RW  float,    4 байта (MB regs 0x0008-0x0009) */
#define MB_ADDR_DISPLAY_LINE_0  0x18    /* R   string,  22 байта (MB regs 0x000C-0x0016) */
#define MB_ADDR_DISPLAY_LINE_1  0x44    /* R   string,  22 байта (MB regs 0x0022-0x002C) */
#define MB_ADDR_DISPLAY_LINE_2  0x70    /* R   string,  22 байта (MB regs 0x0038-0x0042) */
#define MB_ADDR_DISPLAY_LINE_3  0x9C    /* R   string,  22 байта (MB regs 0x004E-0x0058) */
#define MB_ADDR_USR_TEXT        0xC8    /* RW  string,  22 байта (MB regs 0x0064-0x006E) */

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

/* uint32 и float хранятся в порядке CDAB (сначала младшее слово, затем старшее).
   ModbusUtility читает 32-бит значения в порядке CDAB:
     байты в mb_table: [CC DD AA BB]  ←→  значение 0xAABBCCDD
     регистр addr+0 = 0xCCDD (LSW), регистр addr+1 = 0xAABB (MSW)        */

static inline uint32_t MB_ReadUint32(uint16_t addr)
{
    return ((uint32_t)mb_table[addr + 2] << 24) |
           ((uint32_t)mb_table[addr + 3] << 16) |
           ((uint32_t)mb_table[addr]     <<  8) |
            (uint32_t)mb_table[addr + 1];
}

static inline void MB_WriteUint32(uint16_t addr, uint32_t val)
{
    mb_table[addr]     = (uint8_t)((val >>  8) & 0xFFu);  /* CC — LSW high */
    mb_table[addr + 1] = (uint8_t)(val & 0xFFu);           /* DD — LSW low  */
    mb_table[addr + 2] = (uint8_t)(val >> 24);             /* AA — MSW high */
    mb_table[addr + 3] = (uint8_t)((val >> 16) & 0xFFu);  /* BB — MSW low  */
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
