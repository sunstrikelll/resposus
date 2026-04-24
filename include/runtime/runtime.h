#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdint.h>

/* ══════════════════════════════════════════════════════════════════════════
   Режимы рантайма МКВП-02
   ══════════════════════════════════════════════════════════════════════════
     RUNTIME_PRODUCTION — штатная работа изделия:
                          LCD + FSM-меню, 6 кнопок, 4 LED, Modbus RTU.
     RUNTIME_TEST       — технологический тест:
                          Modbus-мастер пишет 4 × 20 симв. в test_line_0..3,
                          прошивка выводит их на LCD (строки 0..2) и
                          индикатор кнопки (строка 3). LED показывают
                          двоичный код нажатой кнопки (1..6).
   ─────────────────────────────────────────────────────────────────────── */
typedef enum {
    RUNTIME_PRODUCTION = 0,
    RUNTIME_TEST       = 1,
} RuntimeMode_t;

/* Число валидных режимов (для range-check во внешнем коде). */
#define RUNTIME_MODE_COUNT   2u

/* Читаемое имя режима (для логов / отладки). */
static inline const char *runtime_mode_name(RuntimeMode_t m)
{
    return (m == RUNTIME_TEST) ? "TEST" : "PRODUCTION";
}

/* Создаёт набор FreeRTOS-задач под выбранный режим.
   Должна быть вызвана ДО vTaskStartScheduler().                          */
void runtime_start(RuntimeMode_t mode);

#endif /* RUNTIME_H */
