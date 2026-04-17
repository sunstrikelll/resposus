/*  main.c — точка входа БУСШ 02.101.01
 *
 *  Последовательность:
 *    1) Инициализация периферии (NVIC, таймеры, USB CDC, Modbus, EEPROM)
 *    2) Самотест EEPROM (блокирующий, ~120 мс + 3 с индикация на LED_POWER)
 *    3) Выбор режима рантайма:
 *         • RUNTIME_PRODUCTION — штатный режим (меню, кнопки, светодиоды)
 *         • RUNTIME_TEST       — тест вывода LCD (4 регистра test_line_N)
 *       Значение берётся из регистра MB_ADDR_RUNTIME_MODE, если оно
 *       валидно (0 или 1), иначе используется RUNTIME_MODE_DEFAULT.
 *    4) runtime_start(mode)  — создаёт набор FreeRTOS-задач
 *    5) vTaskStartScheduler()
 */

#include "gd32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

#include "tim3_ms.h"
#include "usb_cdc.h"
#include "modbus.h"
#include "modbus_table.h"
#include "led.h"
#include "eeprom.h"
#include "runtime.h"

/* ── Режим по умолчанию, если в регистре RUNTIME_MODE что-то «не то» ──
   Чтобы переключиться на тест вывода, достаточно раскомментировать
   вторую строку и закомментировать первую.                              */
#define RUNTIME_MODE_DEFAULT  RUNTIME_PRODUCTION
/* #define RUNTIME_MODE_DEFAULT  RUNTIME_TEST */

/* led1 остаётся только для индикации результата самотеста EEPROM.
   После самотеста управление LED_POWER (PC6) переходит к menu_init()
   (в PRODUCTION) или никому (в TEST — светодиоды не используются).     */
static Led led1 = { .pin = GPIO_PIN_6, .port = GPIOC, .rcu_periph = RCU_GPIOC };

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

/* ─── resolve_runtime_mode ──────────────────────────────────────────────────
   Читает MB_ADDR_RUNTIME_MODE. Если в регистре уже лежит 0 или 1 — берём
   его (так внешний мастер может заранее «попросить» конкретный режим).
   Иначе ставим дефолт.                                                      */
static RuntimeMode_t resolve_runtime_mode(void)
{
    uint8_t m = MB_ReadBits(MB_ADDR_RUNTIME_MODE);
    if (m == (uint8_t)RUNTIME_PRODUCTION) return RUNTIME_PRODUCTION;
    if (m == (uint8_t)RUNTIME_TEST)       return RUNTIME_TEST;
    return RUNTIME_MODE_DEFAULT;
}

/* ─── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    tim_Init();
    usb_cdc_init();
    modbus_init();
    LED_Init(&led1);    /* только для теста EEPROM при старте */
    eeprom_init();

    /* Начальное значение version = 1.0 */
    MB_WriteFloat(MB_ADDR_VERSION, 1.0f);

    /* Тест EEPROM (до планировщика — блокирующий, ~120 мс)
       LED_POWER (PC6) горит = OK, не горит = ошибка EEPROM             */
    eeprom_test();
    tim_delay(3000);    /* 3 с визуальная индикация результата теста */
    LED_Off(&led1);     /* управление LED_POWER переходит к menu_init (в PRODUCTION) */

    /* Выбор режима и запуск задач */
    RuntimeMode_t mode = resolve_runtime_mode();
    runtime_start(mode);

    vTaskStartScheduler();

    for (;;) {}
}
