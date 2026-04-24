/*  task_test_display.c — TEST-режим: визуализация test_line_N
 *
 *  Мастер Modbus пишет 20-символьные строки (Win-1251) в регистры
 *  test_line_0..3, задача их выводит на 3 строки LCD (0..2) и копирует
 *  в display_line_0..2 (чтобы мастер мог проверить, что именно было
 *  отображено).
 *
 *  4-я строка (row 3) — диагностика кнопок.  Формат (20 символов):
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
 *  Таким образом, удерживая кнопку, оператор видит:
 *     • моментальное «имя» (доказательство, что GPIO-пин читается);
 *     • через 30 мс появится "КОРОТКОЕ" (debounce сработал);
 *     • через 1.5 с сменится на "ДЛИННОЕ " (long-press детектор).
 */

#include "task_test_display.h"

#include "FreeRTOS.h"
#include "task.h"

#include "gd32f10x.h"
#include "buttons.h"
#include "modbus_table.h"
#include "lcd_hd44780.h"

/* ── Имена кнопок (ровно 8 символов, без '\0') ─────────────────────────
   Индекс = номер кнопки (0 — ни одна не нажата, 1..6 — кнопки).          */
static const char s_btn_name[7][8] = {
    /*0 none    */ { '-','-','-','-','-','-','-','-' },
    /*1 PRG     */ { 'P','R','G',' ',' ',' ',' ',' ' },
    /*2 ONOFF   */ { 'O','N','O','F','F',' ',' ',' ' },
    /*3 AUTO/MAN*/ { 'A','U','T','O','/','M','A','N' },
    /*4 UP      */ { 'U','P',' ',' ',' ',' ',' ',' ' },
    /*5 DOWN    */ { 'D','O','W','N',' ',' ',' ',' ' },
    /*6 MUTE    */ { 'M','U','T','E',' ',' ',' ',' ' },
};

/* ── Ярлыки типа события (ровно 8 символов, Win-1251) ──────────────────
     none  : "--------"
     short : "КОРОТКОЕ"  = \xCA\xCE\xD0\xCE\xD2\xCA\xCE\xC5
     long  : "ДЛИННОЕ "  = \xC4\xCB\xC8\xCD\xCD\xCE\xC5 + ' '          */
static const char s_evt_none [8] = { '-','-','-','-','-','-','-','-' };
static const char s_evt_short[8] = {
    '\xCA','\xCE','\xD0','\xCE','\xD2','\xCA','\xCE','\xC5'   /* КОРОТКОЕ */
};
static const char s_evt_long [8] = {
    '\xC4','\xCB','\xC8','\xCD','\xCD','\xCE','\xC5',' '       /* ДЛИННОЕ_ */
};

/* Выбрать номер текущей нажатой кнопки (приоритет: 1..6).
   Чтение — через buttons.c (знает порт каждой кнопки: B и D).         */
static uint8_t current_btn_num(void)
{
    if (btn_is_down_idx(BTN_IDX_PRG))      return 1u;
    if (btn_is_down_idx(BTN_IDX_ONOFF))    return 2u;
    if (btn_is_down_idx(BTN_IDX_AUTO_MAN)) return 3u;
    if (btn_is_down_idx(BTN_IDX_UP))       return 4u;
    if (btn_is_down_idx(BTN_IDX_DOWN))     return 5u;
    if (btn_is_down_idx(BTN_IDX_MUTE))     return 6u;
    return 0u;
}

/* Выбрать 8-байтовый ярлык события по латченому коду из MB_ADDR_BTN_EVENT.
   BtnEvent_t:  0x00            — событий не было
                0x01..0x06      — короткое нажатие
                0x80 | 0x0X     — длинное нажатие (high-bit = long)    */
static const char *current_evt_label(void)
{
    uint8_t ev = MB_ReadBits(MB_ADDR_BTN_EVENT);
    if (ev == 0u)          return s_evt_none;
    if (ev & 0x80u)        return s_evt_long;
    return s_evt_short;
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

        /* Строка 3: "<имя-8>  <событие-8>  "
           0..7   — имя удерживаемой сейчас кнопки (raw GPIO);
           8..9   — разделитель (2 пробела);
           10..17 — ярлык последнего debounced-события;
           18..19 — trailing padding (2 пробела).                      */
        {
            uint8_t      n    = current_btn_num();      /* 0..6        */
            const char  *name = s_btn_name[n];
            const char  *evt  = current_evt_label();

            for (uint8_t i = 0; i < 8u;  i++) line[i]        = name[i];
            line[8]  = ' ';
            line[9]  = ' ';
            for (uint8_t i = 0; i < 8u;  i++) line[10u + i]  = evt[i];
            line[18] = ' ';
            line[19] = ' ';
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
