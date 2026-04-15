#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tim3_ms.h"
#include "usb_cdc.h"
#include "modbus.h"
#include "modbus_table.h"
#include "led.h"
#include "lcd_hd44780.h"
#include "eeprom.h"

static Led led1 = { .pin = GPIO_PIN_6, .port = GPIOC, .rcu_periph = RCU_GPIOC };

/* ─── Вспомогательные функции форматирования ────────────────────────────────
   Используются в task_lcd для построения строки без stdlib printf/sprintf.  */

/* Форматировать uint32 правое выравнивание в поле width (пробелы слева).
   Возвращает указатель за последним записанным символом.                    */
static char *fmt_u32_rpad(char *p, uint32_t val, int width)
{
    char tmp[10];
    int  len = 0;
    if (val == 0) {
        tmp[len++] = '0';
    } else {
        uint32_t v = val;
        while (v) { tmp[len++] = (char)('0' + v % 10); v /= 10; }
    }
    /* tmp хранится в обратном порядке */
    int pad = width - len;
    if (pad < 0) pad = 0;
    for (int i = 0; i < pad; i++) *p++ = ' ';
    for (int i = len - 1; i >= 0; i--) *p++ = tmp[i];
    return p;
}

/* Форматировать float с одной цифрой после запятой (например 2.3, 10.7).
   Возвращает указатель за последним записанным символом.                    */
static char *fmt_f1(char *p, float val)
{
    if (val < 0.0f) { *p++ = '-'; val = -val; }
    int32_t i   = (int32_t)val;
    int32_t frac = (int32_t)((val - (float)i) * 10.0f + 0.5f);
    if (frac >= 10) { i++; frac = 0; }

    /* Целая часть */
    char tmp[10]; int len = 0;
    if (i == 0) {
        tmp[len++] = '0';
    } else {
        int32_t v = i;
        while (v) { tmp[len++] = (char)('0' + v % 10); v /= 10; }
    }
    for (int j = len - 1; j >= 0; j--) *p++ = tmp[j];
    *p++ = '.';
    *p++ = (char)('0' + frac);
    return p;
}

/* ─── task_modbus ───────────────────────────────────────────────────────────
   Опрашивает USB CDC, при готовом фрейме — обрабатывает Modbus RTU.        */
static void task_modbus(void *arg)
{
    static uint8_t rx_data[256];
    static uint8_t tx_data[260];

    for (;;)
    {
        if (usb_cdc_getReadyFlag())
        {
            uint16_t rx_len = usb_cdc_receive(rx_data);
            uint16_t tx_len = modbus_process(rx_data, rx_len, tx_data);
            if (tx_len > 0)
                usb_cdc_transmit(tx_data, tx_len);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* ─── task_timer ────────────────────────────────────────────────────────────
   Каждые 50 мс проверяет RESET_TIMER.
   Каждую секунду (20 × 50 мс) инкрементирует счётчик таймера.             */
static void task_timer(void *arg)
{
    TickType_t xLastWake  = xTaskGetTickCount();
    uint8_t    tick_count = 0;

    for (;;)
    {
        /* Сброс таймера по команде из bit_cr */
        if (MB_ReadBits(MB_ADDR_BIT_CR) & MB_BIT_CR_RESET_TIMER)
        {
            MB_WriteUint32(MB_ADDR_TIMER, 0);
            MB_ClearBit(MB_ADDR_BIT_CR, MB_BIT_CR_RESET_TIMER);
        }

        tick_count++;
        if (tick_count >= 20)           /* 20 × 50 мс = 1 с */
        {
            tick_count = 0;
            uint32_t t = MB_ReadUint32(MB_ADDR_TIMER);
            MB_WriteUint32(MB_ADDR_TIMER, t + 1);
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(50));
    }
}

/* ─── task_led ──────────────────────────────────────────────────────────────
   Управляет светодиодом по командам LED_SET / LED_RESET из bit_cr.
   Обновляет бит LED в bit_sr.                                               */
static void task_led(void *arg)
{
    for (;;)
    {
        uint8_t cr = MB_ReadBits(MB_ADDR_BIT_CR);

        if (cr & MB_BIT_CR_LED_SET)
        {
            LED_On(&led1);
            MB_SetBit(MB_ADDR_BIT_SR, MB_BIT_SR_LED);
            MB_ClearBit(MB_ADDR_BIT_CR, MB_BIT_CR_LED_SET);
        }
        if (cr & MB_BIT_CR_LED_RESET)
        {
            LED_Off(&led1);
            MB_ClearBit(MB_ADDR_BIT_SR, MB_BIT_SR_LED);
            MB_ClearBit(MB_ADDR_BIT_CR, MB_BIT_CR_LED_RESET);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ─── task_lcd ──────────────────────────────────────────────────────────────
   Каждые 200 мс обновляет все 4 строки дисплея и копирует их в регистры
   display_line_0..3 в mb_table для чтения через Modbus.

   Строки:
     0  " Тестовое задание"   (фиксированная, Win-1251)
     1  " LED = SET/RESET"    (зависит от состояния светодиода)
     2  <usr_text>            (содержимое регистра usr_text)
     3  "TIM = NNNN v = N.N"  (таймер ≤ 9999, version с 1 дес. знаком)       */

/* " Тестовое задание" в Windows-1251, выровнена до 20 символов пробелами */
static const char s_line0[21] =
    "\x20\xD2\xE5\xF1\xF2\xEE\xE2\xEE\xE5"  /*  Тестовое */
    "\x20\xE7\xE0\xE4\xE0\xED\xE8\xE5"       /*  задание  */
    "\x20\x20\x20";                           /* 3 пробела (итого 20) */

static void task_lcd(void *arg)
{
    char buf[21];
    char *p;

    lcd_init();

    for (;;)
    {
        /* ── Строка 0: фиксированный заголовок ─────────────────────────── */
        lcd_print_win1251_at(0, 0, s_line0);
        MB_WriteString(MB_ADDR_DISPLAY_LINE_0, s_line0);

        /* ── Строка 1: состояние светодиода ─────────────────────────────── */
        {
            const char *s = (MB_ReadBits(MB_ADDR_BIT_SR) & MB_BIT_SR_LED)
                            ? " LED = SET          "
                            : " LED = RESET        ";
            lcd_print_at(1, 0, s);
            MB_WriteString(MB_ADDR_DISPLAY_LINE_1, s);
        }

        /* ── Строка 2: usr_text (Win-1251, дополняется пробелами до 20) ── */
        {
            char line2[21];
            const char *ut = MB_ReadString(MB_ADDR_USR_TEXT);
            int i = 0;
            while (*ut && i < 20) line2[i++] = *ut++;
            while (i < 20)        line2[i++] = ' ';
            line2[20] = '\0';

            lcd_print_win1251_at(2, 0, line2);
            MB_WriteString(MB_ADDR_DISPLAY_LINE_2, line2);
        }

        /* ── Строка 3: "TIM = NNNN v = N.N" ──────────────────────────── */
        {
            /* таймер ограничен 4 цифрами (0–9999) */
            uint32_t t = MB_ReadUint32(MB_ADDR_TIMER) % 10000u;
            float    v = MB_ReadFloat(MB_ADDR_VERSION);

            p = buf;
            /* "TIM = " */
            *p++ = 'T'; *p++ = 'I'; *p++ = 'M'; *p++ = ' '; *p++ = '='; *p++ = ' ';
            /* NNNN — 4 символа, выравнивание по правому краю */
            p = fmt_u32_rpad(p, t, 4);
            /* " v = " */
            *p++ = ' '; *p++ = 'v'; *p++ = ' '; *p++ = '='; *p++ = ' ';
            /* N.N */
            p = fmt_f1(p, v);
            /* дополнить до 20 */
            while (p < buf + 20) *p++ = ' ';
            *p = '\0';

            lcd_print_at(3, 0, buf);
            MB_WriteString(MB_ADDR_DISPLAY_LINE_3, buf);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ─── eeprom_test ───────────────────────────────────────────────────────────
   Тест EEPROM выполняется до запуска планировщика FreeRTOS.

   Фаза 1 — запись тестовых данных:
     buf[  0..100] = 50 + i
     buf[101..399] = 0
     buf[400..508] = 208 - (i - 400)   → 208, 207 … 100
     buf[509]      = 0
   После данных в EEPROM записывается CRC-16 (байты 510-511, little-endian).

   Фаза 2 — чтение и проверка:
     Проверяется CRC, затем байт-в-байт сравниваются прочитанные данные.
     Результат: LED горит  → тест пройден,
                LED не горит → ошибка.                                        */
static void eeprom_test(void)
{
    /* Фаза 1: заполнить буфер */
    static uint8_t wr_buf[EEPROM_DATA_SIZE];   /* 510 байт в .bss (= 0) */

    for (int i = 0; i <= 100; i++)
        wr_buf[i] = (uint8_t)(50 + i);
    /* wr_buf[101..399] = 0  (уже нулевой как static) */
    for (int i = 0; i <= 108; i++)
        wr_buf[400 + i] = (uint8_t)(208 - i);
    /* wr_buf[509] = 0 */

    if (eeprom_write_regs(wr_buf, EEPROM_DATA_SIZE) != 0)
        return;   /* ошибка записи → LED остаётся выключенным */

    /* Фаза 2: проверить CRC и прочитанные данные */
    if (eeprom_check_crc() != 0)
        return;   /* CRC не совпал → тест не пройден */

    static uint8_t rd_buf[EEPROM_DATA_SIZE];
    if (eeprom_read_regs(rd_buf, EEPROM_DATA_SIZE) != 0)
        return;

    for (int i = 0; i < (int)EEPROM_DATA_SIZE; i++) {
        if (rd_buf[i] != wr_buf[i])
            return;   /* несовпадение данных → тест не пройден */
    }

    /* Тест пройден */
    LED_On(&led1);
    MB_SetBit(MB_ADDR_BIT_SR, MB_BIT_SR_LED);
}

/* ─── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    tim_Init();
    usb_cdc_init();
    modbus_init();
    LED_Init(&led1);
    eeprom_init();

    /* Начальное значение version = 1.0 */
    MB_WriteFloat(MB_ADDR_VERSION, 1.0f);

    /* Тест EEPROM (до планировщика — блокирующий, ~120 мс) */
    eeprom_test();
    tim_delay(10000);   /* 10 с: LED горит = OK, не горит = ошибка EEPROM */

    xTaskCreate(task_modbus, "modbus", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(task_timer,  "timer",  configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(task_led,    "led",    configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(task_lcd,    "lcd",    512,                      NULL, 2, NULL);

    vTaskStartScheduler();

    for (;;) {}
}
