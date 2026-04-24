#include "modbus_table.h"

#include "FreeRTOS.h"
#include "task.h"

uint8_t mb_table[MB_TABLE_SIZE];

/* ══════════════════════════════════════════════════════════════════════════
   Строковые операции — см. подробности в modbus_table.h
   ══════════════════════════════════════════════════════════════════════════
   Внутри 16-битного Modbus-регистра символы отображаются мастером как
   «low byte first», поэтому при записи в mb_table меняем местами байты
   в каждой паре (addr — чётный):

       str[0] → mb_table[addr + 1]
       str[1] → mb_table[addr + 0]
       str[2] → mb_table[addr + 3]
       str[3] → mb_table[addr + 2]
       …

   Индекс получается через (i ^ 1u) — младший бит инвертируется. Работает
   ровно при чётном addr (проверено: все MB_ADDR_*_LINE_* чётные).

   MB_WriteString заполняет ВСЕ MB_STRING_LEN байт блока (пустое место —
   нулями) — это важно, чтобы в не-пишущих парах не остался мусор, иначе
   мастер увидит «фантомный» символ. Из-за swap-порядка финальный '\0'
   может лежать на dst[addr+20] либо на dst[addr+21] — оба гарантированно
   сбрасываются во втором цикле.

   Критическая секция нужна, чтобы Modbus-задача (приоритет выше UI) не
   прочитала буфер в момент, когда часть пар уже свопнута, а часть — нет.
*/

void MB_WriteString(uint16_t addr, const char *str)
{
    taskENTER_CRITICAL();
    {
        uint8_t *dst = &mb_table[addr];
        uint8_t  i   = 0;

        /* Копируем максимум MB_STRING_LEN-1 символов, пока не встретим '\0'. */
        while (str[i] && i < (MB_STRING_LEN - 1u)) {
            dst[i ^ 1u] = (uint8_t)str[i];
            i++;
        }
        /* Добиваем нулями (включая финальный байт-терминатор). */
        while (i < MB_STRING_LEN) {
            dst[i ^ 1u] = 0;
            i++;
        }
    }
    taskEXIT_CRITICAL();
}

/* Возвращает в out[] Си-строку из mb_table, отменяя swap пар байтов.
   Результат всегда '\0'-терминирован (если max_len > 0).

   Копируем под критической секцией — чтобы не словить частично обновлённый
   буфер, если master одновременно пишет WRITE_MULTIPLE_REGISTERS.         */
void MB_ReadString(uint16_t addr, char *out, uint8_t max_len)
{
    if (max_len == 0u) return;

    taskENTER_CRITICAL();
    {
        const uint8_t *src = &mb_table[addr];
        uint8_t        i   = 0;
        uint8_t        n   = (max_len < MB_STRING_LEN) ? max_len : MB_STRING_LEN;

        while (i < (uint8_t)(n - 1u)) {
            uint8_t b = src[i ^ 1u];
            out[i++] = (char)b;
            if (b == 0u) break;
        }
        out[i] = '\0';
    }
    taskEXIT_CRITICAL();
}
