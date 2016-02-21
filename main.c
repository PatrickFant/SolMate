#include "msp430f5529.h"
#include "definitions.h"
#include "uart.h"
#include "adc.h"
#include <string.h>

/*
 * main.c
 */

// State variables
volatile char floatswitch_active; // Contains 1 if active, 0 if not
volatile char water_depth; // Contains water depth value
volatile char battery_charge; // Contains battery charge value
volatile char pump_active; // Controls the water pump (0 = off, 1 = on)

// Phone numba
#define MAX_PHONE_LENGTH 16
char phone_number[MAX_PHONE_LENGTH]; // like +14445556666

int main(void)
{
	// Stop watchdog timer for now
    WDTCTL = WDTPW | WDTHOLD;

    // Initialize state variables
    floatswitch_active = 0;
    water_depth = 0;
    battery_charge = 0;
    pump_active = 0;
    memset(phone_number, '\0', MAX_PHONE_LENGTH);

    // Set up button
    P6DIR &= ~INPUT_FLOATSWITCH;
    P6REN |= INPUT_FLOATSWITCH;
    P6OUT |= INPUT_FLOATSWITCH;

    // Set up pump LED
    P6DIR |= LED_PUMP;
    P6OUT &= ~LED_PUMP;

    // Set up msp430 LEDs
    P1DIR |= LED_MSP;
//    P4DIR |= LED_MSP_2;
    P1OUT &= ~LED_MSP;
//    P4OUT &= ~LED_MSP_2;

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

	tx_buffer_reset();
    strcpy(tx_buffer, "AT\r\n");
    uart_send_command();

    // Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    // Start up Timer A0
    TA0CTL = TACLR; // clear first
    TA0CTL = TASSEL__ACLK | ID__8 | MC__STOP; // auxiliary clock (32.768 kHz), divide by 8 (4096 Hz), interrupt enable, stop mode
    TA0CCTL0 = CCIE; // enable capture/compare interrupt
    TA0CCR0 = 1023; // reduces rate to 4 Hz (4096/1024)
    TA0CTL |= MC__UP; // start the timer in up mode (counts to TA0CCR0 then resets to 0)

    // Turn CPU off
    LPM3;

    // Main loop
    while(1)
    {
    	// Check if a UART command has finished and respond accordingly
    	if(uart_command_has_completed)
    	{
    		switch(uart_command_state)
			{
    			case CommandStateSendingAT:
    			{
    				if(uart_command_result == UartResultOK)
    				{
    					// Send ATE0 because we do not need a copy of what we send
    					uart_command_state = CommandStateTurnOffEcho;
    					tx_buffer_reset();
    					strcpy(tx_buffer, "ATE0\r\n");
    					uart_send_command();
    				}
    				break;
    			}
				case CommandStateTurnOffEcho: // Got a response after sending AT
				{
					if(uart_command_result == UartResultOK)
					{
						// Send cmgf
						// This puts the cell module into SMS mode, as opposed to data mode
						uart_command_state = CommandStateGoToSMSMode;
						tx_buffer_reset();
						strcpy(tx_buffer, "AT+CMGF=1\r\n");
						uart_send_command();
					}
					break;
				}

				case CommandStateGoToSMSMode: // Got a response after sending CMGF
				{
					if(uart_command_result == UartResultOK)
					{
//						P4OUT |= LED_MSP_2; // green LED on
						P1OUT &= ~LED_MSP; // red LED off

						// We are now ready to send a text whenever the system needs to
						uart_enter_idle_mode();
					}
					break;
				}

				case CommandStatePrepareWarningSMS: // Got a response after sending CMGS
				{
					if(uart_command_result == UartResultInput)
					{
						// Send the text now
						uart_command_state = CommandStateSendWarningSMS;
						tx_buffer_reset();
						strcpy(tx_buffer, "Msg from Sol-Mate: Check your boat; water level is getting high.\n\x1A");
						uart_send_command();
					}
					break;
				}

				case CommandStateSendWarningSMS: // Got a response after sending the text
				{
					if(uart_command_result == UartResultOK)
					{
//						P4OUT |= LED_MSP_2; // green LED on
						P1OUT &= ~LED_MSP; // red LED off
						sent_text = 1; // Do not send the text again (this is for testing purposes--to send another text you have to restart the MSP)
					}
					break;
				}

				case CommandStateUnsolicitedMsg: // Received a message from the cell module
				{
					// Check what kind of code this is..
					// --SMS--
					// +CMTI: "SM",3\r\n
					if(strstr(rx_buffer, "+CMTI")) // strstr returns null/0 if not found
					{
						// Find the comma
						char *begin_ptr = strchr(rx_buffer, ',');
						if(!begin_ptr) {
							uart_enter_idle_mode();
							break;
						}
						begin_ptr++; // should point to the beginning of the SMS index we need

						// Find the '\r' which is directly following the last character of the SMS index
						char *end_ptr = strchr(begin_ptr, '\r');
						if(!end_ptr) {
							uart_enter_idle_mode();
							break;
						}

						// Create the command to read the sms
						tx_buffer_reset();
						strcat(tx_buffer, "AT+CMGR=");
						strncat(tx_buffer, begin_ptr, end_ptr - begin_ptr); // SMS index
						strcat(tx_buffer, "\r\n");

						// Send the command
						uart_command_state = CommandStateReadSMS;
						uart_send_command();
					}
					else // unrecognized
						uart_enter_idle_mode();

					break;
				}

				case CommandStateReadSMS:
				{
					if(uart_command_result == UartResultOK)
					{
						// +CMGR: "<status>","<origin number>","<??>","<timestamp>"\r\n
						// text contents here\r\n
						// \r\n
						// OK\r\n

						// find the 1st comma
						char *begin_ptr = strchr(rx_buffer, ',');
						if(!begin_ptr || *(begin_ptr+1) != '"') {
							uart_enter_idle_mode();
							break;
						}
						begin_ptr += 2; // Move to the beginning of the number

						// find the ending quotation mark
						char *end_ptr = strchr(begin_ptr, '"');
						if(!end_ptr) {
							uart_enter_idle_mode();
							break;
						}

						// Check if it's too long
						if(end_ptr - begin_ptr > MAX_PHONE_LENGTH) {
							uart_enter_idle_mode();
							break;
						}

						// get the phone number
						strncpy(phone_number, begin_ptr, end_ptr - begin_ptr);

//						P4OUT |= LED_MSP_2; // green LED on
						P1OUT &= ~LED_MSP; // red LED off

						uart_enter_idle_mode();
					}
					else
						uart_enter_idle_mode();

					break;
				}
			}

    		// Turn CPU off until someone calls LPM3_EXIT (uart interrupt handler will)
    		LPM3;
    	}
    }
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void timerA0_interrupt_handler()
{
	// Toggle float switch variable
	floatswitch_active = (P6IN & INPUT_FLOATSWITCH) ? 0 : 1; // false means ground/zero -> switch pressed (1)

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
					tx_buffer_reset();
					strcpy(tx_buffer, "AT+CMGS=\"9783642893\"\r\n");
					uart_send_command();

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
