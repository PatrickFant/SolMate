#include "msp430f5529.h"
#include "definitions.h"

/*
 * adc.h
 */

#ifndef ADC_H_
#define ADC_H_

// For battery charge: 12.9V -> 3.2V (100%)
//                     12.2V -> 2.8V (12%)
// For solar panel: min 0.5V
//                  max 2.4V

// Initialize the ADC
void adc_initialize();

// Run a conversion
__inline void adc_start_conversion();

#endif /* ADC_H_ */
