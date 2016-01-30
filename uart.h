#include "msp430f5529.h"

/*
 * uart.h
 *
 */

#ifndef UART_H_
#define UART_H_

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

// States of the system
enum CommandState {
	CommandStateSendingAT,
	CommandStateGoToSMSMode,
	CommandStateIdle,
	CommandStatePrepareWarningSMS,
	CommandStateSendWarningSMS
};
volatile char uart_command_state; // Controls what commands are sent to the gsm module

// Flags that are set when a UART command is completed (the main loop in main.c checks this
// to see when it should act)
volatile char uart_command_has_completed;
volatile int uart_command_result;

// Only send text once
volatile char sent_text;

// Functions //

// Initialize the USCI module in uart mode
void uart_initialize();

// Is the system currently sending a command?
int uart_command_in_progress();

// Send a string over the uart
void uart_send_str(const char *send_str);

#endif /* UART_H_ */
