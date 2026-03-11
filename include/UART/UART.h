#ifndef UART
#define UART

#include <stdint.h>
#include "..\..\GD_libraries\CMSIS\DeviceSupport\gd32f10x.h"
#include <stdio.h>

#define UART_BUF_SIZE 256
#define RCU_U             RCU_GPIOA
#define PORT            GPIOA
#define PIN_IN          GPIO_PIN_10
#define PIN_OUT         GPIO_PIN_9
#define TWICH_SPEED     115200
#define WORD_LENGHT     USART_WL_8BIT
#define STOP_BIT        USART_STB_1BIT

static volatile uint8_t rx_buf[UART_BUF_SIZE];
static volatile uint16_t rx_index = 0;

static volatile uint8_t packet_ready = 1;
static volatile uint32_t silence_timer = 0;

void uart_init(void);
void USART0_IRQHandler(void);
uint8_t uart_getReadyFlag(void);
uint16_t UART_Receive(uint8_t *buf);
void UART_Transmit(uint8_t *data, uint16_t len);


#endif