#ifndef LCD_HD44780_H
#define LCD_HD44780_H

#include <stdint.h>

/* ── Конфигурация пинов (GPIOB) ──────────────────────────────────────────── */
/* MT-20S4M-2FLW: 20x4, HD44780-совместимый, 4-bit mode                     */
/* RW заземлён (только запись)                                               */

#define LCD_GPIO_PORT   GPIOB
#define LCD_GPIO_RCU    RCU_GPIOB

#define LCD_PIN_RS      GPIO_PIN_8
#define LCD_PIN_E       GPIO_PIN_9
#define LCD_PIN_D4      GPIO_PIN_12
#define LCD_PIN_D5      GPIO_PIN_13
#define LCD_PIN_D6      GPIO_PIN_14
#define LCD_PIN_D7      GPIO_PIN_15

/* ── Параметры дисплея ────────────────────────────────────────────────────── */
#define LCD_COLS        20
#define LCD_ROWS        4

/* ── Команды HD44780 ──────────────────────────────────────────────────────── */
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x06    /* I/D=1, S=0 */
#define LCD_CMD_DISPLAY_ON      0x0C    /* Display ON, Cursor OFF, Blink OFF */
#define LCD_CMD_DISPLAY_OFF     0x08
#define LCD_CMD_FUNCTION_4BIT   0x28    /* DL=0 (4-bit), N=1 (2 lines), F=0 (5x8) */
#define LCD_CMD_SET_DDRAM       0x80

/* ── API ──────────────────────────────────────────────────────────────────── */

void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_putchar(char c);
void lcd_print(const char *str);
void lcd_print_at(uint8_t row, uint8_t col, const char *str);

#endif /* LCD_HD44780_H */
