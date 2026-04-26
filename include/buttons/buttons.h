#ifndef BUTTONS_H
#define BUTTONS_H

#include "gd32f10x.h"
#include <stdint.h>

/* ══════════════════════════════════════════════════════════════════════════
   Аппаратная распиновка кнопок — МКВП-02 (GD32F103RCT6, LQFP64)
   ══════════════════════════════════════════════════════════════════════════
   По схеме БУСШ.02.101.01 Э3 (BUTTON 1..6) и функциональному описанию
   «ФУНКЦИОНАЛЬНОСТЬ КНОПОК»:

     BUTTON 1 → PB2    (пин 28)   Mute / PRG        — заглушить зуммер /
                                                      вход-выход в настройки
     BUTTON 2 → PB3    (пин 55)   Вкл/Выкл          ⚠ по умолчанию JTDO  → нужен SWJ-remap
     BUTTON 3 → PB4    (пин 56)   Авто/Ручной (A/M) ⚠ по умолчанию JTRST → нужен SWJ-remap
     BUTTON 4 → PB5    (пин 57)   Лампа             — освещение / «увеличить»
     BUTTON 5 → PB15   (пин 36)   RB (Выбор)        — включение розеток / «назад»
     BUTTON 6 → PD2    (пин 54)   E                 — аварийный режим
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

#define BTN_PORT_PRG        GPIOB
#define BTN_PIN_PRG         GPIO_PIN_2   /* 1: Mute / PRG                  */

#define BTN_PORT_ONOFF      GPIOB
#define BTN_PIN_ONOFF       GPIO_PIN_3   /* 2: ВКЛ/ВЫКЛ (бывш. JTDO)       */

#define BTN_PORT_AUTO_MAN   GPIOB
#define BTN_PIN_AUTO_MAN    GPIO_PIN_4   /* 3: АВТО/РУЧ (бывш. JTRST)      */

#define BTN_PORT_LAMP       GPIOB
#define BTN_PIN_LAMP        GPIO_PIN_5   /* 4: Лампа / «увеличить»         */

#define BTN_PORT_RB         GPIOB
#define BTN_PIN_RB          GPIO_PIN_15  /* 5: RB (выбор) / «назад»        */

#define BTN_PORT_E          GPIOD
#define BTN_PIN_E           GPIO_PIN_2   /* 6: E — авария                  */

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

typedef enum {
    BTN_EV_NONE       = 0x00u,
    BTN_EV_PRG        = 0x01u,   /* короткое нажатие PRG/Mute (BUTTON 1)  */
    BTN_EV_ONOFF      = 0x02u,   /* короткое нажатие ВКЛ/ВЫКЛ (BUTTON 2)  */
    BTN_EV_AUTO_MAN   = 0x03u,   /* короткое нажатие АВТО/РУЧ (BUTTON 3)  */
    BTN_EV_LAMP       = 0x04u,   /* короткое нажатие Лампа    (BUTTON 4)  */
    BTN_EV_RB         = 0x05u,   /* короткое нажатие RB/Выбор (BUTTON 5)  */
    BTN_EV_E          = 0x06u,   /* короткое нажатие E        (BUTTON 6)  */
    BTN_EV_PRG_LONG   = 0x81u,   /* длинное нажатие PRG (вход в меню)     */
    BTN_EV_ONOFF_LONG = 0x82u,   /* длинное ВКЛ/ВЫКЛ ≥3 с → НОЧНОЙ режим  */
    BTN_EV_LAMP_LONG  = 0x84u,   /* удержание Лампа — «увеличить» в ред.  */
    BTN_EV_E_LONG     = 0x86u,   /* длинное нажатие E (зарезервировано)   */
} BtnEvent_t;

/* ── Индексы кнопок (для btn_is_down_idx) ─────────────────────────────── */
typedef enum {
    BTN_IDX_PRG      = 0u,
    BTN_IDX_ONOFF    = 1u,
    BTN_IDX_AUTO_MAN = 2u,
    BTN_IDX_LAMP     = 3u,
    BTN_IDX_RB       = 4u,
    BTN_IDX_E        = 5u,
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
