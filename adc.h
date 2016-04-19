#include "msp430f5529.h"
#include "definitions.h"

/*
 * adc.h
 */

#ifndef ADC_H_
#define ADC_H_

// Bat: 228 -> 12.9v
//      210 -> 12.55v
//      190 -> 12.2v
//      170 -> 11.85

// For solar panel: min 0.5V -> 0.5/3.3 * 256 = 39
//                  max 2.4V -> 2.4/3.3 * 256 = 186

// Initialize the ADC
void adc_initialize();

// Run a conversion
__inline void adc_start_conversion();

#endif /* ADC_H_ */
