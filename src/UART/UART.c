#include "UART.h"

void uart_init(void)
{
    rcu_periph_clock_enable(RCU_U);
    rcu_periph_clock_enable(RCU_USART0);

    gpio_init(PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, PIN_OUT);

    gpio_init(PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, PIN_IN);

    usart_deinit(USART0);

    usart_baudrate_set(USART0, TWICH_SPEED);
    usart_word_length_set(USART0, WORD_LENGHT);
    usart_stop_bit_set(USART0, STOP_BIT);
    usart_parity_config(USART0, USART_PM_NONE);

    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);

    usart_interrupt_enable(USART0, USART_INT_RBNE);
    nvic_irq_enable(USART0_IRQn, 2, 0);

    usart_enable(USART0);
}

void USART0_IRQHandler(void)
{
    if(usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET)
    {
        uint8_t data = usart_data_receive(USART0);

        if(rx_index < UART_BUF_SIZE)
        {
            rx_buf[rx_index++] = data;
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
    uint16_t len = rx_index;

    for(uint16_t i=0;i<len;i++)
    {
        buf[i] = rx_buf[i];
    }

    rx_index = 0;
    packet_ready = 0;

    return len;
}

void UART_Transmit(uint8_t *data, uint16_t len)
{
    for(uint16_t i=0;i<len;i++)
    {
        while(usart_flag_get(USART0, USART_FLAG_TBE) == RESET);
        usart_data_transmit(USART0, data[i]);
    }
}
