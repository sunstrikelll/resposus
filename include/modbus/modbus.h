#ifndef MODBUS_H
#define MODBUS_H

#include <stdint.h>

/* ── Конфигурация ──────────────────────────────────────────────────────────── */
#define MB_SLAVE_ADDR    1      /* Адрес ведомого */
#define MB_REG_COUNT     500    /* Количество Holding / Input регистров       */
#define MB_COIL_COUNT    500    /* Количество Coils / Discrete Inputs         */

/* ── Коды функций ──────────────────────────────────────────────────────────── */
#define MB_FC_READ_COILS         0x01
#define MB_FC_READ_DISCRETE      0x02
#define MB_FC_READ_HOLDING       0x03
#define MB_FC_READ_INPUT         0x04
#define MB_FC_WRITE_SINGLE_COIL  0x05
#define MB_FC_WRITE_SINGLE_REG   0x06
#define MB_FC_WRITE_MULTI_COILS  0x0F
#define MB_FC_WRITE_MULTI_REGS   0x10

/* ── Коды исключений ───────────────────────────────────────────────────────── */
#define MB_EX_ILLEGAL_FUNCTION   0x01
#define MB_EX_ILLEGAL_DATA_ADDR  0x02
#define MB_EX_ILLEGAL_DATA_VALUE 0x03

/**
 * Инициализация: заполняет holding_regs[0..499] значениями 1..500,
 * обнуляет массив катушек.
 */
void modbus_init(void);

/**
 * Разбирает входящий Modbus RTU фрейм и формирует ответ.
 *
 * @param req     указатель на принятые байты (включая CRC)
 * @param req_len длина принятых байт
 * @param resp    буфер для ответа (минимум 260 байт)
 * @return        длина ответа (0 — отвечать не нужно)
 */
uint16_t modbus_process(const uint8_t *req, uint16_t req_len, uint8_t *resp);

#endif /* MODBUS_H */
