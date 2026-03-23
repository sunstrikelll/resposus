#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "../../GD_libraries/CMSIS/DeviceSupport/gd32f10x.h"
#include <stdio.h>

#define UART_BUF_SIZE   256
#define SILENCE_MS      3

#define RCU_U           RCU_GPIOA
#define PORT            GPIOA
#define PIN_IN          GPIO_PIN_10
#define PIN_OUT         GPIO_PIN_9
#define BAUD_RATE       115200
#define WORD_LENGTH     USART_WL_8BIT
#define STOP_BIT        USART_STB_1BIT

<<<<<<< Updated upstream
/* Переменные определены в UART.c, extern — чтобы избежать дублирования копий */
extern volatile uint8_t  rx_buf[UART_BUF_SIZE];
extern volatile uint16_t rx_index;
extern volatile uint8_t  packet_ready;
extern volatile uint32_t silence_timer;

=======
>>>>>>> Stashed changes
void uart_init(void);
void uart_tick(void);
void USART0_IRQHandler(void);
uint8_t uart_getReadyFlag(void);
uint16_t UART_Receive(uint8_t *buf);
void UART_Transmit(uint8_t *data, uint16_t len);

#endif
