#ifndef MODBUS_H
#define MODBUS_H

#include <stdint.h>
#include "modbus_table.h"

/* ── Конфигурация ────────────────────────────────────────────────────────── */
#define MB_SLAVE_ADDR    1

/* Число Holding/Input регистров = размер mb_table в 16-битных словах.
   MB_TABLE_SIZE = 122 байта → 61 регистр.                                  */
#define MB_REG_COUNT     (MB_TABLE_SIZE / 2)    /* 61 */

#define MB_COIL_COUNT    64     /* Катушки не используются в приложении,
                                   оставлены для совместимости.              */

/* ── Коды функций ────────────────────────────────────────────────────────── */
#define MB_FC_READ_COILS         0x01
#define MB_FC_READ_DISCRETE      0x02
#define MB_FC_READ_HOLDING       0x03
#define MB_FC_READ_INPUT         0x04
#define MB_FC_WRITE_SINGLE_COIL  0x05
#define MB_FC_WRITE_SINGLE_REG   0x06
#define MB_FC_WRITE_MULTI_COILS  0x0F
#define MB_FC_WRITE_MULTI_REGS   0x10

/* ── Коды исключений ─────────────────────────────────────────────────────── */
#define MB_EX_ILLEGAL_FUNCTION   0x01
#define MB_EX_ILLEGAL_DATA_ADDR  0x02
#define MB_EX_ILLEGAL_DATA_VALUE 0x03

void     modbus_init(void);
uint16_t modbus_process(const uint8_t *req, uint16_t req_len, uint8_t *resp);

#endif /* MODBUS_H */
