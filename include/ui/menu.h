#ifndef MENU_H
#define MENU_H

/*  menu.h — МКВП-02: Монитор-контроллер потока воздуха
 *
 *  Состояния конечного автомата:
 *
 *    POWER_ON       — заставка при включении (3 с)
 *    STANDBY        — ожидание (выходы выключены, дисплей: время / нажмите ВКЛ)
 *    MAIN           — рабочий экран (АВТО или РУЧ, РАБОТА или СТОП)
 *    ALARM          — авария (мигающий экран, PRG = квитирование)
 *
 *    SETTINGS       — PASS1: оператор (7 пунктов)
 *    SETTINGS_P2    — PASS2: сервис (калибровка, ПИД, Blackout)
 *    SETTINGS_P3    — PASS3: обслуживание
 *    SETTINGS_P4    — PASS4: заводские (счётчики, datalogger)
 *
 *    SET_FLOW_SP    — уставка потока (м/с)
 *    SET_FLOW_SPR   — уставка повтора (м/с)
 *    SET_ALARM_LOW  — порог «мало потока» (м/с)
 *    SET_ALARM_LOWR — порог сброса аварии (м/с)
 *    SET_ALARM_TIME — задержка аварии 0-180 с
 *    SET_MEM_NOR    — режим памяти ручного выхода: НОРМ / ПАМЯТЬ
 *
 *    SET_SENSOR_Z   — PASS2: калибровка нуля датчика потока
 *    SET_SENSOR_S   — PASS2: калибровка диапазона датчика потока
 *    SET_OUT_Z      — PASS2: калибровка нуля аналогового выхода
 *    SET_OUT_S      — PASS2: калибровка диапазона аналогового выхода
 *    SET_PID_TI     — PASS2: постоянная TI ПИД-регулятора
 *    SET_PID_BAND   — PASS2: пропорциональная полоса ПИД, см/с
 *    SET_BLACKOUT   — PASS2: режим Blackout (отключение питания) ON/OFF
 *
 *    SET_MAINT      — PASS3: счётчик моточасов до обслуживания
 *
 *    SET_COUNT_MAX  — PASS4: лимит счётчика моточасов
 *    SET_DATALOG    — PASS4: datalogger ON/OFF
 */

#include "buttons.h"
#include <stdint.h>

typedef enum {
    MENU_POWER_ON      = 0,
    MENU_STANDBY,
    MENU_MAIN,
    MENU_ALARM,

    /* Подменю по уровням PASS */
    MENU_SETTINGS,           /* PASS1 — оператор */
    MENU_SETTINGS_P2,        /* PASS2 — сервис   */
    MENU_SETTINGS_P3,        /* PASS3 — обслуживание */
    MENU_SETTINGS_P4,        /* PASS4 — заводские */

    /* Редакторы PASS1 */
    MENU_SET_FLOW_SP,
    MENU_SET_FLOW_SPR,
    MENU_SET_ALARM_LOW,
    MENU_SET_ALARM_LOWR,
    MENU_SET_ALARM_TIME,
    MENU_SET_MEM_NOR,

    /* Редакторы PASS2 */
    MENU_SET_SENSOR_Z,
    MENU_SET_SENSOR_S,
    MENU_SET_OUT_Z,
    MENU_SET_OUT_S,
    MENU_SET_PID_TI,
    MENU_SET_PID_BAND,
    MENU_SET_BLACKOUT,

    /* Редакторы PASS3 */
    MENU_SET_MAINT,

    /* Редакторы PASS4 */
    MENU_SET_COUNT_MAX,
    MENU_SET_DATALOG,
} MenuState_t;

void menu_init(void);
void menu_process(BtnEvent_t ev);
void menu_update_display(void);
MenuState_t menu_get_state(void);

#endif /* MENU_H */
