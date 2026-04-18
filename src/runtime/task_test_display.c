/*  task_test_display.c — TEST-режим: визуализация test_line_N
 *
 *  Мастер Modbus пишет 20-символьные строки (Win-1251) в регистры
 *  test_line_0..3, задача их выводит на 3 строки LCD (0..2) и копирует
 *  в display_line_0..2 (чтобы мастер мог проверить, что именно было
 *  отображено).
 *
 *  4-я строка (row 3) занята «живым» индикатором кнопок: показывает
 *  имя и номер физически удерживаемой кнопки в формате
 *      "Btn:PRG      N=0001 "
 *  (синхронно с тем, что зажигают LED в task_test_btn).
 */

#include "task_test_display.h"

#include "FreeRTOS.h"
#include "task.h"

#include "gd32f10x.h"
#include "buttons.h"
#include "modbus_table.h"
#include "lcd_hd44780.h"

/* Плоский список 20-символьных строк индикатора кнопок.
   Индекс = номер кнопки (0 — ни одна не нажата, 1..5 — кнопки).
   Формат:  4 + 8 + 1 + 6 + 1 = 20 символов (ровно ширина экрана). */
static const char * const s_btn_line[6] = {
    /*0 none    */ "Btn:---      N=0000 ",
    /*1 PRG     */ "Btn:PRG      N=0001 ",
    /*2 ONOFF   */ "Btn:ONOFF    N=0010 ",
    /*3 AUTO/MAN*/ "Btn:AUTO/MAN N=0011 ",
    /*4 UP      */ "Btn:UP       N=0100 ",
    /*5 DOWN    */ "Btn:DOWN     N=0101 ",
};

/* Вернуть 1, если кнопка физически замкнута на землю (active-LOW) */
static inline uint8_t btn_down(uint32_t pin)
{
    return (gpio_input_bit_get(BTN_PORT, pin) == RESET) ? 1u : 0u;
}

/* Выбрать номер текущей нажатой кнопки (приоритет: 1..5) */
static uint8_t current_btn_num(void)
{
    if (btn_down(BTN_PIN_PRG))      return 1u;
    if (btn_down(BTN_PIN_ONOFF))    return 2u;
    if (btn_down(BTN_PIN_AUTO_MAN)) return 3u;
    if (btn_down(BTN_PIN_UP))       return 4u;
    if (btn_down(BTN_PIN_DOWN))     return 5u;
    return 0u;
}

static void task_test_display(void *arg)
{
    static const uint16_t src_addr[4] = {
        MB_ADDR_TEST_LINE_0, MB_ADDR_TEST_LINE_1,
        MB_ADDR_TEST_LINE_2, MB_ADDR_TEST_LINE_3
    };
    static const uint16_t dst_addr[4] = {
        MB_ADDR_DISPLAY_LINE_0, MB_ADDR_DISPLAY_LINE_1,
        MB_ADDR_DISPLAY_LINE_2, MB_ADDR_DISPLAY_LINE_3
    };

    TickType_t xLastWake = xTaskGetTickCount();
    (void)arg;

    lcd_init();

    /* Начальная «подсказка», пока мастер не прислал строки */
    MB_WriteString(MB_ADDR_TEST_LINE_0, "--- TEST  MODE ---  ");
    MB_WriteString(MB_ADDR_TEST_LINE_1, "Write 4 strings via ");
    MB_WriteString(MB_ADDR_TEST_LINE_2, "Modbus: test_line_N ");
    MB_WriteString(MB_ADDR_TEST_LINE_3, "each = 20 chars.    ");

    for (;;)
    {
        char line[21];

        /* Строки 0..2 — зеркало test_line_N (пишет Modbus-мастер) */
        for (uint8_t r = 0; r < 3u; r++) {
            const char *src = MB_ReadString(src_addr[r]);
            uint8_t i = 0;
            while (src[i] && i < 20u) {
                line[i] = src[i];
                i++;
            }
            while (i < 20u) line[i++] = ' ';
            line[20] = '\0';

            lcd_print_win1251_at(r, 0, line);
            MB_WriteString(dst_addr[r], line);
        }

        /* Строка 3 — «живой» индикатор кнопки */
        {
            uint8_t n = current_btn_num();          /* 0..5              */
            const char *src = s_btn_line[n];
            for (uint8_t i = 0; i < 20u; i++) line[i] = src[i];
            line[20] = '\0';

            lcd_print_win1251_at(3, 0, line);
            MB_WriteString(dst_addr[3], line);
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(200));
    }
}

void task_test_display_start(void)
{
    xTaskCreate(task_test_display, "tst_lcd", 512, NULL, 2, NULL);
}
