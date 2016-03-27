#include "msp430f5529.h"
#include "definitions.h"
#include "uart.h"
#include "adc.h"
#include "flash.h"
#include <stdbool.h>
#include <string.h>

/*
 * main.c
 */

// State variables
//volatile char floatswitch_active; // Contains 1 if active, 0 if not
volatile char floatswitches; // Contains water depth value (each bit represents a float switch)
volatile char battery_charge; // Contains battery charge value
volatile char solarpanel_voltage; // panel voltage
volatile char pump_active; // Controls the water pump (0 = off, 1 = on)

void toggle_gsm_power(void)
{
	// Set output to be LOW
	P2DIR |= GSM_POWER_CONTROL; // output mode
	P2OUT &= ~GSM_POWER_CONTROL; // low

	// Start timer and run for 1.5 seconds, and call the interrupt handler when it's done
	TA1CTL = TACLR;
	TA1CTL = TASSEL__ACLK | ID__8 | MC__STOP; // aux clock, divide by 8 (so 4096 hz)
	TA1CCTL0 = CCIE; // interrupt enable for ccr0
	TA1CCR0 = 6144 - 1; // 1.5 secs (4096 * 1.5)
	TA1CTL |= MC__UP; // activate timer
}

/**
 * Takes a binary int representing floatswitch values and determines the
 * number of active switches as well as the validity of the reading.
 */
int floatswitch_get_reading(char switches, int number_of_switches)
{
  bool inactive_switch_found = false;
  int active_switch_count = 0;
  
  int i;
  for (i = 0; i < number_of_switches; ++i)
  {
    bool switch_is_active = (switches >> i) & 1;
    if (switch_is_active && !inactive_switch_found)
      ++active_switch_count;
    else if (switch_is_active && inactive_switch_found)
      return -1;
    else // switch is inactive
      inactive_switch_found = true;
  }
  return active_switch_count;
}

// Phone numba
#define MAX_PHONE_LENGTH 16
char phone_number[MAX_PHONE_LENGTH]; // like +14445556666

int main(void)
{
	// Stop watchdog timer for now
    WDTCTL = WDTPW | WDTHOLD;

    // Initialize state variables
//    floatswitch_active = 0;
    floatswitches = 0;
    battery_charge = 0;
    solarpanel_voltage = 0;
    pump_active = 0;

    // Read in the saved phone number from memory, if it is there
    memset(phone_number, '\0', MAX_PHONE_LENGTH);
    if(strncmp(PHONE_ADDRESS, "+1", 2) == 0) // Phone numbers start with +1
    	strncpy(phone_number, PHONE_ADDRESS, MAX_PHONE_LENGTH); // copy from flash into ram

    // Set up float switches
    P1DIR &= ~(FLOATSWITCH_0 | FLOATSWITCH_1 | FLOATSWITCH_2);
    P1REN |= FLOATSWITCH_0 | FLOATSWITCH_1 | FLOATSWITCH_2;
    P1OUT |= FLOATSWITCH_0 | FLOATSWITCH_1 | FLOATSWITCH_2;

    // Set up water pump and solarpanel on/off
    P6DIR |= PUMP_CONTROL | SOLARPANEL_CONTROL;
    P6OUT &= ~(PUMP_CONTROL | SOLARPANEL_CONTROL);

    // Set up msp430 LEDs
    P1DIR |= LED_MSP;
//    P4DIR |= LED_MSP_2;
    P1OUT &= ~LED_MSP;
//    P4OUT &= ~LED_MSP_2;

    // Set up gsm 'power button'
    P1DIR &= ~POWER_BUTTON;
    P1REN |= POWER_BUTTON;
    P1OUT |= POWER_BUTTON;
    P1IE |= POWER_BUTTON; // interrupts
    P1IES |= POWER_BUTTON; // high->low transition

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

	tx_buffer_reset();
    strcpy(tx_buffer, "AT\r\n");
    uart_send_command();

    // Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    // Start up Timer A0
    TA0CTL = TACLR; // clear first
    TA0CTL = TASSEL__ACLK | ID__8 | MC__STOP; // auxiliary clock (32.768 kHz), divide by 8 (4096 Hz), interrupt enable, stop mode
    TA0CCTL0 = CCIE; // enable capture/compare interrupt
    TA0CCR0 = 2047; // reduces rate to 2 times/sec
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
    			case CommandStateSendingAT:
    			{
    				if(uart_command_result == UartResultOK)
    				{
    					// Send ATE0 because we do not need a copy of what we send
//    					uart_command_state = CommandStateTurnOffEcho;
//    					tx_buffer_reset();
//    					strcpy(tx_buffer, "ATE0\r\n");
//    					uart_send_command();
    					uart_command_state = CommandStateGoToSMSMode;
						tx_buffer_reset();
						strcpy(tx_buffer, "AT+CMGF=1\r\n");
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
						strcpy(tx_buffer, "Msg from Sol-Mate: Check your boat; water level is getting high.\r\n\x1A");
						uart_send_command();
					}
					break;
				}

				case CommandStateSendWarningSMS: // Got a response after sending the text
				{
					if(uart_command_result == UartResultOK)
					{
						P1OUT &= ~LED_MSP; // red LED off
						sent_text = 1; // Do not send the text again (this is for testing purposes--to send another text you have to restart the MSP)
					}

					uart_enter_idle_mode();
					break;
				}

				case CommandStateUnsolicitedMsg: // Received a message from the cell module
				{
					P1OUT |= LED_MSP; // red LED on

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
						P1OUT &= ~LED_MSP; // red LED on
						uart_command_state = CommandStateReadSMS;
						uart_send_command();
					}
					else // unrecognized
					{
						P1OUT &= ~LED_MSP;
						uart_enter_idle_mode();
					}

					break;
				}

				case CommandStateReadSMS:
				{
					P1OUT |= LED_MSP; // red LED on
					if(uart_command_result == UartResultOK)
					{
						// +CMGR: "<status>","<origin number>","<??>","<timestamp>"\r\n
						// text contents here\r\n
						// \r\n
						// OK\r\n

						// find the 1st comma
						char *begin_ptr_phone = strchr(rx_buffer, ',');
						if(!begin_ptr_phone || *(begin_ptr_phone+1) != '"') {
							uart_enter_idle_mode();
							break;
						}
						begin_ptr_phone += 2; // Move to the beginning of the number

						// find the ending quotation mark
						char *end_ptr_phone = strchr(begin_ptr_phone, '"');
						if(!end_ptr_phone) {
							uart_enter_idle_mode();
							break;
						}

						// Check if it's too long
						if(end_ptr_phone - begin_ptr_phone > MAX_PHONE_LENGTH) {
							uart_enter_idle_mode();
							break;
						}

						// Look at the contents of the text - it starts right after the first \r\n
						char *begin_ptr_sms = strchr(rx_buffer, '\n');
						if(!begin_ptr_sms) {
							uart_enter_idle_mode();
							break;
						}
						begin_ptr_sms++; // Move to the beginning of the text

						// The text ends right before the next \r\n
						char *end_ptr_sms = strchr(begin_ptr_sms, '\r');
						if(!end_ptr_sms) {
							uart_enter_idle_mode();
							break;
						}

						// Check for the "password"
//						if(strncmp(begin_ptr_sms, "978SolMate", end_ptr_sms - begin_ptr_sms) == 0) // DONT USE THIS HOLY SHIT!!!!
						if(strstr(begin_ptr_sms, "978SolMate"))
						{
							// copy the phone number into ram
							memset(phone_number, '\0', MAX_PHONE_LENGTH);
							strncpy(phone_number, begin_ptr_phone, end_ptr_phone - begin_ptr_phone);

							// Now copy it into flash memory
							flash_erase(PHONE_ADDRESS);
							flash_write_phone_number(phone_number, MAX_PHONE_LENGTH);

							// Send the user an acknowledgement
							P1OUT &= ~LED_MSP; // red LED off
							uart_command_state = CommandStatePreparePhoneSMS;
							tx_buffer_reset();
							strcpy(tx_buffer, "AT+CMGS=\"");
							strncat(tx_buffer, phone_number, MAX_PHONE_LENGTH);
							strcat(tx_buffer, "\"\r\n");
							uart_send_command();
						}
						// Status report?
//						else if(strncmp(begin_ptr_sms, "What's up", end_ptr_sms - begin_ptr_sms) == 0)// <-- NO!!
						else if(strstr(begin_ptr_sms, "What's up"))
						{
							// Send user the status report
							P1OUT &= ~LED_MSP;
							uart_command_state = CommandStatePrepareStatusSMS;
							tx_buffer_reset();
							strcpy(tx_buffer, "AT+CMGS=\"");
							strncat(tx_buffer, phone_number, MAX_PHONE_LENGTH);
							strcat(tx_buffer, "\"\r\n");
							uart_send_command();
						}
						else // Unrecognized text
						{
							P1OUT &= ~LED_MSP;
							uart_enter_idle_mode();
						}
					}
					else
						uart_enter_idle_mode();

					break;
				}

				case CommandStatePreparePhoneSMS:
				{
					P1OUT |= LED_MSP; // red LED on

					if(uart_command_result == UartResultInput)
					{
						// Send the text now
						uart_command_state = CommandStateSendPhoneSMS;
						tx_buffer_reset();
						strcpy(tx_buffer, "Msg from Sol-Mate: Your phone number has been successfully changed.\r\n\x1A");
						uart_send_command();
					}
					break;
				}

				case CommandStatePrepareStatusSMS:
				{
					P1OUT |= LED_MSP; // red led
					if(uart_command_result == UartResultInput)
					{
						// Put together the status text
						uart_command_state = CommandStateSendStatusSMS;
						tx_buffer_reset();
						strcpy(tx_buffer, "Msg from Sol-Mate: Here's your status report.\r\n");

						// Battery status
						if(battery_charge > 248) // 12.9V
							strcat(tx_buffer, "Battery level: Full\r\n");
						else if(battery_charge > 230) // About 50% - 12.55V
							strcat(tx_buffer, "Battery level: Medium\r\n");
						else if(battery_charge > 217) // 12.2V
							strcat(tx_buffer, "Battery level: Low\r\n");
						else
							strcat(tx_buffer, "Battery level: Very Low\r\n");
						
						// Solar panel charge
						if(solarpanel_voltage > 186)
							strcat(tx_buffer, "Charge rate: High\r\n");
						else if(solarpanel_voltage > 113)
							strcat(tx_buffer, "Charge rate: Medium\r\n");
						else if(solarpanel_voltage > 39)
							strcat(tx_buffer, "Charge rate: Low\r\n");
						else
							strcat(tx_buffer, "Charge rate: None\r\n");

						// Water depth
            // If a floatswitch is 0 and a higher floatswitch is 1, the reading is invalid.
						if(floatswitch_get_reading(floatswitches, 3) == 3) // all 3 (111)
							strcat(tx_buffer, "Water level: High\r\n");
						else if(floatswitch_get_reading(floatswitches, 3) == 2) // 2 (110)
							strcat(tx_buffer, "Water level: Medium\r\n");
						else if(floatswitch_get_reading(floatswitches, 3) == 1) // 1 (100)
							strcat(tx_buffer, "Water level: Low\r\n");
						else
							strcat(tx_buffer, "Water level: None\r\n");

						// Bailer
						if(pump_active)
							strcat(tx_buffer, "Water pump: On");
						else
							strcat(tx_buffer, "Water pump: Off");

						strcat(tx_buffer, "\r\n\x1A");
						uart_send_command();
					}
					break;
				}

				case CommandStateSendPhoneSMS:
				case CommandStateSendStatusSMS:
				{
					if(uart_command_result == UartResultOK)
						P1OUT &= ~LED_MSP; // red LED off

					uart_enter_idle_mode();
					break;
				}
			}

    		// Turn CPU off until someone calls LPM0_EXIT (uart interrupt handler will)
    		LPM0;
    	}
    }
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void timerA0_interrupt_handler()
{
	// Toggle float switch variable
//	floatswitch_active = (P6IN & INPUT_FLOATSWITCH) ? 0 : 1; // false means ground/zero -> switch pressed (1)

	// Check the switches
	floatswitches = 0;
	floatswitches |= (P1IN & FLOATSWITCH_0) ? 0 : 0x1; // false means ground -> switch active
	floatswitches |= (P1IN & FLOATSWITCH_1) ? 0 : 0x2; // false means ground -> switch active
	floatswitches |= (P1IN & FLOATSWITCH_2) ? 0 : 0x4; // false means ground -> switch active

	// Check water depth
	if(floatswitches > 0)// || water_depth > 70) // water depth values go from 0 to 255
	{
		if(battery_charge > 100) // around 40% (100 out of 255)
			pump_active = 1;
		else
		{
			pump_active = 0;

			if(floatswitches == 0x7) // All floatswitches are on (7 == 4+2+1)
			{
				// There is not enough charge and too much water, notify over text
				if(sent_text == 0 && uart_command_state == CommandStateIdle)
				{
					P1OUT |= LED_MSP; // red LED on

					// Is there a phone number programmed? If not then don't send the sms
					if(phone_number[0] != '\0')
					{
						// Send the text!!
						uart_command_state = CommandStatePrepareWarningSMS;
						tx_buffer_reset();
						strcpy(tx_buffer, "AT+CMGS=\"");
						strcat(tx_buffer, phone_number);
						strcat(tx_buffer, "\"\r\n");
						uart_send_command();

						sent_text = 1;
					}
				}
			}
		}
	}
	else // No water
		pump_active = 0;

	// Set water pump output and solar panel output
	if(pump_active)
	{
		P6OUT |= PUMP_CONTROL;
		P6OUT &= ~SOLARPANEL_CONTROL;
	}
	else
	{
		P6OUT &= ~PUMP_CONTROL;
		P6OUT |= SOLARPANEL_CONTROL;
	}

	// New conversion
	adc_start_conversion();
}

#pragma vector=TIMER1_A0_VECTOR // TA1CCR0 only
__interrupt void timerA1_interrupt_handler()
{
	// Stop timer
	TA1CTL = TACLR;

	// Set gsm power output back to input/floating mode
	P2DIR &= ~GSM_POWER_CONTROL;

	// Enable port1 interrupts again
	P1IE |= POWER_BUTTON;
}

#pragma vector=PORT1_VECTOR
__interrupt void port1_interrupt_handler()
{
	switch(P1IV)
	{
	case P1IV_P1IFG1:
	{
		toggle_gsm_power();
		P1IE &= ~POWER_BUTTON; // disable port1 interrupts until the timer is done
		break;
	}
	default:
		break;
	}
}

#pragma vector=ADC12_VECTOR
__interrupt void ADC_interrupt_handler()
{
	// Check the interrupt flags
	switch(ADC12IV)
	{
		case ADC12IV_ADC12IFG1: // All readings have finished
			battery_charge = ADC12MEM0; // Save reading
			solarpanel_voltage = ADC12MEM1; // save reading
			break;
		default:
			break;
	}
}
