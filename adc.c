#include "adc.h"

/*
 * adc.c
 */

// Set up the analog to digital converter
void adc_initialize()
{
	ADC_PORT_SEL |= ADC_PIN_BAT_CHARGE | ADC_PIN_SOLARPANEL_VOLTAGE; // Set up pins

	// Set up ADC //
	ADC12CTL0 = ADC12ON | ADC12MSC | ADC12SHT0_2; // Turn on ADC, enable multiple samples, set speed
	ADC12CTL1 = ADC12SHP | ADC12CONSEQ_1; // Sequence-of-channels mode (no repeat)
	ADC12CTL2 = ADC12RES_0; // 8 bit resolution

	ADC12MCTL0 = ADC12INCH_0; // reference Vcc and Vss, channel is A0
	ADC12MCTL1 = ADC12INCH_1 | ADC12EOS; // reference Vcc and Vss, channel is A1, end of sequence

	ADC12IE = ADC12IFG1; // Enable interrupts for mctl1
	// (we check all of them when the last one is done)

	ADC12CTL0 |= ADC12ENC; // Enable conversions
}

// Do the conversions
__inline void adc_start_conversion()
{
	// Start the conversion if not busy
	if(!(ADC12CTL1 & ADC12BUSY))
		ADC12CTL0 |= ADC12SC;
}
