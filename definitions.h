/*
 * definitions.h
 */

#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

// Status leds
#define LED_MSP BIT0 // P1.0
#define LED_MSP_2 BIT7 // P4.7

#define INPUT_FLOATSWITCH BIT3 // P6.3
#define LED_PUMP BIT2 // P6.2

// Pins for receive (rx) and transmit (tx)
#define UART_PIN_RX BIT4 // P3.4
#define UART_PIN_TX BIT3 // P3.3

#define MAX_SMS_INDEX_DIGITS 5 // sms index can have up to 5 digits (99999)

#endif /* DEFINITIONS_H_ */