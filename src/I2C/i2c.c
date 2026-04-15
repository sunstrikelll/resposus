#include "i2c.h"
#include "gd32f10x.h"

/* ── Hardware mapping ────────────────────────────────────────────────────── */
#define I2C_BUS         I2C0
#define I2C_RCU_GPIO    RCU_GPIOB
#define I2C_RCU_PERIPH  RCU_I2C0
#define I2C_GPIO_PORT   GPIOB
#define I2C_PIN_SCL     GPIO_PIN_6
#define I2C_PIN_SDA     GPIO_PIN_7
#define I2C_SPEED_HZ    100000U

/* ── Timeout (iterations, not time) ─────────────────────────────────────── */
#define I2C_TIMEOUT     300000U   /* >> 1 byte @ 100 kHz */

/* Clear ADDSEND by reading STAT0 then STAT1 */
#define CLEAR_ADDSEND() \
    do { (void)I2C_STAT0(I2C_BUS); (void)I2C_STAT1(I2C_BUS); } while (0)

/* ── Internal helpers ────────────────────────────────────────────────────── */

/* Wait until flag becomes SET.  Generates STOP and returns -1 on timeout. */
static int wait_set(uint32_t flag)
{
    uint32_t t = I2C_TIMEOUT;
    while (i2c_flag_get(I2C_BUS, flag) == RESET) {
        if (--t == 0u) {
            i2c_stop_on_bus(I2C_BUS);
            return -1;
        }
    }
    return 0;
}

/* Wait until flag becomes RESET.  Returns -1 on timeout (no STOP). */
static int wait_reset(uint32_t flag)
{
    uint32_t t = I2C_TIMEOUT;
    while (i2c_flag_get(I2C_BUS, flag) != RESET) {
        if (--t == 0u) return -1;
    }
    return 0;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void i2c1_init(void)
{
    rcu_periph_clock_enable(I2C_RCU_GPIO);
    rcu_periph_clock_enable(I2C_RCU_PERIPH);

    /* PB6=SCL, PB7=SDA: alternate function, open-drain */
    gpio_init(I2C_GPIO_PORT, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ,
              I2C_PIN_SCL | I2C_PIN_SDA);

    i2c_deinit(I2C_BUS);
    i2c_clock_config(I2C_BUS, I2C_SPEED_HZ, I2C_DTCY_2);
    i2c_mode_addr_config(I2C_BUS, I2C_I2CMODE_ENABLE,
                         I2C_ADDFORMAT_7BITS, 0x00U);
    i2c_enable(I2C_BUS);
    i2c_ack_config(I2C_BUS, I2C_ACK_ENABLE);
}

int i2c1_write(uint8_t dev_addr, const uint8_t *data, uint16_t len)
{
    if (wait_reset(I2C_FLAG_I2CBSY) != 0) return -1;

    i2c_start_on_bus(I2C_BUS);
    if (wait_set(I2C_FLAG_SBSEND) != 0) return -1;

    i2c_master_addressing(I2C_BUS, (uint32_t)dev_addr, I2C_TRANSMITTER);
    if (wait_set(I2C_FLAG_ADDSEND) != 0) return -1;
    CLEAR_ADDSEND();

    for (uint16_t i = 0u; i < len; i++) {
        if (wait_set(I2C_FLAG_TBE) != 0) return -1;
        i2c_data_transmit(I2C_BUS, data[i]);
    }
    if (wait_set(I2C_FLAG_BTC) != 0) return -1;

    i2c_stop_on_bus(I2C_BUS);
    return 0;
}

int i2c1_write_read(uint8_t dev_addr,
                    const uint8_t *wr, uint16_t wr_len,
                    uint8_t       *rd, uint16_t rd_len)
{
    if (rd_len == 0u)
        return i2c1_write(dev_addr, wr, wr_len);

    /* ── Write phase ──────────────────────────────────────────────────── */
    if (wait_reset(I2C_FLAG_I2CBSY) != 0) return -1;

    i2c_start_on_bus(I2C_BUS);
    if (wait_set(I2C_FLAG_SBSEND) != 0) return -1;

    i2c_master_addressing(I2C_BUS, (uint32_t)dev_addr, I2C_TRANSMITTER);
    if (wait_set(I2C_FLAG_ADDSEND) != 0) return -1;
    CLEAR_ADDSEND();

    for (uint16_t i = 0u; i < wr_len; i++) {
        if (wait_set(I2C_FLAG_TBE) != 0) return -1;
        i2c_data_transmit(I2C_BUS, wr[i]);
    }
    if (wait_set(I2C_FLAG_BTC) != 0) return -1;

    /* ── Repeated START ────────────────────────────────────────────────── */
    i2c_start_on_bus(I2C_BUS);
    if (wait_set(I2C_FLAG_SBSEND) != 0) return -1;

    i2c_master_addressing(I2C_BUS, (uint32_t)dev_addr, I2C_RECEIVER);

    if (rd_len == 1u) {
        /* Disable ACK before clearing ADDSEND so NACK is sent immediately */
        i2c_ack_config(I2C_BUS, I2C_ACK_DISABLE);
        if (wait_set(I2C_FLAG_ADDSEND) != 0) return -1;
        CLEAR_ADDSEND();
        i2c_stop_on_bus(I2C_BUS);
        if (wait_set(I2C_FLAG_RBNE) != 0) return -1;
        rd[0] = (uint8_t)i2c_data_receive(I2C_BUS);

    } else if (rd_len == 2u) {
        /* ACKPOS_NEXT: ACK/NACK applies to the next byte, not the current */
        i2c_ackpos_config(I2C_BUS, I2C_ACKPOS_NEXT);
        i2c_ack_config(I2C_BUS, I2C_ACK_DISABLE);
        if (wait_set(I2C_FLAG_ADDSEND) != 0) return -1;
        CLEAR_ADDSEND();
        if (wait_set(I2C_FLAG_BTC) != 0) return -1;
        i2c_stop_on_bus(I2C_BUS);
        rd[0] = (uint8_t)i2c_data_receive(I2C_BUS);
        rd[1] = (uint8_t)i2c_data_receive(I2C_BUS);

    } else {
        /* N >= 3: ACK all except last two */
        i2c_ack_config(I2C_BUS, I2C_ACK_ENABLE);
        if (wait_set(I2C_FLAG_ADDSEND) != 0) return -1;
        CLEAR_ADDSEND();

        for (uint16_t i = 0u; i < (uint16_t)(rd_len - 3u); i++) {
            if (wait_set(I2C_FLAG_RBNE) != 0) return -1;
            rd[i] = (uint8_t)i2c_data_receive(I2C_BUS);
        }
        /* Third-to-last: wait BTC, then NACK, read */
        if (wait_set(I2C_FLAG_BTC) != 0) return -1;
        i2c_ack_config(I2C_BUS, I2C_ACK_DISABLE);
        rd[rd_len - 3u] = (uint8_t)i2c_data_receive(I2C_BUS);
        /* Second-to-last: wait BTC, STOP, read */
        if (wait_set(I2C_FLAG_BTC) != 0) return -1;
        i2c_stop_on_bus(I2C_BUS);
        rd[rd_len - 2u] = (uint8_t)i2c_data_receive(I2C_BUS);
        /* Last: wait RBNE, read */
        if (wait_set(I2C_FLAG_RBNE) != 0) return -1;
        rd[rd_len - 1u] = (uint8_t)i2c_data_receive(I2C_BUS);
    }

    /* Restore defaults for next transaction */
    i2c_ack_config(I2C_BUS, I2C_ACK_ENABLE);
    i2c_ackpos_config(I2C_BUS, I2C_ACKPOS_CURRENT);
    return 0;
}
