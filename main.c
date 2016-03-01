#include "msp430f5529.h"
#include "definitions.h"
#include "uart.h"
#include "adc.h"
#include "flash.h"
#include "gsm.h"
#include <string.h>

/*
 * main.c
 */

// State variables
volatile char floatswitches; // Contains water depth value (each bit represents a float switch)
volatile char battery_charge; // Contains battery charge value
volatile char solarpanel_voltage; // panel voltage
volatile char pump_active; // Controls the water pump (0 = off, 1 = on)

// Phone numba
#define MAX_PHONE_LENGTH 16
char phone_number[MAX_PHONE_LENGTH]; // like +14445556666

int main(void)
{
  WDTCTL = WDTPW | WDTHOLD; // Stop watchdog timer for now

  // Initialize state variables
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
  P1OUT &= ~LED_MSP;

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
				case CommandStateTurnOffEcho:       // Got a response after sending AT
				  gsm_enter_sms_mode();
					break;
				case CommandStateGoToSMSMode:       // Got a response after sending CMGF
        case CommandStateSendWarningSMS:    // Got a response after sending the text
        case CommandStateSendSMS:
				  gsm_prepare_for_tx();
					break;
				case CommandStatePrepareWarningSMS: // Got a response after sending CMGS
					gsm_send_warning_message();
					break;
				case CommandStateUnsolicitedMsg:    // Received a message from the cell module
				  gsm_handle_unsolicited_message();
					break;
				case CommandStateReadSMS:
				  gsm_read_message();
					break;
				case CommandStatePreparePhoneSMS:
				  gsm_send_confirmation_message();
				  break;
				case CommandStatePrepareStatusSMS:
				  gsm_send_status_message();
				  break;
        default:
          break;
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

// should move this to adc.c sometime
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
