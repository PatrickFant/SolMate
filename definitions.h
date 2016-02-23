/*
 * definitions.h
 */

#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

// Status leds
#define LED_MSP BIT0 // P1.0
#define LED_MSP_2 BIT7 // P4.7

#define FLOATSWITCH_0 BIT2 // P1.2
#define FLOATSWITCH_1 BIT3 // P1.3
#define FLOATSWITCH_2 BIT4 // P1.4
#define PUMP_CONTROL BIT2 // P6.2
#define SOLARPANEL_CONTROL BIT3 // P6.3

// ADC pins
#define ADC_PIN_BAT_CHARGE BIT0 // P6.0
#define ADC_PIN_SOLARPANEL_VOLTAGE BIT1 // P6.1

// Pins for receive (rx) and transmit (tx)
#define UART_PIN_RX BIT4 // P3.4
#define UART_PIN_TX BIT3 // P3.3

#define MAX_SMS_INDEX_DIGITS 5 // sms index can have up to 5 digits (99999)

#endif /* DEFINITIONS_H_ */
