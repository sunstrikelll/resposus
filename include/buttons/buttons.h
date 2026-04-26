#ifndef BUTTONS_H
#define BUTTONS_H

#include "gd32f10x.h"
#include <stdint.h>

/* ══════════════════════════════════════════════════════════════════════════
   Аппаратная распиновка кнопок — МКВП-02 (GD32F103RCT6, LQFP64)
   ══════════════════════════════════════════════════════════════════════════
   По схеме БУСШ.02.101.01 Э3 (BUTTON 1..6) и функциональному описанию
   «ФУНКЦИОНАЛЬНОСТЬ КНОПОК» (passport_v1.5.md §5).
   Порядок BTN_IDX_* приведён в соответствие с разделами §5.1..§5.6
   документа (PRG → ONOFF → LAMP → AUTO_MAN → RB → E).
   Физическая распиновка осталась прежней — она задана платой:

     BUTTON 1 → PB2    (пин 28)   Mute / PRG        §5.1 заглушить зуммер /
                                                      вход-выход в настройки
     BUTTON 2 → PB3    (пин 55)   Вкл/Выкл          §5.2 ⚠ JTDO → нужен SWJ-remap
     BUTTON 3 → PB4    (пин 56)   Авто/Ручной (A/M) §5.4 ⚠ JTRST → нужен SWJ-remap
     BUTTON 4 → PB5    (пин 57)   Лампа             §5.3 освещение / «увеличить»
     BUTTON 5 → PB15   (пин 36)   RB (Выбор)        §5.5 включение розеток / «назад»
     BUTTON 6 → PD2    (пин 54)   E                 §5.6 аварийный режим
                                                      (единственный доступный
                                                       GPIOD-пин в LQFP64)

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

   ─────────────────────────────────────────────────────────────────────── */

/* ── Доковый порядок (passport_v1.5.md §5.1..§5.6) ─────────────────────── */
#define BTN_PORT_PRG        GPIOB
#define BTN_PIN_PRG         GPIO_PIN_2   /* §5.1 BUTTON 1: Mute / PRG       */

#define BTN_PORT_ONOFF      GPIOB
#define BTN_PIN_ONOFF       GPIO_PIN_3   /* §5.2 BUTTON 2: ВКЛ/ВЫКЛ (JTDO)  */

#define BTN_PORT_LAMP       GPIOB
#define BTN_PIN_LAMP        GPIO_PIN_5   /* §5.3 BUTTON 4: Лампа            */

#define BTN_PORT_AUTO_MAN   GPIOB
#define BTN_PIN_AUTO_MAN    GPIO_PIN_4   /* §5.4 BUTTON 3: АВТО/РУЧ (JTRST) */

#define BTN_PORT_RB         GPIOB
#define BTN_PIN_RB          GPIO_PIN_15  /* §5.5 BUTTON 5: RB (Выбор)       */

#define BTN_PORT_E          GPIOD
#define BTN_PIN_E           GPIO_PIN_2   /* §5.6 BUTTON 6: E (авария)       */

#define BTN_COUNT           6u

/* ── Тайминги ────────────────────────────────────────────────────────────
   Период опроса фиксирован в прошивке (BTN_SCAN_MS, ниже). Значения
   debounce и long-press берутся из EEPROM-регистров MB_ADDR_BTN_DEBOUNCE_MS
   и MB_ADDR_BTN_LONG_MS (см. modbus_table.h, группа M). Дефолты задаются
   в settings.c: 30 мс (дребезг) и 3000 мс (длинное нажатие — 3 с удержания
   для «Ночного режима» по документации «Включение и работа прибора»).    */
#define BTN_SCAN_MS         10u

/* ══════════════════════════════════════════════════════════════════════════
   Коды событий кнопок
   ══════════════════════════════════════════════════════════════════════════
   Значения совпадают для «физического» btn_scan() и «виртуального»
   MB_ADDR_BTN_CMD (см. modbus_table.h). Мастер Modbus может записать
   нужный код в регистр, FSM обработает событие и обнулит регистр.       */

/* Коды событий нумеруются по доковому порядку §5.1..§5.6.
   Старшие 4 бита: 0x0 = короткое нажатие, 0x8 = длинное. */
typedef enum {
    BTN_EV_NONE       = 0x00u,
    BTN_EV_PRG        = 0x01u,   /* §5.1 короткое PRG/Mute     (BUTTON 1)  */
    BTN_EV_ONOFF      = 0x02u,   /* §5.2 короткое ВКЛ/ВЫКЛ      (BUTTON 2)  */
    BTN_EV_LAMP       = 0x03u,   /* §5.3 короткое Лампа         (BUTTON 4)  */
    BTN_EV_AUTO_MAN   = 0x04u,   /* §5.4 короткое АВТО/РУЧ      (BUTTON 3)  */
    BTN_EV_RB         = 0x05u,   /* §5.5 короткое RB/Выбор      (BUTTON 5)  */
    BTN_EV_E          = 0x06u,   /* §5.6 короткое E (авария)    (BUTTON 6)  */
    BTN_EV_PRG_LONG   = 0x81u,   /* §5.1 длинное PRG (вход в меню)         */
    BTN_EV_ONOFF_LONG = 0x82u,   /* §5.2 длинное ВКЛ/ВЫКЛ ≥3 с → Ночной    */
    BTN_EV_LAMP_LONG  = 0x83u,   /* §5.3 удержание Лампа — «+» в редакторе */
    BTN_EV_E_LONG     = 0x86u,   /* §5.6 длинное E (зарезервировано)       */
} BtnEvent_t;

/* ── Индексы кнопок (для btn_is_down_idx) — доковый порядок §5.1..§5.6 ── */
typedef enum {
    BTN_IDX_PRG      = 0u,    /* §5.1 PB2  (BUTTON 1)   */
    BTN_IDX_ONOFF    = 1u,    /* §5.2 PB3  (BUTTON 2)   */
    BTN_IDX_LAMP     = 2u,    /* §5.3 PB5  (BUTTON 4)   */
    BTN_IDX_AUTO_MAN = 3u,    /* §5.4 PB4  (BUTTON 3)   */
    BTN_IDX_RB       = 4u,    /* §5.5 PB15 (BUTTON 5)   */
    BTN_IDX_E        = 5u,    /* §5.6 PD2  (BUTTON 6)   */
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

/* Проверка factory-reset combo (passport_v1.5.md §11): при включении
   прибора удерживаются Авто/Ручной (A) + RB (B) + E. Возвращает 1, если
   все три кнопки нажаты в момент вызова. ВАЖНО: вызывать ДО btn_init() —
   функция сама минимально инициализирует тактирование GPIOB/GPIOD и SWJ
   remap, не трогая внутреннее состояние s_btn[]. Подходит для вызова из
   main() между eeprom_init() и settings_load().                          */
uint8_t    btn_factory_reset_combo_held(void);

#endif /* BUTTONS_H */
