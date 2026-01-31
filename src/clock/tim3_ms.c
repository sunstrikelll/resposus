#include "..\include\clock\tim3_ms.h"

static volatile uint32_t delay;

void tim_Init(void)
{
    timer_parameter_struct tpara;

    rcu_periph_clock_enable(RCU_TIMER3);
    timer_deinit(TIMER3);

    timer_struct_para_init(&tpara);
    tpara.prescaler         = 72 - 1;
    tpara.alignedmode       = TIMER_COUNTER_EDGE;
    tpara.counterdirection = TIMER_COUNTER_UP;
    tpara.period            = 1 - 1;                   
    tpara.clockdivision     = TIMER_CKDIV_DIV1;

    timer_init(TIMER3, &tpara);

    timer_interrupt_enable(TIMER3, TIMER_INT_UP);
    nvic_irq_enable(TIMER3_IRQn, 0, 0); 

    timer_enable(TIMER3);

    delay = 0;
}

void tim_delay(uint32_t ms) 
{
    uint32_t start_time = tim_getTime();
    while ((tim_getTime() - start_time) < ms) 
    {
        
    }
}

uint32_t tim_getTime(void)
{
    return delay;
}

