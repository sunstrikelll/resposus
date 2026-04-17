#ifndef RUNTIME_TASK_TIMER_H
#define RUNTIME_TASK_TIMER_H

/*  task_timer — 1-Гц счётчик времени (регистр MB_ADDR_TIMER).
 *  Режим: PRODUCTION.
 *
 *  Стек:      configMINIMAL_STACK_SIZE
 *  Приоритет: 1
 *  Период:    50 мс (20 тиков = 1 с инкремента)
 *
 *  Сброс по биту MB_BIT_CR_RESET_TIMER в регистре bit_cr.
 */
void task_timer_start(void);

#endif /* RUNTIME_TASK_TIMER_H */
