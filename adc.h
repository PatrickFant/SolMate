#include "msp430f5529.h"
#include "definitions.h"

/*
 * adc.h
 */

#ifndef ADC_H_
#define ADC_H_

// For battery charge: 12.9V -> 3.2V (100%) -> 3.2/3.3 * 256 = 248
//                     12.2V -> 2.8V (12%) -> 2.8/3.3 * 256 = 217
// For solar panel: min 0.5V -> 0.5/3.3 * 256 = 39
//                  max 2.4V -> 2.4/3.3 * 256 = 186

// Initialize the ADC
void adc_initialize();

// Run a conversion
__inline void adc_start_conversion();

#endif /* ADC_H_ */
