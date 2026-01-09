#include "..\include\clock\tim3_sec.h"

static volatile uint32_t tick_counter = 0;

void tim_Init(void)
{
    timer_parameter_struct tpara;

    rcu_periph_clock_enable(RCU_TIMER3);
    timer_deinit(TIMER3);

    timer_struct_para_init(&tpara);
    tpara.prescaler         = SystemCoreClock / 1000 - 1; // 1 kHz
    tpara.alignedmode       = TIMER_COUNTER_EDGE;
    tpara.counterdirection = TIMER_COUNTER_UP;
    tpara.period            = 1000 - 1;                   // 1 second
    tpara.clockdivision     = TIMER_CKDIV_DIV1;

    timer_init(TIMER3, &tpara);

    timer_interrupt_enable(TIMER3, TIMER_INT_UP);
    nvic_irq_enable(TIMER3_IRQn, 0, 0);   // ← ВАЖНО

    timer_enable(TIMER3);
}

void tim_delay(uint32_t sec)
{
    uint32_t start_time = tim_getTime();
    while ((tim_getTime() - start_time) < sec)
    {
        __NOP(); // чтобы компилятор не сделал "умную" оптимизацию
    }
}

uint32_t tim_getTime(void)
{
    return tick_counter;
}

void TIMER3_IRQHandler(void)
{
    if (timer_interrupt_flag_get(TIMER3, TIMER_INT_FLAG_UP) == SET)
    {
        timer_interrupt_flag_clear(TIMER3, TIMER_INT_FLAG_UP);
        tick_counter++;
    }
}
