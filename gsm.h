/*
 * gsm.h
 *
 * Created on: Feb 25, 2016
 *     Author: Patrick Fant
 */


#ifndef GSM_H_
#define GSM_H_


/**
 * Send arbitrary message.
 */
void gsm_send_message(char * str);


/**
 * Put GSM module into SMS mode.
 */
void gsm_enter_sms_mode(void);


/**
 * Get system ready to send text.
 */
void gsm_prepare_for_tx(void);


/**
 * Send warning message to GSM module.
 */
void gsm_send_warning_message(void);


/**
 * Handle unsolicited messages.
 */
void gsm_handle_unsolicited_message(void);


/**
 * Read SMS.
 */
void gsm_read_message(void);


/**
 * Prepare phone SMS.
 */
void gsm_send_confirmation_message(void);


/**
 * Prepare status SMS.
 */
void gsm_send_status_message(void);


#endif /* GSM_H_ */
