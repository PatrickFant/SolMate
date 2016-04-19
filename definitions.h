/*
 * definitions.h
 */

#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

// Set whether this code is for the MSP on the dev board or the MSP on the circuit board
#define MSP_ONPCB

#if defined MSP_DEVBOARD

// Status leds
#define LED_MSP BIT0 // P1.0
#define LED_MSP_2 BIT7 // P4.7

#define FLOATSWITCH_0 BIT1 // P1.0
#define FLOATSWITCH_1 BIT1 // P1.1
#define FLOATSWITCH_2 BIT2 // P1.2
#define FLOATSWITCH_3 BIT3 // P1.3
#define FLOATSWITCH_4 BIT4 // P1.4
#define PUMP_CONTROL BIT2 // P6.2
#define SOLARPANEL_CONTROL BIT3 // P6.3

#define POWER_BUTTON BIT1 // P1.1
#define GSM_POWER_CONTROL BIT0 // P2.0
#define GSM_POWER_STATUS BIT2 // P2.2

// ADC pins
#define ADC_PIN_BAT_CHARGE BIT0 // P6.0
#define ADC_PIN_SOLARPANEL_VOLTAGE BIT1 // P6.1

// Pins for receive (rx) and transmit (tx)
#define UART_PIN_RX BIT4 // P3.4
#define UART_PIN_TX BIT3 // P3.3

#elif defined MSP_ONPCB

#define LED_PORT_DIR P6DIR
#define LED_PORT_OUT P6OUT
#define LED_MSP BIT4 // P6.4
#define LED_MSP_2 BIT5 // P6.5

#define FLOAT_PORT_OUT P1OUT
#define FLOAT_PORT_DIR P1DIR
#define FLOAT_PORT_REN P1REN
#define FLOATSWITCH_0 BIT0 // P1.0
#define FLOATSWITCH_1 BIT1 // P1.1
#define FLOATSWITCH_2 BIT2 // P1.2
#define FLOATSWITCH_3 BIT3 // P1.3
#define FLOATSWITCH_4 BIT4 // P1.4
#define FLOATSWITCH_5 BIT5 // P1.5 (are we using this)

#define PUMPSOLAR_PORT_OUT P1OUT
#define PUMPSOLAR_PORT_DIR P1DIR
#define SOLARPANEL_CONTROL BIT6 // P1.6
#define PUMP_CONTROL BIT7 // P1.7

// not used
#define POWER_BUTTON BIT1 // P1.1

// ADC pins
#define ADC_PORT_SEL P6SEL
#define ADC_PIN_BAT_CHARGE BIT0 // P6.0
#define ADC_PIN_SOLARPANEL_VOLTAGE BIT1 // P6.1

// Pins for receive (rx) and transmit (tx), etc. for gsm
#define GSM_PORT_OUT P3OUT
#define GSM_PORT_IN P3IN
#define GSM_PORT_DIR P3DIR
#define GSM_PORT_SEL P3SEL
#define UART_PIN_RX BIT4 // P3.4
#define UART_PIN_TX BIT3 // P3.3
#define GSM_POWER_CONTROL BIT0 // P3.0
#define GSM_POWER_STATUS BIT5 // P3.5

#define GSMPOWER_PORT_OUT P4OUT
#define GSMPOWER_PORT_DIR P4DIR
#define GSMPOWER_ENABLE_PIN BIT0 // P4.0

#endif

#define TIMEOUT_SMS 65535 //(Don't set this more than 65535) 15 seconds with a 4096hz timer
#define MAX_SMS_INDEX_DIGITS 5 // sms index can have up to 5 digits (99999)
#define BATTERY_THRESHOLD_LOW 140 // when the bat was "charged", is pumping, and should stop now
#define BATTERY_THRESHOLD_HIGH 200 // when the bat was "discharged", not pumping, and can start now

#endif /* DEFINITIONS_H_ */
