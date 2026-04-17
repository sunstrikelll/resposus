#include "buttons.h"

/* ── Внутреннее состояние одной кнопки ─────────────────────────────────── */
typedef struct {
    uint8_t  debounce_cnt;   /* счётчик подтверждения дребезга (тики) */
    uint8_t  pressed;        /* 1 = кнопка подтверждена нажатой       */
    uint16_t hold_cnt;       /* счётчик удержания (тики)              */
    uint8_t  long_fired;     /* 1 = длинное событие уже отправлено    */
} BtnState_t;

static BtnState_t s_btn[BTN_COUNT];

static const uint32_t s_pin[BTN_COUNT] = {
    BTN_PIN_PRG, BTN_PIN_ONOFF, BTN_PIN_AUTO_MAN, BTN_PIN_UP, BTN_PIN_DOWN
};

/* Событие при коротком нажатии */
static const BtnEvent_t s_ev_short[BTN_COUNT] = {
    BTN_EV_PRG, BTN_EV_ONOFF, BTN_EV_AUTO_MAN, BTN_EV_UP, BTN_EV_DOWN
};

/* Событие при длинном нажатии (BTN_EV_NONE = нет длинного события) */
static const BtnEvent_t s_ev_long[BTN_COUNT] = {
    BTN_EV_PRG_LONG, BTN_EV_ONOFF_LONG, BTN_EV_NONE, BTN_EV_NONE, BTN_EV_NONE
};

/* ── btn_init ────────────────────────────────────────────────────────────── */
void btn_init(void)
{
    rcu_periph_clock_enable(BTN_RCU);
    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        gpio_init(BTN_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, s_pin[i]);
        s_btn[i].debounce_cnt = 0;
        s_btn[i].pressed      = 0;
        s_btn[i].hold_cnt     = 0;
        s_btn[i].long_fired   = 0;
    }
}

/* ── btn_scan ────────────────────────────────────────────────────────────────
   Вызывать каждые BTN_SCAN_MS мс.
   Возвращает одно событие за вызов (приоритет: первая сработавшая кнопка).  */
BtnEvent_t btn_scan(void)
{
    BtnEvent_t ev = BTN_EV_NONE;

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        /* active-LOW: нажата = RESET (0) */
        uint8_t raw = (gpio_input_bit_get(BTN_PORT, s_pin[i]) == RESET) ? 1u : 0u;

        if (raw) {
            /* ── Кнопка нажата ── */
            if (s_btn[i].debounce_cnt < (uint8_t)BTN_DEBOUNCE_TICKS)
                s_btn[i].debounce_cnt++;

            if (s_btn[i].debounce_cnt >= (uint8_t)BTN_DEBOUNCE_TICKS) {
                s_btn[i].pressed = 1;

                if (s_btn[i].hold_cnt < 0xFFFFu)
                    s_btn[i].hold_cnt++;

                /* Длинное нажатие — однократно при достижении порога */
                if ((s_btn[i].hold_cnt == (uint16_t)BTN_LONG_TICKS) &&
                    !s_btn[i].long_fired                             &&
                    (s_ev_long[i] != BTN_EV_NONE))
                {
                    s_btn[i].long_fired = 1;
                    if (ev == BTN_EV_NONE)
                        ev = s_ev_long[i];
                }
            }
        } else {
            /* ── Кнопка отпущена ── */
            if (s_btn[i].pressed) {
                /* Короткое нажатие — генерируем при отпускании,
                   только если длинное ещё не было сгенерировано    */
                if (!s_btn[i].long_fired && ev == BTN_EV_NONE)
                    ev = s_ev_short[i];

                s_btn[i].pressed    = 0;
                s_btn[i].hold_cnt   = 0;
                s_btn[i].long_fired = 0;
            }
            s_btn[i].debounce_cnt = 0;
        }
    }

    return ev;
}
