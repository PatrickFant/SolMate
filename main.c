#include "msp430f5529.h"
#include "definitions.h"
#include "uart.h"
#include "adc.h"
#include "flash.h"
#include "rtc.h"
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
volatile char battery_can_drain; // 0 -> need to wait for bat to charge to use pump
                                  // 1 -> can pump until battery reaches its lower threshold

// Keep track of time
volatile char tryagain_timeelapsed; // counts units of time (we can specify how long the units are)
volatile unsigned long last_sent_warningtext; // the time when we last sent out a warning text message

// Toggles power for the GSM module.
void toggle_gsm_power(void);

// Returns an int representing the water level, so long as the floatswitch
// reading is valid.
int get_water_level(char switches, int number_of_switches);


// Phone numba
#define MAX_PHONE_LENGTH 16
char phone_number[MAX_PHONE_LENGTH]; // like +14445556666

int main(void)
{
  // Stop watchdog timer for now
  WDTCTL = WDTPW | WDTHOLD;

  // Enable JTAG (keep this line here)
  SYSCTL |= SYSJTAGPIN;

  // Initialize state variables
//floatswitch_active = 0;
  floatswitches = 0;
  battery_charge = 0;
  solarpanel_voltage = 0;
  pump_active = 0;
  tryagain_timeelapsed = 0;
  last_sent_warningtext = 0;

  // Read in the saved phone number from memory, if it is there
  memset(phone_number, '\0', MAX_PHONE_LENGTH);
  if(strncmp(PHONE_ADDRESS, "+1", 2) == 0) // Phone numbers start with +1
    strncpy(phone_number, PHONE_ADDRESS, MAX_PHONE_LENGTH); // copy from flash into ram

  // Set up float switches
  FLOAT_PORT_DIR &= ~(FLOATSWITCH_0 | FLOATSWITCH_1 | FLOATSWITCH_2 | FLOATSWITCH_3 | FLOATSWITCH_4);
  FLOAT_PORT_REN |= FLOATSWITCH_0 | FLOATSWITCH_1 | FLOATSWITCH_2 | FLOATSWITCH_3 | FLOATSWITCH_4;
  FLOAT_PORT_OUT |= FLOATSWITCH_0 | FLOATSWITCH_1 | FLOATSWITCH_2 | FLOATSWITCH_3 | FLOATSWITCH_4;

  // Set up water pump and solarpanel on/off
  PUMPSOLAR_PORT_DIR |= PUMP_CONTROL | SOLARPANEL_CONTROL;
  PUMPSOLAR_PORT_OUT &= ~(PUMP_CONTROL | SOLARPANEL_CONTROL);

  // Set up msp430 LEDs
  LED_PORT_DIR |= (LED_MSP | LED_MSP_2);
  LED_PORT_OUT &= ~(LED_MSP | LED_MSP_2);
//  P1DIR |= LED_MSP;
//    P4DIR |= LED_MSP_2;
//  P1OUT &= ~LED_MSP;
//    P4OUT &= ~LED_MSP_2;

  // Set up gsm 'power button' (not used now)
//  P1DIR &= ~POWER_BUTTON;
//  P1REN |= POWER_BUTTON;
//  P1OUT |= POWER_BUTTON;
//  P1IE |= POWER_BUTTON; // interrupts
//  P1IES |= POWER_BUTTON; // high->low transition

  // Set up gsm power status input
  GSM_PORT_DIR &= ~GSM_POWER_STATUS; // input

  // Power on the voltage regulator for the gsm
  GSMPOWER_PORT_DIR |= GSMPOWER_ENABLE_PIN; // output mode
  GSMPOWER_PORT_OUT |= GSMPOWER_ENABLE_PIN;

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

  // Check if GSM module is on
  while(!(GSM_PORT_IN & GSM_POWER_STATUS)) // is off
  {
    toggle_gsm_power();
    __delay_cycles(20000000); // wait
  }

  // Send an AT first
	LED_PORT_OUT |= LED_MSP;

	tx_buffer_reset();
  strcpy(tx_buffer, "AT\r\n");
  uart_send_command();

  // Start up Timer A0
  TA0CTL = TACLR; // clear first
  TA0CTL = TASSEL__ACLK | ID__8 | MC__STOP; // auxiliary clock (32.768 kHz), divide by 8 (4096 Hz), interrupt enable, stop mode
  TA0CCTL0 = CCIE; // enable capture/compare interrupt
  TA0CCR0 = 4096; // reduces rate to 1 times/sec
  TA0CTL |= MC__UP; // start the timer in up mode (counts to TA0CCR0 then resets to 0)

  // start the clock
  rtc_initialize();

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
          LED_PORT_OUT &= ~(LED_MSP | LED_MSP_2); // leds off

          // We are now ready to send a text whenever the system needs to
          uart_enter_idle_mode();
        }
        else
          uart_enter_idle_mode();
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
          LED_PORT_OUT &= ~LED_MSP; // red LED off
          sent_text = 1; // Do not send the text again (this is for testing purposes--to send another text you have to restart the MSP)

          // Delete all stored messages.
          uart_command_state = CommandStateDeleteSMS;
          tx_buffer_reset();
          strcpy(tx_buffer, "AT+CMGD=1,4\r\n");
          uart_send_command();
        }
        else if(uart_command_result == UartResultError) // sms failed to send
        {
          LED_PORT_OUT |= LED_MSP;

          // Set up timer to try again
          TA2CTL = TACLR;
          TA2CTL = TASSEL__ACLK | ID__8 | MC__STOP;
          TA2CCTL0 = CCIE;
          TA2CCR0 = TIMEOUT_SMS;

          // Prepare again
          uart_command_state = CommandStatePrepareWarningSMS;
          tx_buffer_reset();
          strcpy(tx_buffer, "AT+CMGS=\"");
          strcat(tx_buffer, phone_number);
          strcat(tx_buffer, "\"\r\n");

          // Enable timer, go to sleep (uart will be disabled too because LPM2)
          TA2CTL |= MC__UP;
          LPM2;
        }
        else
          uart_enter_idle_mode();

        break;
      }

      case CommandStateUnsolicitedMsg: // Received a message from the cell module
      {
        LED_PORT_OUT |= LED_MSP; // red LED on

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
          LED_PORT_OUT &= ~LED_MSP; // red LED on
          uart_command_state = CommandStateReadSMS;
          uart_send_command();
        }
        else // unrecognized
        {
          LED_PORT_OUT &= ~LED_MSP;
          uart_enter_idle_mode();
        }

        break;
      }

      case CommandStateReadSMS:
      {
        LED_PORT_OUT |= LED_MSP; // red LED on
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
          if(strstr(begin_ptr_sms, "978SolMate"))
          {
            // copy the phone number into ram
            memset(phone_number, '\0', MAX_PHONE_LENGTH);
            strncpy(phone_number, begin_ptr_phone, end_ptr_phone - begin_ptr_phone);

            // Now copy it into flash memory
            flash_erase(PHONE_ADDRESS);
            flash_write_phone_number(phone_number, MAX_PHONE_LENGTH);

            // Send the user an acknowledgement
            LED_PORT_OUT &= ~LED_MSP; // red LED off
            uart_command_state = CommandStatePreparePhoneSMS;
            tx_buffer_reset();
            strcpy(tx_buffer, "AT+CMGS=\"");
            strncat(tx_buffer, phone_number, MAX_PHONE_LENGTH);
            strcat(tx_buffer, "\"\r\n");
            uart_send_command();
          }
          // Status report?
          else if(strstr(begin_ptr_sms, "What's up"))
          {
            // Send user the status report
            LED_PORT_OUT &= ~LED_MSP;
            uart_command_state = CommandStatePrepareStatusSMS;
            tx_buffer_reset();
            strcpy(tx_buffer, "AT+CMGS=\"");
            strncat(tx_buffer, phone_number, MAX_PHONE_LENGTH);
            strcat(tx_buffer, "\"\r\n");
            uart_send_command();
          }
          else // Unrecognized text
          {
            LED_PORT_OUT &= ~LED_MSP;

            // Delete all stored messages.
            uart_command_state = CommandStateDeleteSMS;
            tx_buffer_reset();
            strcpy(tx_buffer, "AT+CMGD=1,4\r\n");
            uart_send_command();
          }
        }
        else
          uart_enter_idle_mode();

        break;
      }

      case CommandStatePreparePhoneSMS:
      {
        LED_PORT_OUT |= LED_MSP; // red LED on

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
        LED_PORT_OUT |= LED_MSP; // red led
        if(uart_command_result == UartResultInput)
        {
          // Put together the status text
          uart_command_state = CommandStateSendStatusSMS;
          tx_buffer_reset();
          strcpy(tx_buffer, "Msg from Sol-Mate: Here's your status report.\r\n");

          // Battery status
          if(battery_charge > 228) // 12.9V
            strcat(tx_buffer, "Battery level: Full\r\n");
          else if(battery_charge > 210) // About 50% - 12.55V
            strcat(tx_buffer, "Battery level: Medium\r\n");
          else if(battery_charge > 190) // 12.2V
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
          int water_level = get_water_level(floatswitches, 5);
          switch(water_level)
          {
            case 0: // No floatswitches are active.
              strcat(tx_buffer, "Water level: None\r\n");
              break;
            case 1: // Lowest floatswitch is active.
              strcat(tx_buffer, "Water level: Very low\r\n");
              break;
            case 2: // Two lowest floatswitches are active.
              strcat(tx_buffer, "Water level: Low\r\n");
              break;
            case 3: // All three floatswitches are active.
              strcat(tx_buffer, "Water level: Medium\r\n");
              break;
            case 4:
              strcat(tx_buffer, "Water level: High\r\n");
              break;
            case 5:
              strcat(tx_buffer, "Water level: Very high\r\n");
              break;
            default: // Any other combination.
              strcat(tx_buffer, "Water level: ERR INVALID READING\r\n");
              break;
          }

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
      {
        if(uart_command_result == UartResultOK)
        {
          // Delete all stored messages.
          uart_command_state = CommandStateDeleteSMS;
          tx_buffer_reset();
          strcpy(tx_buffer, "AT+CMGD=1,4\r\n");
          uart_send_command();
        }
        else if(uart_command_result == UartResultError) // sms failed to send
        {
          LED_PORT_OUT |= LED_MSP;

          // Set up timer to try again
          TA2CTL = TACLR;
          TA2CTL = TASSEL__ACLK | ID__8 | MC__STOP;
          TA2CCTL0 = CCIE;
          TA2CCR0 = TIMEOUT_SMS; // 4096 times 10 -> 10 seconds

          // Prepare again
          uart_command_state = CommandStatePreparePhoneSMS;
          tx_buffer_reset();
          strcpy(tx_buffer, "AT+CMGS=\"");
          strcat(tx_buffer, phone_number);
          strcat(tx_buffer, "\"\r\n");

          // Enable timer, go to sleep (uart will be disabled too because LPM2)
          TA2CTL |= MC__UP;
          LPM2;
        }

        break;
      }
      case CommandStateSendStatusSMS:
      {
        if(uart_command_result == UartResultOK)
        {
          // Delete all stored messages.
          uart_command_state = CommandStateDeleteSMS;
          tx_buffer_reset();
          strcpy(tx_buffer, "AT+CMGD=1,4\r\n");
          uart_send_command();
        }
        else if(uart_command_result == UartResultError) // sms failed to send
        {
          LED_PORT_OUT |= LED_MSP;

          // Set up timer to try again
          TA2CTL = TACLR;
          TA2CTL = TASSEL__ACLK | ID__8 | MC__STOP;
          TA2CCTL0 = CCIE;
          TA2CCR0 = TIMEOUT_SMS; // 4096 times 10 -> 10 seconds

          // Prepare again
          uart_command_state = CommandStatePrepareStatusSMS;
          tx_buffer_reset();
          strcpy(tx_buffer, "AT+CMGS=\"");
          strcat(tx_buffer, phone_number);
          strcat(tx_buffer, "\"\r\n");

          // Enable timer, go to sleep (uart will be disabled too because LPM2)
          TA2CTL |= MC__UP;
          LPM2;
        }

        break;
      }

      case CommandStateDeleteSMS:
      {
        if(uart_command_result == UartResultOK)
          LED_PORT_OUT &= ~LED_MSP; // red LED off

        uart_enter_idle_mode();
        break;
      }
    }

    // Turn CPU off until someone calls LPM0_EXIT (uart interrupt handler will)
    LPM0;
    }
  }
}


// FUNCTIONS ==================================================================


void toggle_gsm_power(void)
{
	// Set output to be LOW
	GSM_PORT_OUT &= ~GSM_POWER_CONTROL; // low
	GSM_PORT_DIR |= GSM_POWER_CONTROL; // output mode

	// Start timer and run for 1.5 seconds, and call the interrupt handler when it's done
	TA1CTL = TACLR;
	TA1CTL = TASSEL__ACLK | ID__8 | MC__STOP; // aux clock, divide by 8 (so 4096 hz)
	TA1CCTL0 = CCIE; // interrupt enable for ccr0
	TA1CCR0 = 6144 - 1; // 1.5 secs (4096 * 1.5)
	TA1CTL |= MC__UP; // activate timer
}


/**
 * A floatswitch reading is valid if no active switch is higher than an
 * inactive switch.  This function iterates through the floatswitches from
 * lowest to highest and takes note when it encounters an inactive switch so as
 * to identify erroneous readings.
 */
int get_water_level(char switch_states, int number_of_switches)
{
  bool inactive_switch_found = false;
  int active_switch_count = 0;
  
  int i;
  for (i = 0; i < number_of_switches; ++i)
  {
    bool switch_is_active = (switch_states >> i) & 1;
    if (switch_is_active && !inactive_switch_found)
      ++active_switch_count;
    else if (switch_is_active && inactive_switch_found)
      return -1;
    else // switch is inactive
      inactive_switch_found = true;
  }
  return active_switch_count;
}


// INTERRUPT HANDLERS =========================================================


#pragma vector=TIMER0_A0_VECTOR
__interrupt void timerA0_interrupt_handler()
{
  // Get the current time (seconds since the msp started)
  unsigned long current_time = RTCTIM1;
  current_time <<= 16;
  current_time += RTCTIM0;

	// Check the switches
	floatswitches = 0;
	floatswitches |= (P1IN & FLOATSWITCH_0) ? 0x1 : 0; // false means ground -> switch NOT active
	floatswitches |= (P1IN & FLOATSWITCH_1) ? 0x2 : 0;
	floatswitches |= (P1IN & FLOATSWITCH_2) ? 0x4 : 0;
	floatswitches |= (P1IN & FLOATSWITCH_3) ? 0x8 : 0;
	floatswitches |= (P1IN & FLOATSWITCH_4) ? 0x10 : 0;

	// Figure out whether the bat is low or not
	if(battery_charge > BATTERY_THRESHOLD_HIGH)
	  battery_can_drain = 1;
	else if(battery_charge < BATTERY_THRESHOLD_LOW)
	  battery_can_drain = 0;

	// Check water depth
	if(floatswitches > 0)
	{
		if(battery_charge > BATTERY_THRESHOLD_HIGH || (battery_charge > BATTERY_THRESHOLD_LOW && battery_can_drain))
      pump_active = 1;
		else
		{
			pump_active = 0;

			if(get_water_level(floatswitches, 5) >= 2)
			{
				// There is not enough charge and too much water, notify over text
			  // 0x15180 is 86400 (seconds)
				if(uart_command_state == CommandStateIdle && current_time - last_sent_warningtext > 0x15180)
				{
				  LED_PORT_OUT |= LED_MSP; // red LED on

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

						// save the current time
						last_sent_warningtext = current_time;
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
		PUMPSOLAR_PORT_OUT |= PUMP_CONTROL;
		PUMPSOLAR_PORT_OUT &= ~SOLARPANEL_CONTROL;
	}
	else
	{
	  PUMPSOLAR_PORT_OUT &= ~PUMP_CONTROL;
	  PUMPSOLAR_PORT_OUT |= SOLARPANEL_CONTROL;
	}

	// New conversion
	adc_start_conversion();
}

#pragma vector=TIMER1_A0_VECTOR // TA1CCR0 only
__interrupt void timerA1_interrupt_handler()
{
	// Stop timer
	TA1CTL |= MC__STOP;

	// Set gsm power output back to input/floating mode
	GSM_PORT_DIR &= ~GSM_POWER_CONTROL;
}

#pragma vector=TIMER2_A0_VECTOR
__interrupt void timerA2_interrupt_handler() // for TA2CCR0 only
{
  if(tryagain_timeelapsed >= 8) // if TIMEOUT_SMS is 15 seconds then this is 2 minutes (8*15 sec)
  {
    // Reset time counter and stop timer
    TA2CTL |= MC__STOP;
    tryagain_timeelapsed = 0;

    // Finished waiting, now try again and send the command
    LED_PORT_OUT &= ~LED_MSP;
    uart_send_command();
    LPM2_EXIT;
  }
  else
    tryagain_timeelapsed++;
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
