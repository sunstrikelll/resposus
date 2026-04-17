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
 *    1: "  ЧЧ:ММ:СС          "   (счётчик секунд MB_ADDR_TIMER → HH:MM:SS)
 *    2: "  Ожидание...       "
 *    3: "  Нажмите ВКЛ/ВЫКЛ  "
 *
 *  MAIN АВТО:
 *    0: "МКВП-02 АВТО   РАБОТА"  (или СТОП; суффикс R при MODE_REPEAT)
 *    1: "Уст:X.XX Тек:X.XX м/с"
 *    2: "Темп:  XX.X°С       "
 *    3: "PRG:меню A/M:режим  "
 *
 *  MAIN РУЧ:
 *    0: "МКВП-02  РУЧ   РАБОТА"  (суффикс R при MODE_REPEAT)
 *    1: "Вых: XX% Тек:X.XX м/с"
 *    2: "Темп:  XX.X°С       "
 *    3: "UP/DN:+-% PRG:меню  "
 *
 *  MAIN при аварии (inline EMERGENZA):
 *    0: "МКВП-02 АВАРИЯ РАБОТА" (мигает слово АВАРИЯ)
 *    1-2: значения потока/температуры как обычно
 *    3: "PRG: тип аварии     "  — PRG открывает экран ALARM
 *
 *  ALARM (экран типа аварии):
 *    0: "!!!  АВАРИЯ  !!!    "  (мигает)
 *    1: тип аварии (Low Speed / Inverter Fault / Glass Open)
 *    2: "Тек: X.XX м/с       "
 *    3: "PRG:квит A/M:режим  "
 *
 *  SETTINGS (7 пунктов, 3 видимых, прокрутка):
 *    0: "=== НАСТРОЙКИ ===   "
 *    1-3: пункты со стрелкой ">"
 *
 *  Редактирование (SET_FLOW_SP / SET_FLOW_SPR / SET_ALARM_LOW / SET_ALARM_LOWR):
 *    0: заголовок
 *    1: ""
 *    2: "  Знач:  X.XX м/с   "
 *    3: "PRG:OK  A/M:отмена  "
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
#include "gd32f10x.h"
#include <string.h>

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
#define POWERON_TICKS      300u   /* 3 с — заставка включения               */
#define BLINK_TICKS         50u   /* 500 мс — период мигания ALARM/POWER_ON */
#define DISP_TICKS          20u   /* 200 мс — период обновления дисплея     */

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

/* "=== НАСТРОЙКИ ===   " */
static const char S_SETTINGS_HDR[21] =
    "=== "
    "\xCD\xC0\xD1\xD2\xD0\xCE\xC9\xCA\xC8"  /* НАСТРОЙКИ */
    " ===   ";

/* ── Пункты меню настроек (16 символов) ──────────────────────────────── */
/* "Уставка потока  " */
static const char S_ITEM_FLOW_SP[17] =
    "\xD3\xF1\xF2\xE0\xE2\xEA\xE0"  /* Уставка */
    " "
    "\xEF\xEE\xF2\xEE\xEA\xE0"  /* потока */
    "  ";

/* "Уставка повтора " */
static const char S_ITEM_FLOW_SPR[17] =
    "\xD3\xF1\xF2\xE0\xE2\xEA\xE0"  /* Уставка */
    " "
    "\xEF\xEE\xE2\xF2\xEE\xF0\xE0"  /* повтора */
    " ";

/* "Порог аварии    " */
static const char S_ITEM_ALARM_LOW[17] =
    "\xCF\xEE\xF0\xEE\xE3"  /* Порог */
    " "
    "\xE0\xE2\xE0\xF0\xE8\xE8"  /* аварии */
    "    ";

/* "Сброс аварии    " */
static const char S_ITEM_ALARM_LOWR[17] =
    "\xD1\xE1\xF0\xEE\xF1"  /* Сброс */
    " "
    "\xE0\xE2\xE0\xF0\xE8\xE8"  /* аварии */
    "    ";

/* "Задержка аварии " */
static const char S_ITEM_ALARM_TIME[17] =
    "\xC7\xE0\xE4\xE5\xF0\xE6\xEA\xE0"  /* Задержка */
    " "
    "\xE0\xE2\xE0\xF0\xE8\xE8"  /* аварии */
    " ";

/* "Режим ручного   " */
static const char S_ITEM_MEM_NOR[17] =
    "\xD0\xE5\xE6\xE8\xEC"  /* Режим */
    " "
    "\xF0\xF3\xF7\xED\xEE\xE3\xEE"  /* ручного */
    "   ";

/* "Назад           " */
static const char S_ITEM_BACK[17] =
    "\xCD\xE0\xE7\xE0\xE4"  /* Назад */
    "           ";

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
/* "ПРГ:меню А/М:режим  " */
static const char S_HINT_AUTO[21] =
    "\xCF\xD0\xC3"     /* ПРГ */
    ":"
    "\xEC\xE5\xED\xFE"   /* меню */
    " "
    "\xC0/\xCC"         /* А/М */
    ":"
    "\xF0\xE5\xE6\xE8\xEC"  /* режим */
    "  ";

/* "ВВ/ВН:+-% ПРГ:меню  " */
static const char S_HINT_MAN[21] =
    "\xC2\xC2/\xC2\xCD" /* ВВ/ВН */
    ":+-% "
    "\xCF\xD0\xC3"      /* ПРГ */
    ":"
    "\xEC\xE5\xED\xFE"  /* меню */
    "  ";

/* "ПРГ:квит А/М:режим  " */
static const char S_HINT_ALARM[21] =
    "\xCF\xD0\xC3"     /* ПРГ */
    ":"
    "\xEA\xE2\xE8\xF2"  /* квит */
    " "
    "\xC0/\xCC"         /* А/М */
    ":"
    "\xF0\xE5\xE6\xE8\xEC"  /* режим */
    "  ";

/* "ПРГ:ОК  А/М:отмена  " */
static const char S_HINT_EDIT[21] =
    "\xCF\xD0\xC3"     /* ПРГ */
    ":"
    "\xCE\xCA"          /* ОК */
    "  "
    "\xC0/\xCC"         /* А/М */
    ":"
    "\xEE\xF2\xEC\xE5\xED\xE0"  /* отмена */
    "  ";

/* "ПРГ: тип аварии     " — подсказка на MAIN при активной аварии */
static const char S_HINT_MAIN_ALARM[21] =
    "\xCF\xD0\xC3"     /* ПРГ */
    ": "
    "\xF2\xE8\xEF"   /* тип */
    " "
    "\xE0\xE2\xE0\xF0\xE8\xE8"  /* аварии */
    "     ";

/* ═══ Пункты подменю PASS2 (16 символов) ═══ */

/* "Сервис 2 -->    " — вход в PASS2 */
static const char S_ITEM_P2_ENTER[17] =
    "\xD1\xE5\xF0\xE2\xE8\xF1"  /* Сервис */
    " 2 -->    ";

/* "Сервис 3 -->    " */
static const char S_ITEM_P3_ENTER[17] =
    "\xD1\xE5\xF0\xE2\xE8\xF1"  /* Сервис */
    " 3 -->    ";

/* "Сервис 4 -->    " */
static const char S_ITEM_P4_ENTER[17] =
    "\xD1\xE5\xF0\xE2\xE8\xF1"  /* Сервис */
    " 4 -->    ";

/* "Ноль датчика    " */
static const char S_ITEM_SENSOR_Z[17] =
    "\xCD\xEE\xEB\xFC"  /* Ноль */
    " "
    "\xE4\xE0\xF2\xF7\xE8\xEA\xE0"  /* датчика */
    "    ";

/* "Диап. датчика   " */
static const char S_ITEM_SENSOR_S[17] =
    "\xC4\xE8\xE0\xEF"  /* Диап */
    ". "
    "\xE4\xE0\xF2\xF7\xE8\xEA\xE0"  /* датчика */
    "   ";

/* "Ноль выхода     " */
static const char S_ITEM_OUT_Z[17] =
    "\xCD\xEE\xEB\xFC"  /* Ноль */
    " "
    "\xE2\xFB\xF5\xEE\xE4\xE0"  /* выхода */
    "     ";

/* "Диап. выхода    " */
static const char S_ITEM_OUT_S[17] =
    "\xC4\xE8\xE0\xEF"  /* Диап */
    ". "
    "\xE2\xFB\xF5\xEE\xE4\xE0"  /* выхода */
    "    ";

/* "ПИД:инт.время   " */
static const char S_ITEM_PID_TI[17] =
    "\xCF\xC8\xC4"  /* ПИД */
    ":"
    "\xE8\xED\xF2"  /* инт */
    "."
    "\xE2\xF0\xE5\xEC\xFF"  /* время */
    "   ";

/* "ПИД:полоса      " */
static const char S_ITEM_PID_BAND[17] =
    "\xCF\xC8\xC4"  /* ПИД */
    ":"
    "\xEF\xEE\xEB\xEE\xF1\xE0"  /* полоса */
    "      ";

/* "Автопуск        " */
static const char S_ITEM_BLACKOUT[17] =
    "\xC0\xE2\xF2\xEE\xEF\xF3\xF1\xEA"  /* Автопуск */
    "        ";

/* "Обслуживание    " */
static const char S_ITEM_MAINT[17] =
    "\xCE\xE1\xF1\xEB\xF3\xE6\xE8\xE2\xE0\xED\xE8\xE5"  /* Обслуживание */
    "    ";

/* "Макс. счётчик   " */
static const char S_ITEM_COUNT_MAX[17] =
    "\xCC\xE0\xEA\xF1"  /* Макс */
    ". "
    "\xF1\xF7\xB8\xF2\xF7\xE8\xEA"  /* счётчик */
    "   ";

/* "Регистратор     " */
static const char S_ITEM_DATALOG[17] =
    "\xD0\xE5\xE3\xE8\xF1\xF2\xF0\xE0\xF2\xEE\xF0"  /* Регистратор */
    "     ";

/* ═══ Заголовки подменю (20 символов) ═══ */

/* "=== СЕРВИС 2 ====   " */
static const char S_SETTINGS_P2_HDR[21] =
    "=== "
    "\xD1\xC5\xD0\xC2\xC8\xD1"  /* СЕРВИС */
    " 2 ====   ";

/* "=== СЕРВИС 3 ====   " */
static const char S_SETTINGS_P3_HDR[21] =
    "=== "
    "\xD1\xC5\xD0\xC2\xC8\xD1"
    " 3 ====   ";

/* "=== СЕРВИС 4 ====   " */
static const char S_SETTINGS_P4_HDR[21] =
    "=== "
    "\xD1\xC5\xD0\xC2\xC8\xD1"
    " 4 ====   ";

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

/* Курсор меню настроек */
static uint8_t     s_sel         = 0u;
#define SETTINGS_N     8u               /* PASS1: 6 параметров + "Сервис 2"+"Назад"  */
#define SETTINGS_P2_N  9u               /* PASS2: 7 параметров + "Сервис 3"+"Назад"  */
#define SETTINGS_P3_N  4u               /* PASS3: MAINT + COUNT_MAX + "Сервис 4"+"Назад" */
#define SETTINGS_P4_N  2u               /* PASS4: DATALOG + "Назад"                   */

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

/* float с 1 знаком после точки (XX.X), до 5 символов */
static char *fmt_f1(char *p, float v)
{
    if (v < 0.0f) { *p++ = '-'; v = -v; }
    int32_t i  = (int32_t)v;
    int32_t fr = (int32_t)((v - (float)i) * 10.0f + 0.5f);
    if (fr >= 10) { i++; fr = 0; }
    char tmp[6]; int len = 0;
    if (i == 0) tmp[len++] = '0';
    else { int32_t t = i; while (t) { tmp[len++] = (char)('0' + t % 10); t /= 10; } }
    for (int j = len - 1; j >= 0; j--) *p++ = tmp[j];
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

static void lcd_mb_write(uint8_t row, const char *line20)
{
    static const uint16_t mb_line_addr[4] = {
        MB_ADDR_DISPLAY_LINE_0, MB_ADDR_DISPLAY_LINE_1,
        MB_ADDR_DISPLAY_LINE_2, MB_ADDR_DISPLAY_LINE_3
    };
    lcd_print_win1251_at(row, 0, line20);
    MB_WriteString(mb_line_addr[row], line20);
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

    /* ── Подменю ── */
    case MENU_SETTINGS:
    case MENU_SETTINGS_P2:
    case MENU_SETTINGS_P3:
    case MENU_SETTINGS_P4:
        s_sel = 0u;
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

    /* Начальные значения регистров */
    MB_WriteBits (MB_ADDR_MODE_SR,      0u);
    MB_WriteBits (MB_ADDR_MODE_CR,      MB_CMD_NONE);
    MB_WriteBits (MB_ADDR_MANUAL_OUT,   0u);
    MB_WriteFloat(MB_ADDR_SETPOINT,     0.50f);
    MB_WriteFloat(MB_ADDR_FLOW,         0.0f);
    MB_WriteFloat(MB_ADDR_EXT_TEMP,     0.0f);
    MB_WriteBits (MB_ADDR_ALARM_FLAGS,  0u);
    MB_WriteBits (MB_ADDR_LED_STATE,    0u);
    MB_WriteBits (MB_ADDR_BTN_EVENT,    0u);
    MB_WriteFloat(MB_ADDR_FLOW_SP_R,    0.30f);
    MB_WriteFloat(MB_ADDR_ALARM_LOW,    0.20f);
    MB_WriteFloat(MB_ADDR_ALARM_LOW_R,  0.25f);
    MB_WriteBits (MB_ADDR_ALARM_DELAY,  5u);
    MB_WriteBits (MB_ADDR_MANUAL_MEM,   MB_MANUAL_NOR);

    /* PASS2: калибровка и ПИД */
    MB_WriteFloat(MB_ADDR_SENSOR_ZERO,  0.00f);
    MB_WriteFloat(MB_ADDR_SENSOR_SPAN,  5.00f);
    MB_WriteFloat(MB_ADDR_PID_TI,       2.7f);
    MB_WriteFloat(MB_ADDR_PID_BAND,     27.0f);
    MB_WriteBits (MB_ADDR_OUT_ZERO_PCT, 0u);
    MB_WriteBits (MB_ADDR_OUT_SPAN_PCT, 100u);
    MB_WriteBits (MB_ADDR_BLACKOUT_EN,  1u);

    /* PASS3: обслуживание */
    MB_WriteFloat(MB_ADDR_MAINT_HOURS,  0.0f);

    /* PASS4: счётчики и datalogger */
    MB_WriteFloat(MB_ADDR_COUNT_MAX,    9999.0f);
    MB_WriteBits (MB_ADDR_DATALOG_EN,   0u);
    MB_WriteBits (MB_ADDR_DATALOG_SEC,  60u);

    enter_state(MENU_POWER_ON);
}

/* ══════════════════════════════════════════════════════════════════════════
   menu_process — вызывать каждые 10 мс
   ══════════════════════════════════════════════════════════════════════════ */
void menu_process(BtnEvent_t ev)
{
    s_state_ticks++;

    /* Мигание */
    s_blink_tick++;
    if (s_blink_tick >= (uint16_t)BLINK_TICKS) {
        s_blink_tick = 0u;
        s_blink_on   = s_blink_on ? 0u : 1u;
    }

    /* Обновление дисплея по таймеру */
    s_disp_tick++;
    if (s_disp_tick >= (uint16_t)DISP_TICKS) {
        s_disp_tick = 0u;
        menu_update_display();
    }

    /* Команды от Modbus-мастера */
    handle_modbus_cmd();

    /* Записать событие кнопки в регистр */
    if (ev != BTN_EV_NONE)
        MB_WriteBits(MB_ADDR_BTN_EVENT, (uint8_t)ev);

    /* Авария: поддерживаем бит MODE_ALARM в актуальном состоянии.
       Документ: при аварии на MAIN вместо режима выводится EMERGENZA;
       переход на экран типа аварии — только по нажатию PRG оператором. */
    if (MB_ReadBits(MB_ADDR_ALARM_FLAGS))
        MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);
    else
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);

    /* ── Обработка событий кнопок по состоянию ── */
    switch (s_state) {

    /* ── POWER_ON: через 3 с → MAIN ── */
    case MENU_POWER_ON:
        if (s_state_ticks >= (uint16_t)POWERON_TICKS)
            enter_state(MENU_MAIN);
        break;

    /* ── STANDBY: ВКЛ/ВЫКЛ → POWER_ON ── */
    case MENU_STANDBY:
        if (ev == BTN_EV_ONOFF || ev == BTN_EV_ONOFF_LONG)
            enter_state(MENU_POWER_ON);
        break;

    /* ── MAIN: основной рабочий экран ── */
    case MENU_MAIN: {
        uint8_t sr     = MB_ReadBits(MB_ADDR_MODE_SR);
        uint8_t is_man = (sr & MB_BIT_MODE_MANUAL) ? 1u : 0u;

        switch (ev) {
        case BTN_EV_ONOFF:
            /* Короткое ON/OFF → старт/стоп процесса */
            if (sr & MB_BIT_MODE_WORK)
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
            else
                MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
            break;

        case BTN_EV_ONOFF_LONG:
            /* Длинное ON/OFF ≥ 1.5 с → ожидание */
            enter_state(MENU_STANDBY);
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
            /* Короткое PRG в MAIN = Mute (заглушить зуммер).
               Если активна авария — переход на экран типа аварии. */
            if (sr & MB_BIT_MODE_ALARM)
                enter_state(MENU_ALARM);
            /* Иначе — no-op (Mute зуммера, зуммера пока нет) */
            break;

        case BTN_EV_PRG_LONG:
            /* Длинное PRG ≥ 1.5 с → войти в настройки */
            enter_state(MENU_SETTINGS);
            break;

        case BTN_EV_UP:
            if (!is_man) {
                /* АВТО: увеличить уставку потока */
                float sp = MB_ReadFloat(MB_ADDR_SETPOINT) + FLOW_STEP;
                if (sp > FLOW_MAX) sp = FLOW_MAX;
                MB_WriteFloat(MB_ADDR_SETPOINT, sp);
            } else {
                /* РУЧ: увеличить выход */
                uint8_t out = MB_ReadBits(MB_ADDR_MANUAL_OUT);
                if (out < 100u) out++;
                MB_WriteBits(MB_ADDR_MANUAL_OUT, out);
            }
            break;

        case BTN_EV_DOWN:
            if (!is_man) {
                float sp = MB_ReadFloat(MB_ADDR_SETPOINT) - FLOW_STEP;
                if (sp < FLOW_MIN) sp = FLOW_MIN;
                MB_WriteFloat(MB_ADDR_SETPOINT, sp);
            } else {
                uint8_t out = MB_ReadBits(MB_ADDR_MANUAL_OUT);
                if (out > 0u) out--;
                MB_WriteBits(MB_ADDR_MANUAL_OUT, out);
            }
            break;

        default: break;
        }
        break;
    }

    /* ── ALARM: экран типа аварии ── */
    case MENU_ALARM:
        switch (ev) {
        case BTN_EV_PRG:
            /* Квитирование: сбросить только если причина устранена (док).
               Если причина ещё присутствует — флаги сохраняются,
               но экран возвращается в MAIN (будет inline EMERGENZA). */
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
        default: break;
        }
        break;

    /* ── Подменю PASS1-PASS4 (табличный диспетчер) ─────────────────── */
    case MENU_SETTINGS:
    case MENU_SETTINGS_P2:
    case MENU_SETTINGS_P3:
    case MENU_SETTINGS_P4: {
        /* Таблицы переходов: targets[s_sel] = куда идти по PRG.
           Последний элемент всегда «Назад» и совпадает с back. */
        static const MenuState_t SUB_P1[] = {
            MENU_SET_FLOW_SP, MENU_SET_FLOW_SPR,
            MENU_SET_ALARM_LOW, MENU_SET_ALARM_LOWR,
            MENU_SET_ALARM_TIME, MENU_SET_MEM_NOR,
            MENU_SETTINGS_P2, MENU_MAIN
        };
        static const MenuState_t SUB_P2[] = {
            MENU_SET_SENSOR_Z, MENU_SET_SENSOR_S,
            MENU_SET_OUT_Z,    MENU_SET_OUT_S,
            MENU_SET_PID_TI,   MENU_SET_PID_BAND,
            MENU_SET_BLACKOUT,
            MENU_SETTINGS_P3, MENU_SETTINGS
        };
        static const MenuState_t SUB_P3[] = {
            MENU_SET_MAINT, MENU_SET_COUNT_MAX,
            MENU_SETTINGS_P4, MENU_SETTINGS_P2
        };
        static const MenuState_t SUB_P4[] = {
            MENU_SET_DATALOG, MENU_SETTINGS_P3
        };

        const MenuState_t *targets;
        uint8_t            count;
        MenuState_t        back;
        switch (s_state) {
        case MENU_SETTINGS:
            targets = SUB_P1; count = SETTINGS_N;    back = MENU_MAIN;         break;
        case MENU_SETTINGS_P2:
            targets = SUB_P2; count = SETTINGS_P2_N; back = MENU_SETTINGS;     break;
        case MENU_SETTINGS_P3:
            targets = SUB_P3; count = SETTINGS_P3_N; back = MENU_SETTINGS_P2;  break;
        default: /* MENU_SETTINGS_P4 */
            targets = SUB_P4; count = SETTINGS_P4_N; back = MENU_SETTINGS_P3;  break;
        }

        switch (ev) {
        case BTN_EV_UP:
            if (s_sel > 0u) s_sel--;
            break;
        case BTN_EV_DOWN:
            if (s_sel < (uint8_t)(count - 1u)) s_sel++;
            break;
        case BTN_EV_PRG:
            enter_state((s_sel < count) ? targets[s_sel] : back);
            break;
        case BTN_EV_PRG_LONG:
            /* Длинное PRG — выход в MAIN только из PASS1 (как было). */
            if (s_state == MENU_SETTINGS) enter_state(MENU_MAIN);
            break;
        case BTN_EV_AUTO_MAN:
            enter_state(back);
            break;
        case BTN_EV_ONOFF:
            enter_state(MENU_MAIN);
            break;
        default: break;
        }
        break;
    }

    /* ═══ Обобщённое редактирование: float / uint / toggle ═══
       Возврат в родительское подменю определяется принадлежностью
       состояния (PASS1/PASS2/PASS3/PASS4).                               */
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
        /* Куда возвращаться после PRG (OK) или AUTO_MAN (отмена): */
        MenuState_t parent;
        switch (s_state) {
        case MENU_SET_FLOW_SP:
        case MENU_SET_FLOW_SPR:
        case MENU_SET_ALARM_LOW:
        case MENU_SET_ALARM_LOWR:
        case MENU_SET_ALARM_TIME:
        case MENU_SET_MEM_NOR:
            parent = MENU_SETTINGS;     break;
        case MENU_SET_SENSOR_Z:
        case MENU_SET_SENSOR_S:
        case MENU_SET_OUT_Z:
        case MENU_SET_OUT_S:
        case MENU_SET_PID_TI:
        case MENU_SET_PID_BAND:
        case MENU_SET_BLACKOUT:
            parent = MENU_SETTINGS_P2;  break;
        case MENU_SET_MAINT:
        case MENU_SET_COUNT_MAX:
            parent = MENU_SETTINGS_P3;  break;
        default:
            parent = MENU_SETTINGS_P4;  break;
        }

        switch (ev) {
        case BTN_EV_UP:
            if (s_edit_kind == EDIT_KIND_FLOAT) {
                s_edit_float += s_edit_step;
                if (s_edit_float > s_edit_max) s_edit_float = s_edit_max;
            } else if (s_edit_kind == EDIT_KIND_UINT) {
                if (s_edit_uint < s_edit_umax) s_edit_uint++;
            } else {
                s_edit_uint = s_edit_uint ? 0u : 1u;
            }
            break;

        case BTN_EV_DOWN:
            if (s_edit_kind == EDIT_KIND_FLOAT) {
                s_edit_float -= s_edit_step;
                if (s_edit_float < s_edit_min) s_edit_float = s_edit_min;
            } else if (s_edit_kind == EDIT_KIND_UINT) {
                if (s_edit_uint > 0u) s_edit_uint--;
            } else {
                s_edit_uint = s_edit_uint ? 0u : 1u;
            }
            break;

        case BTN_EV_PRG:
            if (s_edit_kind == EDIT_KIND_FLOAT)
                MB_WriteFloat(s_edit_reg, s_edit_float);
            else
                MB_WriteBits (s_edit_reg, s_edit_uint);
            enter_state(parent);
            break;

        case BTN_EV_AUTO_MAN:
            enter_state(parent);   /* отмена */
            break;

        default: break;
        }
        break;
    }
    }

    leds_update();
}

/* ══════════════════════════════════════════════════════════════════════════
   Хелпер: отрисовка подменю (курсор, прокрутка окна из 3-х видимых пунктов)
   ══════════════════════════════════════════════════════════════════════════ */
static void render_submenu(const char *hdr, const char * const *items, uint8_t n)
{
    char line[21];
    char *p;

    lcd_mb_write(0, hdr);

    uint8_t win = (s_sel > 1u) ? (uint8_t)(s_sel - 1u) : 0u;
    if (n > 3u) {
        if (win + 3u > n) win = (uint8_t)(n - 3u);
    } else {
        win = 0u;
    }
    uint8_t rows = (n < 3u) ? n : 3u;

    for (uint8_t r = 0u; r < 3u; r++) {
        p = line;
        if (r < rows) {
            uint8_t idx = (uint8_t)(win + r);
            *p++ = (s_sel == idx) ? '>' : ' ';
            *p++ = ' ';
            const char *src = items[idx];
            uint8_t cnt = 0u;
            while (*src && cnt < 16u) { *p++ = *src++; cnt++; }
        }
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write((uint8_t)(r + 1u), line);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   menu_update_display — вызывается каждые 200 мс
   ══════════════════════════════════════════════════════════════════════════ */
void menu_update_display(void)
{
    char  line[21];
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
        p = fmt_f1(p, MB_ReadFloat(MB_ADDR_VERSION));
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
        p = fmt_time(p, MB_ReadUint32(MB_ADDR_TIMER));
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write(1, line);
        lcd_mb_write(2, S_STANDBY_WAIT);
        lcd_mb_write(3, S_STANDBY_BTN);
        break;

    /* ────────────────── MAIN ────────────────── */
    case MENU_MAIN: {
        uint8_t sr     = MB_ReadBits(MB_ADDR_MODE_SR);
        uint8_t is_man = (sr & MB_BIT_MODE_MANUAL) ? 1u : 0u;
        uint8_t is_wk  = (sr & MB_BIT_MODE_WORK)   ? 1u : 0u;
        uint8_t is_alm = (sr & MB_BIT_MODE_ALARM)  ? 1u : 0u;
        uint8_t is_rep = (sr & MB_BIT_MODE_REPEAT) ? 1u : 0u;
        float   sp     = MB_ReadFloat(MB_ADDR_SETPOINT);
        float   flow   = MB_ReadFloat(MB_ADDR_FLOW);
        float   temp   = MB_ReadFloat(MB_ADDR_EXT_TEMP);

        /* строка 0: "МКВП-02 АВАРИЯ РАБОТА" или "МКВП-02 АВТО  РАБОТА" и т.д.
           При аварии слово режима заменяется на "АВАРИЯ" (мигает). */
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
            /* "Вых: XX% Тек:X.XX м/с" */
            *p++ = '\xC2'; *p++ = '\xFB'; *p++ = '\xF5'; *p++ = ':'; *p++ = ' '; /* Вых:  */
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

        /* строка 3: подсказка (при аварии — подсказка перехода на тип) */
        if (is_alm)     lcd_mb_write(3, S_HINT_MAIN_ALARM);
        else if (is_man)lcd_mb_write(3, S_HINT_MAN);
        else            lcd_mb_write(3, S_HINT_AUTO);
        break;
    }

    /* ────────────────── ALARM ────────────────── */
    case MENU_ALARM: {
        /* строка 0: мигающая шапка */
        if (s_blink_on) lcd_mb_write(0, S_ALARM_HDR);
        else            lcd_mb_blank(0);

        /* строка 1: тип аварии */
        {
            uint8_t af = MB_ReadBits(MB_ADDR_ALARM_FLAGS);
            p = line;
            if (af & MB_ALARM_FLOW_LOW) {
                /* "Мало потока!        " */
                *p++ = '\xCC'; *p++ = '\xE0'; *p++ = '\xEB'; *p++ = '\xEE'; /* Мало */
                *p++ = ' ';
                *p++ = '\xEF'; *p++ = '\xEE'; *p++ = '\xF2'; *p++ = '\xEE'; /* пото */
                *p++ = '\xEA'; *p++ = '\xE0'; *p++ = '!';                   /* ка!  */
            } else if (af & MB_ALARM_INVERTER) {
                /* "Авар.инвертора!     " */
                *p++ = '\xC0'; *p++ = '\xE2'; *p++ = '\xE0'; *p++ = '\xF0'; /* Авар */
                *p++ = '.';
                *p++ = '\xE8'; *p++ = '\xED'; *p++ = '\xE2'; *p++ = '\xE5'; /* инве */
                *p++ = '\xF0'; *p++ = '\xF2'; *p++ = '\xEE'; *p++ = '\xF0'; /* ртор */
                *p++ = '\xE0'; *p++ = '!';                                   /* а!   */
            } else if (af & MB_ALARM_DOOR_OPEN) {
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

    /* ────────────────── SETTINGS (PASS1) ────────────────── */
    case MENU_SETTINGS: {
        static const char * const items[SETTINGS_N] = {
            S_ITEM_FLOW_SP, S_ITEM_FLOW_SPR,
            S_ITEM_ALARM_LOW, S_ITEM_ALARM_LOWR,
            S_ITEM_ALARM_TIME, S_ITEM_MEM_NOR,
            S_ITEM_P2_ENTER, S_ITEM_BACK
        };
        render_submenu(S_SETTINGS_HDR, items, (uint8_t)SETTINGS_N);
        break;
    }

    /* ────────────────── SETTINGS_P2 (PASS2) ────────────────── */
    case MENU_SETTINGS_P2: {
        static const char * const items[SETTINGS_P2_N] = {
            S_ITEM_SENSOR_Z, S_ITEM_SENSOR_S,
            S_ITEM_OUT_Z,    S_ITEM_OUT_S,
            S_ITEM_PID_TI,   S_ITEM_PID_BAND,
            S_ITEM_BLACKOUT,
            S_ITEM_P3_ENTER, S_ITEM_BACK
        };
        render_submenu(S_SETTINGS_P2_HDR, items, (uint8_t)SETTINGS_P2_N);
        break;
    }

    /* ────────────────── SETTINGS_P3 (PASS3) ────────────────── */
    case MENU_SETTINGS_P3: {
        static const char * const items[SETTINGS_P3_N] = {
            S_ITEM_MAINT, S_ITEM_COUNT_MAX, S_ITEM_P4_ENTER, S_ITEM_BACK
        };
        render_submenu(S_SETTINGS_P3_HDR, items, (uint8_t)SETTINGS_P3_N);
        break;
    }

    /* ────────────────── SETTINGS_P4 (PASS4) ────────────────── */
    case MENU_SETTINGS_P4: {
        static const char * const items[SETTINGS_P4_N] = {
            S_ITEM_DATALOG, S_ITEM_BACK
        };
        render_submenu(S_SETTINGS_P4_HDR, items, (uint8_t)SETTINGS_P4_N);
        break;
    }

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
