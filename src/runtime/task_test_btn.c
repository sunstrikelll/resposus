/*  task_test_btn.c — TEST-режим: запись кода кнопки в btn_event
 *                     + визуальная диагностика светодиодами.
 *
 *  Номер нажатой кнопки (1…6) выводится в ДВОИЧНОМ виде на 4 LED.
 *  Младший разряд (bit 0) — PC6, старший (bit 3) — PC9.
 *  LED горят пока кнопка физически удерживается (чтение GPIO
 *  напрямую, без антидребезга — нужно «сырое» поведение).
 *
 *       №  |  бин   | LED9 LED8 LED7 LED6
 *     ─────┼────────┼────────────────────
 *      PRG       1  |  0001  |  0    0    0    1
 *      ONOFF     2  |  0010  |  0    0    1    0
 *      AUTO_MAN  3  |  0011  |  0    0    1    1
 *      ▲ UP      4  |  0100  |  0    1    0    0
 *      ▼ DOWN    5  |  0101  |  0    1    0    1
 *      MUTE      6  |  0110  |  0    1    1    0
 *
 *  При одновременном нажатии нескольких кнопок на LED выводится
 *  младшая по индексу (приоритет PRG > ONOFF > AUTO/MAN > UP > DOWN > MUTE).
 *
 *  Параллельно debounced-событие кнопки пишется в Modbus-регистр
 *  MB_ADDR_BTN_EVENT для внешнего теста через MBU3.
 */

#include "task_test_btn.h"

#include "FreeRTOS.h"
#include "task.h"

#include "gd32f10x.h"
#include "buttons.h"
#include "modbus_table.h"

/* ── Светодиоды (та же разводка, что в production-меню) ───────────────── */
#define LED_PORT        GPIOC
#define LED_RCU         RCU_GPIOC
#define LED_PIN_POWER   GPIO_PIN_6
#define LED_PIN_WORK    GPIO_PIN_7
#define LED_PIN_ALARM   GPIO_PIN_8
#define LED_PIN_MANUAL  GPIO_PIN_9
#define LED_PIN_ALL     (LED_PIN_POWER | LED_PIN_WORK | \
                         LED_PIN_ALARM | LED_PIN_MANUAL)

static void leds_init(void)
{
    rcu_periph_clock_enable(LED_RCU);
    gpio_init(LED_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_2MHZ, LED_PIN_ALL);
    gpio_bit_reset(LED_PORT, LED_PIN_ALL);
}

/* Включить только указанную маску, остальные погасить */
static void leds_apply(uint32_t on_mask)
{
    uint32_t off_mask = LED_PIN_ALL & ~on_mask;
    if (off_mask) gpio_bit_reset(LED_PORT, off_mask);
    if (on_mask)  gpio_bit_set  (LED_PORT, on_mask);
}

/* «Сырое» состояние кнопки (без debounce) теперь даёт сам модуль
   buttons — он знает порт каждой кнопки (GPIOB и GPIOD разнесены). */

static void task_test_btn(void *arg)
{
    TickType_t xLastWake = xTaskGetTickCount();
    (void)arg;

    leds_init();

    for (;;)
    {
        /* --- 1) светодиоды: двоичное представление номера кнопки -------- */
        uint8_t btn_num = 0u;   /* 0 = ни одна не нажата */

        /* Приоритет: первая обнаруженная нажатая кнопка */
        if      (btn_is_down_idx(BTN_IDX_PRG))      btn_num = 1u;
        else if (btn_is_down_idx(BTN_IDX_ONOFF))    btn_num = 2u;
        else if (btn_is_down_idx(BTN_IDX_AUTO_MAN)) btn_num = 3u;
        else if (btn_is_down_idx(BTN_IDX_UP))       btn_num = 4u;
        else if (btn_is_down_idx(BTN_IDX_DOWN))     btn_num = 5u;
        else if (btn_is_down_idx(BTN_IDX_MUTE))     btn_num = 6u;

        /* btn_num в двоичном виде на 4 LED (LSB = PC6) */
        uint32_t mask = 0u;
        if (btn_num & 0x01u) mask |= LED_PIN_POWER;   /* bit 0 → PC6 */
        if (btn_num & 0x02u) mask |= LED_PIN_WORK;    /* bit 1 → PC7 */
        if (btn_num & 0x04u) mask |= LED_PIN_ALARM;   /* bit 2 → PC8 */
        if (btn_num & 0x08u) mask |= LED_PIN_MANUAL;  /* bit 3 → PC9 */

        leds_apply(mask);

        /* --- 2) debounced события — в Modbus-регистр для MBU3 ---------- */
        BtnEvent_t ev = btn_scan();
        if (ev != BTN_EV_NONE)
            MB_WriteBits(MB_ADDR_BTN_EVENT, (uint8_t)ev);

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(BTN_SCAN_MS));
    }
}

void task_test_btn_start(void)
{
    xTaskCreate(task_test_btn, "tst_btn", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
}
