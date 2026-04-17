/*  task_timer.c — счётчик секунд + обработка RESET_TIMER
 *
 *  Каждые 50 мс:
 *    • Если выставлен бит MB_BIT_CR_RESET_TIMER в bit_cr —
 *      обнуляет MB_ADDR_TIMER и сбрасывает бит.
 *    • Раз в 20 тиков (1 с) инкрементирует 32-битный счётчик в регистре.
 */

#include "task_timer.h"

#include "FreeRTOS.h"
#include "task.h"

#include "modbus_table.h"

static void task_timer(void *arg)
{
    TickType_t xLastWake  = xTaskGetTickCount();
    uint8_t    tick_count = 0;
    (void)arg;

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

void task_timer_start(void)
{
    xTaskCreate(task_timer, "timer", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}
