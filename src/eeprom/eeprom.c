#include "eeprom.h"
#include "i2c.h"

/* AT24C64: 7-bit addr 0x50  →  write-address byte 0xA0 */
#define EEPROM_I2C_ADDR   0xA0U
#define EEPROM_PAGE_SIZE  32U    /* байт на страницу (AT24C64) */

/* ── CRC-16/IBM (Modbus, полином 0xA001) ────────────────────────────────── */
static uint16_t crc16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0u; i < len; i++) {
        crc ^= (uint16_t)buf[i];
        for (uint8_t j = 0u; j < 8u; j++) {
            if (crc & 0x0001u)
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ~5 мс задержка на завершение цикла записи AT24C64 (≤ 5 мс по datasheet).
   108 МГц, ~3 такта на итерацию → 180 000 ≈ 5 мс.                         */
static void delay_write_cycle(void)
{
    volatile uint32_t cnt = 180000U;
    while (cnt--) {}
}

/* Записать chunk байт по адресу EEPROM mem_addr.
   chunk ≤ EEPROM_PAGE_SIZE, не должен пересекать страничную границу.       */
static int page_write(uint16_t mem_addr, const uint8_t *data, uint8_t chunk)
{
    uint8_t buf[2U + EEPROM_PAGE_SIZE];
    buf[0] = (uint8_t)(mem_addr >> 8);
    buf[1] = (uint8_t)(mem_addr & 0xFFu);
    for (uint8_t i = 0u; i < chunk; i++) {
        buf[2u + i] = data[i];
    }
    return i2c1_write(EEPROM_I2C_ADDR, buf, (uint16_t)(2u + chunk));
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void eeprom_init(void)
{
    i2c1_init();
}

int eeprom_write_regs(const uint8_t *data, uint16_t len)
{
    if (len > EEPROM_DATA_SIZE) return -1;

    /* Вычислить CRC до записи */
    uint16_t crc = crc16(data, len);

    /* Записать данные постранично */
    uint16_t mem_addr  = 0u;
    uint16_t remaining = len;

    while (remaining > 0u) {
        uint16_t page_space = (uint16_t)(EEPROM_PAGE_SIZE
                                         - (mem_addr % EEPROM_PAGE_SIZE));
        uint16_t chunk      = (remaining < page_space) ? remaining : page_space;

        if (page_write(mem_addr, data, (uint8_t)chunk) != 0) return -1;
        delay_write_cycle();

        data      += chunk;
        mem_addr  += chunk;
        remaining -= chunk;
    }

    /* Записать CRC16 по смещению EEPROM_CRC_OFFSET (510): little-endian */
    uint8_t crc_buf[2] = {
        (uint8_t)(crc & 0xFFu),
        (uint8_t)(crc >> 8)
    };
    if (page_write(EEPROM_CRC_OFFSET, crc_buf, 2u) != 0) return -1;
    delay_write_cycle();

    return 0;
}

int eeprom_read_regs(uint8_t *data, uint16_t len)
{
    if (len > EEPROM_DATA_SIZE) return -1;
    uint8_t addr[2] = {0u, 0u};
    return i2c1_write_read(EEPROM_I2C_ADDR, addr, 2u, data, len);
}

int eeprom_check_crc(uint16_t len)
{
    static uint8_t tmp[EEPROM_SIZE];   /* 512 байт в .bss */
    uint8_t addr[2] = {0u, 0u};

    if (len > EEPROM_DATA_SIZE) return -1;

    /* Читаем весь чип одним блоком: данные [0..len-1] + CRC по смещ. 510..511. */
    if (i2c1_write_read(EEPROM_I2C_ADDR, addr, 2u, tmp, EEPROM_SIZE) != 0)
        return -1;

    /* Считаем CRC ровно по тому количеству байт, которое было записано
       (eeprom_write_regs использует ту же длину).  Это исправляет баг
       прошлой версии: вычисление по фиксированным 510 байтам не сходилось
       с записанным CRC, и settings_load() при каждом старте откатывался
       на заводские дефолты — пользовательские уставки терялись.            */
    uint16_t computed = crc16(tmp, len);
    uint16_t stored   = (uint16_t)((uint16_t)tmp[EEPROM_CRC_OFFSET]
                                   | ((uint16_t)tmp[EEPROM_CRC_OFFSET + 1u] << 8));
    return (computed == stored) ? 0 : -1;
}
