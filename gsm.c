/*
 * gsm.c
 *
 * Created on: Feb 25, 2016
 *     Author: Patrick Fant
 */


#include "gsm.h"
#include "flash.h"
#include "msp430f5529.h"
#include "uart.h"
#include <string.h>


/**
 * Update command state and transmit message to GSM module.
 */
void gsm_send_message(char * str)
{
  if (uart_command_result == UartResultInput)
  {
    P1OUT |= LED_MSP;
    uart_command_state = CommandStateSendSMS;
    tx_buffer_reset();
    strcpy(tx_buffer, str);
    strcat(tx_buffer, "\r\n\x1A");
    uart_send_command();
  }
}


/**
 * Update command state, load AT message into UART tx buffer, and send to GSM 
 * module.
 */
void gsm_enter_sms_mode(void)
{
  if (uart_command_result == UartResultOK)
  {
    uart_command_state = CommandStateGoToSMSMode;
    tx_buffer_reset();
    strcpy(tx_buffer, "AT+CMGF=1\r\n");
    uart_send_command();
  }
}


/**
 * If AT returns OK, turn off red LED and idle UART.
 */
void gsm_prepare_for_tx(void)
{
  if (uart_command_result == UartResultOK)
  {
    P1OUT &= ~LED_MSP;
    uart_enter_idle_mode();
  }
}


/**
 * Update command state, load warning message into UART tx buffer, and send
 * to GSM module.
 */
void gsm_send_warning_message(void)
{
  gsm_send_message("Msg from Sol-Mate: Check your boat; "
                   "water level is getting high.");
}


/**
 * Handle unsolicited messages.
 */
void gsm_handle_unsolicited_message(void)
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
      return;
    }
    begin_ptr++; // should point to the beginning of the SMS index we need

    // Find the '\r' which is directly following the last character of the SMS index
    char *end_ptr = strchr(begin_ptr, '\r');
    if(!end_ptr) {
      uart_enter_idle_mode();
      return;
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
}


/**
 * Read SMS.
 */
void gsm_read_message(void)
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
      return;
    }
    begin_ptr_phone += 2; // Move to the beginning of the number

    // find the ending quotation mark
    char *end_ptr_phone = strchr(begin_ptr_phone, '"');
    if(!end_ptr_phone) {
      uart_enter_idle_mode();
      return;
    }

    // Check if it's too long
    if(end_ptr_phone - begin_ptr_phone > MAX_PHONE_LENGTH) {
      uart_enter_idle_mode();
      return;
    }

    // Look at the contents of the text - it starts right after the first \r\n
    char *begin_ptr_sms = strchr(rx_buffer, '\n');
    if(!begin_ptr_sms) {
      uart_enter_idle_mode();
      return;
    }
    begin_ptr_sms++; // Move to the beginning of the text

    // The text ends right before the next \r\n
    char *end_ptr_sms = strchr(begin_ptr_sms, '\r');
    if(!end_ptr_sms) {
      uart_enter_idle_mode();
      return;
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
      P1OUT &= ~LED_MSP; // red LED off
      uart_command_state = CommandStatePreparePhoneSMS;
      tx_buffer_reset();
      strcpy(tx_buffer, "AT+CMGS=\"");
      strncat(tx_buffer, phone_number, MAX_PHONE_LENGTH);
      strcat(tx_buffer, "\"\r\n");
      uart_send_command();
    }
    // Status report?
//            else if(strncmp(begin_ptr_sms, "What's up", end_ptr_sms - begin_ptr_sms) == 0)// <-- NO!!
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
}


/**
 * Prepare phone SMS.
 */
void gsm_send_confirmation_message(void)
{
  gsm_send_message("Msg from Sol-Mate: Your phone number has been successfully"
                   "changed.");
}


/**
 * Prepare status SMS.
 */
void gsm_send_status_message(void)
{
  char str[120];
  
  strcpy(str, "Msg from Sol-Mate: Here's your status report.\r\n");
  
  if (battery_charge > 230)
    strcat(str, "Battery level: Full -- ");
  else if (battery_charge > 100)
    strcat(str, "Battery level: Medium -- ");
  else
    strcat(str, "Battery level: Low -- ");
  
  if (solarpanel_voltage > 230)
    strcat(str, "Charge rate: High -- ");
  else if (solarpanel_voltage > 100)
    strcat(str, "Charge rate: Medium -- ");
  else if (solarpanel_voltage > 30)
    strcat(str, "Charge rate: Low -- ");
  else
    strcat(str, "Charge rate: None -- ");
  
  // All cases?
  if (floatswitches == 0x7) // all 3
    strcat(str, "Water level: High");
  else if (floatswitches == 0x3) // 2
    strcat(str, "Water level: Medium");
  else if (floatswitches == 0x1) // 1
    strcat(str, "Water level: Medium");
  else
    strcat(str, "Water level: None");
  
  gsm_send_message(str);
}
