#include "msp430f5529.h"
#include "definitions.h"

/*
 * adc.h
 */

#ifndef ADC_H_
#define ADC_H_

// Initialize the ADC
void adc_initialize();

// Run a conversion
__inline void adc_start_conversion();

#endif /* ADC_H_ */
