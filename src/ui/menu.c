/*  menu.c — МКВП-02: Монитор-контроллер потока воздуха
 *
 *  Дисплей 20×2.  Все строковые литералы — UTF-8 (исходник в UTF-8);
 *  преобразование в коды HD44780 выполняется автоматически драйвером
 *  LCD (lcd_print_utf8 / lcd_utf8_to_win1251).
 *
 *  Состояния:
 *    POWER_ON   — заставка 4 с
 *    STANDBY    — ожидание
 *    MAIN       — рабочий экран (АВТО/РУЧ, РАБОТА/СТОП)
 *    ALARM      — экран типа аварии
 *    PASSWORD   — ввод 4-значного пароля
 *    SETTINGS_* — список параметров текущего PASS-окна
 *    SET_*      — редактор одного параметра
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

#define FW_VERSION_F   1.0f

/* ══════════════════════════════════════════════════════════════════════════
   Светодиоды (active-HIGH, GPIOC)
   ══════════════════════════════════════════════════════════════════════════ */
#define LED_PORT        GPIOC
#define LED_RCU         RCU_GPIOC
#define LED_PIN_POWER   GPIO_PIN_6
#define LED_PIN_WORK    GPIO_PIN_7
#define LED_PIN_ALARM   GPIO_PIN_8
#define LED_PIN_MANUAL  GPIO_PIN_9

/* ══════════════════════════════════════════════════════════════════════════
   Тайминги (1 тик = 10 мс)
   ══════════════════════════════════════════════════════════════════════════ */
#define POWERON_TICKS         400u
#define BLINK_TICKS            50u
#define DISP_TICKS             50u
#define ALARM_ROTATE_TICKS    500u
#define WORK_GRACE_TICKS     4500u
#define LAMP_AUTO_OFF_TICKS 180000u
#define SOCKET_DELAY_TICKS    50u
#define INACT_TIMEOUT_TICKS 12000u
#define BOOST_TICKS          100u
#define BUZZER_PERIOD_TICKS 1000u
#define BUZZER_ON_TICKS      200u

/* ══════════════════════════════════════════════════════════════════════════
   UTF-8 → Win-1251 (для динамической сборки строк).  Используется внутри
   render-функций, где буфер собирается посимвольно через указатель p.
   ══════════════════════════════════════════════════════════════════════════ */
static char *put(char *p, const char *utf8)
{
    while (*utf8) {
        uint8_t c = (uint8_t)*utf8++;
        if (c < 0x80) {
            *p++ = (char)c;
        } else if (c == 0xD0 && (uint8_t)*utf8) {
            uint8_t c2 = (uint8_t)*utf8++;
            if (c2 == 0x81)                            *p++ = (char)0xA8;
            else if (c2 >= 0x90 && c2 <= 0xBF)         *p++ = (char)(0xC0 + (c2 - 0x90));
            else                                        *p++ = '?';
        } else if (c == 0xD1 && (uint8_t)*utf8) {
            uint8_t c2 = (uint8_t)*utf8++;
            if (c2 == 0x91)                            *p++ = (char)0xB8;
            else if (c2 >= 0x80 && c2 <= 0x8F)         *p++ = (char)(0xF0 + (c2 - 0x80));
            else                                        *p++ = '?';
        } else if (c == 0xC2 && (uint8_t)*utf8) {
            uint8_t c2 = (uint8_t)*utf8++;
            *p++ = (char)c2;        /* °, «, »  и пр. — те же коды */
        } else if (c == 0xE2 && (uint8_t)*utf8 && (uint8_t)utf8[1]) {
            uint8_t c2 = (uint8_t)*utf8++;
            uint8_t c3 = (uint8_t)*utf8++;
            if (c2 == 0x80 && (c3 == 0x93 || c3 == 0x94)) *p++ = '-';
            else                                            *p++ = '?';
        } else {
            *p++ = (char)c;         /* старые \xCC-литералы пройдут как есть */
        }
    }
    return p;
}

/* ══════════════════════════════════════════════════════════════════════════
   Внутреннее состояние
   ══════════════════════════════════════════════════════════════════════════ */
static MenuState_t s_state       = MENU_POWER_ON;
static uint16_t    s_state_ticks = 0;
static uint16_t    s_blink_tick  = 0;
static uint8_t     s_blink_on    = 0;
static uint16_t    s_disp_tick   = 0;

static uint16_t s_alarm_rot_tick    = 0u;
static uint8_t  s_alarm_view_idx    = 0u;
static uint16_t s_work_grace_tick   = 0u;
static uint8_t  s_work_was_on       = 0u;
static uint32_t s_lamp_off_tick     = 0u;
static uint16_t s_socket_pending    = 0u;
static uint8_t  s_socket_pending_on = 0u;
static uint16_t s_inact_tick        = 0u;
static uint16_t s_boost_tick        = 0u;
static uint8_t  s_boost_save_out    = 0u;

static uint8_t  s_emerg_was         = 0u;
static uint8_t  s_emerg_save_out    = 0u;
static uint8_t  s_emerg_save_man    = 0u;

static uint16_t s_buzzer_tick       = 0u;
static uint8_t  s_buzzer_muted      = 0u;
static uint8_t  s_alarm_was_on      = 0u;

static uint8_t  s_pw_field          = 0u;
static uint8_t  s_pw_digits[4]      = {0,0,0,0};
static uint8_t  s_pw_attempts       = 0u;

static uint8_t  s_pass_list_idx     = 0u;

/* Редактируемые черновики */
static float       s_edit_float   = 0.0f;
static uint8_t     s_edit_uint    = 0u;
static uint16_t    s_edit_reg     = 0u;
static float       s_edit_min     = 0.0f;
static float       s_edit_max     = 0.0f;
static uint8_t     s_edit_dec     = 2u;
static uint8_t     s_edit_umax    = 100u;
static uint8_t     s_edit_int_dig  = 1u;
static uint8_t     s_edit_total_dig = 3u;
static uint8_t     s_edit_digit   = 0u;
static const char *s_edit_hdr     = NULL;
static const char *s_edit_unit    = "";

typedef enum {
    EDIT_KIND_FLOAT  = 0,
    EDIT_KIND_UINT   = 1,
    EDIT_KIND_TOGGLE = 2,
} EditKind_t;
static EditKind_t  s_edit_kind = EDIT_KIND_FLOAT;

typedef enum {
    TOG_MEM_NOR = 0,
    TOG_ON_OFF  = 1,
} ToggleLabel_t;
static ToggleLabel_t s_edit_tog_lbl = TOG_MEM_NOR;

/* ══════════════════════════════════════════════════════════════════════════
   Текстовые константы — UTF-8.  Длина массива не задаётся: компилятор
   сам подберёт по строковому литералу.
   ══════════════════════════════════════════════════════════════════════════ */

static const char S_INIT[]         = "  Инициализация...  ";
static const char S_STANDBY_BTN[]  = "   Нажмите ВКЛ      ";
static const char S_ALARM_HDR[]    = "!!!  АВАРИЯ  !!!    ";
static const char S_PWD_HDR[]      = "  ПАРОЛЬ ВХОДА      ";
static const char S_PWD_BAD[]      = "  Неверный пароль   ";
static const char S_HINT_BOOST[]   = "   ВЕНТИЛЯЦИЯ MAX   ";

/* Заголовки экранов редактирования (короткие, до 20 видимых символов) */
static const char S_HDR_FLOW_SP[]    = "-- Уставка потока --";
static const char S_HDR_FLOW_SPR[]   = "-- Уставка повтора -";
static const char S_HDR_ALARM_LOW[]  = "--- Порог аварии ---";
static const char S_HDR_ALARM_LOWR[] = "--- Сброс аварии ---";
static const char S_HDR_ALARM_TIME[] = "-- Задержка аварии -";
static const char S_HDR_MEM_NOR[]    = "--- Режим ручного --";
static const char S_HDR_SENSOR_Z[]   = "-- Ноль датчика ----";
static const char S_HDR_SENSOR_S[]   = "-- Диап. датчика ---";
static const char S_HDR_OUT_Z[]      = "--- Ноль выхода ----";
static const char S_HDR_OUT_S[]      = "--- Диап. выхода ---";
static const char S_HDR_PID_TI[]     = "--- ПИД:инт.время --";
static const char S_HDR_PID_BAND[]   = "--- ПИД:полоса -----";
static const char S_HDR_BLACKOUT[]   = "----- Автопуск -----";
static const char S_HDR_MAINT[]      = "--- Обслуживание ---";
static const char S_HDR_COUNT_MAX[]  = "--- Макс. счётчик --";
static const char S_HDR_DATALOG[]    = "--- Регистратор ----";

/* Названия пунктов в списке PASS (короче 20 символов) */
static const char S_NM_FLOW_SP[]    = "Уставка потока";
static const char S_NM_FLOW_SPR[]   = "Уставка повтора";
static const char S_NM_ALARM_LOW[]  = "Порог аварии";
static const char S_NM_ALARM_LOWR[] = "Сброс аварии";
static const char S_NM_ALARM_TIME[] = "Задержка аварии";
static const char S_NM_MEM_NOR[]    = "Режим ручного";
static const char S_NM_SENSOR_Z[]   = "Ноль датчика";
static const char S_NM_SENSOR_S[]   = "Диап. датчика";
static const char S_NM_OUT_Z[]      = "Ноль выхода";
static const char S_NM_OUT_S[]      = "Диап. выхода";
static const char S_NM_PID_TI[]     = "ПИД: время";
static const char S_NM_PID_BAND[]   = "ПИД: полоса";
static const char S_NM_BLACKOUT[]   = "Автопуск";
static const char S_NM_MAINT[]      = "Обслуживание";
static const char S_NM_COUNT_MAX[]  = "Макс. счётчик";
static const char S_NM_DATALOG[]    = "Регистратор";

/* Суффиксы единиц */
static const char S_U_MS[]   = "м/с";
static const char S_U_CMS[]  = "см/с";
static const char S_U_PCT[]  = "%";
static const char S_U_SEC[]  = "с";
static const char S_U_HRS[]  = "ч";

/* ══════════════════════════════════════════════════════════════════════════
   Последовательности параметров каждого PASS
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

static MenuState_t pass_list_state(uint8_t pass_idx)
{
    switch (pass_idx) {
    case 0u: return MENU_SETTINGS;
    case 1u: return MENU_SETTINGS_P2;
    case 2u: return MENU_SETTINGS_P3;
    case 3u: return MENU_SETTINGS_P4;
    default: return MENU_MAIN;
    }
}

static uint8_t list_pass_idx(MenuState_t st)
{
    switch (st) {
    case MENU_SETTINGS:    return 0u;
    case MENU_SETTINGS_P2: return 1u;
    case MENU_SETTINGS_P3: return 2u;
    case MENU_SETTINGS_P4: return 3u;
    default:               return 0u;
    }
}

static const char *param_name(MenuState_t st)
{
    switch (st) {
    case MENU_SET_FLOW_SP:    return S_NM_FLOW_SP;
    case MENU_SET_FLOW_SPR:   return S_NM_FLOW_SPR;
    case MENU_SET_ALARM_LOW:  return S_NM_ALARM_LOW;
    case MENU_SET_ALARM_LOWR: return S_NM_ALARM_LOWR;
    case MENU_SET_ALARM_TIME: return S_NM_ALARM_TIME;
    case MENU_SET_MEM_NOR:    return S_NM_MEM_NOR;
    case MENU_SET_SENSOR_Z:   return S_NM_SENSOR_Z;
    case MENU_SET_SENSOR_S:   return S_NM_SENSOR_S;
    case MENU_SET_OUT_Z:      return S_NM_OUT_Z;
    case MENU_SET_OUT_S:      return S_NM_OUT_S;
    case MENU_SET_PID_TI:     return S_NM_PID_TI;
    case MENU_SET_PID_BAND:   return S_NM_PID_BAND;
    case MENU_SET_BLACKOUT:   return S_NM_BLACKOUT;
    case MENU_SET_MAINT:      return S_NM_MAINT;
    case MENU_SET_COUNT_MAX:  return S_NM_COUNT_MAX;
    case MENU_SET_DATALOG:    return S_NM_DATALOG;
    default:                  return "";
    }
}

static void editor_commit_draft(void)
{
    if (s_edit_kind == EDIT_KIND_FLOAT)
        MB_WriteFloat(s_edit_reg, s_edit_float);
    else
        MB_WriteBits (s_edit_reg, s_edit_uint);
}

/* ══════════════════════════════════════════════════════════════════════════
   Форматирование чисел
   ══════════════════════════════════════════════════════════════════════════ */
static char *fmt_f2(char *p, float v)
{
    if (v < 0.0f)  v = 0.0f;
    if (v > 9.99f) v = 9.99f;
    int32_t i   = (int32_t)v;
    int32_t fr2 = (int32_t)((v - (float)i) * 100.0f + 0.5f);
    if (fr2 >= 100) { i++; fr2 = 0; }
    *p++ = (char)('0' + (i % 10));
    *p++ = '.';
    *p++ = (char)('0' + fr2 / 10);
    *p++ = (char)('0' + fr2 % 10);
    return p;
}

static char *fmt_f1(char *p, float v)
{
    if (v < -99.9f) v = -99.9f;
    if (v >  99.9f) v =  99.9f;
    if (v < 0.0f) { *p++ = '-'; v = -v; }
    int32_t i  = (int32_t)v;
    int32_t fr = (int32_t)((v - (float)i) * 10.0f + 0.5f);
    if (fr >= 10) { i++; fr = 0; if (i > 99) i = 99; }
    if (i >= 10) *p++ = (char)('0' + (i / 10));
    *p++ = (char)('0' + (i % 10));
    *p++ = '.';
    *p++ = (char)('0' + fr);
    return p;
}

static char *fmt_u8_3(char *p, uint8_t v)
{
    p[0] = (v >= 100u) ? (char)('0' + v / 100u) : ' ';
    p[1] = (v >=  10u) ? (char)('0' + (v / 10u) % 10u) : ((v >= 100u) ? '0' : ' ');
    p[2] = (char)('0' + v % 10u);
    return p + 3;
}

static char *fmt_u16_4(char *p, uint16_t v)
{
    if (v > 9999u) v = 9999u;
    uint16_t th = v / 1000u;
    uint16_t hu = (v / 100u) % 10u;
    uint16_t te = (v / 10u)  % 10u;
    uint16_t on = v % 10u;
    p[0] = (th)             ? (char)('0' + th) : ' ';
    p[1] = (th || hu)       ? (char)('0' + hu) : ' ';
    p[2] = (th || hu || te) ? (char)('0' + te) : ' ';
    p[3] = (char)('0' + on);
    return p + 4;
}

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
   Редактирование по разрядам
   ══════════════════════════════════════════════════════════════════════════ */
static uint8_t int_digit_count_f(float maxv)
{
    int32_t imax = (int32_t)maxv;
    if (imax >= 1000) return 4u;
    if (imax >=  100) return 3u;
    if (imax >=   10) return 2u;
    return 1u;
}

static uint8_t int_digit_count_u(uint8_t umax)
{
    if (umax >= 100u) return 3u;
    if (umax >=  10u) return 2u;
    return 1u;
}

static float digit_step_value(uint8_t digit_idx, uint8_t int_dig, uint8_t dec)
{
    int8_t exponent = (int8_t)(int_dig - 1) - (int8_t)digit_idx;
    float v = 1.0f;
    if (exponent >= 0) {
        for (int8_t i = 0; i < exponent; i++) v *= 10.0f;
    } else {
        for (int8_t i = 0; i < -exponent; i++) v *= 0.1f;
    }
    (void)dec;
    return v;
}

static uint8_t digit_to_char_pos(uint8_t digit_idx, uint8_t int_dig, uint8_t dec)
{
    if (dec == 0u || digit_idx < int_dig) return digit_idx;
    return (uint8_t)(digit_idx + 1u);
}

/* ══════════════════════════════════════════════════════════════════════════
   Запись в LCD + зеркало в Modbus.  Вход — UTF-8.
   Внутри: UTF-8 → Win-1251 → 20-байтный буфер → LCD + Modbus.
   ══════════════════════════════════════════════════════════════════════════ */
static void lcd_mb_write(uint8_t row, const char *line_utf8)
{
    static const uint16_t mb_line_addr[2] = {
        MB_ADDR_DISPLAY_LINE_0, MB_ADDR_DISPLAY_LINE_1
    };
    if (row >= 2u) return;

    char buf[21];
    lcd_utf8_to_win1251(line_utf8, buf, 20u);
    lcd_print_win1251_at(row, 0, buf);
    MB_WriteString(mb_line_addr[row], buf);
}

static void lcd_mb_write_w1251(uint8_t row, const char *line_w1251)
{
    /* Перегрузка для уже-собранных Win-1251 буферов (динамические render-функции). */
    static const uint16_t mb_line_addr[2] = {
        MB_ADDR_DISPLAY_LINE_0, MB_ADDR_DISPLAY_LINE_1
    };
    if (row >= 2u) return;

    char buf[21];
    uint8_t i = 0u;
    if (line_w1251 != NULL) {
        while (i < 20u && line_w1251[i] != '\0') { buf[i] = line_w1251[i]; i++; }
    }
    while (i < 20u) buf[i++] = ' ';
    buf[20] = '\0';

    lcd_print_win1251_at(row, 0, buf);
    MB_WriteString(mb_line_addr[row], buf);
}

static void lcd_mb_blank(uint8_t row)
{
    lcd_mb_write_w1251(row, "                    ");
}

/* ══════════════════════════════════════════════════════════════════════════
   Светодиоды
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
static void enter_state(MenuState_t st);

static void enter_pass(uint8_t pass_idx)
{
    if (pass_idx >= PASS_COUNT) return;
    if (PASSES[pass_idx].n <= 1u) {
        enter_state(PASSES[pass_idx].seq[0]);
        return;
    }
    s_pass_list_idx = 0u;
    enter_state(pass_list_state(pass_idx));
}

static void enter_state(MenuState_t st)
{
    s_state       = st;
    s_state_ticks = 0u;

    switch (st) {
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

    case MENU_SETTINGS:
    case MENU_SETTINGS_P2:
    case MENU_SETTINGS_P3:
    case MENU_SETTINGS_P4: {
        uint8_t p = list_pass_idx(st);
        if (PASSES[p].n <= 1u) {
            s_state = PASSES[p].seq[0];
            enter_state(s_state);
            return;
        }
        if (s_pass_list_idx >= PASSES[p].n) s_pass_list_idx = 0u;
        break;
    }

    case MENU_SET_FLOW_SP:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_SETPOINT;
        s_edit_float = MB_ReadFloat(MB_ADDR_SETPOINT);
        s_edit_min   = 0.0f; s_edit_max = 9.9f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_FLOW_SP;
        break;
    case MENU_SET_FLOW_SPR:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_FLOW_SP_R;
        s_edit_float = MB_ReadFloat(MB_ADDR_FLOW_SP_R);
        s_edit_min   = 0.0f; s_edit_max = 9.9f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_FLOW_SPR;
        break;
    case MENU_SET_ALARM_LOW:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_ALARM_LOW;
        s_edit_float = MB_ReadFloat(MB_ADDR_ALARM_LOW);
        s_edit_min   = 0.0f; s_edit_max = 9.9f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_ALARM_LOW;
        break;
    case MENU_SET_ALARM_LOWR:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_ALARM_LOW_R;
        s_edit_float = MB_ReadFloat(MB_ADDR_ALARM_LOW_R);
        s_edit_min   = 0.0f; s_edit_max = 9.9f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_ALARM_LOWR;
        break;

    case MENU_SET_SENSOR_Z:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_SENSOR_ZERO;
        s_edit_float = MB_ReadFloat(MB_ADDR_SENSOR_ZERO);
        s_edit_min   = 0.0f; s_edit_max = 9.9f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_SENSOR_Z;
        break;
    case MENU_SET_SENSOR_S:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_SENSOR_SPAN;
        s_edit_float = MB_ReadFloat(MB_ADDR_SENSOR_SPAN);
        s_edit_min   = 0.0f; s_edit_max = 9.9f;
        s_edit_dec   = 2u;   s_edit_unit = S_U_MS;  s_edit_hdr = S_HDR_SENSOR_S;
        break;
    case MENU_SET_PID_TI:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_PID_TI;
        s_edit_float = MB_ReadFloat(MB_ADDR_PID_TI);
        s_edit_min   = 0.0f; s_edit_max = 9.9f;
        s_edit_dec   = 1u;   s_edit_unit = S_U_SEC; s_edit_hdr = S_HDR_PID_TI;
        break;
    case MENU_SET_PID_BAND:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_PID_BAND;
        s_edit_float = MB_ReadFloat(MB_ADDR_PID_BAND);
        s_edit_min   = 1.0f; s_edit_max = 999.0f;
        s_edit_dec   = 0u;   s_edit_unit = S_U_CMS; s_edit_hdr = S_HDR_PID_BAND;
        break;

    case MENU_SET_MAINT:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_MAINT_HOURS;
        s_edit_float = MB_ReadFloat(MB_ADDR_MAINT_HOURS);
        s_edit_min   = 0.0f; s_edit_max = 9999.0f;
        s_edit_dec   = 0u;   s_edit_unit = S_U_HRS; s_edit_hdr = S_HDR_MAINT;
        break;
    case MENU_SET_COUNT_MAX:
        s_edit_kind  = EDIT_KIND_FLOAT;
        s_edit_reg   = MB_ADDR_COUNT_MAX;
        s_edit_float = MB_ReadFloat(MB_ADDR_COUNT_MAX);
        s_edit_min   = 0.0f; s_edit_max = 9999.0f;
        s_edit_dec   = 0u;   s_edit_unit = S_U_HRS; s_edit_hdr = S_HDR_COUNT_MAX;
        break;

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

    case MENU_PASSWORD:
        s_pw_field    = 0u;
        s_pw_digits[0] = s_pw_digits[1] = s_pw_digits[2] = s_pw_digits[3] = 0u;
        s_pw_attempts = 0u;
        break;
    }

    if (s_edit_kind == EDIT_KIND_FLOAT) {
        if (s_edit_float < s_edit_min) s_edit_float = s_edit_min;
        if (s_edit_float > s_edit_max) s_edit_float = s_edit_max;
        s_edit_int_dig   = int_digit_count_f(s_edit_max);
        s_edit_total_dig = (uint8_t)(s_edit_int_dig + s_edit_dec);
        if (s_edit_total_dig == 0u) s_edit_total_dig = 1u;
        s_edit_digit = (uint8_t)(s_edit_total_dig - 1u);
    } else if (s_edit_kind == EDIT_KIND_UINT) {
        if (s_edit_uint > s_edit_umax) s_edit_uint = s_edit_umax;
        s_edit_int_dig   = int_digit_count_u(s_edit_umax);
        s_edit_dec       = 0u;
        s_edit_total_dig = s_edit_int_dig;
        s_edit_digit = (uint8_t)(s_edit_total_dig - 1u);
    } else if (s_edit_kind == EDIT_KIND_TOGGLE) {
        if (s_edit_uint > 1u) s_edit_uint = 1u;
        s_edit_int_dig   = 0u;
        s_edit_total_dig = 0u;
        s_edit_digit     = 0u;
    }

    s_disp_tick = (uint16_t)DISP_TICKS;
}

/* ══════════════════════════════════════════════════════════════════════════
   Modbus команды
   ══════════════════════════════════════════════════════════════════════════ */
static void handle_menu_goto(void)
{
    uint8_t code = MB_ReadBits(MB_ADDR_MENU_GOTO);
    if (code == 0u) return;
    MB_WriteBits(MB_ADDR_MENU_GOTO, 0u);
    if (code > (uint8_t)MENU_PASSWORD) return;
    enter_state((MenuState_t)code);
}

static void handle_modbus_cmd(void)
{
    uint8_t cmd = MB_ReadBits(MB_ADDR_MODE_CR);
    if (cmd == MB_CMD_NONE) return;
    MB_WriteBits(MB_ADDR_MODE_CR, MB_CMD_NONE);

    switch (cmd) {
    case MB_CMD_POWER_ON:
        if (s_state == MENU_STANDBY) enter_state(MENU_POWER_ON);
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
    case MB_CMD_NIGHT_TOGGLE: {
        uint8_t sr = MB_ReadBits(MB_ADDR_MODE_SR);
        if (sr & MB_BIT_MODE_NIGHT) MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
        else                        MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
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
        if (sr & MB_BIT_MODE_EMERGENCY) MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
        else                            MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
        break;
    }
    case MB_CMD_BUZZER_MUTE:
        s_buzzer_muted = 1u;
        MB_WriteBits(MB_ADDR_BUZZER_STATE, MB_BUZZER_OFF);
        break;
    default: break;
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   menu_init
   ══════════════════════════════════════════════════════════════════════════ */
void menu_init(void)
{
    leds_init();
    lcd_init();

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
   Хелперы редактора
   ══════════════════════════════════════════════════════════════════════════ */
static void editor_inc_digit(void)
{
    if (s_edit_kind == EDIT_KIND_FLOAT) {
        float step = digit_step_value(s_edit_digit, s_edit_int_dig, s_edit_dec);
        s_edit_float += step;
        if (s_edit_float > s_edit_max) s_edit_float = s_edit_max;
    } else if (s_edit_kind == EDIT_KIND_UINT) {
        float step = digit_step_value(s_edit_digit, s_edit_int_dig, 0u);
        int32_t v  = (int32_t)s_edit_uint + (int32_t)(step + 0.5f);
        if (v > (int32_t)s_edit_umax) v = (int32_t)s_edit_umax;
        s_edit_uint = (uint8_t)v;
    } else {
        s_edit_uint = s_edit_uint ? 0u : 1u;
    }
}

static void editor_dec_digit(void)
{
    if (s_edit_kind == EDIT_KIND_FLOAT) {
        float step = digit_step_value(s_edit_digit, s_edit_int_dig, s_edit_dec);
        s_edit_float -= step;
        if (s_edit_float < s_edit_min) s_edit_float = s_edit_min;
    } else if (s_edit_kind == EDIT_KIND_UINT) {
        float step = digit_step_value(s_edit_digit, s_edit_int_dig, 0u);
        int32_t v  = (int32_t)s_edit_uint - (int32_t)(step + 0.5f);
        if (v < 0) v = 0;
        s_edit_uint = (uint8_t)v;
    } else {
        s_edit_uint = s_edit_uint ? 0u : 1u;
    }
}

static void editor_digit_left(void)
{
    if (s_edit_total_dig <= 1u) return;
    if (s_edit_digit == 0u) s_edit_digit = (uint8_t)(s_edit_total_dig - 1u);
    else                    s_edit_digit--;
}

static void editor_digit_right(void)
{
    if (s_edit_total_dig <= 1u) return;
    s_edit_digit = (uint8_t)((s_edit_digit + 1u) % s_edit_total_dig);
}

static void editor_back(void)
{
    uint8_t pass_idx = 0u, p_idx = 0u;
    if (locate_param(s_state, &pass_idx, &p_idx) &&
        PASSES[pass_idx].n > 1u) {
        s_pass_list_idx = p_idx;
        enter_state(pass_list_state(pass_idx));
    } else {
        enter_state(MENU_MAIN);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   menu_process
   ══════════════════════════════════════════════════════════════════════════ */
void menu_process(BtnEvent_t ev)
{
    s_state_ticks++;

    s_blink_tick++;
    if (s_blink_tick >= (uint16_t)BLINK_TICKS) {
        s_blink_tick = 0u;
        s_blink_on   = s_blink_on ? 0u : 1u;
    }

    s_disp_tick++;

    handle_modbus_cmd();
    handle_menu_goto();

    if (ev != BTN_EV_NONE)
        MB_WriteBits(MB_ADDR_BTN_EVENT, (uint8_t)ev);

    uint8_t sr_now    = MB_ReadBits(MB_ADDR_MODE_SR);
    uint8_t work_now  = (sr_now & MB_BIT_MODE_WORK) ? 1u : 0u;

    if (!s_work_was_on && work_now) {
        s_work_grace_tick = (uint16_t)WORK_GRACE_TICKS;
        s_boost_tick      = (uint16_t)BOOST_TICKS;
        s_boost_save_out  = MB_ReadBits(MB_ADDR_MANUAL_OUT);
        MB_WriteBits(MB_ADDR_MANUAL_OUT, 100u);
    }
    if (s_work_was_on && !work_now) s_lamp_off_tick = 0u;
    s_work_was_on = work_now;

    if (s_boost_tick > 0u) {
        s_boost_tick--;
        if (s_boost_tick == 0u) MB_WriteBits(MB_ADDR_MANUAL_OUT, s_boost_save_out);
    }
    if (s_work_grace_tick > 0u) s_work_grace_tick--;

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
            if (s_emerg_save_man) MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            else                  MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
        }
        if (emerg_now && MB_ReadBits(MB_ADDR_MANUAL_OUT) != 100u)
            MB_WriteBits(MB_ADDR_MANUAL_OUT, 100u);
        s_emerg_was = emerg_now;
    }

    if (s_socket_pending > 0u) {
        s_socket_pending--;
        if (s_socket_pending == 0u) {
            uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
            if (s_socket_pending_on) out |= MB_OUT_SOCKET;
            else                     out &= (uint8_t)~MB_OUT_SOCKET;
            MB_WriteBits(MB_ADDR_OUTPUT_STATE, out);
        }
    }

    {
        uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
        if (!work_now && (out & MB_OUT_LAMP)) {
            s_lamp_off_tick++;
            if (s_lamp_off_tick >= (uint32_t)LAMP_AUTO_OFF_TICKS) {
                MB_WriteBits(MB_ADDR_OUTPUT_STATE, (uint8_t)(out & ~MB_OUT_LAMP));
                s_lamp_off_tick = 0u;
            }
        } else {
            s_lamp_off_tick = 0u;
        }
    }

    if (MB_ReadBits(MB_ADDR_ALARM_FLAGS) && s_work_grace_tick == 0u)
        MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);
    else
        MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);

    s_alarm_rot_tick++;
    if (s_alarm_rot_tick >= (uint16_t)ALARM_ROTATE_TICKS) {
        s_alarm_rot_tick = 0u;
        s_alarm_view_idx = (uint8_t)((s_alarm_view_idx + 1u) % 3u);
    }

    {
        uint8_t alarm_now = (MB_ReadBits(MB_ADDR_MODE_SR) & MB_BIT_MODE_ALARM) ? 1u : 0u;
        if (!s_alarm_was_on && alarm_now) {
            s_buzzer_muted = 0u;
            s_buzzer_tick  = 0u;
        }
        s_alarm_was_on = alarm_now;

        uint8_t buzzer = MB_BUZZER_OFF;
        if (alarm_now && !s_buzzer_muted) {
            s_buzzer_tick++;
            if (s_buzzer_tick >= (uint16_t)BUZZER_PERIOD_TICKS) s_buzzer_tick = 0u;
            buzzer = (s_buzzer_tick < (uint16_t)BUZZER_ON_TICKS) ? MB_BUZZER_ON : MB_BUZZER_OFF;
        } else {
            s_buzzer_tick = 0u;
            buzzer = MB_BUZZER_OFF;
        }
        if (MB_ReadBits(MB_ADDR_BUZZER_STATE) != buzzer)
            MB_WriteBits(MB_ADDR_BUZZER_STATE, buzzer);
    }

    {
        uint8_t in_cfg = (s_state >= MENU_SETTINGS &&
                          s_state <= MENU_SET_DATALOG) ||
                         s_state == MENU_PASSWORD;
        if (in_cfg) {
            if (ev != BTN_EV_NONE) s_inact_tick = 0u;
            else                   s_inact_tick++;
            if (s_inact_tick >= (uint16_t)INACT_TIMEOUT_TICKS) {
                s_inact_tick = 0u;
                enter_state(MENU_MAIN);
            }
        } else {
            s_inact_tick = 0u;
        }
    }

    switch (s_state) {

    case MENU_POWER_ON:
        if (s_state_ticks >= (uint16_t)POWERON_TICKS)
            enter_state(MENU_MAIN);
        break;

    case MENU_STANDBY:
        if (ev == BTN_EV_ONOFF || ev == BTN_EV_ONOFF_LONG)
            enter_state(MENU_POWER_ON);
        break;

    case MENU_MAIN: {
        uint8_t sr     = MB_ReadBits(MB_ADDR_MODE_SR);
        uint8_t is_man = (sr & MB_BIT_MODE_MANUAL) ? 1u : 0u;

        switch (ev) {
        case BTN_EV_ONOFF:
            if (sr & MB_BIT_MODE_NIGHT) {
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
                enter_state(MENU_STANDBY);
            } else {
                if (sr & MB_BIT_MODE_WORK) MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
                else                       MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_WORK);
            }
            break;
        case BTN_EV_ONOFF_LONG:
            if (sr & MB_BIT_MODE_NIGHT) MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
            else                        MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_NIGHT);
            break;
        case BTN_EV_AUTO_MAN:
            if (is_man) MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            else {
                if (MB_ReadBits(MB_ADDR_MANUAL_MEM) == MB_MANUAL_NOR)
                    MB_WriteBits(MB_ADDR_MANUAL_OUT, 0u);
                MB_SetBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            }
            break;
        case BTN_EV_PRG:
            if (sr & MB_BIT_MODE_ALARM) {
                s_buzzer_muted = 1u;
                MB_WriteBits(MB_ADDR_BUZZER_STATE, MB_BUZZER_OFF);
                enter_state(MENU_ALARM);
            }
            break;
        case BTN_EV_PRG_LONG:
            enter_state(MENU_PASSWORD);
            break;
        case BTN_EV_LAMP: {
            uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
            MB_WriteBits(MB_ADDR_OUTPUT_STATE, (uint8_t)(out ^ MB_OUT_LAMP));
            break;
        }
        case BTN_EV_RB: {
            uint8_t out = MB_ReadBits(MB_ADDR_OUTPUT_STATE);
            s_socket_pending_on = (uint8_t)((out & MB_OUT_SOCKET) ? 0u : 1u);
            s_socket_pending    = (uint16_t)SOCKET_DELAY_TICKS;
            break;
        }
        case BTN_EV_E:
            if (sr & MB_BIT_MODE_EMERGENCY) MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            else                            MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_EMERGENCY);
            break;
        default: break;
        }
        break;
    }

    case MENU_ALARM:
        switch (ev) {
        case BTN_EV_PRG:
            s_buzzer_muted = 1u;
            MB_WriteBits(MB_ADDR_BUZZER_STATE, MB_BUZZER_OFF);
            MB_WriteBits(MB_ADDR_ALARM_FLAGS, 0u);
            MB_ClearBit (MB_ADDR_MODE_SR, MB_BIT_MODE_ALARM);
            enter_state(MENU_MAIN);
            break;
        case BTN_EV_AUTO_MAN:
            if (MB_ReadBits(MB_ADDR_MODE_SR) & MB_BIT_MODE_MANUAL)
                MB_ClearBit(MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
            else
                MB_SetBit  (MB_ADDR_MODE_SR, MB_BIT_MODE_MANUAL);
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

    case MENU_SETTINGS:
    case MENU_SETTINGS_P2:
    case MENU_SETTINGS_P3:
    case MENU_SETTINGS_P4: {
        uint8_t p = list_pass_idx(s_state);
        uint8_t n = PASSES[p].n;
        if (n == 0u) { enter_state(MENU_MAIN); break; }

        switch (ev) {
        case BTN_EV_LAMP:
        case BTN_EV_LAMP_LONG:
            if (n > 1u) {
                if (s_pass_list_idx == 0u) s_pass_list_idx = (uint8_t)(n - 1u);
                else                       s_pass_list_idx--;
            }
            break;
        case BTN_EV_ONOFF:
            if (n > 1u) s_pass_list_idx = (uint8_t)((s_pass_list_idx + 1u) % n);
            break;
        case BTN_EV_PRG:
            enter_state(PASSES[p].seq[s_pass_list_idx]);
            break;
        case BTN_EV_PRG_LONG:
        case BTN_EV_E:
            enter_state(MENU_MAIN);
            break;
        default: break;
        }
        break;
    }

    case MENU_PASSWORD:
        switch (ev) {
        case BTN_EV_LAMP:
        case BTN_EV_LAMP_LONG:
            s_pw_digits[s_pw_field] = (uint8_t)((s_pw_digits[s_pw_field] + 1u) % 10u);
            break;
        case BTN_EV_ONOFF:
            s_pw_digits[s_pw_field] = (uint8_t)((s_pw_digits[s_pw_field] + 9u) % 10u);
            break;
        case BTN_EV_AUTO_MAN:
            s_pw_field = (uint8_t)((s_pw_field + 1u) & 0x03u);
            break;
        case BTN_EV_RB:
            s_pw_field = (uint8_t)((s_pw_field + 3u) & 0x03u);
            break;
        case BTN_EV_PRG: {
            uint16_t code = (uint16_t)(
                s_pw_digits[0] * 1000u +
                s_pw_digits[1] * 100u  +
                s_pw_digits[2] * 10u   +
                s_pw_digits[3]);
            if (code == 0u || code == 1u)      enter_pass(0u);
            else if (code == 2u)               enter_pass(1u);
            else if (code == 3u)               enter_pass(2u);
            else if (code == 4u)               enter_pass(3u);
            else {
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
        case BTN_EV_E:
            enter_state(MENU_MAIN);
            break;
        default: break;
        }
        break;

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
        switch (ev) {
        case BTN_EV_LAMP:
        case BTN_EV_LAMP_LONG: editor_inc_digit();   break;
        case BTN_EV_ONOFF:     editor_dec_digit();   break;
        case BTN_EV_AUTO_MAN:  editor_digit_right(); break;
        case BTN_EV_RB:        editor_digit_left();  break;
        case BTN_EV_PRG:
            editor_commit_draft();
            editor_back();
            break;
        case BTN_EV_PRG_LONG:
            editor_commit_draft();
            (void)settings_save();
            enter_state(MENU_MAIN);
            break;
        case BTN_EV_E:
            editor_back();
            break;
        default: break;
        }
        break;
    }

    leds_update();

    if (s_disp_tick >= (uint16_t)DISP_TICKS || ev != BTN_EV_NONE) {
        s_disp_tick = 0u;
        menu_update_display();
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   Render-хелперы для редактора
   ══════════════════════════════════════════════════════════════════════════ */
static void put_centered(char *buf, const char *utf8)
{
    /* Сначала переведём UTF-8 → Win-1251 в темп.буфер фиксированной длины,
       чтобы корректно центрировать по символам, а не по байтам.            */
    char tmp[21];
    lcd_utf8_to_win1251(utf8, tmp, 20u);
    /* tmp заполнен пробелами справа. Подсчитаем содержательную длину. */
    uint8_t len = 20u;
    while (len > 0u && tmp[len - 1u] == ' ') len--;

    for (uint8_t i = 0u; i < 20u; i++) buf[i] = ' ';
    if (len >= 20u) {
        for (uint8_t i = 0u; i < 20u; i++) buf[i] = tmp[i];
    } else {
        uint8_t off = (uint8_t)((20u - len) / 2u);
        for (uint8_t i = 0u; i < len; i++) buf[off + i] = tmp[i];
    }
}

static char *render_value(char *p, uint8_t blink_phase)
{
    char vbuf[8];
    uint8_t vlen = 0u;

    if (s_edit_kind == EDIT_KIND_FLOAT) {
        char *q = vbuf;
        if (s_edit_dec >= 2u)      q = fmt_f2(q, s_edit_float);
        else if (s_edit_dec == 1u) q = fmt_f1(q, s_edit_float);
        else {
            float fv = s_edit_float;
            if (fv < 0.0f)    fv = 0.0f;
            if (fv > 9999.0f) fv = 9999.0f;
            q = fmt_u16_4(q, (uint16_t)(fv + 0.5f));
        }
        vlen = (uint8_t)(q - vbuf);
    } else if (s_edit_kind == EDIT_KIND_UINT) {
        char *q = vbuf;
        q = fmt_u8_3(q, s_edit_uint);
        vlen = (uint8_t)(q - vbuf);
    }

    if (s_edit_total_dig > 0u && !blink_phase) {
        uint8_t pos = digit_to_char_pos(s_edit_digit, s_edit_int_dig, s_edit_dec);
        if (s_edit_kind == EDIT_KIND_FLOAT && s_edit_dec == 0u && vlen > s_edit_int_dig) {
            pos = (uint8_t)(pos + (vlen - s_edit_int_dig));
        }
        if (pos < vlen) vbuf[pos] = '_';
    }

    for (uint8_t i = 0u; i < vlen; i++) *p++ = vbuf[i];
    return p;
}

/* ══════════════════════════════════════════════════════════════════════════
   menu_update_display
   ══════════════════════════════════════════════════════════════════════════ */
void menu_update_display(void)
{
    char  line[24];
    char *p;

    switch (s_state) {

    case MENU_POWER_ON:
        p = line;
        p = put(p, "    МКВП-02 v");
        p = fmt_f1(p, FW_VERSION_F);
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write_w1251(0, line);
        lcd_mb_write(1, S_INIT);
        break;

    case MENU_STANDBY:
        p = line;
        p = put(p, " МКВП-02  ");
        p = fmt_time(p, (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ));
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write_w1251(0, line);
        lcd_mb_write(1, S_STANDBY_BTN);
        break;

    case MENU_MAIN: {
        uint8_t sr      = MB_ReadBits(MB_ADDR_MODE_SR);
        uint8_t is_man  = (sr & MB_BIT_MODE_MANUAL)    ? 1u : 0u;
        uint8_t is_wk   = (sr & MB_BIT_MODE_WORK)      ? 1u : 0u;
        uint8_t is_alm  = (sr & MB_BIT_MODE_ALARM)     ? 1u : 0u;
        uint8_t is_emrg = (sr & MB_BIT_MODE_EMERGENCY) ? 1u : 0u;
        float   sp      = MB_ReadFloat(MB_ADDR_SETPOINT);
        float   flow    = MB_ReadFloat(MB_ADDR_FLOW);

        if (is_emrg && s_blink_on) {
            lcd_mb_write(0, "  ## EMERGENZA ##   ");
        } else if (s_boost_tick > 0u) {
            lcd_mb_write(0, S_HINT_BOOST);
        } else {
            p = line;
            p = put(p, "МКВП-02 ");
            if (is_alm && s_blink_on)      p = put(p, "АВАРИЯ");
            else if (is_alm)               p = put(p, "      ");
            else if (is_man)               p = put(p, "РУЧ   ");
            else                           p = put(p, "АВТО  ");
            p = put(p, " ");
            if (is_wk) p = put(p, "РАБОТА");
            else       p = put(p, "СТОП  ");
            while (p < line + 20) *p++ = ' ';
            *p = '\0';
            lcd_mb_write_w1251(0, line);
        }

        p = line;
        if (!is_man) {
            p = put(p, "У:");
            p = fmt_f2(p, sp);
            p = put(p, " Т:");
            p = fmt_f2(p, flow);
            p = put(p, " м/с");
        } else {
            p = put(p, "Вых:");
            p = fmt_u8_3(p, MB_ReadBits(MB_ADDR_MANUAL_OUT));
            p = put(p, "% Т:");
            p = fmt_f2(p, flow);
            p = put(p, " м/с");
        }
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write_w1251(1, line);
        break;
    }

    case MENU_ALARM: {
        if (s_blink_on) lcd_mb_write(0, S_ALARM_HDR);
        else            lcd_mb_blank(0);

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
        if      (shown == MB_ALARM_FLOW_LOW)  p = put(p, "Мало потока!");
        else if (shown == MB_ALARM_INVERTER)  p = put(p, "Авар.инвертора!");
        else if (shown == MB_ALARM_DOOR_OPEN) p = put(p, "Дверь открыта!");
        else                                  *p++ = '?';
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write_w1251(1, line);
        break;
    }

    case MENU_SETTINGS:
    case MENU_SETTINGS_P2:
    case MENU_SETTINGS_P3:
    case MENU_SETTINGS_P4: {
        uint8_t p_idx = list_pass_idx(s_state);
        uint8_t n     = PASSES[p_idx].n;
        if (s_pass_list_idx >= n) s_pass_list_idx = 0u;

        p = line;
        p = put(p, " PASS");
        *p++ = (char)('1' + p_idx);
        p = put(p, "  [");
        *p++ = (char)('1' + s_pass_list_idx);
        *p++ = '/';
        *p++ = (char)('0' + n);
        *p++ = ']';
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write_w1251(0, line);

        const char *name = param_name(PASSES[p_idx].seq[s_pass_list_idx]);
        put_centered(line, name);
        line[20] = '\0';
        if (n > 1u) {
            line[0]  = '<';
            line[19] = '>';
        }
        lcd_mb_write_w1251(1, line);
        break;
    }

    case MENU_PASSWORD:
        if (s_pw_attempts > 0u) lcd_mb_write(0, S_PWD_BAD);
        else                    lcd_mb_write(0, S_PWD_HDR);

        p = line;
        for (uint8_t k = 0u; k < 6u; k++) *p++ = ' ';
        for (uint8_t k = 0u; k < 4u; k++) {
            char d = (char)('0' + s_pw_digits[k]);
            if (k == s_pw_field && !s_blink_on) d = '_';
            *p++ = d;
        }
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write_w1251(1, line);
        break;

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
        lcd_mb_write(0, s_edit_hdr ? s_edit_hdr : "                    ");

        p = line;
        if (s_edit_kind == EDIT_KIND_TOGGLE) {
            p = put(p, "Режим: ");
            if (s_edit_tog_lbl == TOG_MEM_NOR) {
                if (s_edit_uint == MB_MANUAL_MEM) p = put(p, "ПАМЯТЬ");
                else                              p = put(p, "НОРМ");
            } else {
                if (s_edit_uint) p = put(p, "ВКЛ");
                else             p = put(p, "ВЫКЛ");
            }
        } else {
            p = put(p, "Знач: ");
            p = render_value(p, s_blink_on);
            *p++ = ' ';
            const char *u = s_edit_unit;
            /* единицы — UTF-8 строка, прогоняем через put */
            p = put(p, u);
        }
        while (p < line + 20) *p++ = ' ';
        *p = '\0';
        lcd_mb_write_w1251(1, line);
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
