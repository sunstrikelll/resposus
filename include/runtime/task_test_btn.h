#ifndef RUNTIME_TASK_TEST_BTN_H
#define RUNTIME_TASK_TEST_BTN_H

/*  task_test_btn — TEST-режим: трансляция событий кнопок в Modbus.
 *
 *  Каждые BTN_SCAN_MS (10 мс) опрашивает 5 кнопок (PRG/ONOFF/AUTO_MAN/
 *  UP/DOWN) и при ненулевом событии пишет его код в регистр
 *  MB_ADDR_BTN_EVENT. Без меню, без светодиодов — чисто диагностика.
 *
 *  Стек:      configMINIMAL_STACK_SIZE
 *  Приоритет: 2
 */
void task_test_btn_start(void);

#endif /* RUNTIME_TASK_TEST_BTN_H */
