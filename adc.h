#include "msp430f5529.h"

/*
 * adc.h
 */

#ifndef ADC_H_
#define ADC_H_

#define ADC_PIN_WATERDEPTH BIT0 // P6.0
#define ADC_PIN_BAT_CHARGE BIT1 // P6.1
#define ADC_PIN_SOLARPANEL_VOLTAGE BIT4 // P6.4

// Initialize the ADC
void adc_initialize();

// Run a conversion
__inline void adc_start_conversion();

#endif /* ADC_H_ */
