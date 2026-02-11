#include "..\include\PWM\PWM.h"

void pwm_init(void) 
{

    rcu_periph_clock_enable(GPIO_CLK);
    gpio_init(PWM_PORT, PWM_MODE, PWM_FREQ, PWM_PIN);
    rcu_periph_clock_enable(TIMER_GPIO_CLK);

    timer_deinit(PWM_TIMER);
    timer_oc_parameter_struct timer_ocinitpara;
    timer_parameter_struct timer_initpara;

    timer_initpara.prescaler         = PROCMGFREQ - 1;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = PROCPERIOD - 1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(PWM_TIMER, &timer_initpara);

    timer_channel_output_struct_para_init(&timer_ocinitpara);
    timer_ocinitpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocinitpara.outputnstate = TIMER_CCXN_DISABLE;
    timer_ocinitpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocinitpara.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    timer_ocinitpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_ocinitpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;
    
    timer_channel_output_config(PWM_TIMER, PWM_CHANNEL, &timer_ocinitpara);

    timer_channel_output_mode_config(PWM_TIMER, PWM_CHANNEL, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(PWM_TIMER, PWM_CHANNEL, TIMER_OC_SHADOW_ENABLE);

    timer_channel_output_pulse_value_config(PWM_TIMER, PWM_CHANNEL, 0);

    timer_auto_reload_shadow_enable(PWM_TIMER);

    timer_enable(PWM_TIMER);
}

void pwm_setVoltage(uint8_t percent) 
{
    if (percent > 100) percent = 100;
    uint32_t pulse = (percent * PROCPERIOD) / FOR_TO_PRECENT;
    timer_channel_output_pulse_value_config(PWM_TIMER, TIMER_CH_0, pulse);
}