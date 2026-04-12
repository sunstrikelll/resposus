#include "lcd_hd44780.h"
#include "gd32f10x.h"

/* ── Адреса начала строк в DDRAM (20x4) ──────────────────────────────────── */
static const uint8_t row_offsets[LCD_ROWS] = { 0x00, 0x40, 0x14, 0x54 };

/* ── Микросекундная задержка (72 MHz, -O0...-Og) ─────────────────────────── */
static void delay_us(volatile uint32_t us)
{
    /* ~12 тактов на итерацию при -Og; 72 MHz → 72 такта/мкс → ~6 итер/мкс */
    us *= 6;
    while (us--) {
        __asm volatile ("nop");
    }
}

static void delay_ms(uint32_t ms)
{
    while (ms--) {
        delay_us(1000);
    }
}

/* ── Низкоуровневая работа с пинами ──────────────────────────────────────── */

static void lcd_pulse_enable(void)
{
    gpio_bit_set(LCD_GPIO_PORT, LCD_PIN_E);
    delay_us(1);
    gpio_bit_reset(LCD_GPIO_PORT, LCD_PIN_E);
    delay_us(50);
}

static void lcd_write_nibble(uint8_t nibble)
{
    if (nibble & 0x01) gpio_bit_set(LCD_GPIO_PORT, LCD_PIN_D4);
    else               gpio_bit_reset(LCD_GPIO_PORT, LCD_PIN_D4);

    if (nibble & 0x02) gpio_bit_set(LCD_GPIO_PORT, LCD_PIN_D5);
    else               gpio_bit_reset(LCD_GPIO_PORT, LCD_PIN_D5);

    if (nibble & 0x04) gpio_bit_set(LCD_GPIO_PORT, LCD_PIN_D6);
    else               gpio_bit_reset(LCD_GPIO_PORT, LCD_PIN_D6);

    if (nibble & 0x08) gpio_bit_set(LCD_GPIO_PORT, LCD_PIN_D7);
    else               gpio_bit_reset(LCD_GPIO_PORT, LCD_PIN_D7);

    lcd_pulse_enable();
}

static void lcd_send(uint8_t data, uint8_t rs)
{
    if (rs) gpio_bit_set(LCD_GPIO_PORT, LCD_PIN_RS);
    else    gpio_bit_reset(LCD_GPIO_PORT, LCD_PIN_RS);

    lcd_write_nibble((uint8_t)(data >> 4));   /* старший ниббл */
    lcd_write_nibble((uint8_t)(data & 0x0F)); /* младший ниббл */
}

static void lcd_command(uint8_t cmd)
{
    lcd_send(cmd, 0);
    if (cmd == LCD_CMD_CLEAR || cmd == LCD_CMD_HOME)
        delay_ms(2);
    else
        delay_us(50);
}

static void lcd_data(uint8_t data)
{
    lcd_send(data, 1);
    delay_us(50);
}

/* ── Публичное API ────────────────────────────────────────────────────────── */

void lcd_init(void)
{
    /* Тактирование и настройка пинов */
    rcu_periph_clock_enable(LCD_GPIO_RCU);
    gpio_init(LCD_GPIO_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              LCD_PIN_RS | LCD_PIN_E | LCD_PIN_D4 | LCD_PIN_D5 |
              LCD_PIN_D6 | LCD_PIN_D7);

    gpio_bit_reset(LCD_GPIO_PORT, LCD_PIN_RS | LCD_PIN_E |
                   LCD_PIN_D4 | LCD_PIN_D5 | LCD_PIN_D6 | LCD_PIN_D7);

    /* ── Инициализация HD44780 по datasheet (4-bit) ──────────────────────── */
    delay_ms(50);               /* ожидание после подачи питания */

    gpio_bit_reset(LCD_GPIO_PORT, LCD_PIN_RS);

    /* Три попытки перевести в 8-bit (сброс в известное состояние) */
    lcd_write_nibble(0x03);
    delay_ms(5);
    lcd_write_nibble(0x03);
    delay_us(150);
    lcd_write_nibble(0x03);
    delay_us(150);

    /* Переключение в 4-bit режим */
    lcd_write_nibble(0x02);
    delay_us(150);

    /* Теперь можно отправлять полные команды (4-bit) */
    lcd_command(LCD_CMD_FUNCTION_4BIT);     /* 4-bit, 2 строки, 5x8 */
    lcd_command(LCD_CMD_DISPLAY_OFF);
    lcd_command(LCD_CMD_CLEAR);
    lcd_command(LCD_CMD_ENTRY_MODE);        /* инкремент, без сдвига */
    lcd_command(LCD_CMD_DISPLAY_ON);        /* дисплей вкл, курсор выкл */
}

void lcd_clear(void)
{
    lcd_command(LCD_CMD_CLEAR);
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
    if (row >= LCD_ROWS) row = LCD_ROWS - 1;
    if (col >= LCD_COLS) col = LCD_COLS - 1;
    lcd_command((uint8_t)(LCD_CMD_SET_DDRAM | (row_offsets[row] + col)));
}

/* ── UTF-8 → CP1251 для кириллицы ─────────────────────────────────────────── */
/* UTF-8 кириллица: 0xD0 0x90..0xBF = А..п, 0xD1 0x80..0x8F = р..я          */
/* CP1251:          А..Я = 0xC0..0xDF, а..я = 0xE0..0xFF                     */
/* Ё = UTF-8 D0 81 → CP1251 0xA8,  ё = UTF-8 D1 91 → CP1251 0xB8           */
static uint8_t lcd_utf8_to_cp1251(const uint8_t **pp)
{
    const uint8_t *p = *pp;
    uint8_t c = *p++;

    if (c < 0x80) {                     /* ASCII */
        *pp = p;
        return c;
    }

    if (c == 0xD0) {
        uint8_t c2 = *p++;
        *pp = p;
        if (c2 == 0x81) return 0xA8;    /* Ё */
        if (c2 >= 0x90 && c2 <= 0xBF)   /* А(0x90)..п(0xBF) → 0xC0..0xEF */
            return (uint8_t)(c2 + 0x30);
        return '?';
    }

    if (c == 0xD1) {
        uint8_t c2 = *p++;
        *pp = p;
        if (c2 == 0x91) return 0xB8;    /* ё */
        if (c2 >= 0x80 && c2 <= 0x8F)   /* р(0x80)..я(0x8F) → 0xF0..0xFF */
            return (uint8_t)(c2 + 0x70);
        return '?';
    }

    /* Неизвестный многобайтовый — пропустить continuation bytes */
    while ((*p & 0xC0) == 0x80) p++;
    *pp = p;
    return '?';
}

void lcd_putchar(char c)
{
    lcd_data((uint8_t)c);
}

void lcd_print(const char *str)
{
    const uint8_t *p = (const uint8_t *)str;
    while (*p) {
        lcd_data(lcd_utf8_to_cp1251(&p));
    }
}

void lcd_print_at(uint8_t row, uint8_t col, const char *str)
{
    lcd_set_cursor(row, col);
    lcd_print(str);
}
