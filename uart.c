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
#define MAX_RX_BUFFER 65
#define MAX_TX_BUFFER 65

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

// Initializes the msp's UART on the USCI A0
void uart_initialize()
{
	// Initialize variables
	rx_buffer_index = 0;
	tx_buffer_index = 0;
	uart_state = UartStateIdle;

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

// The USCIA0 interrupt (called for receiving and transmitting)
#pragma vector=USCI_A0_VECTOR
interrupt void uart_interrupt_handler()
{
	// Check the interrupt flag to see if this is rx or tx //
	// We are reading from UCA0IV, which automatically resets the interrupt flag
	switch(UCA0IV)
	{
		case UCA0IVRX: // Received a byte
		{
			if(rx_buffer_index < MAX_RX_BUFFER) // Make sure we don't read more bytes than the buffer can hold
			{
				rx_buffer[rx_buffer_index] = UCA0RXBUF; // Copy the received byte into buffer
				rx_buffer_index++; // Increment the buffer index
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
			else // *** this needs to be moved to when the ending sequence is received ***
				uart_state = UartStateIdle; // We are done transmitting

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
