#include "modbus_table.h"

#include "FreeRTOS.h"
#include "task.h"

uint8_t mb_table[MB_TABLE_SIZE];

/* ── MB_ReadString ────────────────────────────────────────────────────────
   Возвращает указатель на буфер строки внутри mb_table. Читатель видит
   22 байта; nul-терминатор гарантируется MB_WriteString (последний байт
   всегда 0 — зона длиной MB_STRING_LEN-1 + '\0').

   Сам указатель получается без критической секции, но если вызывающая
   сторона копирует по нему символы, то для полной согласованности лучше
   копировать по 20 байт в локальный буфер под taskENTER_CRITICAL().      */
const char *MB_ReadString(uint16_t addr)
{
    return (const char *)&mb_table[addr];
}

/* ── MB_WriteString ────────────────────────────────────────────────────────
   Атомарная запись строки (≤21 символ) в mb_table[addr .. addr+21].
   Завершающий '\0' гарантирован (байт addr+21 = 0).

   Критическая секция нужна, чтобы Modbus-задача (приоритет выше UI) не
   могла прочитать буфер между memset и strncpy — иначе master видит
   «огрызок» строки (часть символов заменена нулями).                     */
void MB_WriteString(uint16_t addr, const char *str)
{
    taskENTER_CRITICAL();
    {
        uint8_t *dst = &mb_table[addr];
        uint8_t  i   = 0;

        /* Копируем максимум MB_STRING_LEN-1 символов, пока не встретим '\0'. */
        while (str[i] && i < (MB_STRING_LEN - 1u)) {
            dst[i] = (uint8_t)str[i];
            i++;
        }
        /* Добиваем нулями (включая финальный байт-терминатор). */
        while (i < MB_STRING_LEN) {
            dst[i] = 0;
            i++;
        }
    }
    taskEXIT_CRITICAL();
}
