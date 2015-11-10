#include "msp430f5529.h"
#include "uart.h"
#include "adc.h"

/*
 * main.c
 */

#define LED_MSP BIT0 // P1.0
#define LED_MSP_2 BIT7 // P4.7

#define INPUT_FLOATSWITCH BIT6 // P1.6
#define LED_PUMP BIT2 // P6.2

// State variables
volatile char floatswitch_active; // Contains 1 if active, 0 if not
volatile char water_depth; // Contains water depth value
volatile char battery_charge; // Contains battery charge value
volatile char pump_active; // Controls the water pump (0 = off, 1 = on)

// All possible commands that the msp430 can send
enum CommandState {
	CommandStateSendingAT,
	CommandStateSendingCMGF,
	CommandStateReadyForCMGS,
	CommandStateSendingCMGS,
	CommandStateSendingText
};
volatile char command_state; // Controls what commands are sent to the gsm module

// Only send text once
volatile char sent_text;

// Called when a uart command is done
void uart_completion_handler(int result);

int main(void)
{
	// Stop watchdog timer for now
    WDTCTL = WDTPW | WDTHOLD;

    // Initialize state variables
    floatswitch_active = 0;
    water_depth = 0;
    battery_charge = 0;
    pump_active = 0;
    command_state = CommandStateSendingAT;
    sent_text = 0;

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
    P1OUT |= LED_MSP;
    P4DIR |= LED_MSP_2;
    P4OUT &= ~LED_MSP_2;

    // Initialize the uart and ADC, start ADC conversion
    uart_initialize();
    uart_set_completion_handler(uart_completion_handler);
    adc_initialize();

    // Wait a bit
    __delay_cycles(1048576); // 1 second

    // Start conversion
    adc_start_conversion();

    // Enable watchdog interrupts and interrupts in general
    SFRIE1 |= WDTIE;
    _BIS_SR(GIE);

    // Send an AT first
    uart_send_str("AT\n");

	// Start up watchdog timer
	WDTCTL = WDTPW | WDTSSEL__ACLK | WDTTMSEL | WDTIS_5; // about 1/4 second interval

	return 0;
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

#pragma vector=WDT_VECTOR
__interrupt void watchdog_interrupt_handler()
{
	// Check water depth
	if(floatswitch_active || water_depth > 70) // WATERRR -- the values go from 0 to 255
	{
		if(battery_charge > 100) // around 40% (100 out of 255)
			pump_active = 1;
		else
		{
			pump_active = 0;

			if(water_depth > 127)
			{
				// There is not enough charge and too much water, notify over text

				if(sent_text == 0 && command_state == CommandStateReadyForCMGS)
				{
					P4OUT &= ~LED_MSP_2; // green LED off
					P1OUT |= LED_MSP; // red LED on

					// Send the text!!
					command_state = CommandStateSendingCMGS;
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

// Called when a command is finished
void uart_completion_handler(int result)
{
	switch(command_state)
	{
		case CommandStateSendingAT:
		{
			if(result == UartResultOK) // Response after sending AT
			{
				// Send cmgf
				command_state = CommandStateSendingCMGF;
				uart_send_str("AT+CMGF=1\n");
			}
			break;
		}

		case CommandStateSendingCMGF: // Response after sending CMGF
		{
			if(result == UartResultOK)
			{
				P4OUT |= LED_MSP_2; // green LED on
				P1OUT &= ~LED_MSP; // red LED off

				command_state = CommandStateReadyForCMGS;
			}
			break;
		}

		case CommandStateSendingCMGS: // Response after sending CMGS
		{
			if(result == UartResultInput)
			{
				// Send the text now
				command_state = CommandStateSendingText;
				uart_send_str("Msg from Sol-Mate: Check your boat; water level is getting high.\n\x1A");
			}
			break;
		}

		case CommandStateSendingText: // Response after sending the text
		{
			if(result == UartResultOK)
			{
				P4OUT |= LED_MSP_2; // green LED on
				P1OUT &= ~LED_MSP; // red LED off
				sent_text = 1;
			}
		}
	}
}
