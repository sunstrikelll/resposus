#ifndef LCD_HD44780_H
#define LCD_HD44780_H

#include <stdint.h>

/* ── Pin mapping (from schematic БУСШ.02.101.01 Э3) ──────────────────────────
   20×2 HD44780-compatible LCD, 4-bit mode, RW tied to GND.

   RS  → PB8  (GPIOB, pin 61 on MCU)
   EN  → PC4  (GPIOC, pin 24 on MCU)
   D4  → PC0  (GPIOC, pin  8 on MCU)
   D5  → PC1  (GPIOC, pin  9 on MCU)
   D6  → PC2  (GPIOC, pin 10 on MCU)
   D7  → PC3  (GPIOC, pin 11 on MCU)                                        */

#define LCD_RS_PORT     GPIOB
#define LCD_RS_RCU      RCU_GPIOB
#define LCD_PIN_RS      GPIO_PIN_8

#define LCD_DATA_PORT   GPIOC
#define LCD_DATA_RCU    RCU_GPIOC
#define LCD_PIN_E       GPIO_PIN_4
#define LCD_PIN_D4      GPIO_PIN_0
#define LCD_PIN_D5      GPIO_PIN_1
#define LCD_PIN_D6      GPIO_PIN_2
#define LCD_PIN_D7      GPIO_PIN_3

/* ── Display geometry ────────────────────────────────────────────────────── */
#define LCD_COLS        20
#define LCD_ROWS        2

/* ── API ──────────────────────────────────────────────────────────────────── */
void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_putchar(char c);
void lcd_print(const char *str);
void lcd_print_at(uint8_t row, uint8_t col, const char *str);

/* Вывод строки в кодировке Windows-1251 (legacy).
   Использовать с литералами вида "\xCF\xD0\xC8\xC2\xC5\xD2" или
   с файлами, сохранёнными в Windows-1251.                        */
void lcd_print_win1251(const char *str);
void lcd_print_win1251_at(uint8_t row, uint8_t col, const char *str);

/* Вывод строки в кодировке UTF-8 (исходники .c — UTF-8).
   Кириллица (русские буквы, ё, Ё) автоматически преобразуется в
   ROM-коды HD44780; ASCII и спецсимволы (°, «, ») — пропускаются.
   Подсчёт длины — по символам, а не байтам.                        */
void lcd_print_utf8(const char *str);
void lcd_print_utf8_at(uint8_t row, uint8_t col, const char *str);

/* Перекодировать UTF-8 → Win-1251 в выходной буфер dst шириной width
   символов (с заполнением пробелами и обрезкой по символам, не байтам).
   dst должен быть длиной width+1.                                  */
void lcd_utf8_to_win1251(const char *src, char *dst, uint8_t width);

#endif /* LCD_HD44780_H */
