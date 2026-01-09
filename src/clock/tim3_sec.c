#include "..\include\clock\tim3_sec.h"

static volatile uint32_t delay;

/*!
    \brief      configure systick
    \param[in]  none
    \param[out] none
    \retval     none
*/

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

/*!
    \brief      delay a time in milliseconds
    \param[in]  count: count in milliseconds
    \param[out] none
    \retval     none
*/

void tim_delay(uint32_t sec)
{
    delay = sec;

    while(0U != delay){
    }
}

/*!
    \brief      delay decrement
    \param[in]  none
    \param[out] none
    \retval     none
*/

uint32_t tim_getTime(void)
{
    return delay;
}

void delay_decrement(void)
{
    if (0U != delay){
        delay--;
    }
}
