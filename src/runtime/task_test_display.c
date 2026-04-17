/*  task_test_display.c — TEST-режим: визуализация test_line_N
 *
 *  Мастер Modbus пишет 20-символьные строки (Win-1251) в регистры
 *  test_line_0..3, задача их выводит на 4 строки LCD и копирует в
 *  display_line_0..3 (чтобы мастер мог проверить, что именно было
 *  отображено).
 */

#include "task_test_display.h"

#include "FreeRTOS.h"
#include "task.h"

#include "modbus_table.h"
#include "lcd_hd44780.h"

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

        for (uint8_t r = 0; r < 4u; r++) {
            /* Читаем строку из test_line_N, выравниваем до 20 символов */
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

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(200));
    }
}

void task_test_display_start(void)
{
    xTaskCreate(task_test_display, "tst_lcd", 512, NULL, 2, NULL);
}
