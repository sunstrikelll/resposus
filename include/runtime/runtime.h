#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdint.h>

/* ── Режим рантайма ─────────────────────────────────────────────────────────
   PRODUCTION — штатный режим: меню БУСШ, кнопки, светодиоды,
                обмен с блоком процесса через Modbus-регистры.
   TEST       — тест вывода на дисплей: Modbus-мастер пишет в 4 регистра
                test_line_0..3 (по 20 символов каждый), они отображаются
                на 4 строках LCD. Кнопки пишут код события в btn_event.
   ─────────────────────────────────────────────────────────────────────────── */
typedef enum {
    RUNTIME_PRODUCTION = 0,
    RUNTIME_TEST       = 1,
} RuntimeMode_t;

/* Создаёт FreeRTOS-задачи под выбранный режим.
   Должна быть вызвана ДО vTaskStartScheduler().                          */
void runtime_start(RuntimeMode_t mode);

#endif /* RUNTIME_H */
