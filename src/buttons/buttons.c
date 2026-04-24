#include "buttons.h"
#include "modbus_table.h"

/* ── Внутреннее состояние одной кнопки ─────────────────────────────────── */
typedef struct {
    uint8_t  debounce_cnt;   /* счётчик подтверждения дребезга (тики) */
    uint8_t  pressed;        /* 1 = кнопка подтверждена нажатой       */
    uint16_t hold_cnt;       /* счётчик удержания (тики)              */
    uint8_t  long_fired;     /* 1 = длинное событие уже отправлено    */
} BtnState_t;

/* Пин-дескриптор: порт + номер пина. Порты у кнопок РАЗНЫЕ (B и D),
   поэтому глобального BTN_PORT больше нет — здесь таблица. */
typedef struct {
    uint32_t port;
    uint32_t pin;
} BtnPin_t;

static BtnState_t s_btn[BTN_COUNT];

static const BtnPin_t s_pin[BTN_COUNT] = {
    { BTN_PORT_PRG,      BTN_PIN_PRG      },  /* BTN_IDX_PRG      (BUTTON 1) */
    { BTN_PORT_ONOFF,    BTN_PIN_ONOFF    },  /* BTN_IDX_ONOFF    (BUTTON 2) */
    { BTN_PORT_AUTO_MAN, BTN_PIN_AUTO_MAN },  /* BTN_IDX_AUTO_MAN (BUTTON 3) */
    { BTN_PORT_UP,       BTN_PIN_UP       },  /* BTN_IDX_UP       (BUTTON 4) */
    { BTN_PORT_DOWN,     BTN_PIN_DOWN     },  /* BTN_IDX_DOWN     (BUTTON 5) */
    { BTN_PORT_MUTE,     BTN_PIN_MUTE     },  /* BTN_IDX_MUTE     (BUTTON 6) */
};

/* Событие при коротком нажатии */
static const BtnEvent_t s_ev_short[BTN_COUNT] = {
    BTN_EV_PRG, BTN_EV_ONOFF, BTN_EV_AUTO_MAN,
    BTN_EV_UP,  BTN_EV_DOWN,  BTN_EV_MUTE
};

/* Событие при длинном нажатии (BTN_EV_NONE = нет длинного события) */
static const BtnEvent_t s_ev_long[BTN_COUNT] = {
    BTN_EV_PRG_LONG, BTN_EV_ONOFF_LONG, BTN_EV_NONE,
    BTN_EV_NONE,     BTN_EV_NONE,       BTN_EV_MUTE_LONG
};

/* ── btn_is_down_idx ─────────────────────────────────────────────────────
   Возвращает «сырое» состояние кнопки (без debounce): 1 если физически
   замкнута на землю, 0 — иначе.                                         */
uint8_t btn_is_down_idx(BtnIndex_t idx)
{
    if ((uint8_t)idx >= BTN_COUNT) return 0u;
    return (gpio_input_bit_get(s_pin[idx].port, s_pin[idx].pin) == RESET) ? 1u : 0u;
}

/* ── btn_init ────────────────────────────────────────────────────────────── */
void btn_init(void)
{
    /* Включаем тактирование портов GPIOB и GPIOD (кнопки на обоих). */
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOD);

    /* ВАЖНО: PB3 (JTDO) и PB4 (JTRST) после сброса заняты JTAG-DP.
       BUTTON 2 и BUTTON 3 на плате МКВП-02 разведены именно на эти пины
       (см. схему БУСШ.02.101.01 Э3). Пока не выполнен SWJ-remap — GPIO-
       конфигурация не имеет эффекта, gpio_input_bit_get() всегда читает
       1 (внутренний pull-up JTAG), и кнопки «не нажимаются».

       GPIO_SWJ_SWDPENABLE_REMAP = «JTAG-DP OFF, SW-DP ON» — освобождает
       PA15, PB3, PB4 под GPIO, SWD-отладка сохраняется (PA13/PA14).    */
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        gpio_init(s_pin[i].port, GPIO_MODE_IN_FLOATING,
                  GPIO_OSPEED_10MHZ, s_pin[i].pin);
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
        uint8_t raw = (gpio_input_bit_get(s_pin[i].port, s_pin[i].pin) == RESET) ? 1u : 0u;

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

/* ── btn_scan_with_cmd ───────────────────────────────────────────────────────
   Объединяет физический опрос и виртуальную кнопку. Используется в
   штатном режиме (task_ui) — позволяет проверять логику FSM и меню через
   Modbus-мастера, без физических кнопок.

   Протокол:
     1) Мастер пишет код события (BTN_EV_*) в регистр MB_ADDR_BTN_CMD.
     2) Эта функция читает регистр; если там не-ноль — возвращает этот
        код и сразу очищает регистр (auto-clear), чтобы событие
        сработало ровно один раз.
     3) Если BTN_CMD = 0, возвращается результат btn_scan() (физ. кнопки).

   Физическая кнопка имеет приоритет над виртуальной, чтобы случайная
   запись мастера не блокировала живой ввод.                              */
BtnEvent_t btn_scan_with_cmd(void)
{
    BtnEvent_t ev = btn_scan();
    if (ev != BTN_EV_NONE) {
        /* Если пришло физ. событие — всё равно чистим виртуальный регистр,
           чтобы не остался «залипший» код при последующих вызовах.       */
        if (MB_ReadBits(MB_ADDR_BTN_CMD) != 0u)
            MB_WriteBits(MB_ADDR_BTN_CMD, 0u);
        return ev;
    }

    uint8_t cmd = MB_ReadBits(MB_ADDR_BTN_CMD);
    if (cmd != 0u) {
        MB_WriteBits(MB_ADDR_BTN_CMD, 0u);    /* auto-clear */
        return (BtnEvent_t)cmd;
    }
    return BTN_EV_NONE;
}
