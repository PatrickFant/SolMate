#include "msp430f5529.h"
#include "definitions.h"
#include "uart.h"
#include "adc.h"

/*
 * main.c
 */

// State variables
volatile char floatswitch_active; // Contains 1 if active, 0 if not
volatile char water_depth; // Contains water depth value
volatile char battery_charge; // Contains battery charge value
volatile char pump_active; // Controls the water pump (0 = off, 1 = on)

// Called when a uart command is done
//void uart_completion_handler(int result);

int main(void)
{
	// Stop watchdog timer for now
    WDTCTL = WDTPW | WDTHOLD;

    // Initialize state variables
    floatswitch_active = 0;
    water_depth = 0;
    battery_charge = 0;
    pump_active = 0;

    // Set up button
    P1DIR &= ~INPUT_FLOATSWITCH;
    P1REN |= INPUT_FLOATSWITCH;
    P1OUT |= INPUT_FLOATSWITCH;
    P1IES |= INPUT_FLOATSWITCH;
    P1IE |= INPUT_FLOATSWITCH;

    // Set up pump LED
    P6DIR |= LED_PUMP;
    P6OUT &= ~LED_PUMP;

    // Set up msp430 LEDs
    P1DIR |= LED_MSP;
    P4DIR |= LED_MSP_2;
    P1OUT &= ~LED_MSP;
    P4OUT &= ~LED_MSP_2;

    // Initialize the uart and ADC, start ADC conversion
    uart_initialize();
    adc_initialize();

    // Wait a bit
    __delay_cycles(1048576); // 1 second

    // Enable watchdog interrupts and interrupts in general
    SFRIE1 |= WDTIE;
    _BIS_SR(GIE);

    // Start conversion
	adc_start_conversion();

    // Send an AT first
	P1OUT |= LED_MSP;
	P4OUT &= ~LED_MSP_2;
    uart_send_str("AT\n");

    // Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    // Start up Timer A0
    TA0CTL = TACLR; // clear first
    TA0CTL = TASSEL__ACLK | ID__8 | MC__STOP; // auxiliary clock (32.768 kHz), divide by 8 (4096 Hz), interrupt enable, stop mode
    TA0CCTL0 = CCIE; // enable capture/compare interrupt
    TA0CCR0 = 1023; // reduces rate to 4 Hz (4096/1024)
    TA0CTL |= MC__UP; // start the timer in up mode (counts to TA0CCR0 then resets to 0)

    // Turn CPU off
    LPM0;

    // Main loop
    while(1)
    {
    	// Check if a UART command has finished and respond accordingly
    	if(uart_command_has_completed)
    	{
    		switch(uart_command_state)
			{
				case CommandStateSendingAT: // Got a response after sending AT
				{
					if(uart_command_result == UartResultOK)
					{
						// Send cmgf
						// This puts the cell module into SMS mode, as opposed to data mode
						uart_command_state = CommandStateGoToSMSMode;
						uart_send_str("AT+CMGF=1\n");
					}
					break;
				}

				case CommandStateGoToSMSMode: // Got a response after sending CMGF
				{
					if(uart_command_result == UartResultOK)
					{
						P4OUT |= LED_MSP_2; // green LED on
						P1OUT &= ~LED_MSP; // red LED off

						// We are now ready to send a text whenever the system needs to
						uart_command_state = CommandStateIdle;
						uart_command_has_completed = 0; // In general, reset (zero) this flag if uart_send_str(..) is not called
					}
					break;
				}

				case CommandStatePrepareWarningSMS: // Got a response after sending CMGS
				{
					if(uart_command_result == UartResultInput)
					{
						// Send the text now
						uart_command_state = CommandStateSendWarningSMS;
						uart_send_str("Msg from Sol-Mate: Check your boat; water level is getting high.\n\x1A");
					}
					break;
				}

				case CommandStateSendWarningSMS: // Got a response after sending the text
				{
					if(uart_command_result == UartResultOK)
					{
						P4OUT |= LED_MSP_2; // green LED on
						P1OUT &= ~LED_MSP; // red LED off
						sent_text = 1; // Do not send the text again (this is for testing purposes--to send another text you have to restart the MSP)
					}
					break;
				}
			}

    		// Turn CPU off until someone calls LPM0_EXIT (uart interrupt handler will)
    		LPM0;
    	}
    }
}

#pragma vector=PORT1_VECTOR
__interrupt void port1_interrupt_handler()
{
	// Toggle float switch variable
	floatswitch_active = (P1IN & INPUT_FLOATSWITCH) ? 0 : 1; // false means ground/zero -> switch pressed (1)

	// Switch interrupt edge select
	P1IES ^= INPUT_FLOATSWITCH;

	// Reset flag
	P1IFG = 0;
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void timerA0_interrupt_handler()
{
	// Check water depth
	if(floatswitch_active || water_depth > 70) // water depth values go from 0 to 255
	{
		if(battery_charge > 100) // around 40% (100 out of 255)
			pump_active = 1;
		else
		{
			pump_active = 0;

			if(water_depth > 127)
			{
				// There is not enough charge and too much water, notify over text
				if(sent_text == 0 && uart_command_state == CommandStateIdle)
				{
					P4OUT &= ~LED_MSP_2; // green LED off
					P1OUT |= LED_MSP; // red LED on

					// Send the text!!
					uart_command_state = CommandStatePrepareWarningSMS;
					uart_send_str("AT+CMGS=\"9783642893\"\n");
					sent_text = 1;
				}
			}
		}
	}
	else // No water
		pump_active = 0;

	// Set pump LED output
	if(pump_active)
		P6OUT |= LED_PUMP;
	else
		P6OUT &= ~LED_PUMP;

	// New conversion
	adc_start_conversion();
}

// should move this to adc.c sometime
#pragma vector=ADC12_VECTOR
__interrupt void ADC_interrupt_handler()
{
	// Check the interrupt flags
	switch(ADC12IV)
	{
		case ADC12IV_ADC12IFG1: // Both readings have finished
			water_depth = ADC12MEM0; // Save reading
			battery_charge = ADC12MEM1; // Save reading
			break;
	}
}
