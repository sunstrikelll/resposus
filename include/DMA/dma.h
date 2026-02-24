#ifndef ADC_H
#define ADC_H

#include "gd32f10x.h"

void adc_init(void);
uint16_t adc_getValue(void);

#endif