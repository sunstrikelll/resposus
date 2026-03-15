#include "gd32f10x.h"
#include "tim3_ms.h"
#include "UART.h"
#include "modbus.h"

int main(void)
{
    tim_Init();
    uart_init();
    modbus_init();

    /* Максимальный ответ FC03/FC04: 3 + 125*2 + 2 = 255 байт */
    uint8_t rx_data[256];
    uint8_t tx_data[260];

    while (1)
    {
        if (uart_getReadyFlag())
        {
            uint16_t rx_len = UART_Receive(rx_data);
            uint16_t tx_len = modbus_process(rx_data, rx_len, tx_data);

            if (tx_len > 0)
                UART_Transmit(tx_data, tx_len);
        }
    }
}
