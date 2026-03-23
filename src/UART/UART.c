#include "UART.h"

/* Единственное определение переменных модуля */
<<<<<<< Updated upstream
<<<<<<< Updated upstream
volatile uint8_t  rx_buf[UART_BUF_SIZE];
volatile uint16_t rx_index    = 0;
volatile uint8_t  packet_ready = 0;
volatile uint32_t silence_timer = 0;
=======
=======
>>>>>>> Stashed changes
static volatile uint8_t  rx_buf[UART_BUF_SIZE];
static volatile uint16_t rx_index    = 0;
static volatile uint8_t  packet_ready = 0;
static volatile uint32_t silence_timer = 0;
<<<<<<< Updated upstream
>>>>>>> Stashed changes
=======
>>>>>>> Stashed changes

void uart_init(void)
{
    rcu_periph_clock_enable(RCU_U);
    rcu_periph_clock_enable(RCU_USART0);

    gpio_init(PORT, GPIO_MODE_AF_PP,      GPIO_OSPEED_50MHZ, PIN_OUT);
    gpio_init(PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, PIN_IN);

    usart_deinit(USART0);

    usart_baudrate_set(USART0, BAUD_RATE);
    usart_word_length_set(USART0, WORD_LENGTH);
    usart_stop_bit_set(USART0, STOP_BIT);
    usart_parity_config(USART0, USART_PM_NONE);

    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);

    usart_interrupt_enable(USART0, USART_INT_RBNE);
    nvic_irq_enable(USART0_IRQn, 2, 0);

    usart_enable(USART0);
}

/* Вызывается из TIMER3_IRQHandler каждые 1 мс */
void uart_tick(void)
{
    if (rx_index > 0)
    {
        silence_timer++;
        if (silence_timer >= SILENCE_MS)
        {
            packet_ready  = 1;
            silence_timer = 0;
        }
    }
}

void USART0_IRQHandler(void)
{
    /* Сброс флагов ошибок (overrun, noise, framing): чтение DATA-регистра
       после чтения STATUS-регистра сбрасывает эти флаги на GD32F10x */
    if (usart_flag_get(USART0, USART_FLAG_ORERR) != RESET ||
        usart_flag_get(USART0, USART_FLAG_NERR)  != RESET ||
        usart_flag_get(USART0, USART_FLAG_FERR)  != RESET)
    {
        (void)usart_data_receive(USART0);
        return;
    }

    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET)
    {
        uint16_t data = usart_data_receive(USART0);

        if (rx_index < UART_BUF_SIZE)
        {
            rx_buf[rx_index++] = (uint8_t)data;
        }

        silence_timer = 0;
    }
}

uint8_t uart_getReadyFlag(void)
{
    return packet_ready;
}

uint16_t UART_Receive(uint8_t *buf)
{
    /* Атомарное чтение: запрещаем прерывания на время копирования буфера */
    __disable_irq();

    uint16_t len = rx_index;
    for (uint16_t i = 0; i < len; i++)
    {
        buf[i] = rx_buf[i];
    }
    rx_index      = 0;
    packet_ready  = 0;
    silence_timer = 0;

    __enable_irq();

    return len;
}

void UART_Transmit(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        while (usart_flag_get(USART0, USART_FLAG_TBE) == RESET);
        usart_data_transmit(USART0, data[i]);
    }
}
