#ifndef I2C_H
#define I2C_H

#include <stdint.h>

/* I2C0 polling driver.
   PB6 = SCL, PB7 = SDA, open-drain AF, 100 kHz.                           */

void i2c1_init(void);

/* Write `len` bytes to device with 8-bit address `dev_addr` (e.g. 0xA0).
   Returns 0 on success, -1 on timeout/error.                               */
int  i2c1_write(uint8_t dev_addr, const uint8_t *data, uint16_t len);

/* Write `wr_len` bytes then repeated-START and read `rd_len` bytes.
   Used for random-address reads (e.g. EEPROM).
   Returns 0 on success, -1 on timeout/error.                               */
int  i2c1_write_read(uint8_t dev_addr,
                     const uint8_t *wr, uint16_t wr_len,
                     uint8_t       *rd, uint16_t rd_len);

#endif /* I2C_H */
