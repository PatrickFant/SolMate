#include "uart.h"
#include "string.h"

/*
 * uart.c
 *
 */

// For some reason these constants are not defined in the msp header file, but they are on
// page 967 of the MSP430x5 and MSP430x6 manual. But more likely I am dumb and can't find them
#define UCA0IVRX 0x2
#define UCA0IVTX 0x4

// Maximum buffer sizes in bytes for sending and receiving
#define MAX_RX_BUFFER 97
#define MAX_TX_BUFFER 97

// Buffers for sending and receiving data //
char rx_buffer[MAX_RX_BUFFER]; // The receive buffer
char tx_buffer[MAX_TX_BUFFER]; // The transmit buffer

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
#define LENGTH_OK 4 // length of result_OK
#define LENGTH_ERROR 7 // length of result_ERROR
#define LENGTH_INPUT 4
const char result_OK[] = "OK\r\n";
const char result_ERROR[] = "ERROR\r\n";
const char result_INPUT[] = "\r\n> ";
volatile unsigned int result_OK_counter; // This gets incremented when a received character matches result_OK
volatile unsigned int result_ERROR_counter;
volatile unsigned int result_INPUT_counter;

// Holds a pointer to the function to call when a command is done
void (*completion_handler)(int result);

// Initializes the msp's UART on the USCI A0
void uart_initialize()
{
	// Initialize variables
	rx_buffer_index = 0;
	tx_buffer_index = 0;
	uart_state = UartStateIdle;
	result_OK_counter = 0;
	result_ERROR_counter = 0;
	result_INPUT_counter = 0;

	// Enable uart mode on the correct pins
	P3SEL |= UART_PIN_RX | UART_PIN_TX;

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
	// the transmit interrupt is called as soon as interrupts are enabled (once))
	UCA0IFG = 0;
}

// Sets the completion handler function
void uart_set_completion_handler(void (*handler)(int))
{
	completion_handler = handler;
}

// The USCIA0 interrupt (called for receiving and transmitting)
#pragma vector=USCI_A0_VECTOR
__interrupt void uart_interrupt_handler()
{
	// Check the interrupt flag to see if this is rx or tx //
	// We are reading from UCA0IV, which automatically resets the interrupt flag
	switch(UCA0IV)
	{
		case UCA0IVRX: // Received a byte
		{
			if(rx_buffer_index < MAX_RX_BUFFER) // Make sure we don't read more bytes than the buffer can hold
			{
				char rx_byte = UCA0RXBUF; // Get the received byte
				rx_buffer[rx_buffer_index] = rx_byte; // Copy the received byte into buffer
				rx_buffer_index++; // Increment the buffer index

				// Check if we got OK
				if(rx_byte == result_OK[result_OK_counter])
				{
					if(++result_OK_counter == LENGTH_OK) // We have gotten "OK\r\n"
					{
						uart_state = UartStateIdle;
						completion_handler(UartResultOK);
						return;
					}
				}
				else // wrong character, start over
					if(rx_byte == result_OK[0])
						result_OK_counter = 1; // start of new (possible) match
					else
						result_OK_counter = 0; // no match at all right now

				// Check if we got ERROR
				if(rx_byte == result_ERROR[result_ERROR_counter])
				{
					if(++result_ERROR_counter == LENGTH_ERROR) // we have "ERROR\r\n"
					{
						uart_state = UartStateIdle;
						completion_handler(UartResultError);
						return;
					}
				}
				else // wrong character, start over
					if(rx_byte == result_ERROR[0])
						result_ERROR_counter = 1; // start of new (possible) match
					else
						result_ERROR_counter = 0; // no match at all right now

				// Check if we got an input character "\r\n> "
				if(rx_byte == result_INPUT[result_INPUT_counter])
				{
					if(++result_INPUT_counter == LENGTH_INPUT) // matched "\r\n> "
					{
						uart_state = UartStateIdle;
						completion_handler(UartResultInput);
						return;
					}
				}
				else // wrong character, start over
					if(rx_byte == result_INPUT[0])
						result_INPUT_counter = 1; // start of new (possible) match
					else
						result_INPUT_counter = 0; // no match at all right now
			}

			break;
		}

		case UCA0IVTX: // Ready to transmit a new byte
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

	// reset results counters
	result_OK_counter = 0;
	result_ERROR_counter = 0;
	result_INPUT_counter = 0;
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
void uart_send_str(const char *send_str)
{
	// Stop if an operation is already happening
	if(uart_state != UartStateIdle)
		return;

	// Reset the buffers
	tx_buffer_reset();
	rx_buffer_reset();

	// Copy send_str to tx_buffer
	unsigned int send_str_len = strlen(send_str);
	strncpy(tx_buffer, send_str, send_str_len);

	// Don't allow sending strings until this one is finished
	uart_state = UartStateBusy;

	// Put the first byte into the transmit buffer (this starts the process)
	tx_buffer_index = 1; // Interrupt handler will start at the second byte (index 1)
	UCA0TXBUF = tx_buffer[0];
}

// Returns 1 if the uart is currently sending a command, and 0 if it is not
int uart_command_in_progress()
{
	if(uart_state == UartStateIdle)
		return 0;
	else
		return 1;
}
