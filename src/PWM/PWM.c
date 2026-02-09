#include "..\include\PWM\PWM.h"

#define PROCPERIOD         1000
#define PROCMGFREQ         72
#define GPIO_CLK           RCU_GPIOA
#define TIMER_GPIO_CLK     RCU_TIMER4
#define FOR_TO_PRECENT     100


void pwm_init(void) 
{

    rcu_periph_clock_enable(GPIO_CLK);
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
    rcu_periph_clock_enable(TIMER_GPIO_CLK);

    timer_deinit(TIMER4);
    timer_oc_parameter_struct timer_ocinitpara;
    timer_parameter_struct timer_initpara;

    timer_initpara.prescaler         = PROCMGFREQ - 1;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = PROCPERIOD - 1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER4, &timer_initpara);

    timer_channel_output_struct_para_init(&timer_ocinitpara);
    timer_ocinitpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocinitpara.outputnstate = TIMER_CCXN_DISABLE;
    timer_ocinitpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocinitpara.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    timer_ocinitpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_ocinitpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;
    
    timer_channel_output_config(TIMER4, TIMER_CH_0, &timer_ocinitpara);

    timer_channel_output_mode_config(TIMER4, TIMER_CH_0, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER4, TIMER_CH_0, TIMER_OC_SHADOW_DISABLE);

    timer_channel_output_pulse_value_config(TIMER4, PWM_CHANNEL, 0);

    timer_oc_shadow_enable(TIMER4);

    timer_enable(TIMER4);
}

void pwm_setVoltage(uint8_t percent) 
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    uint32_t pulse = (percent * PROCPERIOD) / FOR_TO_PRECENT;
    timer_channel_output_pulse_value_config(TIMER4, TIMER_CH_0, pulse);
}