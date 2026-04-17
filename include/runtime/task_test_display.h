#ifndef RUNTIME_TASK_TEST_DISPLAY_H
#define RUNTIME_TASK_TEST_DISPLAY_H

/*  task_test_display — TEST-режим: вывод 4 строк на LCD.
 *
 *  Каждые 200 мс читает регистры test_line_0..3 (RW, по 20 символов),
 *  выравнивает до ровно 20 символов пробелами, пишет на 4 строки LCD
 *  (Win-1251) и копирует в display_line_0..3 — мастер может прочитать
 *  обратно то, что реально попало на экран.
 *
 *  Сама инициализирует LCD через lcd_init() (в TEST menu_init не
 *  вызывается). Изначально выводит подсказку «--- TEST MODE ---».
 *
 *  Стек:      512 слов (LCD + локальный буфер строки)
 *  Приоритет: 2
 */
void task_test_display_start(void);

#endif /* RUNTIME_TASK_TEST_DISPLAY_H */
