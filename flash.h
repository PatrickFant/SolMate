/*
 * flash.h
 *
 *  Created on: Feb 21, 2016
 *      Author: Pat
 */

#ifndef FLASH_H_
#define FLASH_H_


#include <msp430f5529.h>


#define FLASH_BUFFER_SIZE 128


// Address of phone number in memory.
volatile char * phone_address = (char *) 0x1980;


// Erase block of flash memory pointed to by address.
void flash_erase(char * address) {
	_DINT();                             // Disable interrupts.
	while(BUSY & FCTL3);                 // Check if Flash being used
	FCTL2 = FWKEY + FSSEL_1 + FN3;       // Clk = SMCLK/4
	FCTL1 = FWKEY + ERASE;               // Set Erase bit
	FCTL3 = FWKEY;                       // Clear Lock bit
	*address = 0;                           // Dummy write to erase Flash segment
	while(BUSY & FCTL3);                 // Check if Flash being used
	FCTL1 = FWKEY;                       // Clear WRT bit
	FCTL3 = FWKEY + LOCK;                // Set LOCK bit
	_EINT();
}


// Write buffer to block of flash memory pointed to by address.
void flash_write(char * address, char * buffer) {
	_DINT();                             // Disable interrupts.
	int i = 0;
	FCTL2 = FWKEY + FSSEL_1 + FN0;       // Clk = SMCLK/4
	FCTL3 = FWKEY;                       // Clear Lock bit
	FCTL1 = FWKEY + WRT;                 // Set WRT bit for write operation

	for (i = 0; i < FLASH_BUFFER_SIZE; ++i)
		*address++ = buffer[i];         // copy value to flash

	FCTL1 = FWKEY;                        // Clear WRT bit
	FCTL3 = FWKEY + LOCK;                 // Set LOCK bit
	_EINT();
}


// Write phone number to flash memory.
void flash_write_phone_number(char * phone_number, unsigned char max_length) {
	char buffer[FLASH_BUFFER_SIZE] = {0};
	int i;
	// Write phone_number into buffer.
	for (i = 0; i < max_length; ++i)
		buffer[i] = phone_number[i];
	flash_write(phone_address, buffer);
}


#endif /* FLASH_H_ */
