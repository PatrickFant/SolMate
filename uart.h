#include "msp430f5529.h"

/*
 * uart.h
 *
 */

#ifndef UART_H_
#define UART_H_

// Pins for receive (rx) and transmit (tx)
#define UART_PIN_RX BIT4 // P3.4
#define UART_PIN_TX BIT3 // P3.3

// Set the speed here
#define BAUDSPEED_9600

// Values for 9600 baud
#if defined BAUDSPEED_9600
#define BAUDSPEED_BR0 0x6
#define BAUDSPEED_BR1 0x0
#define BAUDSPEED_MCTL 0xD1
// Values for 14400
#elif defined BAUDSPEED_14400
#define BAUDSPEED_BR0 0x4
#define BAUDSPEED_BR1 0x0
#define BAUDSPEED_MCTL 0x91
#endif

// Return possibilities
enum ReturnResult {
	UartResultUndefined = -1,
	UartResultOK = 0,
	UartResultError = 1,
	UartResultInput = 2
};

// Functions //

// Initialize the USCI module in uart mode
void uart_initialize();

// Set the function to call when a command is finished
void uart_set_completion_handler(void (*handler)(int result));

// Is the system currently sending a command?
int uart_command_in_progress();

// Send a string over the uart
void uart_send_str(const char *send_str);

#endif /* UART_H_ */
