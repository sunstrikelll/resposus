#include "..\include\PWM\PWM.h"

void pwm_init(void) 
{

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_TIMER2);

    gpio_init(PWM_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, PWM_PIN);

    timer_oc_parameter_struct timer_oc_initpara;
    timer_parameter_struct timer_initpara;

    timer_deinit(PWM_TIMER);

    timer_initpara.prescaler         = 72 - 1;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = 1000 - 1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(PWM_TIMER, &timer_initpara);

    timer_oc_initpara.ocpolarity  = TIMER_OC_POLARITY_HIGH;
    timer_oc_initpara.outputstate = TIMER_CCX_ENABLE;
    timer_oc_init(PWM_TIMER, PWM_CHANNEL, &timer_oc_initpara);

    timer_channel_output_mode_config(PWM_TIMER, PWM_CHANNEL, TIMER_OC_MODE_PWM1);
    timer_channel_output_shadow_config(PWM_TIMER, PWM_CHANNEL, TIMER_OC_SHADOW_DISABLE);

    timer_channel_output_pulse_value_set(PWM_TIMER, PWM_CHANNEL, 0);

    timer_enable(PWM_TIMER);
}

void pwm_setVoltage(uint8_t percent) 
{
    uint32_t arr_value = TIMER_CAR(PWM_TIMER);
    uint32_t pulse = (percent * (timer_parameter_read(PWM_TIMER, arr_value) + 1)) / 100;
    timer_channel_output_pulse_value_set(PWM_TIMER, PWM_CHANNEL, pulse);
}