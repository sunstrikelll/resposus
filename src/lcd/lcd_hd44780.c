#include "lcd_hd44780.h"
#include "gd32f10x.h"
#include "gd32f10x_timer.h"
#include "FreeRTOS.h"
#include "task.h"

/* DDRAM row start addresses for 20x4 */
static const uint8_t row_offsets[LCD_ROWS] = { 0x00, 0x40, 0x14, 0x54 };

/* ── Delays ──────────────────────────────────────────────────────────────────
   TIMER3 is configured: prescaler=95, period=999 → counter ticks at 1 MHz
   (1 µs per count), resets to 0 every 1000 counts (1 ms).
   delay_us() is accurate at any optimisation level; max us = 999.           */

static void delay_us(uint32_t us)
{
    uint32_t start = TIMER_CNT(TIMER3) & 0xFFFFU;
    uint32_t elapsed;
    do {
        uint32_t now = TIMER_CNT(TIMER3) & 0xFFFFU;
        elapsed = (now >= start) ? (now - start) : (1000u + now - start);
    } while (elapsed < us);
}

/* For ms-range delays use the FreeRTOS tick (must be called from task ctx) */
static void delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* ── Nibble write ────────────────────────────────────────────────────────────
   Puts nibble[3:0] onto D7..D4 and generates one E pulse.
   RS must be set before calling.                                             */
static void lcd_write_nibble(uint8_t nibble)
{
    gpio_bit_write(LCD_DATA_PORT, LCD_PIN_D4, (nibble & 0x01) ? SET : RESET);
    gpio_bit_write(LCD_DATA_PORT, LCD_PIN_D5, (nibble & 0x02) ? SET : RESET);
    gpio_bit_write(LCD_DATA_PORT, LCD_PIN_D6, (nibble & 0x04) ? SET : RESET);
    gpio_bit_write(LCD_DATA_PORT, LCD_PIN_D7, (nibble & 0x08) ? SET : RESET);

    delay_us(2);                              /* data setup before E rises  */
    gpio_bit_set(LCD_DATA_PORT, LCD_PIN_E);
    delay_us(2);                              /* E high: min 450 ns         */
    gpio_bit_reset(LCD_DATA_PORT, LCD_PIN_E);
    delay_us(100);                            /* E low / execution: min 37µs */
}

/* ── Byte write ──────────────────────────────────────────────────────────── */
static void lcd_send(uint8_t byte, uint8_t rs)
{
    gpio_bit_write(LCD_RS_PORT, LCD_PIN_RS, rs ? SET : RESET);
    lcd_write_nibble(byte >> 4);    /* high nibble first */
    lcd_write_nibble(byte & 0x0F);  /* low nibble        */
}

/* ── Command ─────────────────────────────────────────────────────────────── */
static void lcd_cmd(uint8_t cmd)
{
    lcd_send(cmd, 0);
    /* Clear (0x01) and Return Home (0x02) need >1.52 ms */
    if (cmd <= 0x03)
        delay_ms(2);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void lcd_init(void)
{
    /* Clock RS port (GPIOB) */
    rcu_periph_clock_enable(LCD_RS_RCU);
    gpio_init(LCD_RS_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, LCD_PIN_RS);
    gpio_bit_reset(LCD_RS_PORT, LCD_PIN_RS);

    /* Clock data/enable port (GPIOC) */
    rcu_periph_clock_enable(LCD_DATA_RCU);
    gpio_init(LCD_DATA_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
              LCD_PIN_E | LCD_PIN_D4 | LCD_PIN_D5 | LCD_PIN_D6 | LCD_PIN_D7);
    gpio_bit_reset(LCD_DATA_PORT,
                   LCD_PIN_E | LCD_PIN_D4 | LCD_PIN_D5 | LCD_PIN_D6 | LCD_PIN_D7);

    delay_ms(50);                   /* power-on: wait >40 ms               */

    /* ── HD44780 4-bit init sequence (datasheet fig. 24) ──────────────────
       RS is already low; send 0x03 three times to guarantee reset.        */
    lcd_write_nibble(0x03);         /* attempt 1                           */
    delay_ms(5);                    /* >4.1 ms                             */

    lcd_write_nibble(0x03);         /* attempt 2                           */
    delay_us(200);                  /* >100 µs                             */

    lcd_write_nibble(0x03);         /* attempt 3                           */
    delay_us(200);

    lcd_write_nibble(0x02);         /* switch to 4-bit mode                */
    delay_us(200);

    /* ── Configure display (now in 4-bit mode, full 2-nibble commands) ── */
    lcd_cmd(0x28);  /* Function Set: 4-bit | 2 lines | 5×8 dots           */
    lcd_cmd(0x08);  /* Display OFF                                         */
    lcd_cmd(0x01);  /* Clear display                                       */
    lcd_cmd(0x06);  /* Entry mode: cursor right, no display shift          */
    lcd_cmd(0x0C);  /* Display ON, cursor OFF, blink OFF                   */
}

void lcd_clear(void)
{
    lcd_cmd(0x01);
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
    if (row >= LCD_ROWS) row = LCD_ROWS - 1;
    if (col >= LCD_COLS) col = LCD_COLS - 1;
    lcd_cmd((uint8_t)(0x80 | (row_offsets[row] + col)));
}

void lcd_putchar(char c)
{
    lcd_send((uint8_t)c, 1);
}

void lcd_print(const char *str)
{
    while (*str)
        lcd_putchar(*str++);
}

void lcd_print_at(uint8_t row, uint8_t col, const char *str)
{
    lcd_set_cursor(row, col);
    lcd_print(str);
}

/* ── Windows-1251 → HD44780 Cyrillic ROM (A02) ───────────────────────────
   Символы 0xC0–0xFF (А–я). Буквы, совпадающие с Latin, используют те же
   коды ASCII; уникальные кириллические — слоты ROM 0xA0–0xC7/0xE0–0xE6.  */
static const uint8_t win1251_to_lcd[64] = {
    /* C0 А */0x41, /* C1 Б */0xA0, /* C2 В */0x42, /* C3 Г */0xA1,
    /* C4 Д */0xE0, /* C5 Е */0x45, /* C6 Ж */0xA3, /* C7 З */0xA4,
    /* C8 И */0xA5, /* C9 Й */0xA6, /* CA К */0x4B, /* CB Л */0xA7,
    /* CC М */0x4D, /* CD Н */0x48, /* CE О */0x4F, /* CF П */0xA8,
    /* D0 Р */0x50, /* D1 С */0x43, /* D2 Т */0x54, /* D3 У */0xA9,
    /* D4 Ф */0xAA, /* D5 Х */0x58, /* D6 Ц */0xE1, /* D7 Ч */0xAB,
    /* D8 Ш */0xAC, /* D9 Щ */0xE2, /* DA Ъ */0xAD, /* DB Ы */0xAE,
    /* DC Ь */0xAF, /* DD Э */0xB0, /* DE Ю */0xB1, /* DF Я */0xA2,
    /* E0 а */0x61, /* E1 б */0xB2, /* E2 в */0xB3, /* E3 г */0xB4,
    /* E4 д */0xE3, /* E5 е */0x65, /* E6 ж */0xB6, /* E7 з */0xB7,
    /* E8 и */0xB8, /* E9 й */0xB9, /* EA к */0xBA, /* EB л */0xBB,
    /* EC м */0xBC, /* ED н */0xBD, /* EE о */0x6F, /* EF п */0xBE,
    /* F0 р */0x70, /* F1 с */0x63, /* F2 т */0xBF, /* F3 у */0x79,
    /* F4 ф */0xE4, /* F5 х */0x78, /* F6 ц */0xE5, /* F7 ч */0xC0,
    /* F8 ш */0xC1, /* F9 щ */0xE6, /* FA ъ */0xC2, /* FB ы */0xC3,
    /* FC ь */0xC4, /* FD э */0xC5, /* FE ю */0xC6, /* FF я */0xC7,
};

void lcd_print_win1251(const char *str)
{
    uint8_t c;
    while ((c = (uint8_t)*str++) != 0)
    {
        if (c >= 0xC0)
            lcd_send(win1251_to_lcd[c - 0xC0], 1);
        else if (c == 0xB0)
            lcd_send(0xDF, 1);  /* ° (Win-1251) → градус в ROM HD44780 */
        else
            lcd_send(c, 1);     /* ASCII и спецсимволы — без изменений */
    }
}

void lcd_print_win1251_at(uint8_t row, uint8_t col, const char *str)
{
    lcd_set_cursor(row, col);
    lcd_print_win1251(str);
}
