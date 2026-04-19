#ifndef BUTTONS_H
#define BUTTONS_H

#include "gd32f10x.h"
#include <stdint.h>

/* ── Назначение пинов ───────────────────────────────────────────────────────
   Все кнопки: active-LOW (нажата = GND).
   Режим пина: INPUT_FLOATING (внешняя подтяжка к VCC на плате).
   ─────────────────────────────────────────────────────────────────────────── */
#define BTN_PORT            GPIOB
#define BTN_RCU             RCU_GPIOB

#define BTN_PIN_PRG         GPIO_PIN_0   /* PRG / ВВОД / Mute              */
#define BTN_PIN_ONOFF       GPIO_PIN_1   /* ВКЛ/ВЫКЛ                       */
#define BTN_PIN_AUTO_MAN    GPIO_PIN_2   /* АВТО/РУЧ                       */
#define BTN_PIN_UP          GPIO_PIN_3   /* ▲ Увеличить / вверх            */
#define BTN_PIN_DOWN        GPIO_PIN_4   /* ▼ Уменьшить / вниз             */
#define BTN_COUNT           5u

/* ── Тайминги (мс, период опроса BTN_SCAN_MS) ──────────────────────────── */
#define BTN_SCAN_MS         10u
#define BTN_DEBOUNCE_MS     30u
#define BTN_LONG_PRESS_MS   1500u

#define BTN_DEBOUNCE_TICKS  (BTN_DEBOUNCE_MS  / BTN_SCAN_MS)   /* 3  */
#define BTN_LONG_TICKS      (BTN_LONG_PRESS_MS / BTN_SCAN_MS)  /* 150 */

/* ── Коды событий ────────────────────────────────────────────────────────── */
typedef enum {
    BTN_EV_NONE       = 0x00u,
    BTN_EV_PRG        = 0x01u,   /* короткое нажатие PRG              */
    BTN_EV_ONOFF      = 0x02u,   /* короткое нажатие ВКЛ/ВЫКЛ         */
    BTN_EV_AUTO_MAN   = 0x03u,   /* короткое нажатие АВТО/РУЧ         */
    BTN_EV_UP         = 0x04u,   /* короткое нажатие ▲                */
    BTN_EV_DOWN       = 0x05u,   /* короткое нажатие ▼                */
    BTN_EV_PRG_LONG   = 0x81u,   /* длинное нажатие PRG (вход в меню) */
    BTN_EV_ONOFF_LONG = 0x82u,   /* длинное нажатие ВКЛ/ВЫКЛ          */
} BtnEvent_t;

/* ── API ─────────────────────────────────────────────────────────────────── */
void       btn_init(void);
BtnEvent_t btn_scan(void);   /* вызывать каждые BTN_SCAN_MS мс */

#endif /* BUTTONS_H */
