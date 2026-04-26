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

/* Порядок ровно соответствует enum BtnIndex_t (§5.1..§5.6).             */
static const BtnPin_t s_pin[BTN_COUNT] = {
    { BTN_PORT_PRG,      BTN_PIN_PRG      },  /* §5.1 BTN_IDX_PRG      (BUTTON 1) */
    { BTN_PORT_ONOFF,    BTN_PIN_ONOFF    },  /* §5.2 BTN_IDX_ONOFF    (BUTTON 2) */
    { BTN_PORT_LAMP,     BTN_PIN_LAMP     },  /* §5.3 BTN_IDX_LAMP     (BUTTON 4) */
    { BTN_PORT_AUTO_MAN, BTN_PIN_AUTO_MAN },  /* §5.4 BTN_IDX_AUTO_MAN (BUTTON 3) */
    { BTN_PORT_RB,       BTN_PIN_RB       },  /* §5.5 BTN_IDX_RB       (BUTTON 5) */
    { BTN_PORT_E,        BTN_PIN_E        },  /* §5.6 BTN_IDX_E        (BUTTON 6) */
};

/* Событие при коротком нажатии — индекс = BTN_IDX_*. */
static const BtnEvent_t s_ev_short[BTN_COUNT] = {
    BTN_EV_PRG,        /* §5.1 */
    BTN_EV_ONOFF,      /* §5.2 */
    BTN_EV_LAMP,       /* §5.3 */
    BTN_EV_AUTO_MAN,   /* §5.4 */
    BTN_EV_RB,         /* §5.5 */
    BTN_EV_E           /* §5.6 */
};

/* Событие при длинном нажатии (BTN_EV_NONE = нет длинного события).
   PRG_LONG    — вход в конфигурацию (§5.1).
   ONOFF_LONG  — удержание ≥ BTN_LONG_MS (3 с) → Ночной режим (§5.2).
   LAMP_LONG   — удержание «Лампа» → «увеличить» в редакторе (§5.3).
   AUTO_MAN    — длинного события нет (§5.4).
   RB          — длинного события нет (§5.5).
   E_LONG      — зарезервировано (§5.6). */
static const BtnEvent_t s_ev_long[BTN_COUNT] = {
    BTN_EV_PRG_LONG,    /* §5.1 */
    BTN_EV_ONOFF_LONG,  /* §5.2 */
    BTN_EV_LAMP_LONG,   /* §5.3 */
    BTN_EV_NONE,        /* §5.4 */
    BTN_EV_NONE,        /* §5.5 */
    BTN_EV_E_LONG       /* §5.6 */
};

/* ── btn_is_down_idx ─────────────────────────────────────────────────────
   Возвращает «сырое» состояние кнопки (без debounce): 1 если физически
   замкнута на землю, 0 — иначе.                                         */
uint8_t btn_is_down_idx(BtnIndex_t idx)
{
    if ((uint8_t)idx >= BTN_COUNT) return 0u;
    return (gpio_input_bit_get(s_pin[idx].port, s_pin[idx].pin) == RESET) ? 1u : 0u;
}

/* ── btn_factory_reset_combo_held ────────────────────────────────────────
   passport_v1.5.md §11: восстановление заводских установок —
   power-on с одновременным удержанием A (Авто/Ручной) + B (RB) + E.

   Функция выполняется на ранней стадии main() — до btn_init() — поэтому
   сама подключает тактирование GPIOB/GPIOD, делает SWJ remap (для PB4),
   и устанавливает входной режим только трёх нужных пинов.
   Возвращает 1, если все три кнопки нажаты (active-LOW = RESET).        */
uint8_t btn_factory_reset_combo_held(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_AF);
    /* PB4 = JTRST по умолчанию: без SWJ-remap читался бы как 1. */
    gpio_pin_remap_config(GPIO_SWJ_SWDPENABLE_REMAP, ENABLE);

    gpio_init(BTN_PORT_AUTO_MAN, GPIO_MODE_IN_FLOATING,
              GPIO_OSPEED_10MHZ, BTN_PIN_AUTO_MAN);
    gpio_init(BTN_PORT_RB,       GPIO_MODE_IN_FLOATING,
              GPIO_OSPEED_10MHZ, BTN_PIN_RB);
    gpio_init(BTN_PORT_E,        GPIO_MODE_IN_FLOATING,
              GPIO_OSPEED_10MHZ, BTN_PIN_E);

    /* Небольшая задержка для стабилизации pull-up'ов после переключения
       JTRST в GPIO. ~5..10 мкс достаточно; Cortex-M3 на 72 МГц делает
       пустую петлю быстро, поэтому крутим 1000 итераций. */
    for (volatile uint32_t i = 0u; i < 1000u; i++) { __asm__ volatile (""); }

    uint8_t a = (gpio_input_bit_get(BTN_PORT_AUTO_MAN, BTN_PIN_AUTO_MAN) == RESET) ? 1u : 0u;
    uint8_t b = (gpio_input_bit_get(BTN_PORT_RB,       BTN_PIN_RB)       == RESET) ? 1u : 0u;
    uint8_t e = (gpio_input_bit_get(BTN_PORT_E,        BTN_PIN_E)        == RESET) ? 1u : 0u;

    return (a && b && e) ? 1u : 0u;
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

    /* Тайминги читаем РАЗ В ВЫЗОВ из EEPROM-регистров — чтобы смена порогов
       через Modbus вступала в силу сразу, без ресета.

       ВАЖНО: значения могут быть «битыми» сразу после прошивки на плату,
       где EEPROM был записан старой версией прошивки (другая раскладка
       регистров — CRC ещё валиден, но в новых ячейках 0/0xFF).  Если
       прочитанные значения вне здравого диапазона — откатываемся на
       compile-time defaults (30 мс / 3000 мс), эквивалентные заводским.
       Без этой защиты обе константы обнуляются, кламп даёт 1 тик —
       любое нажатие мгновенно регистрируется как LONG и короткие
       события не приходят на FSM.                                      */
    uint16_t deb_ms  = MB_ReadU16(MB_ADDR_BTN_DEBOUNCE_MS);
    uint16_t long_ms = MB_ReadU16(MB_ADDR_BTN_LONG_MS);
    if (deb_ms == 0u || deb_ms > 1000u)            deb_ms  = 30u;
    if (long_ms < 200u || long_ms > 30000u)        long_ms = 3000u;

    uint16_t deb_ticks  = (uint16_t)(deb_ms  / BTN_SCAN_MS);
    if (deb_ticks  == 0u) deb_ticks  = 1u;
    uint16_t long_ticks = (uint16_t)(long_ms / BTN_SCAN_MS);
    if (long_ticks == 0u) long_ticks = 1u;

    for (uint8_t i = 0; i < BTN_COUNT; i++) {
        /* active-LOW: нажата = RESET (0) */
        uint8_t raw = (gpio_input_bit_get(s_pin[i].port, s_pin[i].pin) == RESET) ? 1u : 0u;

        if (raw) {
            /* ── Кнопка нажата ── */
            if (s_btn[i].debounce_cnt < 0xFFu &&
                (uint16_t)s_btn[i].debounce_cnt < deb_ticks)
                s_btn[i].debounce_cnt++;

            if ((uint16_t)s_btn[i].debounce_cnt >= deb_ticks) {
                s_btn[i].pressed = 1;

                if (s_btn[i].hold_cnt < 0xFFFFu)
                    s_btn[i].hold_cnt++;

                /* Длинное нажатие — однократно при достижении порога */
                if ((s_btn[i].hold_cnt == long_ticks) &&
                    !s_btn[i].long_fired              &&
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
