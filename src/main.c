#include "gd32f10x.h"
#include "tim3_ms.h"
#include <stdint.h>
#include "PWM.h"
#include "dma.h"
#include "led.h"
#include "UART.h"
#include <string.h>
#include <stdio.h>

#define LED_GPIO_PORT          GPIOB
#define LED_PIN                GPIO_PIN_13
#define LED_GPIO_CLK           RCU_GPIOB

Led led1 = {LED_PIN, LED_GPIO_PORT, LED_GPIO_CLK};


int main(void) 
{
    tim_Init();
    uart_init();

    uint8_t rx_data[256];
    uint8_t tx_data[300];
    while (1)
    {
        if(uart_getReadyFlag())
        {
            uint16_t len = UART_Receive(rx_data);

            sprintf((char*)tx_data,
                    "From MCU to PC: %.*s\r\n",
                    len,
                    rx_data);

            UART_Transmit(tx_data, strlen((char*)tx_data));
        }
    }
    
}


