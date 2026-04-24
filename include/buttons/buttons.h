#ifndef BUTTONS_H
#define BUTTONS_H

#include "gd32f10x.h"
#include <stdint.h>

/* ══════════════════════════════════════════════════════════════════════════
   Аппаратная распиновка кнопок — МКВП-02 (GD32F103RCT6, LQFP64)
   ══════════════════════════════════════════════════════════════════════════
   По схеме БУСШ.02.101.01 Э3 (BUTTON 1..6):

     BUTTON 1 → PB2    (пин 28)
     BUTTON 2 → PB3    (пин 55)   ⚠ по умолчанию JTDO  → нужен SWJ-remap
     BUTTON 3 → PB4    (пин 56)   ⚠ по умолчанию JTRST → нужен SWJ-remap
     BUTTON 4 → PB5    (пин 57)
     BUTTON 5 → PB15   (пин 36)
     BUTTON 6 → PD2    (пин 54)   ◄ единственный доступный пин GPIOD в LQFP64

   ЗАМЕЧАНИЯ:
     • Кнопки разнесены по ДВУМ портам (GPIOB и GPIOD). Глобального
       BTN_PORT больше нет — каждая кнопка несёт свой порт/пин.
     • PB3 и PB4 после сброса заняты JTAG-функциями (JTDO/JTRST).
       Отключение JTAG-DP (с сохранением SW-DP) выполнено в btn_init()
       через gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE).
       SWD-отладка при этом продолжает работать (PA13/PA14).
     • PB15 — по умолчанию SPI2_MOSI (alt-function), но как INPUT
       работает без remap (alt-function активна только при OUTPUT).
     • PD2 — единственный GPIO-пин порта D, выведенный в LQFP64.

   Все кнопки — active-LOW (нажата = GND). На плате внешние pull-up к
   +3.3 В; внутренний режим пина — INPUT_FLOATING.

   Логические имена (PRG/ONOFF/…) оставлены для совместимости с FSM
   меню; порядок соответствует BUTTON 1..6 на схеме.
   ─────────────────────────────────────────────────────────────────────── */

#define BTN_PORT_PRG        GPIOB
#define BTN_PIN_PRG         GPIO_PIN_2   /* 1: PRG / ВВОД                 */

#define BTN_PORT_ONOFF      GPIOB
#define BTN_PIN_ONOFF       GPIO_PIN_3   /* 2: ВКЛ/ВЫКЛ (бывш. JTDO)      */

#define BTN_PORT_AUTO_MAN   GPIOB
#define BTN_PIN_AUTO_MAN    GPIO_PIN_4   /* 3: АВТО/РУЧ (бывш. JTRST)     */

#define BTN_PORT_UP         GPIOB
#define BTN_PIN_UP          GPIO_PIN_5   /* 4: ▲ Увеличить / вверх        */

#define BTN_PORT_DOWN       GPIOB
#define BTN_PIN_DOWN        GPIO_PIN_15  /* 5: ▼ Уменьшить / вниз         */

#define BTN_PORT_MUTE       GPIOD
#define BTN_PIN_MUTE        GPIO_PIN_2   /* 6: ЗВУК / F1 (квитирование)   */

#define BTN_COUNT           6u

/* ── Тайминги (мс, период опроса BTN_SCAN_MS) ──────────────────────────── */
#define BTN_SCAN_MS         10u
#define BTN_DEBOUNCE_MS     30u
#define BTN_LONG_PRESS_MS   1500u

#define BTN_DEBOUNCE_TICKS  (BTN_DEBOUNCE_MS  / BTN_SCAN_MS)   /* 3  */
#define BTN_LONG_TICKS      (BTN_LONG_PRESS_MS / BTN_SCAN_MS)  /* 150 */

/* ══════════════════════════════════════════════════════════════════════════
   Коды событий кнопок
   ══════════════════════════════════════════════════════════════════════════
   Значения совпадают для «физического» btn_scan() и «виртуального»
   MB_ADDR_BTN_CMD (см. modbus_table.h). Мастер Modbus может записать
   нужный код в регистр, FSM обработает событие и обнулит регистр.       */

typedef enum {
    BTN_EV_NONE       = 0x00u,
    BTN_EV_PRG        = 0x01u,   /* короткое нажатие PRG (BUTTON 1)     */
    BTN_EV_ONOFF      = 0x02u,   /* короткое нажатие ВКЛ/ВЫКЛ (BUTTON 2)*/
    BTN_EV_AUTO_MAN   = 0x03u,   /* короткое нажатие АВТО/РУЧ (BUTTON 3)*/
    BTN_EV_UP         = 0x04u,   /* короткое нажатие ▲ (BUTTON 4)       */
    BTN_EV_DOWN       = 0x05u,   /* короткое нажатие ▼ (BUTTON 5)       */
    BTN_EV_MUTE       = 0x06u,   /* короткое нажатие MUTE (BUTTON 6)    */
    BTN_EV_PRG_LONG   = 0x81u,   /* длинное нажатие PRG (вход в меню)   */
    BTN_EV_ONOFF_LONG = 0x82u,   /* длинное нажатие ВКЛ/ВЫКЛ (STANDBY)  */
    BTN_EV_MUTE_LONG  = 0x86u,   /* длинное нажатие MUTE/F1             */
} BtnEvent_t;

/* ── Индексы кнопок (для btn_is_down_idx) ─────────────────────────────── */
typedef enum {
    BTN_IDX_PRG      = 0u,
    BTN_IDX_ONOFF    = 1u,
    BTN_IDX_AUTO_MAN = 2u,
    BTN_IDX_UP       = 3u,
    BTN_IDX_DOWN     = 4u,
    BTN_IDX_MUTE     = 5u,
} BtnIndex_t;

/* ── API ─────────────────────────────────────────────────────────────────── */
void       btn_init(void);
BtnEvent_t btn_scan(void);               /* каждые BTN_SCAN_MS мс          */

/* Собрать события из физических кнопок + виртуального регистра BTN_CMD.
   Регистр BTN_CMD очищается внутри (auto-clear после обработки).        */
BtnEvent_t btn_scan_with_cmd(void);

/* Сырое состояние кнопки (без debounce): 1=замкнута на землю.
   Используется тестовыми задачами для «живой» диагностики.             */
uint8_t    btn_is_down_idx(BtnIndex_t idx);

#endif /* BUTTONS_H */
