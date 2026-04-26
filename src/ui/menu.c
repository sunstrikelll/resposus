/*  menu.c — МКВП-02: Монитор-контроллер потока воздуха
 *
 *  Взаимодействие с «блоком процесса» через Modbus-таблицу:
 *
 *  Читает (пишет блок процесса):
 *    MB_ADDR_FLOW          — текущий поток воздуха, м/с (float)
 *    MB_ADDR_EXT_TEMP      — внешняя температура, °С (float)
 *    MB_ADDR_ALARM_FLAGS   — флаги аварий
 *
 *  Пишет (читает блок процесса):
 *    MB_ADDR_MODE_SR       — статус устройства (биты POWER/WORK/ALARM/MANUAL)
 *    MB_ADDR_SETPOINT      — уставка потока, м/с
 *    MB_ADDR_FLOW_SP_R     — уставка повтора, м/с
 *    MB_ADDR_ALARM_LOW     — порог «мало потока», м/с
 *    MB_ADDR_ALARM_LOW_R   — порог сброса аварии, м/с
 *    MB_ADDR_ALARM_DELAY   — задержка аварии, с
 *    MB_ADDR_MANUAL_OUT    — выход ручного режима, 0-100%
 *    MB_ADDR_MANUAL_MEM    — режим памяти: НОРМ(0) / ПАМЯТЬ(1)
 *    MB_ADDR_LED_STATE     — зеркало светодиодов
 *    MB_ADDR_BTN_EVENT     — последнее событие кнопки
 *    MB_ADDR_DISPLAY_LINE_0..3 — содержимое строк LCD
 *
 *  Читает (команды от Modbus-мастера):
 *    MB_ADDR_MODE_CR       — очищается сразу после обработки
 *    MB_ADDR_MENU_GOTO     — прыжок в произвольное состояние меню
 *                            (auto-clear после enter_state)
 *
 *  ─── Дисплей 20×4, Win-1251 ───────────────────────────────────────────────
 *
 *  POWER_ON:
 *    0: "    МКВП-02         "
 *    1: "  Инициализация...  "
 *    2: "  ver: X.X          "
 *    3: "                    "
 *
 *  STANDBY:
 *    0: "    МКВП-02         "
 *    1: "  ЧЧ:ММ:СС          "   (секунды с запуска → HH:MM:SS)
 *    2: "  Ожидание...       "
 *    3: "  Нажмите ВКЛ/ВЫКЛ  "
 *
 *  MAIN АВТО:
 *    0: "МКВП-02 АВТО   РАБОТА"  (или СТОП; суффикс R при MODE_REPEAT)
 *    1: "Уст:X.XX Тек:X.XX м/с"
 *    2: "Темп:  XX.X°С       "
 *    3: "ПРГ:меню А/Р:режим  "
 *
 *  MAIN РУЧ:
 *    0: "МКВП-02  РУЧ   РАБОТА"  (суффикс R при MODE_REPEAT)
 *    1: "Вых:XXX% Тек:X.XX м/с"
 *    2: "Темп:  XX.X°С       "
 *    3: "A/M:авто ПРГ:меню   "  (настройка % — через SETTINGS)
 *
 *  MAIN при аварии (inline EMERGENZA):
 *    0: "МКВП-02 АВАРИЯ РАБОТА" (мигает слово АВАРИЯ)
 *    1-2: значения потока/температуры как обычно
 *    3: "ПРГ: тип аварии     "  — ПРГ открывает экран ALARM
 *
 *  ALARM (экран типа аварии):
 *    0: "!!!  АВАРИЯ  !!!    "  (мигает)
 *    1: тип аварии (Мало потока! / Авар.инвертора! / Дверь открыта!)
 *    2: "Тек: X.XX м/с       "
 *    3: "ПРГ:квит А/Р:режим  "
 *
 *  Конфигурация (PASS1..PASS4) — поэкранная циклическая навигация:
 *    Каждый параметр — собственный экран (нет «списка с курсором»).
 *      PRG-short    — сохранить и следующий параметр в текущем PASS (цикл);
 *      PRG-long     — сохранить и выйти в MAIN;
 *      Лампа / удерж. Лампа — увеличить значение;
 *      ВКЛ/ВЫКЛ    — уменьшить значение;
 *      Авто/Ручной — следующее окно настроек (next PASS, циклически);
 *      RB          — предыдущее окно настроек (prev PASS, циклически);
 *      E           — Аварийный режим (доступен из любого экрана).
 *
 *  Экран редактирования (SET_FLOW_SP / SET_FLOW_SPR / SET_ALARM_LOW / SET_ALARM_LOWR):
 *    0: заголовок (например, "-- Уставка потока --")
 *    1: ""
 *    2: "  Знач:  X.XX м/с   "
 *    3: "ЛМП:+ ВКЛ:- ПРГ:след"
 *
 *  SET_ALARM_TIME:
 *    2: "  Задержка:  XXX с  "
 *
 *  SET_MEM_NOR:
 *    2: "  Режим: НОРМ/ПАМЯТЬ"
 */

#include "menu.h"
#include "modbus_table.h"
#include "lcd_hd44780.h"
#include "led.h"
#include "settings.h"
#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* Версия прошивки — выводится на splash-экране POWER_ON.
   Ранее хранилась в Modbus-регистре; теперь — compile-time константа.    */
#define FW_VERSION_F   1.0f

/* ══════════════════════════════════════════════════════════════════════════
   Пины светодиодов (active-HIGH push-pull, порт C)
   ══════════════════════════════════════════════════════════════════════════ */
#define LED_PORT        GPIOC
#define LED_RCU         RCU_GPIOC
#define LED_PIN_POWER   GPIO_PIN_6   /* зелёный: питание / активен          */
#define LED_PIN_WORK    GPIO_PIN_7   /* зелёный: процесс запущен            */
#define LED_PIN_ALARM   GPIO_PIN_8   /* красный: авария                     */
#define LED_PIN_MANUAL  GPIO_PIN_9   /* жёлтый:  ручной режим               */

/* ══════════════════════════════════════════════════════════════════════════
   Тайминги (единица = 1 вызов menu_process = 10 мс)
   ══════════════════════════════════════════════════════════════════════════ */
#define POWERON_TICKS      400u   /* 4 с — диагностика при старте (§6)      */
#define BLINK_TICKS         50u   /* 500 мс — период мигания ALARM/POWER_ON */
#define DISP_TICKS          50u   /* 500 мс — период обновления дисплея     */

/* ══════════════════════════════════════════════════════════════════════════
   Доковые таймеры (passport_v1.5.md §5..§10)
   Единица — 10-мс тик menu_process().
   ══════════════════════════════════════════════════════════════════════════ */
#define ALARM_ROTATE_TICKS    500u   /* 5 с — пауза между сообщениями alarm   */
#define WORK_GRACE_TICKS     4500u   /* 45 с — глушение тревог после WORK on  */
#define LAMP_AUTO_OFF_TICKS 180000u  /* 30 мин — автовыкл лампы при WORK=off  */
#define SOCKET_DELAY_TICKS    50u    /* 0.5 с — задержка включения SOCKET     */
#define INACT_TIMEOUT_TICKS 12000u   /* 2 мин — таймаут неактивности config   */
#define BOOST_TICKS          100u    /* 1 с — макс. вых. при старте WORK      */
#define BUZZER_PERIOD_TICKS 1000u    /* 10 с — период цикла зуммера (§8)      */
#define BUZZER_ON_TICKS      200u    /* 2 с  — длительность сигнала в цикле   */

/* ══════════════════════════════════════════════════════════════════════════
   Параметры редактирования
   ══════════════════════════════════════════════════════════════════════════ */
#define FLOW_STEP       0.1f      /* шаг изменения потока, м/с              */
#define FLOW_MAX        9.9f      /* максимальная уставка, м/с              */
#define FLOW_MIN        0.0f      /* минимальная уставка, м/с               */
#define ALARM_TIME_MAX  180u      /* максимальная задержка аварии, с        */

/* ══════════════════════════════════════════════════════════════════════════
   Строковые константы Win-1251 (ровно 20 символов + '\0')
   ══════════════════════════════════════════════════════════════════════════ */

/* "    МКВП-02         " */
static const char S_TITLE[21] =
    "    "
    "\xCC\xCA\xC2\xCF"  /* МКВП */
    "-02"
    "         ";

/* "  Инициализация...  " */
static const char S_INIT[21] =
    "  "
    "\xC8\xED\xE8\xF6\xE8\xE0\xEB\xE8\xE7\xE0\xF6\xE8\xFF"  /* Инициализация */
    "... ";

/* "  Ожидание...       " */
static const char S_STANDBY_WAIT[21] =
    "  "
    "\xCE\xE6\xE8\xE4\xE0\xED\xE8\xE5"  /* Ожидание */
    "...       ";

/* "  Нажмите ВКЛ/ВЫКЛ  " */
static const char S_STANDBY_BTN[21] =
    "  "
    "\xCD\xE0\xE6\xEC\xE8\xF2\xE5"  /* Нажмите */
    " "
    "\xC2\xCA\xCB"   /* ВКЛ */
    "/"
    "\xC2\xDB\xCA\xCB"  /* ВЫКЛ */
    "  ";

/* "!!!  АВАРИЯ  !!!    " */
static const char S_ALARM_HDR[21] =
    "!!!"
    "  "
    "\xC0\xC2\xC0\xD0\xC8\xFF"  /* АВАРИЯ */
    "  "
    "!!!    ";

/* "  ## EMERGENZA ##   " — баннер на row 0 при MB_BIT_MODE_EMERGENCY (§5.6).
   Сохранён латинский оригинал термина из документа.                       */
static const char S_EMERG_HDR[21] =
    "  ## EMERGENZA ##   ";

/* "  E:сброс аварии    " — подсказка при активном EMERGENZA */
static const char S_HINT_EMERG[21] =
    "  E:"
    "\xF1\xE1\xF0\xEE\xF1"      /* сброс */
    " "
    "\xE0\xE2\xE0\xF0\xE8\xE8"  /* аварии */
    "    ";

/* "--- ПАРОЛЬ ВХОДА ---" */
static const char S_PWD_HDR[21] =
    "--- "
    "\xCF\xC0\xD0\xCE\xCB\xDC"  /* ПАРОЛЬ */
    " "
    "\xC2\xF5\xEE\xE4\xE0"      /* входа */
    " ---";

/* "ПРГ:OK A/B:след/пред" — подсказка PASSWORD */
static const char S_HINT_PWD[21] =
    "\xCF\xD0\xC3"  /* ПРГ */
    ":OK "
    "\xC0/B:"        /* A/B: */
    "\xF1\xEB\xE5\xE4"   /* след */
    "/"
    "\xEF\xF0\xE5\xE4"   /* пред */
    "";

/* "  Неверный пароль   " — экран ошибки */
static const char S_PWD_BAD[21] =
    "  "
    "\xCD\xE5\xE2\xE5\xF0\xED\xFB\xE9"  /* Неверный */
    " "
    "\xEF\xE0\xF0\xEE\xEB\xFC"          /* пароль */
    "   ";

/* "   ВЕНТИЛЯЦИЯ MAX   " — подсказка boost при старте WORK (1 с) */
static const char S_HINT_BOOST[21] =
    "   "
    "\xC2\xE5\xED\xF2\xE8\xEB\xFF\xF6\xE8\xFF"  /* Вентиляция (10) */
    " MAX"
    "   ";

/* "=== НАСТРОЙКИ ===   " */
static const char S_SETTINGS_HDR[21] =
    "=== "
    "\xCD\xC0\xD1\xD2\xD0\xCE\xC9\xCA\xC8"  /* НАСТРОЙКИ */
    " ===   ";

/* (Списочные пункты подменю S_ITEM_* удалены: доковая модель
   §5/§10 не предусматривает экрана-списка с курсором. Заголовки
   ставятся непосредственно на экранах редакторов S_HDR_*.) */

/* ── Заголовки экранов редактирования (20 символов) ─────────────────── */
/* "-- Уставка потока --" */
static const char S_HDR_FLOW_SP[21] =
    "-- "
    "\xD3\xF1\xF2\xE0\xE2\xEA\xE0"  /* Уставка */
    " "
    "\xEF\xEE\xF2\xEE\xEA\xE0"  /* потока */
    " --";

/* "-- Уставка повтора -" */
static const char S_HDR_FLOW_SPR[21] =
    "-- "
    "\xD3\xF1\xF2\xE0\xE2\xEA\xE0"  /* Уставка */
    " "
    "\xEF\xEE\xE2\xF2\xEE\xF0\xE0"  /* повтора */
    " -";

/* "--- Порог аварии ---" */
static const char S_HDR_ALARM_LOW[21] =
    "--- "
    "\xCF\xEE\xF0\xEE\xE3"  /* Порог */
    " "
    "\xE0\xE2\xE0\xF0\xE8\xE8"  /* аварии */
    " ---";

/* "--- Сброс аварии ---" */
static const char S_HDR_ALARM_LOWR[21] =
    "--- "
    "\xD1\xE1\xF0\xEE\xF1"  /* Сброс */
    " "
    "\xE0\xE2\xE0\xF0\xE8\xE8"  /* аварии */
    " ---";

/* "-- Задержка аварии -" */
static const char S_HDR_ALARM_TIME[21] =
    "-- "
    "\xC7\xE0\xE4\xE5\xF0\xE6\xEA\xE0"  /* Задержка */
    " "
    "\xE0\xE2\xE0\xF0\xE8\xE8"  /* аварии */
    " -";

/* "--- Режим ручного --" */
static const char S_HDR_MEM_NOR[21] =
    "--- "
    "\xD0\xE5\xE6\xE8\xEC"  /* Режим */
    " "
    "\xF0\xF3\xF7\xED\xEE\xE3\xEE"  /* ручного */
    " --";

/* ── Подсказки кнопок (20 символов) ─────────────────────────────────── */
/* "ПРГ:меню А/Р:режим  " */
static const char S_HINT_AUTO[21] =
    "\xCF\xD0\xC3"     /* ПРГ */
    ":"
    "\xEC\xE5\xED\xFE"   /* меню */
    " "
    "\xC0/\xD0"         /* А/Р */
    ":"
    "\xF0\xE5\xE6\xE8\xEC"  /* режим */
    "  ";

/* "A/M:авто ПРГ:меню   " — подсказка в MAIN РУЧ.
   В новой раскладке кнопок больше нет ВВ/ВН «стрелок» на главном
   экране; выход % настраивается через SETTINGS → Уставка/Ноль выхода.  */
static const char S_HINT_MAN[21] =
    "A/M:"
    "\xE0\xE2\xF2\xEE"  /* авто */
    " "
    "\xCF\xD0\xC3"       /* ПРГ */
    ":"
    "\xEC\xE5\xED\xFE"   /* меню */
    "   ";

/* "ПРГ:квит А/Р:режим  " */
static const char S_HINT_ALARM[21] =
    "\xCF\xD0\xC3"     /* ПРГ */
    ":"
    "\xEA\xE2\xE8\xF2"  /* квит */
    " "
    "\xC0/\xD0"         /* А/Р */
    ":"
    "\xF0\xE5\xE6\xE8\xEC"  /* режим */
    "  ";

/* "ЛМП:+ ВКЛ:- ПРГ:след"
   Подсказка по документации «ФУНКЦИОНАЛЬНОСТЬ КНОПОК» (§5):
     Лампа короткое/удерж. = увеличить значение;
     ВКЛ/ВЫКЛ короткое     = уменьшить значение;
     PRG короткое          = сохранить и перейти к следующему параметру;
     PRG длинное (≥3 с)    = сохранить и выйти из конфигурации в MAIN;
     A/M  короткое         = следующее окно настроек (next PASS);
     RB   короткое         = предыдущее окно настроек (prev PASS).      */
static const char S_HINT_EDIT[21] =
    "\xCB\xCC\xCF"      /* ЛМП */
    ":+ "
    "\xC2\xCA\xCB"      /* ВКЛ */
    ":- "
    "\xCF\xD0\xC3"      /* ПРГ */
    ":"
    "\xF1\xEB\xE5\xE4"  /* след */
    "";

/* "ПРГ: тип аварии     " — подсказка на MAIN при активной аварии */
static const char S_HINT_MAIN_ALARM[21] =
    "\xCF\xD0\xC3"     /* ПРГ */
    ": "
    "\xF2\xE8\xEF"   /* тип */
    " "
    "\xE0\xE2\xE0\xF0\xE8\xE8"  /* аварии */
    "     ";

/* (Списочные пункты PASS2-PASS4 (S_ITEM_*) и заголовки списочных
   экранов (S_SETTINGS_P2/3/4_HDR) удалены вместе со списочной моделью —
   доковая поэкранная навигация PASS-окон использует только S_HDR_*. ) */

/* ═══ Заголовки экранов редактирования PASS2-4 (20 символов) ═══ */

/* "-- Ноль датчика ----" */
static const char S_HDR_SENSOR_Z[21] =
    "-- "
    "\xCD\xEE\xEB\xFC"  /* Ноль */
    " "
    "\xE4\xE0\xF2\xF7\xE8\xEA\xE0"  /* датчика */
    " ----";

/* "-- Диап. датчика ---" */
static const char S_HDR_SENSOR_S[21] =
    "-- "
    "\xC4\xE8\xE0\xEF"  /* Диап */
    ". "
    "\xE4\xE0\xF2\xF7\xE8\xEA\xE0"  /* датчика */
    " ---";

/* "--- Ноль выхода ----" */
static const char S_HDR_OUT_Z[21] =
    "--- "
    "\xCD\xEE\xEB\xFC"  /* Ноль */
    " "
    "\xE2\xFB\xF5\xEE\xE4\xE0"  /* выхода */
    " ----";

/* "--- Диап. выхода ---" */
static const char S_HDR_OUT_S[21] =
    "--- "
    "\xC4\xE8\xE0\xEF"  /* Диап */
    ". "
    "\xE2\xFB\xF5\xEE\xE4\xE0"  /* выхода */
    " ---";

/* "--- ПИД:инт.время --" */
static const char S_HDR_PID_TI[21] =
    "--- "
    "\xCF\xC8\xC4"  /* ПИД */
    ":"
    "\xE8\xED\xF2"  /* инт */
    "."
    "\xE2\xF0\xE5\xEC\xFF"  /* время */
    " --";

/* "--- ПИД:полоса -----" */
static const char S_HDR_PID_BAND[21] =
    "--- "
    "\xCF\xC8\xC4"  /* ПИД */
    ":"
    "\xEF\xEE\xEB\xEE\xF1\xE0"  /* полоса */
    " -----";

/* "----- Автопуск -----" */
static const char S_HDR_BLACKOUT[21] =
    "----- "
    "\xC0\xE2\xF2\xEE\xEF\xF3\xF1\xEA"  /* Автопуск */
    " -----";

/* "--- Обслуживание ---" */
static const char S_HDR_MAINT[21] =
    "--- "
    "\xCE\xE1\xF1\xEB\xF3\xE6\xE8\xE2\xE0\xED\xE8\xE5"  /* Обслуживание */
    " ---";

/* "--- Макс. счётчик --" */
static const char S_HDR_COUNT_MAX[21] =
    "--- "
    "\xCC\xE0\xEA\xF1"  /* Макс */
    ". "
    "\xF1\xF7\xB8\xF2\xF7\xE8\xEA"  /* счётчик */
    " --";

/* "--- Регистратор ----" */
static const char S_HDR_DATALOG[21] =
    "--- "
    "\xD0\xE5\xE3\xE8\xF1\xF2\xF0\xE0\xF2\xEE\xF0"  /* Регистратор */
    " ----";

/* ═══ Суффиксы единиц (null-terminated Win-1251) ═══ */
static const char S_U_MS[]   = "\xEC/\xF1";    /* м/с   */
static const char S_U_CMS[]  = "\xF1\xEC/\xF1";/* см/с  */
static const char S_U_PCT[]  = "%";
static const char S_U_SEC[]  = "\xF1";         /* с     */
static const char S_U_HRS[]  = "\xF7";         /* ч     */

/* ══════════════════════════════════════════════════════════════════════════
   Внутреннее состояние
   ══════════════════════════════════════════════════════════════════════════ */
static MenuState_t s_state       = MENU_POWER_ON;
static uint16_t    s_state_ticks = 0;   /* тиков в текущем состоянии         */
static uint16_t    s_blink_tick  = 0;   /* счётчик мигания                   */
static uint8_t     s_blink_on    = 0;   /* 0/1 — фаза мигания                */
static uint16_t    s_disp_tick   = 0;   /* счётчик до обновления дисплея     */

/* (Курсор и счётчики списочных подменю удалены — доковая модель
   §5/§10 не предполагает экрана-списка с курсором; PASS-окно — это
   набор экранов-параметров, переключаемых короткими нажатиями PRG.) */

/* ── Доковые таймеры/счётчики (passport_v1.5.md §5..§10) ────────────────── */
static uint16_t s_alarm_rot_tick   = 0u;   /* счётчик 5-с ротации тревог    */
static uint8_t  s_alarm_view_idx   = 0u;   /* 0=LOW, 1=INV, 2=DOOR          */
static uint16_t s_work_grace_tick  = 0u;   /* 45-с grace после WORK on      */
static uint8_t  s_work_was_on      = 0u;   /* для детектора 0→1 на WORK     */
static uint32_t s_lamp_off_tick    = 0u;   /* 30-мин счётчик автовыкл LAMP  */
static uint16_t s_socket_pending   = 0u;   /* >0 — таймер до flip SOCKET    */
static uint8_t  s_socket_pending_on = 0u;  /* куда переключим SOCKET        */
static uint16_t s_inact_tick       = 0u;   /* 2-мин таймаут неактивности    */
static uint16_t s_boost_tick       = 0u;   /* 1-с boost при WORK on         */
static uint8_t  s_boost_save_out   = 0u;   /* сохр. MANUAL_OUT перед boost  */

/* ── Override EMERGENZA (§5.6): пока бит MB_BIT_MODE_EMERGENCY активен,
   MANUAL_OUT принудительно = 100 %, режим РУЧ.  При снятии бита —
   восстанавливаем сохранённые значения.                                    */
static uint8_t  s_emerg_was        = 0u;   /* предыдущее значение бита      */
static uint8_t  s_emerg_save_out   = 0u;   /* сохр. MANUAL_OUT при входе    */
static uint8_t  s_emerg_save_man   = 0u;   /* сохр. бит MANUAL при входе    */

/* ── Зуммер тревоги (passport_v1.5.md §8) ────────────────────────────────
   Регистр MB_ADDR_BUZZER_STATE отражает текущее состояние сигнала (0/1)
   для возможной аппаратной интеграции (драйвер пьезоэлемента подключается
   к выходу контроллера и читает этот регистр через периодический опрос
   или GPIO-зеркало).                                                      */
static uint16_t s_buzzer_tick      = 0u;   /* счётчик цикла 10 с             */
static uint8_t  s_buzzer_muted     = 0u;   /* 1 — заглушено до новой тревоги */
static uint8_t  s_alarm_was_on     = 0u;   /* для детектора 0→1 на ALARM    */

/* ── PASSWORD-вход (passport_v1.5.md §10) ────────────────────────────────── */
static uint8_t  s_pw_field         = 0u;   /* активная цифра 0..3           */
static uint8_t  s_pw_digits[4]     = {0,0,0,0}; /* введённые цифры         */
static uint8_t  s_pw_attempts      = 0u;   /* 0..3 — счёт попыток           */

/* Редактируемые переменные (черновики — не пишутся в регистры до PRG) */
static float       s_edit_float   = 0.0f;   /* черновик float-параметра       */
static uint8_t     s_edit_uint    = 0u;     /* черновик uint-параметра         */
static uint16_t    s_edit_reg     = 0u;     /* адрес регистра для записи       */

/* Параметры текущего редактора (настраиваются в enter_state) */
static float        s_edit_min    = 0.0f;   /* минимум диапазона              */
static float        s_edit_max    = 0.0f;   /* максимум диапазона             */
static float        s_edit_step   = 0.1f;   /* шаг изменения                  */
static uint8_t      s_edit_dec    = 2u;     /* знаков после точки 0/1/2       */
static uint8_t      s_edit_umax   = 100u;   /* верх. предел для uint-редактора*/
static const char  *s_edit_hdr    = NULL;   /* 20-символьный заголовок        */
static const char  *s_edit_unit   = "";     /* суффикс единиц                 */

/* Тип текущего редактора */
typedef enum {
    EDIT_KIND_FLOAT   = 0,    /* float со step, min, max, decimals, unit   */
    EDIT_KIND_UINT    = 1,    /* uint8 0..umax (ALARM_TIME, OUT_Z/S, DATALOG_SEC) */
    EDIT_KIND_TOGGLE  = 2,    /* 0/1 toggle: MEM_NOR, BLACKOUT, DATALOG     */
} EditKind_t;
static EditKind_t   s_edit_kind   = EDIT_KIND_FLOAT;

/* Подтип toggle-редактора для локализации надписей */
typedef enum {
    TOG_MEM_NOR   = 0,   /* НОРМ / ПАМЯТЬ */
    TOG_ON_OFF    = 1,   /* OFF  / ON     */
} ToggleLabel_t;
static ToggleLabel_t s_edit_tog_lbl = TOG_MEM_NOR;

/* ══════════════════════════════════════════════════════════════════════════
   Последовательности параметров каждого PASS (passport_v1.5.md §10).

   Внутри одного PASS короткое нажатие PRG циклически переключает
   параметры, кнопка A/M переходит к первому параметру следующего PASS,
   RB — к первому параметру предыдущего PASS.
   ══════════════════════════════════════════════════════════════════════════ */

static const MenuState_t PASS1_seq[] = {
    MENU_SET_FLOW_SP,    MENU_SET_FLOW_SPR,
    MENU_SET_ALARM_LOW,  MENU_SET_ALARM_LOWR,
    MENU_SET_ALARM_TIME, MENU_SET_MEM_NOR,
};
static const MenuState_t PASS2_seq[] = {
    MENU_SET_SENSOR_Z, MENU_SET_SENSOR_S,
    MENU_SET_OUT_Z,    MENU_SET_OUT_S,
    MENU_SET_PID_TI,   MENU_SET_PID_BAND,
    MENU_SET_BLACKOUT,
};
static const MenuState_t PASS3_seq[] = {
    MENU_SET_MAINT, MENU_SET_COUNT_MAX,
};
static const MenuState_t PASS4_seq[] = {
    MENU_SET_DATALOG,
};

typedef struct {
    const MenuState_t *seq;
    uint8_t            n;
} PassDef_t;

#define PASS_COUNT  4u
static const PassDef_t PASSES[PASS_COUNT] = {
    { PASS1_seq, (uint8_t)(sizeof(PASS1_seq) / sizeof(PASS1_seq[0])) },
    { PASS2_seq, (uint8_t)(sizeof(PASS2_seq) / sizeof(PASS2_seq[0])) },
    { PASS3_seq, (uint8_t)(sizeof(PASS3_seq) / sizeof(PASS3_seq[0])) },
    { PASS4_seq, (uint8_t)(sizeof(PASS4_seq) / sizeof(PASS4_seq[0])) },
};

/* Найти, в каком PASS сейчас находимся, и индекс параметра внутри него.
   Возвращает 1 при успехе, 0 если состояние st — не редактор. */
static uint8_t locate_param(MenuState_t st, uint8_t *pass_out, uint8_t *idx_out)
{
    for (uint8_t p = 0u; p < PASS_COUNT; p++) {
        for (uint8_t i = 0u; i < PASSES[p].n; i++) {
            if (PASSES[p].seq[i] == st) {
                if (pass_out) *pass_out = p;
                if (idx_out)  *idx_out  = i;
                return 1u;
            }
        }
    }
    return 0u;
}

/* Сохранить текущий черновик в Modbus-регистр (без EEPROM-flush). */
static void editor_commit_draft(void)
{
    if (s_edit_kind == EDIT_KIND_FLOAT)
        MB_WriteFloat(s_edit_reg, s_edit_float);
    else
        MB_WriteBits (s_edit_reg, s_edit_uint);
}

/* ══════════════════════════════════════════════════════════════════════════
   Вспомогательные функции форматирования
   ══════════════════════════════════════════════════════════════════════════ */

/* float с 2 знаками после точки (X.XX), 4 символа, max 9.99 */
static char *fmt_f2(char *p, float v)
{
    if (v < 0.0f)  v = 0.0f;
    if (v > 9.99f) v = 9.99f;
    int32_t i   = (int32_t)v;
    int32_t fr2 = (int32_t)((v - (float)i) * 100.0f + 0.5f);
    if (fr2 >= 100) { i++; fr2 = 0; }
    *p++ = (char)('0' + (i % 10));          /* всегда 1 цифра (0-9)  */
    *p++ = '.';
    *p++ = (char)('0' + fr2 / 10);
    *p++ = (char)('0' + fr2 % 10);
    return p;
}

/* float с 1 знаком после точки (XX.X), ровно до 5 символов ("-99.9"..."99.9").
   Значения вне диапазона ±99.9 «прищипываются», чтобы исключить выход
   за пределы экранной строки при случайной записи в регистр извне.        */
static char *fmt_f1(char *p, float v)
{
    if (v < -99.9f) v = -99.9f;
    if (v >  99.9f) v =  99.9f;
    if (v < 0.0f) { *p++ = '-'; v = -v; }
    int32_t i  = (int32_t)v;                  /* 0..99 */
    int32_t fr = (int32_t)((v - (float)i) * 10.0f + 0.5f);
    if (fr >= 10) { i++; fr = 0; if (i > 99) i = 99; }
    if (i >= 10) *p++ = (char)('0' + (i / 10));
    *p++ = (char)('0' + (i % 10));
    *p++ = '.';
    *p++ = (char)('0' + fr);
    return p;
}

/* uint8 в 3 символа с ведущими пробелами ("  5", " 50", "100") */
static char *fmt_u8_3(char *p, uint8_t v)
{
    p[0] = (v >= 100u) ? (char)('0' + v / 100u) : ' ';
    p[1] = (v >=  10u) ? (char)('0' + (v / 10u) % 10u) : ((v >= 100u) ? '0' : ' ');
    p[2] = (char)('0' + v % 10u);
    return p + 3;
}

/* uint16 в 4 символа с ведущими пробелами ("   5", "  50", " 999", "9999") */
static char *fmt_u16_4(char *p, uint16_t v)
{
    if (v > 9999u) v = 9999u;
    uint16_t th = v / 1000u;
    uint16_t hu = (v / 100u) % 10u;
    uint16_t te = (v / 10u)  % 10u;
    uint16_t on = v % 10u;
    p[0] = (th)                      ? (char)('0' + th) : ' ';
    p[1] = (th || hu)                ? (char)('0' + hu) : ' ';
    p[2] = (th || hu || te)          ? (char)('0' + te) : ' ';
    p[3] = (char)('0' + on);
    return p + 4;
}

/* Время HH:MM:SS из секунд (ровно 8 символов) */
static char *fmt_time(char *p, uint32_t sec)
{
    uint32_t h = sec / 3600u;
    uint32_t m = (sec % 3600u) / 60u;
    uint32_t s = sec % 60u;
    if (h > 99u) h = 99u;
    *p++ = (char)('0' + h / 10u);
    *p++ = (char)('0' + h % 10u);
    *p++ = ':';
    *p++ = (char)('0' + m / 10u);
    *p++ = (char)('0' + m % 10u);
    *p++ = ':';
    *p++ = (char)('0' + s / 10u);
    *p++ = (char)('0' + s % 10u);
    return p;
}

/* ══════════════════════════════════════════════════════════════════════════
   Вспомогательные функции вывода на дисплей
   ══════════════════════════════════════════════════════════════════════════ */

/*  Единственная точка вывода на LCD + в Modbus-таблицу.
 *
 *  Здесь строго нормализуется длина выводимой строки до 20 видимых
 *  символов (ширина экрана 20х4):
 *      • если источник короче 20 — добивается пробелами;
 *      • если длиннее или вовсе не заканчивается '\0' — обрезается.
 *  Без этой защиты HD44780 переносил бы «хвост» первой строки (col>=20)
 *  в DDRAM третьей строки (row=2), что приводит к наложению текста
 *  между строками на экране.
 */
static void lcd_mb_write(uint8_t row, const char *line)
{
    static const uint16_t mb_line_addr[4] = {
        MB_ADDR_DISPLAY_LINE_0, MB_ADDR_DISPLAY_LINE_1,
        MB_ADDR_DISPLAY_LINE_2, MB_ADDR_DISPLAY_LINE_3
    };
    if (row >= 4u) return;

    char buf[21];
    uint8_t i = 0u;
    if (line != NULL) {
        while (i < 20u && line[i] != '\0') { buf[i] = line[i]; i++; }
    }
    while (i < 20u) { buf[i] = ' '; i++; }
    buf[20] = '\0';

    lcd_print_win1251_at(row, 0, buf);
    MB_WriteString(mb_line_addr[row], buf);
}

static void lcd_mb_blank(uint8_t row)
{
    lcd_mb_write(row, "                    ");
}

/* ══════════════════════════════════════════════════════════════════════════
   Управление светодиодами
   ══════════════════════════════════════════════════════════════════════════ */

static void leds_init(void)
{
    rcu_periph_clock_enable(LED_RCU);
    const uint32_t pins = LED_PIN_POWER | LED_PIN_WORK |
                          LED_PIN_ALARM | LED_PIN_MANUAL;
    gpio_init(LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, pins);
    gpio_bit_reset(LED_PORT, pins);
}

static void led_set(uint32_t pin, uint8_t on)
{
    if (on) gpio_bit_set  (LED_PORT, pin);
    else    gpio_bit_reset(LED_PORT, pin);
}

static void leds_update(void)
{
    uint8_t sr    = MB_ReadBits(MB_ADDR_MODE_SR);
    uint8_t power = (sr & MB_BIT_MODE_POWER)  ? 1u : 0u;
    uint8_t work  = (sr & MB_BIT_MODE_WORK)   ? 1u : 0u;
    uint8_t alarm = (sr & MB_BIT_MODE_ALARM)  ? 1u : 0u;
    uint8_t man   = (sr & MB_BIT_MODE_MANUAL) ? 1u : 0u;

    /* ALARM и POWER_ON-заставка — мигают */
    uint8_t alarm_vis = alarm ? s_blink_on : 0u;
    uint8_t power_vis = (s_state == MENU_POWER_ON) ? s_blink_on : power;

    led_set(LED_PIN_POWER,  power_vis);
    led_set(LED_PIN_WORK,   work);
    led_set(LED_PIN_ALARM,  alarm_vis);
    led_set(LED_PIN_MANUAL, man);

    uint8_t ls = 0u;
    if (power_vis) ls |= MB_LED_POWER;
    if (work)      ls |= MB_LED_WORK;
    if (alarm_vis) ls |= MB_LED_ALARM;
    if (man)       ls |= MB_LED_MANUAL;
    MB_WriteBits(MB_ADDR_LED_STATE, ls);
}

/* ══════════════════════════════════════════════════════════════════════════
   Переход в состояние
   ══════════════════════════════════════════════════════════════════════════ */

static void enter_state(MenuState_t st)
{
    /* Доковая модель конфигурации (passport_v1.5.md §5, §10):
       КАЖДЫЙ параметр — собственный экран, между параметрами
       текущего PASS-окна переключаемся короткими нажатиями PRG,
       а между PASS-окнами — кнопками A/M и RB.
       Списочного экрана PASS «с курсором» в оригинальном приборе нет —
       поэтому MENU_SETTINGS / MENU_SETTINGS_P2..P4 рассматриваем как
       алиасы «первого параметра соответствующего PASS», чтобы внешние
       тестовые скрипты, пишущие в MB_ADDR_MENU_GOTO, продолжали работать. */
    switch (st) {
    case MENU_SETTINGS:    st = MENU_SET_FLOW_SP;  break;
    case MENU_SETTINGS_P2: st = MENU_SET_SENSOR_Z; break;
    case MENU_SETTINGS_P3: st = MENU_SET_MAINT;    break;
    case MENU_SETTINGS_P4: st = MENU_SET_DATALOG;  break;
    default: break;
    }

    s_state       = st;
    s_state_ticks = 0u;

    switch (st) {
    /* ── Базовые состояния ── */
    case MENU_POWER_ON:
        MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_POWER);
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_STANDBY);
        break;

    case MENU_STANDBY:
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_POWER);
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
        MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_STANDBY);
        break;

    case MENU_MAIN:
        MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_POWER);
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_STANDBY);
        break;

    case MENU_ALARM:
        MB_SetBit(MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);
        break;

    /* MENU_SETTINGS_P* — после редиректа сюда не попадаем; оставлено
       пустым, чтобы избежать -Wswitch при включённом enum-strict. */
    case MENU_SETTINGS:
    case MENU_SETTINGS_P2:
    case MENU_SETTINGS_P3:
    case MENU_SETTINGS_P4:
        break;

    /* ── Редактирование float-параметров PASS1 (м/с) ── */
    case MENU_SET_FLOW_SP:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_SETPOINT;
        s_edit_float = MB_ReadFloat(MB_ADDR_SETPOINT);
        s_edit_min   = 0.0f; s_edit_max = 9.9f; s_edit_step = 0.1f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_FLOW_SP;
        break;
    case MENU_SET_FLOW_SPR:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_FLOW_SP_R;
        s_edit_float = MB_ReadFloat(MB_ADDR_FLOW_SP_R);
        s_edit_min   = 0.0f; s_edit_max = 9.9f; s_edit_step = 0.1f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_FLOW_SPR;
        break;
    case MENU_SET_ALARM_LOW:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_ALARM_LOW;
        s_edit_float = MB_ReadFloat(MB_ADDR_ALARM_LOW);
        s_edit_min   = 0.0f; s_edit_max = 9.9f; s_edit_step = 0.1f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_ALARM_LOW;
        break;
    case MENU_SET_ALARM_LOWR:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_ALARM_LOW_R;
        s_edit_float = MB_ReadFloat(MB_ADDR_ALARM_LOW_R);
        s_edit_min   = 0.0f; s_edit_max = 9.9f; s_edit_step = 0.1f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_ALARM_LOWR;
        break;

    /* ── Редакторы PASS2 (float) ── */
    case MENU_SET_SENSOR_Z:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_SENSOR_ZERO;
        s_edit_float = MB_ReadFloat(MB_ADDR_SENSOR_ZERO);
        s_edit_min   = 0.0f; s_edit_max = 9.9f; s_edit_step = 0.1f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_SENSOR_Z;
        break;
    case MENU_SET_SENSOR_S:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_SENSOR_SPAN;
        s_edit_float = MB_ReadFloat(MB_ADDR_SENSOR_SPAN);
        s_edit_min   = 0.0f; s_edit_max = 9.9f; s_edit_step = 0.1f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_SENSOR_S;
        break;
    case MENU_SET_PID_TI:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_PID_TI;
        s_edit_float = MB_ReadFloat(MB_ADDR_PID_TI);
        s_edit_min   = 0.0f; s_edit_max = 9.9f; s_edit_step = 0.1f;
        s_edit_dec   = 1u;   s_edit_unit = S_U_SEC; s_edit_hdr = S_HDR_PID_TI;
        break;
    case MENU_SET_PID_BAND:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_PID_BAND;
        s_edit_float = MB_ReadFloat(MB_ADDR_PID_BAND);
        s_edit_min   = 1.0f; s_edit_max = 999.0f; s_edit_step = 1.0f;
        s_edit_dec   = 0u;   s_edit_unit = S_U_CMS; s_edit_hdr = S_HDR_PID_BAND;
        break;

    /* ── Редакторы PASS3 (float) ── */
    case MENU_SET_MAINT:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_MAINT_HOURS;
        s_edit_float = MB_ReadFloat(MB_ADDR_MAINT_HOURS);
        s_edit_min   = 0.0f; s_edit_max = 9999.0f; s_edit_step = 10.0f;
        s_edit_dec   = 0u;   s_edit_unit = S_U_HRS; s_edit_hdr = S_HDR_MAINT;
        break;

    /* ── Редакторы PASS3 (float) — продолжение ── */
    case MENU_SET_COUNT_MAX:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_COUNT_MAX;
        s_edit_float = MB_ReadFloat(MB_ADDR_COUNT_MAX);
        s_edit_min   = 0.0f; s_edit_max = 9999.0f; s_edit_step = 10.0f;
        s_edit_dec   = 0u;   s_edit_unit = S_U_HRS; s_edit_hdr = S_HDR_COUNT_MAX;
        break;

    /* ── Редактирование uint-параметров ── */
    case MENU_SET_ALARM_TIME:
        s_edit_kind = EDIT_KIND_UINT;
        s_edit_reg  = MB_ADDR_ALARM_DELAY;
        s_edit_uint = MB_ReadBits(MB_ADDR_ALARM_DELAY);
        s_edit_umax = 180u;  s_edit_unit = S_U_SEC; s_edit_hdr = S_HDR_ALARM_TIME;
        break;
    case MENU_SET_OUT_Z:
        s_edit_kind = EDIT_KIND_UINT;
        s_edit_reg  = MB_ADDR_OUT_ZERO_PCT;
        s_edit_uint = MB_ReadBits(MB_ADDR_OUT_ZERO_PCT);
        s_edit_umax = 100u;  s_edit_unit = S_U_PCT; s_edit_hdr = S_HDR_OUT_Z;
        break;
    case MENU_SET_OUT_S:
        s_edit_kind = EDIT_KIND_UINT;
        s_edit_reg  = MB_ADDR_OUT_SPAN_PCT;
        s_edit_uint = MB_ReadBits(MB_ADDR_OUT_SPAN_PCT);
        s_edit_umax = 100u;  s_edit_unit = S_U_PCT; s_edit_hdr = S_HDR_OUT_S;
        break;

    /* ── Toggle-редакторы ── */
    case MENU_SET_MEM_NOR:
        s_edit_kind    = EDIT_KIND_TOGGLE;
        s_edit_reg     = MB_ADDR_MANUAL_MEM;
        s_edit_uint    = MB_ReadBits(MB_ADDR_MANUAL_MEM);
        s_edit_tog_lbl = TOG_MEM_NOR;
        s_edit_hdr     = S_HDR_MEM_NOR;
        break;
    case MENU_SET_BLACKOUT:
        s_edit_kind    = EDIT_KIND_TOGGLE;
        s_edit_reg     = MB_ADDR_BLACKOUT_EN;
        s_edit_uint    = MB_ReadBits(MB_ADDR_BLACKOUT_EN);
        s_edit_tog_lbl = TOG_ON_OFF;
        s_edit_hdr     = S_HDR_BLACKOUT;
        break;
    case MENU_SET_DATALOG:
        s_edit_kind    = EDIT_KIND_TOGGLE;
        s_edit_reg     = MB_ADDR_DATALOG_EN;
        s_edit_uint    = MB_ReadBits(MB_ADDR_DATALOG_EN);
        s_edit_tog_lbl = TOG_ON_OFF;
        s_edit_hdr     = S_HDR_DATALOG;
        break;

    /* ── PASSWORD-вход: 4-значный код, обнуление полей при входе ── */
    case MENU_PASSWORD:
        s_pw_field    = 0u;
        s_pw_digits[0] = s_pw_digits[1] = s_pw_digits[2] = s_pw_digits[3] = 0u;
        s_pw_attempts = 0u;
        break;
    }

    /* Ограничить значение, прочитанное из Modbus, диапазоном редактора,
       чтобы не показывать на экране внедиапазонное значение. */
    if (s_edit_kind == EDIT_KIND_FLOAT) {
        if (s_edit_float < s_edit_min) s_edit_float = s_edit_min;
        if (s_edit_float > s_edit_max) s_edit_float = s_edit_max;
    } else if (s_edit_kind == EDIT_KIND_UINT) {
        if (s_edit_uint > s_edit_umax) s_edit_uint = s_edit_umax;
    } else if (s_edit_kind == EDIT_KIND_TOGGLE) {
        if (s_edit_uint > 1u) s_edit_uint = 1u;
    }

    /* Пометить экран «грязным», чтобы ближайший menu_process() (≤ 10 мс)
       его перерисовал. Прямой вызов menu_update_display() отсюда НЕЛЬЗЯ:
       enter_state() может быть вызван несколько раз подряд (длинное+
       короткое нажатие, чейн из handle_modbus_cmd, автопереход и т. д.),
       а полный ре-рендер занимает ~10 мс — два подряд «съедают» дедлайн
       задачи UI и ломают дебаунс (пропадают нажатия).                    */
    s_disp_tick = (uint16_t)DISP_TICKS;
}

/* ══════════════════════════════════════════════════════════════════════════
   Обработка menu_goto (тестовый прыжок в любое состояние меню)
   ══════════════════════════════════════════════════════════════════════════
   Мастер Modbus пишет в MB_ADDR_MENU_GOTO код состояния из MenuState_t
   (0..MENU_SET_DATALOG). Если значение корректно — вызываем enter_state и
   сразу обнуляем регистр. Это позволяет за один Modbus-write попасть в
   любой редактор или подменю, минуя навигацию кнопками — удобно для
   автотестов «пройти по всем пунктам».                                   */

static void handle_menu_goto(void)
{
    uint8_t code = MB_ReadBits(MB_ADDR_MENU_GOTO);
    if (code == 0u) return;                 /* 0 = no-op                 */
    MB_WriteBits(MB_ADDR_MENU_GOTO, 0u);    /* auto-clear                */

    if (code > (uint8_t)MENU_PASSWORD)      /* вне диапазона — игнор     */
        return;

    enter_state((MenuState_t)code);
}

/* ══════════════════════════════════════════════════════════════════════════
   Обработка команд Modbus-мастера (MODE_CR)
   ══════════════════════════════════════════════════════════════════════════ */

static void handle_modbus_cmd(void)
{
    uint8_t cmd = MB_ReadBits(MB_ADDR_MODE_CR);
    if (cmd == MB_CMD_NONE) return;
    MB_WriteBits(MB_ADDR_MODE_CR, MB_CMD_NONE);

    switch (cmd) {
    case MB_CMD_POWER_ON:
        if (s_state == MENU_STANDBY)
            enter_state(MENU_POWER_ON);
        break;
    case MB_CMD_STANDBY:
        enter_state(MENU_STANDBY);
        break;
    case MB_CMD_START:
        if (s_state == MENU_MAIN)
            MB_SetBit(MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
        break;
    case MB_CMD_STOP:
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
        break;
    case MB_CMD_SET_AUTO:
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
        break;
    case MB_CMD_SET_MANUAL:
        MB_SetBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
        break;
    case MB_CMD_ACK_ALARM:
        MB_WriteBits(MB_ADDR_ALARM_FLAGS, 0u);
        MB_ClearBit (MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);
        if (s_state == MENU_ALARM)
            enter_state(MENU_MAIN);
        break;

    /* ── Виртуальное зеркало физических кнопок «Лампа/RB/E/ночной» ──
       Позволяет мастеру Modbus переключать эти режимы без физического
       нажатия (удобно для тестов и для дистанционного управления).    */
    case MB_CMD_NIGHT_TOGGLE: {
        uint8_t sr = MB_ReadBits(MB_ADDR_MODE_SR);
        if (sr & MB_BIT_MODE_NIGHT)
            MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
        else
            MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
        break;
    }
    case MB_CMD_LAMP_TOGGLE: {
        uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
        MB_WriteBits(MB_ADDR_OUTPUT_STATE, (uint8_t)(out ^ MB_OUT_LAMP));
        break;
    }
    case MB_CMD_SOCKET_TOGGLE: {
        uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
        MB_WriteBits(MB_ADDR_OUTPUT_STATE, (uint8_t)(out ^ MB_OUT_SOCKET));
        break;
    }
    case MB_CMD_EMERGENCY_TOGGLE: {
        uint8_t sr = MB_ReadBits(MB_ADDR_MODE_SR);
        if (sr & MB_BIT_MODE_EMERGENCY)
            MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
        else
            MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
        break;
    }

    /* Удалённый Mute (§5.1, §8): эквивалент короткого PRG в MAIN при
       активной тревоге — глушит зуммер до возникновения новой тревоги. */
    case MB_CMD_BUZZER_MUTE:
        s_buzzer_muted = 1u;
        MB_WriteBits(MB_ADDR_BUZZER_STATE, MB_BUZZER_OFF);
        break;

    default:
        break;
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   menu_init
   ══════════════════════════════════════════════════════════════════════════ */
void menu_init(void)
{
    leds_init();
    lcd_init();

    /* Runtime-регистры (не в EEPROM) — обнуляем при старте.
       Все настройки (уставки, калибровка, ПИД, тайминги кнопок) уже
       загружены из EEPROM в settings_load() до menu_init().              */
    MB_WriteBits (MB_ADDR_MODE_SR,      0u);
    MB_WriteBits (MB_ADDR_MODE_CR,      MB_CMD_NONE);
    MB_WriteBits (MB_ADDR_MANUAL_OUT,   0u);
    MB_WriteBits (MB_ADDR_OUTPUT_STATE, 0u);
    MB_WriteBits (MB_ADDR_BUZZER_STATE, MB_BUZZER_OFF);
    MB_WriteFloat(MB_ADDR_FLOW,         0.0f);
    MB_WriteFloat(MB_ADDR_EXT_TEMP,     0.0f);
    MB_WriteBits (MB_ADDR_ALARM_FLAGS,  0u);
    MB_WriteBits (MB_ADDR_LED_STATE,    0u);
    MB_WriteBits (MB_ADDR_BTN_EVENT,    0u);

    enter_state(MENU_POWER_ON);
}

/* ══════════════════════════════════════════════════════════════════════════
   menu_process — вызывать каждые 10 мс
   ══════════════════════════════════════════════════════════════════════════ */
void menu_process(BtnEvent_t ev)
{
    s_state_ticks++;

    /* Мигание (АВАРИЯ-заголовок, splash POWER_ON, светодиоды ALARM/POWER) */
    s_blink_tick++;
    if (s_blink_tick >= (uint16_t)BLINK_TICKS) {
        s_blink_tick = 0u;
        s_blink_on   = s_blink_on ? 0u : 1u;
    }

    /* Счётчик до очередного обновления по таймеру.
       Само обновление вызывается в конце menu_process() — чтобы отразить
       уже обработанные события кнопок, а не их «предыдущее» состояние.   */
    s_disp_tick++;

    /* Команды от Modbus-мастера */
    handle_modbus_cmd();
    handle_menu_goto();

    /* Записать событие кнопки в регистр */
    if (ev != BTN_EV_NONE)
        MB_WriteBits(MB_ADDR_BTN_EVENT, (uint8_t)ev);

    /* ── Доковые таймеры (passport_v1.5.md §5..§10) ─────────────────────
       Все счётчики ведутся каждый тик menu_process (10 мс).               */

    uint8_t sr_now    = MB_ReadBits(MB_ADDR_MODE_SR);
    uint8_t work_now  = (sr_now & MB_BIT_MODE_WORK) ? 1u : 0u;

    /* §6: 1-с boost при переходе WORK 0→1 — выводим МАКС.выход;
       §8: 45-с grace — глушение тревог после старта вентиляции.          */
    if (!s_work_was_on && work_now) {
        s_work_grace_tick = (uint16_t)WORK_GRACE_TICKS;
        s_boost_tick      = (uint16_t)BOOST_TICKS;
        s_boost_save_out  = MB_ReadBits(MB_ADDR_MANUAL_OUT);
        /* На время boost принудительно показываем 100 % */
        MB_WriteBits(MB_ADDR_MANUAL_OUT, 100u);
    }
    if (s_work_was_on && !work_now) {
        /* WORK→off: запускаем 30-мин счётчик автовыкл лампы */
        s_lamp_off_tick = 0u;
    }
    s_work_was_on = work_now;

    if (s_boost_tick > 0u) {
        s_boost_tick--;
        if (s_boost_tick == 0u) {
            /* Закончился 1-с boost — восстановить прежний MANUAL_OUT */
            MB_WriteBits(MB_ADDR_MANUAL_OUT, s_boost_save_out);
        }
    }
    if (s_work_grace_tick > 0u) s_work_grace_tick--;

    /* §5.6: Аварийный режим (EMERGENZA) — пока бит активен, выходы на
       максимум.  При входе сохраняем MANUAL_OUT и бит MANUAL (чтобы
       при выходе вернуть прежнее состояние); насильно выставляем
       режим РУЧ + MANUAL_OUT = 100 %.  Каждый тик подтверждаем 100 %
       (защита от внешних записей в регистр через Modbus).               */
    {
        uint8_t emerg_now = (sr_now & MB_BIT_MODE_EMERGENCY) ? 1u : 0u;
        if (!s_emerg_was && emerg_now) {
            s_emerg_save_out = MB_ReadBits(MB_ADDR_MANUAL_OUT);
            s_emerg_save_man = (sr_now & MB_BIT_MODE_MANUAL) ? 1u : 0u;
            MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            MB_WriteBits(MB_ADDR_MANUAL_OUT, 100u);
        }
        if (s_emerg_was && !emerg_now) {
            MB_WriteBits(MB_ADDR_MANUAL_OUT, s_emerg_save_out);
            if (s_emerg_save_man)
                MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            else
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
        }
        if (emerg_now && MB_ReadBits(MB_ADDR_MANUAL_OUT) != 100u)
            MB_WriteBits(MB_ADDR_MANUAL_OUT, 100u);
        s_emerg_was = emerg_now;
    }

    /* §5.5: задержка 0,5 с перед переключением реле SOCKET */
    if (s_socket_pending > 0u) {
        s_socket_pending--;
        if (s_socket_pending == 0u) {
            uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
            if (s_socket_pending_on) out |= MB_OUT_SOCKET;
            else                     out &= (uint8_t)~MB_OUT_SOCKET;
            MB_WriteBits(MB_ADDR_OUTPUT_STATE, out);
        }
    }

    /* §5.3: автовыкл лампы через 30 мин при WORK=off */
    {
        uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
        if (!work_now && (out & MB_OUT_LAMP)) {
            s_lamp_off_tick++;
            if (s_lamp_off_tick >= (uint32_t)LAMP_AUTO_OFF_TICKS) {
                MB_WriteBits(MB_ADDR_OUTPUT_STATE,
                             (uint8_t)(out & ~MB_OUT_LAMP));
                s_lamp_off_tick = 0u;
            }
        } else {
            s_lamp_off_tick = 0u;
        }
    }

    /* Авария: поддерживаем бит MODE_ALARM в актуальном состоянии.
       Документ §8: первые 45 с после старта WORK любые тревоги
       игнорируются (grace).                                              */
    if (MB_ReadBits(MB_ADDR_ALARM_FLAGS) && s_work_grace_tick == 0u)
        MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);
    else
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);

    /* §8: ротация активных тревог (раз в 5 с показываем следующую) */
    s_alarm_rot_tick++;
    if (s_alarm_rot_tick >= (uint16_t)ALARM_ROTATE_TICKS) {
        s_alarm_rot_tick = 0u;
        s_alarm_view_idx = (uint8_t)((s_alarm_view_idx + 1u) % 3u);
    }

    /* §8: зуммер тревоги — цикл 10 с / 2 с пока активна тревога и не
       заглушено кнопкой Mute.  Любая новая тревога (0→1 на MODE_ALARM)
       снимает mute, чтобы оператор гарантированно услышал.               */
    {
        uint8_t alarm_now = (MB_ReadBits(MB_ADDR_MODE_SR) & MB_BIT_MODE_ALARM)
                            ? 1u : 0u;
        if (!s_alarm_was_on && alarm_now) {
            s_buzzer_muted = 0u;
            s_buzzer_tick  = 0u;
        }
        s_alarm_was_on = alarm_now;

        uint8_t buzzer = MB_BUZZER_OFF;
        if (alarm_now && !s_buzzer_muted) {
            s_buzzer_tick++;
            if (s_buzzer_tick >= (uint16_t)BUZZER_PERIOD_TICKS)
                s_buzzer_tick = 0u;
            buzzer = (s_buzzer_tick < (uint16_t)BUZZER_ON_TICKS)
                     ? MB_BUZZER_ON : MB_BUZZER_OFF;
        } else {
            s_buzzer_tick = 0u;
            buzzer = MB_BUZZER_OFF;
        }
        if (MB_ReadBits(MB_ADDR_BUZZER_STATE) != buzzer)
            MB_WriteBits(MB_ADDR_BUZZER_STATE, buzzer);
    }

    /* §10: 2-мин таймаут неактивности в конфигурации */
    {
        uint8_t in_cfg = (s_state >= MENU_SET_FLOW_SP &&
                          s_state <= MENU_SET_DATALOG) ||
                         s_state == MENU_PASSWORD;
        if (in_cfg) {
            if (ev != BTN_EV_NONE)
                s_inact_tick = 0u;
            else
                s_inact_tick++;
            if (s_inact_tick >= (uint16_t)INACT_TIMEOUT_TICKS) {
                /* Тихий выход: незавершённое значение НЕ сохраняем,
                   чтобы случайный недоредакт не попал в EEPROM.          */
                s_inact_tick = 0u;
                enter_state(MENU_MAIN);
            }
        } else {
            s_inact_tick = 0u;
        }
    }

    /* ── Обработка событий кнопок по состоянию ── */
    switch (s_state) {

    /* ── POWER_ON: через 3 с → MAIN ── */
    case MENU_POWER_ON:
        if (s_state_ticks >= (uint16_t)POWERON_TICKS)
            enter_state(MENU_MAIN);
        break;

    /* ── STANDBY ─────────────────────────────────────────────────────
       По документации «Включение и работа прибора»: кнопка ВКЛ/ВЫКЛ
       выводит прибор из ожидания. Остальные кнопки в STANDBY неактивны
       (в т. ч. Лампа — явно указано: «в ожидании неактивна»).           */
    case MENU_STANDBY:
        if (ev == BTN_EV_ONOFF || ev == BTN_EV_ONOFF_LONG)
            enter_state(MENU_POWER_ON);
        break;

    /* ── MAIN: основной рабочий экран ─────────────────────────────────
       Раскладка кнопок по «ФУНКЦИОНАЛЬНОСТЬ КНОПОК»:
         PRG/Mute      — заглушить зуммер (перейти на ALARM, если активен).
                         Длинное удержание (≥3 с) — вход в конфигурацию.
         ВКЛ/ВЫКЛ      — короткое: старт/стоп процесса.
                         Длинное (≥3 с): переключить «Ночной режим».
         Авто/Ручной   — переключить АВТО ↔ РУЧ.
         Лампа         — переключить реле освещения (output_state.LAMP).
         RB (Выбор)    — переключить реле розеток   (output_state.SOCKET).
         E             — переключить «Аварийный режим» (выходы на макс.). */
    case MENU_MAIN: {
        uint8_t sr     = MB_ReadBits(MB_ADDR_MODE_SR);
        uint8_t is_man = (sr & MB_BIT_MODE_MANUAL) ? 1u : 0u;

        switch (ev) {
        case BTN_EV_ONOFF:
            if (sr & MB_BIT_MODE_NIGHT) {
                /* §5.2: короткое нажатие при активном Ночном режиме →
                   возврат в ожидание.  NIGHT и STANDBY одновременно не
                   живут — поэтому сразу снимаем бит NIGHT и переводим
                   FSM в MENU_STANDBY (gracefully завершит текущий цикл). */
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
                enter_state(MENU_STANDBY);
            } else {
                /* Короткое ON/OFF → старт/стоп процесса */
                if (sr & MB_BIT_MODE_WORK)
                    MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
                else
                    MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
            }
            break;

        case BTN_EV_ONOFF_LONG:
            /* Длинное ON/OFF (≥ 3 с по умолчанию) → Ночной режим ON/OFF
               (§5.2 «Удержание > 3 с → пониженная скорость»).            */
            if (sr & MB_BIT_MODE_NIGHT)
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
            else
                MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
            break;

        case BTN_EV_AUTO_MAN:
            /* Переключение АВТО ↔ РУЧ */
            if (is_man) {
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            } else {
                /* При переходе в РУЧ: НОРМ → выход=0, ПАМЯТЬ → оставить */
                if (MB_ReadBits(MB_ADDR_MANUAL_MEM) == MB_MANUAL_NOR)
                    MB_WriteBits(MB_ADDR_MANUAL_OUT, 0u);
                MB_SetBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            }
            break;

        case BTN_EV_PRG:
            /* Короткое PRG/Mute:
               • активна авария — заглушить зуммер (§5.1) и перейти
                 на экран типа аварии для оператора;
               • нет аварии     — no-op.                                  */
            if (sr & MB_BIT_MODE_ALARM) {
                s_buzzer_muted = 1u;
                MB_WriteBits(MB_ADDR_BUZZER_STATE, MB_BUZZER_OFF);
                enter_state(MENU_ALARM);
            }
            break;

        case BTN_EV_PRG_LONG:
            /* Длинное PRG (≥ BTN_LONG_MS) → войти в настройки.
               По доковой модели §10: предварительно требуется ввод
               4-значного пароля. После успешного ввода — переход
               к первому параметру PASS1.                                 */
            enter_state(MENU_PASSWORD);
            break;

        case BTN_EV_LAMP: {
            /* Переключить реле освещения */
            uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
            MB_WriteBits(MB_ADDR_OUTPUT_STATE, (uint8_t)(out ^ MB_OUT_LAMP));
            break;
        }

        case BTN_EV_RB: {
            /* §5.5: переключить реле розеток с задержкой 0,5 с от момента
               нажатия до фактического срабатывания. Подбираем целевое
               состояние сейчас, а сам бит выставит обратный отсчёт
               s_socket_pending в шапке menu_process().                    */
            uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
            s_socket_pending_on = (uint8_t)((out & MB_OUT_SOCKET) ? 0u : 1u);
            s_socket_pending    = (uint16_t)SOCKET_DELAY_TICKS;
            break;
        }

        case BTN_EV_E:
            /* Переключить Аварийный режим (выходы на максимум).
               По документации: «нажать ещё раз — вернуть как было».    */
            if (sr & MB_BIT_MODE_EMERGENCY)
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            else
                MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            break;

        default: break;
        }
        break;
    }

    /* ── ALARM: экран типа аварии ───────────────────────────────────── */
    case MENU_ALARM:
        switch (ev) {
        case BTN_EV_PRG:
            /* Квитирование (§5.1, §8): глушим зуммер до новой тревоги,
               сбрасываем флаги аварий и возвращаемся в MAIN.
               Если причина не устранена — ALARM_FLAGS будет восстановлен
               процессорной задачей при следующем тике, а зуммер останется
               muted (новая 0→1 транзиция произойдёт при следующей
               отдельной тревоге).                                        */
            s_buzzer_muted = 1u;
            MB_WriteBits(MB_ADDR_BUZZER_STATE, MB_BUZZER_OFF);
            MB_WriteBits(MB_ADDR_ALARM_FLAGS, 0u);
            MB_ClearBit (MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);
            enter_state(MENU_MAIN);
            break;
        case BTN_EV_AUTO_MAN:
            /* Переключить режим не выходя из ALARM */
            if (MB_ReadBits(MB_ADDR_MODE_SR) & MB_BIT_MODE_MANUAL)
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            else
                MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            break;
        case BTN_EV_E:
            /* Аварийная кнопка доступна из любого экрана */
            if (MB_ReadBits(MB_ADDR_MODE_SR) & MB_BIT_MODE_EMERGENCY)
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            else
                MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            break;
        default: break;
        }
        break;

    /* ── MENU_SETTINGS_P* — устаревшие списочные экраны ───────────────
       Не используются (passport_v1.5.md §5/§10: отдельных экранов-списков
       в оригинале нет; PRG-short циклически переключает параметры PASS).
       Если по какой-то причине FSM окажется здесь — выйти в MAIN.       */
    case MENU_SETTINGS:
    case MENU_SETTINGS_P2:
    case MENU_SETTINGS_P3:
    case MENU_SETTINGS_P4:
        enter_state(MENU_MAIN);
        break;

    /* ── MENU_PASSWORD: ввод 4-значного пароля (passport_v1.5.md §10) ──
         ↑ (Лампа) / ↓ (ВКЛ/ВЫКЛ)  — изменить активную цифру (0..9, цикл);
         A (Авто/Ручной)            — следующее поле (0→1→2→3→0);
         B (RB)                      — предыдущее поле;
         PRG короткое                — подтвердить;
         PRG длинное                 — отмена, выход в MAIN;
         E                           — переключить аварийный режим.
       Валидация пароля производится в момент подтверждения; при ошибке
       выводится S_PWD_BAD на 1 с и поле обнуляется.                     */
    case MENU_PASSWORD:
        switch (ev) {
        case BTN_EV_LAMP:
        case BTN_EV_LAMP_LONG:
            s_pw_digits[s_pw_field] = (uint8_t)((s_pw_digits[s_pw_field] + 1u) % 10u);
            break;
        case BTN_EV_ONOFF:
            s_pw_digits[s_pw_field] =
                (uint8_t)((s_pw_digits[s_pw_field] + 9u) % 10u);
            break;
        case BTN_EV_AUTO_MAN:
            s_pw_field = (uint8_t)((s_pw_field + 1u) & 0x03u);
            break;
        case BTN_EV_RB:
            s_pw_field = (uint8_t)((s_pw_field + 3u) & 0x03u);
            break;
        case BTN_EV_PRG: {
            /* Простая 4-цифровая аутентификация:
               "0001" → PASS1 (оператор);  пустые "0000" — тоже принимаем
               как dev-shortcut. Реальные пароли могут быть выданы продавцом
               и захардкожены здесь (или храниться в EEPROM-зоне M).      */
            uint16_t code = (uint16_t)(
                s_pw_digits[0] * 1000u +
                s_pw_digits[1] * 100u  +
                s_pw_digits[2] * 10u   +
                s_pw_digits[3]);
            if (code == 0u || code == 1u) {
                enter_state(MENU_SET_FLOW_SP);
            } else if (code == 2u) {
                enter_state(MENU_SET_SENSOR_Z);
            } else if (code == 3u) {
                enter_state(MENU_SET_MAINT);
            } else if (code == 4u) {
                enter_state(MENU_SET_DATALOG);
            } else {
                /* Неверный пароль: подсветить ошибку и выйти при 3 попытках */
                if (++s_pw_attempts >= 3u) {
                    enter_state(MENU_MAIN);
                } else {
                    s_pw_digits[0] = s_pw_digits[1] =
                    s_pw_digits[2] = s_pw_digits[3] = 0u;
                    s_pw_field = 0u;
                }
            }
            break;
        }
        case BTN_EV_PRG_LONG:
            enter_state(MENU_MAIN);
            break;
        case BTN_EV_E:
            if (MB_ReadBits(MB_ADDR_MODE_SR) & MB_BIT_MODE_EMERGENCY)
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            else
                MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            break;
        default: break;
        }
        break;

    /* ═══════════════════════════════════════════════════════════════════
       Обобщённый редактор-цикл (passport_v1.5.md §5, §10).

       Каждый редактор — отдельный экран одного параметра.  Внутри
       PASS-окна параметры зациклены: PRG-short переходит к следующему,
       AUTO_MAN — к первому параметру следующего PASS, RB — предыдущего.

         LAMP / LAMP_LONG → увеличить;
         ВКЛ/ВЫКЛ          → уменьшить;
         PRG короткое      → сохранить в регистр + следующий параметр PASS;
         PRG длинное       → сохранить + EEPROM + выход в MAIN;
         AUTO_MAN          → сохранить в регистр + первый параметр next PASS;
         RB                → сохранить в регистр + первый параметр prev PASS;
         E                 → переключить аварийный режим (любой экран).
       ═══════════════════════════════════════════════════════════════════ */
    case MENU_SET_FLOW_SP:
    case MENU_SET_FLOW_SPR:
    case MENU_SET_ALARM_LOW:
    case MENU_SET_ALARM_LOWR:
    case MENU_SET_ALARM_TIME:
    case MENU_SET_MEM_NOR:
    case MENU_SET_SENSOR_Z:
    case MENU_SET_SENSOR_S:
    case MENU_SET_OUT_Z:
    case MENU_SET_OUT_S:
    case MENU_SET_PID_TI:
    case MENU_SET_PID_BAND:
    case MENU_SET_BLACKOUT:
    case MENU_SET_MAINT:
    case MENU_SET_COUNT_MAX:
    case MENU_SET_DATALOG: {
        /* Локализовать текущий параметр в таблице PASS-ов */
        uint8_t pass_idx = 0u, p_idx = 0u;
        (void)locate_param(s_state, &pass_idx, &p_idx);

        switch (ev) {
        /* ── Увеличить ── (Лампа короткое или удержание) */
        case BTN_EV_LAMP:
        case BTN_EV_LAMP_LONG:
            if (s_edit_kind == EDIT_KIND_FLOAT) {
                s_edit_float += s_edit_step;
                if (s_edit_float > s_edit_max) s_edit_float = s_edit_max;
            } else if (s_edit_kind == EDIT_KIND_UINT) {
                if (s_edit_uint < s_edit_umax) s_edit_uint++;
            } else {
                s_edit_uint = s_edit_uint ? 0u : 1u;
            }
            break;

        /* ── Уменьшить ── (ВКЛ/ВЫКЛ короткое) */
        case BTN_EV_ONOFF:
            if (s_edit_kind == EDIT_KIND_FLOAT) {
                s_edit_float -= s_edit_step;
                if (s_edit_float < s_edit_min) s_edit_float = s_edit_min;
            } else if (s_edit_kind == EDIT_KIND_UINT) {
                if (s_edit_uint > 0u) s_edit_uint--;
            } else {
                s_edit_uint = s_edit_uint ? 0u : 1u;
            }
            break;

        /* ── PRG короткое: сохранить значение и перейти к следующему
              параметру в текущем PASS-окне (циклически).
              EEPROM-flush НЕ делаем здесь, чтобы не «дёргать» AT24 на
              каждом нажатии — финальная запись произойдёт по PRG-LONG
              (выход) или при переходе AUTO_MAN/RB. Регистр Modbus
              обновляется немедленно, чтобы изменение было «живым».      */
        case BTN_EV_PRG: {
            editor_commit_draft();
            uint8_t n = PASSES[pass_idx].n;
            uint8_t next_idx = (uint8_t)((p_idx + 1u) % n);
            enter_state(PASSES[pass_idx].seq[next_idx]);
            break;
        }

        /* ── PRG длинное: сохранить + flush в EEPROM + выход в MAIN ── */
        case BTN_EV_PRG_LONG:
            editor_commit_draft();
            (void)settings_save();
            enter_state(MENU_MAIN);
            break;

        /* ── AUTO_MAN: сохранить и к следующему PASS-окну (циклически) ── */
        case BTN_EV_AUTO_MAN: {
            editor_commit_draft();
            (void)settings_save();
            uint8_t np = (uint8_t)((pass_idx + 1u) % PASS_COUNT);
            enter_state(PASSES[np].seq[0]);
            break;
        }

        /* ── RB: сохранить и к предыдущему PASS-окну (циклически) ── */
        case BTN_EV_RB: {
            editor_commit_draft();
            (void)settings_save();
            uint8_t pp = (uint8_t)((pass_idx + PASS_COUNT - 1u) % PASS_COUNT);
            enter_state(PASSES[pp].seq[0]);
            break;
        }

        /* ── E: аварийный режим доступен из любого экрана редактора ── */
        case BTN_EV_E:
            if (MB_ReadBits(MB_ADDR_MODE_SR) & MB_BIT_MODE_EMERGENCY)
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            else
                MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            break;

        default: break;
        }
        break;
    }
    }

    leds_update();

    /* Отрисовка экрана — гейт:
         • штатное обновление — по таймеру (DISP_TICKS, 500 мс);
         • событие кнопки — принудительно, немедленно (отклик «в том же кадре»).
       Важно: безусловный вызов menu_update_display() на каждый 10-мс тик
       блокирует задачу UI на ~17 мс подряд (busy-wait LCD), из-за чего
       Modbus-задача голодает и кнопки «один раз из 100».                  */
    if (s_disp_tick >= (uint16_t)DISP_TICKS || ev != BTN_EV_NONE) {
        s_disp_tick = 0u;
        menu_update_display();
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   menu_update_display — перерисовать весь экран.

   Вызывается из menu_process() по трём триггерам:
     • таймер    — s_disp_tick ≥ DISP_TICKS (1500 мс);
     • событие кнопки — ev != BTN_EV_NONE   (немедленно);
     • переход состояния — enter_state()    (выставляет s_disp_tick = DISP_TICKS,
                                              редрав произойдёт в тот же тик).
   ══════════════════════════════════════════════════════════════════════════ */
void menu_update_display(void)
{
    /* Буфер с запасом: реальная ширина экрана 20, lcd_mb_write() усекает
       до 20 символов. Запас нужен только как «ловушка» для редких ошибок
       формирования строки.                                                */
    char  line[24];
    char *p;

    switch (s_state) {

    /* ────────────────── POWER_ON ────────────────── */
    case MENU_POWER_ON:
        lcd_mb_write(0, S_TITLE);
        lcd_mb_write(1, S_INIT);
        /* строка 2: версия прошивки */
        p = line;
        *p++ = ' '; *p++ = ' ';
        *p++ = '\xE2'; *p++ = '\xE5'; *p++ = '\xF0'; /* вер */
        *p++ = '.'; *p++ = ' ';
        p = fmt_f1(p, FW_VERSION_F);
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write(2, line);
        lcd_mb_blank(3);
        break;

    /* ────────────────── STANDBY ────────────────── */
    case MENU_STANDBY:
        lcd_mb_write(0, S_TITLE);
        /* строка 1: время HH:MM:SS из счётчика секунд */
        p = line;
        *p++ = ' '; *p++ = ' ';
        /* Секунды с момента старта берутся из FreeRTOS-тиков (регистр
           TIMER убран из карты Modbus — он был неиспользуемым мастером). */
        p = fmt_time(p, (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ));
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write(1, line);
        lcd_mb_write(2, S_STANDBY_WAIT);
        lcd_mb_write(3, S_STANDBY_BTN);
        break;

    /* ────────────────── MAIN ────────────────── */
    case MENU_MAIN: {
        uint8_t sr      = MB_ReadBits(MB_ADDR_MODE_SR);
        uint8_t is_man  = (sr & MB_BIT_MODE_MANUAL)    ? 1u : 0u;
        uint8_t is_wk   = (sr & MB_BIT_MODE_WORK)      ? 1u : 0u;
        uint8_t is_alm  = (sr & MB_BIT_MODE_ALARM)     ? 1u : 0u;
        uint8_t is_rep  = (sr & MB_BIT_MODE_REPEAT)    ? 1u : 0u;
        uint8_t is_emrg = (sr & MB_BIT_MODE_EMERGENCY) ? 1u : 0u;
        float   sp      = MB_ReadFloat(MB_ADDR_SETPOINT);
        float   flow    = MB_ReadFloat(MB_ADDR_FLOW);
        float   temp    = MB_ReadFloat(MB_ADDR_EXT_TEMP);

        /* строка 0: при EMERGENZA (§5.6) — мигающий баннер «EMERGENZA»
           поверх обычной строки статуса.  Имеет приоритет выше ALARM,
           т. к. требует немедленной реакции оператора (выходы — 100 %).  */
        if (is_emrg) {
            if (s_blink_on) lcd_mb_write(0, S_EMERG_HDR);
            else            lcd_mb_blank(0);
        } else {
            /* "МКВП-02 АВАРИЯ РАБОТА" / "МКВП-02 АВТО  РАБОТА" и т. п.
               При аварии слово режима заменяется на «АВАРИЯ» (мигает).   */
            p = line;
            *p++ = '\xCC'; *p++ = '\xCA'; *p++ = '\xC2'; *p++ = '\xCF';  /* МКВП */
            *p++ = '-'; *p++ = '0'; *p++ = '2'; *p++ = ' ';
            if (is_alm && s_blink_on) {
                /* "АВАРИЯ" — 6 символов */
                *p++ = '\xC0'; *p++ = '\xC2'; *p++ = '\xC0';
                *p++ = '\xD0'; *p++ = '\xC8'; *p++ = '\xFF';
            } else if (is_alm) {
                /* На второй полупериод мигания — 6 пробелов */
                *p++ = ' '; *p++ = ' '; *p++ = ' ';
                *p++ = ' '; *p++ = ' '; *p++ = ' ';
            } else if (is_man) {
                /* "РУЧ" + опц. П (повтор) + пробелы до 6 символов поля */
                *p++ = '\xD0'; *p++ = '\xD3'; *p++ = '\xD7';   /* РУЧ */
                if (is_rep) { *p++ = '\xCF'; *p++ = ' '; *p++ = ' '; } /* П */
                else        { *p++ = ' '; *p++ = ' '; *p++ = ' '; }
            } else {
                /* "АВТО" + опц. П (повтор) + пробелы до 6 символов поля */
                *p++ = '\xC0'; *p++ = '\xC2'; *p++ = '\xD2'; *p++ = '\xCE'; /* АВТО */
                if (is_rep) { *p++ = '\xCF'; *p++ = ' '; } /* П */
                else        { *p++ = ' '; *p++ = ' '; }
            }
            *p++ = ' ';
            if (is_wk) {
                *p++ = '\xD0'; *p++ = '\xC0'; *p++ = '\xC1'; /* РАБ */
                *p++ = '\xCE'; *p++ = '\xD2'; *p++ = '\xC0'; /* ОТА */
            } else {
                *p++ = '\xD1'; *p++ = '\xD2'; *p++ = '\xCE'; *p++ = '\xCF'; /* СТОП */
                *p++ = ' '; *p++ = ' ';
            }
            while (p < line + 20) *p++ = ' ';
            *p = '\0';
            lcd_mb_write(0, line);
        }

        /* строка 1: уставка и текущий поток */
        p = line;
        if (!is_man) {
            /* "Уст:X.XX Тек:X.XX м/с" */
            *p++ = '\xD3'; *p++ = '\xF1'; *p++ = '\xF2'; *p++ = ':';  /* Уст: */
            p = fmt_f2(p, sp);
            *p++ = ' ';
            *p++ = '\xD2'; *p++ = '\xE5'; *p++ = '\xEA'; *p++ = ':';  /* Тек: */
            p = fmt_f2(p, flow);
            *p++ = '\xEC'; *p++ = '/'; *p++ = '\xF1';                 /* м/с */
        } else {
            /* "Вых:XX% Тек:X.XX м/с"  — ровно 20 символов
               (4 + 3 + 2 + 4 + 4 + 3 = 20).                         */
            *p++ = '\xC2'; *p++ = '\xFB'; *p++ = '\xF5'; *p++ = ':'; /* Вых: */
            p = fmt_u8_3(p, MB_ReadBits(MB_ADDR_MANUAL_OUT));
            *p++ = '%'; *p++ = ' ';
            *p++ = '\xD2'; *p++ = '\xE5'; *p++ = '\xEA'; *p++ = ':'; /* Тек: */
            p = fmt_f2(p, flow);
            *p++ = '\xEC'; *p++ = '/'; *p++ = '\xF1';                 /* м/с */
        }
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write(1, line);

        /* строка 2: температура */
        p = line;
        *p++ = '\xD2'; *p++ = '\xE5'; *p++ = '\xEC'; *p++ = '\xEF'; *p++ = ':'; /* Темп: */
        *p++ = ' '; *p++ = ' ';
        p = fmt_f1(p, temp);
        *p++ = '\xB0'; *p++ = '\xD1';  /* °С */
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write(2, line);

        /* строка 3: подсказка (по убыванию приоритета)
             • EMERGENZA (§5.6) — «E:сброс аварии»;
             • boost (1 с после WORK on, §6) — «ВЕНТИЛЯЦИЯ MAX»;
             • при аварии — приглашение перейти на экран типа аварии;
             • иначе — стандартная подсказка кнопок (АВТО/РУЧ).            */
        if (is_emrg)         lcd_mb_write(3, S_HINT_EMERG);
        else if (s_boost_tick > 0u)
                             lcd_mb_write(3, S_HINT_BOOST);
        else if (is_alm)     lcd_mb_write(3, S_HINT_MAIN_ALARM);
        else if (is_man)     lcd_mb_write(3, S_HINT_MAN);
        else                 lcd_mb_write(3, S_HINT_AUTO);
        break;
    }

    /* ────────────────── ALARM ────────────────── */
    case MENU_ALARM: {
        /* строка 0: мигающая шапка */
        if (s_blink_on) lcd_mb_write(0, S_ALARM_HDR);
        else            lcd_mb_blank(0);

        /* строка 1: тип аварии — ротация активных тревог по очереди (§8).
           s_alarm_view_idx крутится 0→1→2→0 каждые 5 с;
           ищем ПЕРВЫЙ активный бит начиная с этого индекса.               */
        {
            uint8_t af = MB_ReadBits(MB_ADDR_ALARM_FLAGS);
            static const uint8_t bits[3] = {
                MB_ALARM_FLOW_LOW, MB_ALARM_INVERTER, MB_ALARM_DOOR_OPEN
            };
            uint8_t shown = 0u;
            for (uint8_t k = 0u; k < 3u; k++) {
                uint8_t i = (uint8_t)((s_alarm_view_idx + k) % 3u);
                if (af & bits[i]) { shown = bits[i]; break; }
            }

            p = line;
            if (shown == MB_ALARM_FLOW_LOW) {
                /* "Мало потока!        " */
                *p++ = '\xCC'; *p++ = '\xE0'; *p++ = '\xEB'; *p++ = '\xEE'; /* Мало */
                *p++ = ' ';
                *p++ = '\xEF'; *p++ = '\xEE'; *p++ = '\xF2'; *p++ = '\xEE'; /* пото */
                *p++ = '\xEA'; *p++ = '\xE0'; *p++ = '!';                   /* ка!  */
            } else if (shown == MB_ALARM_INVERTER) {
                /* "Авар.инвертора!     " */
                *p++ = '\xC0'; *p++ = '\xE2'; *p++ = '\xE0'; *p++ = '\xF0'; /* Авар */
                *p++ = '.';
                *p++ = '\xE8'; *p++ = '\xED'; *p++ = '\xE2'; *p++ = '\xE5'; /* инве */
                *p++ = '\xF0'; *p++ = '\xF2'; *p++ = '\xEE'; *p++ = '\xF0'; /* ртор */
                *p++ = '\xE0'; *p++ = '!';                                   /* а!   */
            } else if (shown == MB_ALARM_DOOR_OPEN) {
                /* "Дверь открыта!      " */
                *p++ = '\xC4'; *p++ = '\xE2'; *p++ = '\xE5'; *p++ = '\xF0'; /* Двер */
                *p++ = '\xFC'; *p++ = ' ';                                   /* ь    */
                *p++ = '\xEE'; *p++ = '\xF2'; *p++ = '\xEA'; *p++ = '\xF0'; /* откр */
                *p++ = '\xFB'; *p++ = '\xF2'; *p++ = '\xE0'; *p++ = '!';   /* ыта! */
            } else {
                *p++ = '?';
            }
            while (p < line + 20) *p++ = ' ';
            *p = '\0';
        }
        lcd_mb_write(1, line);

        /* строка 2: текущий поток */
        p = line;
        *p++ = '\xD2'; *p++ = '\xE5'; *p++ = '\xEA'; *p++ = ':'; *p++ = ' '; /* Тек:  */
        p = fmt_f2(p, MB_ReadFloat(MB_ADDR_FLOW));
        *p++ = ' '; *p++ = '\xEC'; *p++ = '/'; *p++ = '\xF1';  /* м/с */
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write(2, line);

        lcd_mb_write(3, S_HINT_ALARM);
        break;
    }

    /* ── Списочные SETTINGS-экраны устарели (passport_v1.5.md §5/§10).
       Доковая модель: каждый параметр PASS — отдельный экран, между
       ними циклически переключаемся короткими нажатиями PRG, между
       PASS-окнами — кнопками A/M (вперёд) и RB (назад).
       Эти ветки достижимы только через menu_goto извне; рисуем заглушку,
       чтобы не оставлять экран случайной мусором.                       */
    case MENU_SETTINGS:
    case MENU_SETTINGS_P2:
    case MENU_SETTINGS_P3:
    case MENU_SETTINGS_P4:
        lcd_mb_write(0, S_SETTINGS_HDR);
        lcd_mb_blank(1);
        lcd_mb_blank(2);
        lcd_mb_blank(3);
        break;

    /* ═══════════ MENU_PASSWORD: ввод 4-значного пароля ═══════════
       Раскладка экрана:
         0: "--- ПАРОЛЬ входа ---"
         1: ""
         2: "       *N**         "  (мигает активная цифра, прочее = '*')
         3: "ПРГ:OK A/B:след/пред"
       При s_pw_attempts ≥ 1 вместо строки 1 показываем «Неверный пароль». */
    case MENU_PASSWORD:
        lcd_mb_write(0, S_PWD_HDR);
        if (s_pw_attempts > 0u) lcd_mb_write(1, S_PWD_BAD);
        else                    lcd_mb_blank(1);

        /* строка 2: 4 цифры с пробелами вокруг.
           Активную цифру мигаем (показываем цифру / маскируем '_').       */
        p = line;
        for (uint8_t k = 0u; k < 8u; k++) *p++ = ' ';
        for (uint8_t k = 0u; k < 4u; k++) {
            char d = (char)('0' + s_pw_digits[k]);
            if (k == s_pw_field && !s_blink_on)
                d = '_';
            *p++ = d;
        }
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write(2, line);

        lcd_mb_write(3, S_HINT_PWD);
        break;

    /* ═══════════ Обобщённый редактор float/uint/toggle ═══════════ */
    case MENU_SET_FLOW_SP:
    case MENU_SET_FLOW_SPR:
    case MENU_SET_ALARM_LOW:
    case MENU_SET_ALARM_LOWR:
    case MENU_SET_ALARM_TIME:
    case MENU_SET_MEM_NOR:
    case MENU_SET_SENSOR_Z:
    case MENU_SET_SENSOR_S:
    case MENU_SET_OUT_Z:
    case MENU_SET_OUT_S:
    case MENU_SET_PID_TI:
    case MENU_SET_PID_BAND:
    case MENU_SET_BLACKOUT:
    case MENU_SET_MAINT:
    case MENU_SET_COUNT_MAX:
    case MENU_SET_DATALOG:
        lcd_mb_write(0, s_edit_hdr ? s_edit_hdr : S_SETTINGS_HDR);
        lcd_mb_blank(1);

        p = line;
        *p++ = ' '; *p++ = ' ';

        if (s_edit_kind == EDIT_KIND_FLOAT) {
            /* "  Знач:  <число> <ед>" */
            *p++ = '\xC7'; *p++ = '\xED'; *p++ = '\xE0'; *p++ = '\xF7'; *p++ = ':'; /* Знач: */
            *p++ = ' ';
            if (s_edit_dec >= 2u)      p = fmt_f2(p, s_edit_float);
            else if (s_edit_dec == 1u) p = fmt_f1(p, s_edit_float);
            else {
                /* dec=0: целое число до 9999 (PID_BAND, MAINT, COUNT_MAX) */
                float fv = s_edit_float;
                if (fv < 0.0f)    fv = 0.0f;
                if (fv > 9999.0f) fv = 9999.0f;
                p = fmt_u16_4(p, (uint16_t)(fv + 0.5f));
            }
            *p++ = ' ';
            const char *u = s_edit_unit;
            while (*u && p < line + 19) *p++ = *u++;
        } else if (s_edit_kind == EDIT_KIND_UINT) {
            /* "  Знач:   XXX <ед>" */
            *p++ = '\xC7'; *p++ = '\xED'; *p++ = '\xE0'; *p++ = '\xF7'; *p++ = ':'; /* Знач: */
            *p++ = ' '; *p++ = ' ';
            p = fmt_u8_3(p, s_edit_uint);
            *p++ = ' ';
            const char *u = s_edit_unit;
            while (*u && p < line + 19) *p++ = *u++;
        } else {
            /* TOGGLE */
            *p++ = '\xD0'; *p++ = '\xE5'; *p++ = '\xE6'; *p++ = '\xE8'; *p++ = '\xEC'; *p++ = ':'; /* Режим: */
            *p++ = ' ';
            if (s_edit_tog_lbl == TOG_MEM_NOR) {
                if (s_edit_uint == MB_MANUAL_MEM) {
                    /* ПАМЯТЬ */
                    *p++ = '\xCF'; *p++ = '\xC0'; *p++ = '\xCC';
                    *p++ = '\xFF'; *p++ = '\xD2'; *p++ = '\xFC';
                } else {
                    /* НОРМ */
                    *p++ = '\xCD'; *p++ = '\xCE'; *p++ = '\xD0'; *p++ = '\xCC';
                }
            } else {
                /* ВКЛ / ВЫКЛ */
                if (s_edit_uint) {
                    *p++ = '\xC2'; *p++ = '\xCA'; *p++ = '\xCB';          /* ВКЛ */
                } else {
                    *p++ = '\xC2'; *p++ = '\xDB'; *p++ = '\xCA'; *p++ = '\xCB'; /* ВЫКЛ */
                }
            }
        }

        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write(2, line);

        lcd_mb_write(3, S_HINT_EDIT);
        break;
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   menu_get_state
   ══════════════════════════════════════════════════════════════════════════ */
MenuState_t menu_get_state(void)
{
    return s_state;
}
