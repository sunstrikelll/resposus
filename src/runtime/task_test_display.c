/*  task_test_display.c — TEST-режим: визуализация test_line_N
 *
 *  Дисплей 20×2.  Мастер Modbus пишет 20-символьную строку (Win-1251)
 *  в регистр test_line_0, задача выводит её в строку 0 LCD и копирует
 *  в display_line_0 (для контроля мастером).
 *
 *  Строка 1 LCD — диагностика кнопок.  Формат (20 символов):
 *
 *      "<имя-8>  <событие-8>  "
 *
 *        имя     — физически удерживаемая сейчас кнопка ("PRG     ",
 *                  "AUTO/MAN", "--------" если ни одна не нажата).
 *                  Читается напрямую из GPIO, без debounce.
 *        событие — ПОСЛЕДНЕЕ debounced-событие из MB_ADDR_BTN_EVENT,
 *                  декодированное в текст:
 *                      "КОРОТКОЕ" — короткое нажатие
 *                      "ДЛИННОЕ " — длинное нажатие (≥ 1.5 с)
 *                      "--------" — событий ещё не было
 *
 *  Регистры test_line_1..3 / display_line_1..3 сохраняются как зеркала
 *  Modbus-таблицы, но на физический LCD не выводятся (геометрия 2 строки).
 */

#include "task_test_display.h"

#include "FreeRTOS.h"
#include "task.h"

#include "gd32f10x.h"
#include "buttons.h"
#include "modbus_table.h"
#include "lcd_hd44780.h"

/* ── Имена кнопок (ровно 8 символов, без '\0') ─────────────────────────
   Индекс = код BTN_EV_* (1..6) в доковом порядке §5.1..§5.6.            */
static const char *s_btn_name[7] = {
    /*0 none    */ "--------",
    /*1 PRG     */ "PRG     ",
    /*2 ONOFF   */ "ONOFF   ",
    /*3 LAMP    */ "LAMP    ",
    /*4 AUTO/MAN*/ "AUTO/MAN",
    /*5 RB      */ "RB      ",
    /*6 E       */ "E       ",
};

/* ── Ярлыки типа события (ровно 8 символов после UTF-8→Win-1251) ─── */
static const char *s_evt_none_utf8  = "--------";
static const char *s_evt_short_utf8 = "КОРОТКОЕ";
static const char *s_evt_long_utf8  = "ДЛИННОЕ ";

/* Выбрать номер текущей нажатой кнопки (приоритет: 1..6 по §5.1..§5.6).
   Возвращаемое число совпадает с кодом BTN_EV_* короткого нажатия.    */
static uint8_t current_btn_num(void)
{
    if (btn_is_down_idx(BTN_IDX_PRG))      return 1u;  /* §5.1 */
    if (btn_is_down_idx(BTN_IDX_ONOFF))    return 2u;  /* §5.2 */
    if (btn_is_down_idx(BTN_IDX_LAMP))     return 3u;  /* §5.3 */
    if (btn_is_down_idx(BTN_IDX_AUTO_MAN)) return 4u;  /* §5.4 */
    if (btn_is_down_idx(BTN_IDX_RB))       return 5u;  /* §5.5 */
    if (btn_is_down_idx(BTN_IDX_E))        return 6u;  /* §5.6 */
    return 0u;
}

/* Выбрать 8-байтовый ярлык события по латченому коду из MB_ADDR_BTN_EVENT.
   BtnEvent_t:  0x00            — событий не было
                0x01..0x06      — короткое нажатие
                0x80 | 0x0X     — длинное нажатие (high-bit = long)    */
static const char *current_evt_label_utf8(void)
{
    uint8_t ev = MB_ReadBits(MB_ADDR_BTN_EVENT);
    if (ev == 0u)          return s_evt_none_utf8;
    if (ev & 0x80u)        return s_evt_long_utf8;
    return s_evt_short_utf8;
}

static void task_test_display(void *arg)
{
    static const uint16_t src_addr[2] = {
        MB_ADDR_TEST_LINE_0, MB_ADDR_TEST_LINE_1
    };
    static const uint16_t dst_addr[2] = {
        MB_ADDR_DISPLAY_LINE_0, MB_ADDR_DISPLAY_LINE_1
    };

    TickType_t xLastWake = xTaskGetTickCount();
    (void)arg;

    lcd_init();

    MB_WriteString(MB_ADDR_TEST_LINE_0, "--- TEST  MODE ---  ");
    MB_WriteString(MB_ADDR_TEST_LINE_1, "Write line0 via MB ");

    for (;;)
    {
        char line[21];

        /* Строка 0 LCD — зеркало test_line_0. */
        MB_ReadString(src_addr[0], line, 21);
        {
            uint8_t i = 0;
            while (line[i] && i < 20u) i++;
            while (i < 20u) line[i++] = ' ';
            line[20] = '\0';
        }
        lcd_print_win1251_at(0, 0, line);
        MB_WriteString(dst_addr[0], line);

        /* Строка 1 LCD: "<имя-8>  <событие-8>  "
           Имя и ярлык события — UTF-8 строки; конвертируем каждый
           в 8-символьный Win-1251 фрагмент через lcd_utf8_to_win1251. */
        {
            uint8_t      n    = current_btn_num();      /* 0..6        */
            const char  *name = s_btn_name[n];
            const char  *evt  = current_evt_label_utf8();

            char name_w[9];
            char evt_w[9];
            lcd_utf8_to_win1251(name, name_w, 8u);
            lcd_utf8_to_win1251(evt,  evt_w,  8u);

            for (uint8_t i = 0; i < 8u;  i++) line[i]        = name_w[i];
            line[8]  = ' ';
            line[9]  = ' ';
            for (uint8_t i = 0; i < 8u;  i++) line[10u + i]  = evt_w[i];
            line[18] = ' ';
            line[19] = ' ';
            line[20] = '\0';

            lcd_print_win1251_at(1, 0, line);
            MB_WriteString(dst_addr[1], line);
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(200));
    }
}

void task_test_display_start(void)
{
    xTaskCreate(task_test_display, "tst_lcd", 512, NULL, 2, NULL);
}
