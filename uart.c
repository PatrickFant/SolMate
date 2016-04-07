#include "uart.h"
#include "definitions.h"
#include <string.h>

/*
 * uart.c
 *
 */

// To keep track of the current index of the buffers
volatile unsigned int rx_buffer_index;
volatile unsigned int tx_buffer_index;

// Represents whether a command is being currently 'worked on' or not,
// not to be confused with the UCA0STAT register with the UCIDLE/UCBUSY bits
enum UartState {
	UartStateIdle,
	UartStateBusy
};
volatile char uart_state;

// Results that the cell module can send back to us
const char result_OK[] = "OK\r\n";
const char result_ERROR[] = "ERROR\r\n";
const char result_INPUT[] = "\r\n> ";
const char *result_OK_ptr; // This gets incremented when a received character matches result_OK
const char *result_ERROR_ptr;
const char *result_INPUT_ptr;

// Called when a uart command is done
//void completion_handler(int result);

// Initializes the msp's UART on the USCI A0
void uart_initialize()
{
	// Initialize variables
	rx_buffer_index = 0;
	tx_buffer_index = 0;
	uart_state = UartStateIdle;
	uart_command_state = CommandStateSendingAT;
	result_OK_ptr = result_OK; // Start at beginning of array
	result_ERROR_ptr = result_ERROR;
	result_INPUT_ptr = result_INPUT;
	uart_command_has_completed = 0;
	uart_command_result = UartResultUndefined;
	sent_text = 0;

	// Enable uart mode on the correct pins
	GSM_PORT_SEL |= UART_PIN_RX | UART_PIN_TX;

	// Configure the USCI for uart
	UCA0CTL1 |= UCSWRST; // Keep the USCI in reset mode
	UCA0CTL1 |= UCSSEL__SMCLK; // sub-main clock source
	UCA0BR0 = BAUDSPEED_BR0; // Set up the baud speed
	UCA0BR1 = BAUDSPEED_BR1;
	UCA0MCTL = BAUDSPEED_MCTL;
	UCA0CTL1 &= ~UCSWRST; // Turn off reset mode

	// Enable uart interrupts for receive and transmit
	UCA0IE |= UCRXIE | UCTXIE;

	// Clear all usci interrupt flags (this prevents the usual behavior where
	// the transmit interrupt is called (one time) as soon as interrupts are enabled)
//	UCA0IFG = 0;
}

// The USCIA0 interrupt (called for receiving and transmitting)
#pragma vector=USCI_A0_VECTOR
__interrupt void uart_interrupt_handler()
{
	// Check the interrupt flag to see if this is rx or tx //
	// We are reading from UCA0IV, which automatically resets the interrupt flag
	switch(UCA0IV)
	{
		case USCI_UCRXIFG: // Received a byte
		{
//			TA0R = 0;
//			TA0CTL |= MC__CONTINUOUS;

			if(rx_buffer_index < MAX_RX_BUFFER) // Make sure we don't read more bytes than the buffer can hold
			{
				// ** To-do: Add code to only store the bytes coming in some cases (e.g. when receiving an sms)

				char rx_byte = UCA0RXBUF; // Get the received byte
				rx_buffer[rx_buffer_index] = rx_byte; // Copy the received byte into buffer

				// For unsolicited messages
				if(uart_command_state == CommandStateIdle)
				{
					// Check for \r\n
					if(rx_buffer[rx_buffer_index - 1] == '\r' && rx_byte == '\n')
					{
//						P4OUT &= ~LED_MSP_2; // green LED off
						P1OUT |= LED_MSP; // red LED on

						// Stop here and go to main loop to decode the received message
						uart_state = UartStateIdle;
						UCA0IE &= ~UCRXIE; // Turn off receive interrupts for now
						uart_command_has_completed = 1;
						uart_command_state = CommandStateUnsolicitedMsg; // Going to process it in the main loop
						LPM0_EXIT; // Turn on cpu
						return;
					}
				}

				// The following string-checking code is a limited version of the KMP algorithm. It doesn't
				// use a pre-computed prefix array to match the strings and account for recurring patterns.
				// If one of the responses we want to match ends up requiring a precomputed prefix then we'll
				// need to change this code to run the full KMP algorithm.

				// Check if we got OK
				if(rx_byte == *result_OK_ptr)
				{
					result_OK_ptr++;
					if(*result_OK_ptr == '\0') // End of string array
					{
						uart_state = UartStateIdle; // Done running a command
						UCA0IE &= ~UCRXIE; // Turn off receive interrupts for now
						uart_command_result = UartResultOK; // Tells main loop what the result is
						uart_command_has_completed = 1; // Tells main loop that we're done
						LPM0_EXIT; // Turn on CPU to run the main loop
//						TA0CTL |= MC__STOP;
						return;
					}
				}
				else // wrong character, start over
					if(rx_byte == result_OK[0])
						result_OK_ptr = result_OK + 1; // start of new (possible) match
					else
						result_OK_ptr = result_OK; // no match at all right now

				// Check if we got ERROR
				if(rx_byte == *result_ERROR_ptr)
				{
					result_ERROR_ptr++; // we have a match
					if(*result_ERROR_ptr == '\0')
					{
						uart_state = UartStateIdle; // Done running a command
						UCA0IE &= ~UCRXIE; // Turn off receive interrupts for now
						uart_command_result = UartResultError; // Tells main loop what the result is
						uart_command_has_completed = 1; // Tells main loop that we're done
						LPM0_EXIT; // Turn on CPU to run the main loop
						return;
					}
				}
				else // wrong character, start over
					if(rx_byte == result_ERROR[0])
						result_ERROR_ptr = result_ERROR + 1; // start of new (possible) match
					else
						result_ERROR_ptr = result_ERROR; // no match at all right now

				// Check if we got an input character "\r\n> "
				if(rx_byte == *result_INPUT_ptr)
				{
					result_INPUT_ptr++;
					if(*result_INPUT_ptr == '\0') // Match found
					{
						uart_state = UartStateIdle; // Done running a command
						UCA0IE &= ~UCRXIE; // Turn off receive interrupts for now
						uart_command_result = UartResultInput; // Tells main loop what the result is
						uart_command_has_completed = 1; // Tells main loop that we're done
						LPM0_EXIT; // Turn on CPU to run the main loop
						return;
					}
				}
				else // wrong character, start over
					if(rx_byte == result_INPUT[0])
						result_INPUT_ptr = result_INPUT + 1; // start of new (possible) match
					else
						result_INPUT_ptr = result_INPUT; // no match at all right now

				// Increment the buffer index
				rx_buffer_index++;
			}

			break;
		}

		case USCI_UCTXIFG: // Ready to transmit a new byte
		{
			// Get the desired byte to send
			char tx_byte = tx_buffer[tx_buffer_index];

			// Send a byte if we are not at the end of the buffer yet
			if(tx_byte != '\0')
			{
				UCA0TXBUF = tx_byte; // Send the byte
				tx_buffer_index++; // Increment the buffer index
			}

			break;
		}
	}
}

// Clears out the receive buffer and sets the buffer index to zero
void rx_buffer_reset()
{
	// Set everything to nul-character
	memset(&rx_buffer, '\0', MAX_RX_BUFFER);

	// Reset current index
	rx_buffer_index = 0;

	// reset results pointers to beginning of their respective arrays
	result_OK_ptr = result_OK;
	result_ERROR_ptr = result_ERROR;
	result_INPUT_ptr = result_INPUT;
}

// Clears out the transmit buffer and sets the buffer index to zero
void tx_buffer_reset()
{
	// Set everything to nul-character
	memset(&tx_buffer, '\0', MAX_TX_BUFFER);

	// Reset current index
	tx_buffer_index = 0;
}

// Send a character array to the cell module while also recording the
// response
//void uart_send_str(const char *send_str)
void uart_send_command()
{
	// Stop if an operation is already happening
	if(uart_state != UartStateIdle)
		return;

	// Reset the buffers, flags
	uart_command_has_completed = 0;
	uart_command_result = UartResultUndefined;
	rx_buffer_reset();

	// Don't allow sending strings until this one is finished
	uart_state = UartStateBusy;

	// Enable rx interrupts
	UCA0IE |= UCRXIE;

	// Put the first byte into the transmit buffer (this starts the process)
	tx_buffer_index = 1; // Interrupt handler will start at the second byte (index 1)
	UCA0TXBUF = tx_buffer[0];
}

// Go into idle mode
void uart_enter_idle_mode()
{
	uart_command_state = CommandStateIdle;
	uart_command_has_completed = 0; // In general, reset (zero) this flag if uart_send_str(..) is not called
	rx_buffer_reset(); // Clear rx buffer (make room for messages from the module)
	UCA0IE |= UCRXIE; // enable rx interrupt
}

// Returns 1 if the uart is currently sending a command, and 0 if it is not
int uart_command_in_progress()
{
	if(uart_state == UartStateIdle)
		return 0;
	else
		return 1;
}

// Called when a command is finished
//void completion_handler(int result)
//{
//}
