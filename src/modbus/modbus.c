#include "modbus.h"
#include <string.h>

/* ── Внутренние данные ─────────────────────────────────────────────────────── */

static uint16_t holding_regs[MB_REG_COUNT];

/* Катушки и дискретные входы: 1 бит на единицу, packed в байты.
   Discrete inputs отображены на те же данные, что и Coils (read-only вид). */
#define MB_COIL_BYTES ((MB_COIL_COUNT + 7) / 8)
static uint8_t coils[MB_COIL_BYTES];

/* ── CRC-16 (полином 0xA001, LSB первый — стандарт Modbus) ─────────────────── */
static uint16_t crc16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001)
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ── Сборка фрейма исключения ──────────────────────────────────────────────── */
static uint16_t build_exception(uint8_t addr, uint8_t fc,
                                uint8_t ex_code, uint8_t *resp)
{
    resp[0] = addr;
    resp[1] = (uint8_t)(fc | 0x80u);
    resp[2] = ex_code;
    uint16_t crc = crc16(resp, 3);
    resp[3] = (uint8_t)(crc & 0xFFu);
    resp[4] = (uint8_t)(crc >> 8);
    return 5;
}

/* ── Публичные функции ─────────────────────────────────────────────────────── */

void modbus_init(void)
{
    for (uint16_t i = 0; i < MB_REG_COUNT; i++)
        holding_regs[i] = (uint16_t)(i + 1);   /* 1 .. 500 */

    memset(coils, 0, sizeof(coils));
}

uint16_t modbus_process(const uint8_t *req, uint16_t req_len, uint8_t *resp)
{
    /* Минимальный корректный фрейм: addr(1)+fc(1)+data(min 2)+crc(2) = 6 байт.
       Но FC05/FC06 ровно 8, FC03 req тоже 8 — проверяем после разбора FC. */
    if (req_len < 4)
        return 0;

    /* Проверка CRC: последние 2 байта = CRC (LSB, MSB) */
    uint16_t crc_recv = (uint16_t)((uint16_t)req[req_len - 1] << 8) | req[req_len - 2];
    if (crc_recv != crc16(req, (uint16_t)(req_len - 2)))
        return 0;

    uint8_t  addr = req[0];
    uint8_t  fc   = req[1];

    /* Адрес 0 — широковещательный: выполняем команду, но не отвечаем.
       Пока упрощённо: просто игнорируем не-наш адрес. */
    if (addr != MB_SLAVE_ADDR)
        return 0;

    uint16_t start_addr, quantity, reg_val;
    uint8_t  byte_count;
    uint16_t resp_len, crc;

    switch (fc)
    {
    /* ────────────────────────────────────────────────────────────────────────
       FC 01 — Read Coils
    ──────────────────────────────────────────────────────────────────────── */
    case MB_FC_READ_COILS:
        if (req_len < 6)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        start_addr = (uint16_t)((uint16_t)req[2] << 8 | req[3]);
        quantity   = (uint16_t)((uint16_t)req[4] << 8 | req[5]);
        if (quantity == 0 || quantity > 2000)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        if ((uint32_t)start_addr + quantity > MB_COIL_COUNT)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_ADDR, resp);

        byte_count = (uint8_t)((quantity + 7) / 8);
        resp[0] = addr;
        resp[1] = fc;
        resp[2] = byte_count;
        memset(&resp[3], 0, byte_count);
        for (uint16_t i = 0; i < quantity; i++) {
            uint16_t idx = (uint16_t)(start_addr + i);
            uint8_t  bit = (uint8_t)((coils[idx / 8] >> (idx % 8)) & 0x01u);
            resp[3 + i / 8] |= (uint8_t)(bit << (i % 8));
        }
        resp_len = (uint16_t)(3 + byte_count);
        crc = crc16(resp, resp_len);
        resp[resp_len++] = (uint8_t)(crc & 0xFFu);
        resp[resp_len++] = (uint8_t)(crc >> 8);
        return resp_len;

    /* ────────────────────────────────────────────────────────────────────────
       FC 02 — Read Discrete Inputs (отображены на те же данные, что Coils)
    ──────────────────────────────────────────────────────────────────────── */
    case MB_FC_READ_DISCRETE:
        if (req_len < 6)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        start_addr = (uint16_t)((uint16_t)req[2] << 8 | req[3]);
        quantity   = (uint16_t)((uint16_t)req[4] << 8 | req[5]);
        if (quantity == 0 || quantity > 2000)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        if ((uint32_t)start_addr + quantity > MB_COIL_COUNT)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_ADDR, resp);

        byte_count = (uint8_t)((quantity + 7) / 8);
        resp[0] = addr;
        resp[1] = fc;
        resp[2] = byte_count;
        memset(&resp[3], 0, byte_count);
        for (uint16_t i = 0; i < quantity; i++) {
            uint16_t idx = (uint16_t)(start_addr + i);
            uint8_t  bit = (uint8_t)((coils[idx / 8] >> (idx % 8)) & 0x01u);
            resp[3 + i / 8] |= (uint8_t)(bit << (i % 8));
        }
        resp_len = (uint16_t)(3 + byte_count);
        crc = crc16(resp, resp_len);
        resp[resp_len++] = (uint8_t)(crc & 0xFFu);
        resp[resp_len++] = (uint8_t)(crc >> 8);
        return resp_len;

    /* ────────────────────────────────────────────────────────────────────────
       FC 03 — Read Holding Registers
    ──────────────────────────────────────────────────────────────────────── */
    case MB_FC_READ_HOLDING:
        if (req_len < 6)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        start_addr = (uint16_t)((uint16_t)req[2] << 8 | req[3]);
        quantity   = (uint16_t)((uint16_t)req[4] << 8 | req[5]);
        /* Максимум 125: ответ = 3 + 125*2 + 2 = 255 байт */
        if (quantity == 0 || quantity > 125)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        if ((uint32_t)start_addr + quantity > MB_REG_COUNT)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_ADDR, resp);

        resp[0] = addr;
        resp[1] = fc;
        resp[2] = (uint8_t)(quantity * 2);
        for (uint16_t i = 0; i < quantity; i++) {
            uint16_t val = holding_regs[start_addr + i];
            resp[3 + i * 2]     = (uint8_t)(val >> 8);
            resp[3 + i * 2 + 1] = (uint8_t)(val & 0xFFu);
        }
        resp_len = (uint16_t)(3 + quantity * 2);
        crc = crc16(resp, resp_len);
        resp[resp_len++] = (uint8_t)(crc & 0xFFu);
        resp[resp_len++] = (uint8_t)(crc >> 8);
        return resp_len;

    /* ────────────────────────────────────────────────────────────────────────
       FC 04 — Read Input Registers (отображены на Holding Registers)
    ──────────────────────────────────────────────────────────────────────── */
    case MB_FC_READ_INPUT:
        if (req_len < 6)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        start_addr = (uint16_t)((uint16_t)req[2] << 8 | req[3]);
        quantity   = (uint16_t)((uint16_t)req[4] << 8 | req[5]);
        if (quantity == 0 || quantity > 125)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        if ((uint32_t)start_addr + quantity > MB_REG_COUNT)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_ADDR, resp);

        resp[0] = addr;
        resp[1] = fc;
        resp[2] = (uint8_t)(quantity * 2);
        for (uint16_t i = 0; i < quantity; i++) {
            uint16_t val = holding_regs[start_addr + i];
            resp[3 + i * 2]     = (uint8_t)(val >> 8);
            resp[3 + i * 2 + 1] = (uint8_t)(val & 0xFFu);
        }
        resp_len = (uint16_t)(3 + quantity * 2);
        crc = crc16(resp, resp_len);
        resp[resp_len++] = (uint8_t)(crc & 0xFFu);
        resp[resp_len++] = (uint8_t)(crc >> 8);
        return resp_len;

    /* ────────────────────────────────────────────────────────────────────────
       FC 05 — Write Single Coil
    ──────────────────────────────────────────────────────────────────────── */
    case MB_FC_WRITE_SINGLE_COIL:
        if (req_len < 6)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        start_addr = (uint16_t)((uint16_t)req[2] << 8 | req[3]);
        reg_val    = (uint16_t)((uint16_t)req[4] << 8 | req[5]);
        /* Допустимо только 0x0000 (OFF) или 0xFF00 (ON) */
        if (reg_val != 0x0000u && reg_val != 0xFF00u)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        if (start_addr >= MB_COIL_COUNT)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_ADDR, resp);

        if (reg_val == 0xFF00u)
            coils[start_addr / 8] |=  (uint8_t)(1u << (start_addr % 8));
        else
            coils[start_addr / 8] &= (uint8_t)~(1u << (start_addr % 8));

        /* Ответ — зеркало запроса (первые 6 байт) с новым CRC */
        memcpy(resp, req, 6);
        crc = crc16(resp, 6);
        resp[6] = (uint8_t)(crc & 0xFFu);
        resp[7] = (uint8_t)(crc >> 8);
        return 8;

    /* ────────────────────────────────────────────────────────────────────────
       FC 06 — Write Single Register
    ──────────────────────────────────────────────────────────────────────── */
    case MB_FC_WRITE_SINGLE_REG:
        if (req_len < 6)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        start_addr = (uint16_t)((uint16_t)req[2] << 8 | req[3]);
        reg_val    = (uint16_t)((uint16_t)req[4] << 8 | req[5]);
        if (start_addr >= MB_REG_COUNT)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_ADDR, resp);

        holding_regs[start_addr] = reg_val;

        memcpy(resp, req, 6);
        crc = crc16(resp, 6);
        resp[6] = (uint8_t)(crc & 0xFFu);
        resp[7] = (uint8_t)(crc >> 8);
        return 8;

    /* ────────────────────────────────────────────────────────────────────────
       FC 15 (0x0F) — Write Multiple Coils
    ──────────────────────────────────────────────────────────────────────── */
    case MB_FC_WRITE_MULTI_COILS:
        if (req_len < 7)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        start_addr = (uint16_t)((uint16_t)req[2] << 8 | req[3]);
        quantity   = (uint16_t)((uint16_t)req[4] << 8 | req[5]);
        byte_count = req[6];
        if (quantity == 0 || byte_count != (uint8_t)((quantity + 7) / 8))
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        if ((uint32_t)start_addr + quantity > MB_COIL_COUNT)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_ADDR, resp);
        if (req_len < (uint16_t)(7u + byte_count + 2u))
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);

        for (uint16_t i = 0; i < quantity; i++) {
            uint8_t  bit     = (uint8_t)((req[7 + i / 8] >> (i % 8)) & 0x01u);
            uint16_t bit_idx = (uint16_t)(start_addr + i);
            if (bit)
                coils[bit_idx / 8] |=  (uint8_t)(1u << (bit_idx % 8));
            else
                coils[bit_idx / 8] &= (uint8_t)~(1u << (bit_idx % 8));
        }

        resp[0] = addr;
        resp[1] = fc;
        resp[2] = req[2];
        resp[3] = req[3];
        resp[4] = req[4];
        resp[5] = req[5];
        crc = crc16(resp, 6);
        resp[6] = (uint8_t)(crc & 0xFFu);
        resp[7] = (uint8_t)(crc >> 8);
        return 8;

    /* ────────────────────────────────────────────────────────────────────────
       FC 16 (0x10) — Write Multiple Registers
    ──────────────────────────────────────────────────────────────────────── */
    case MB_FC_WRITE_MULTI_REGS:
        if (req_len < 7)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        start_addr = (uint16_t)((uint16_t)req[2] << 8 | req[3]);
        quantity   = (uint16_t)((uint16_t)req[4] << 8 | req[5]);
        byte_count = req[6];
        /* Максимум 123 регистра: запрос = 7 + 123*2 + 2 = 255 байт */
        if (quantity == 0 || quantity > 123 || byte_count != (uint8_t)(quantity * 2))
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);
        if ((uint32_t)start_addr + quantity > MB_REG_COUNT)
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_ADDR, resp);
        if (req_len < (uint16_t)(7u + byte_count + 2u))
            return build_exception(addr, fc, MB_EX_ILLEGAL_DATA_VALUE, resp);

        for (uint16_t i = 0; i < quantity; i++) {
            holding_regs[start_addr + i] =
                (uint16_t)((uint16_t)req[7 + i * 2] << 8 | req[7 + i * 2 + 1]);
        }

        resp[0] = addr;
        resp[1] = fc;
        resp[2] = req[2];
        resp[3] = req[3];
        resp[4] = req[4];
        resp[5] = req[5];
        crc = crc16(resp, 6);
        resp[6] = (uint8_t)(crc & 0xFFu);
        resp[7] = (uint8_t)(crc >> 8);
        return 8;

    /* ────────────────────────────────────────────────────────────────────────
       Неизвестный код функции
    ──────────────────────────────────────────────────────────────────────── */
    default:
        return build_exception(addr, fc, MB_EX_ILLEGAL_FUNCTION, resp);
    }
}
