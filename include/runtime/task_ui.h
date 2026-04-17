#ifndef RUNTIME_TASK_UI_H
#define RUNTIME_TASK_UI_H

/*  task_ui — основной HMI-цикл в PRODUCTION-режиме.
 *
 *  Каждые BTN_SCAN_MS (10 мс):
 *    • btn_scan()       — опрос 5 кнопок с debounce/long-press
 *    • menu_process(ev) — конечный автомат меню (управляет LCD и
 *                         светодиодами POWER/WORK/ALARM/MANUAL, читает и
 *                         пишет регистры MODE_SR / MODE_CR / SETPOINT /
 *                         MANUAL_OUT / DISPLAY_LINE_N и т.п.)
 *
 *  Стек:      512 слов (LCD-форматирование + menu FSM)
 *  Приоритет: 3 (самый высокий — кнопки не должны теряться)
 */
void task_ui_start(void);

#endif /* RUNTIME_TASK_UI_H */
