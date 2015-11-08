#include "msp430f5529.h"
#include "uart.h"

/*
 * main.c
 */

int main(void)
{
	// Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    // Set up button
    P2DIR &= ~BIT1; // input
    P2REN |= BIT1;
    P2OUT |= BIT1;
    P2IES |= BIT1;
    P2IE |= BIT1;

//    // Set up LED
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;

    // Initialize the uart
    uart_initialize();

    // Enable interrupts in general
    _BIS_SR(GIE);
	
	return 0;
}

#pragma vector=PORT2_VECTOR
interrupt void port2_interrupt_handler()
{
	// led
	P1OUT ^= BIT0;

	// Send a string
	uart_send_str("hello");

	// Reset flag
	P2IFG &= ~BIT1;
}
