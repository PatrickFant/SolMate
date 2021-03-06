/*
 * flash.h
 *
 *  Created on: Feb 21, 2016
 *      Author: Patrick Fant
 */

#ifndef FLASH_H_
#define FLASH_H_


#include <msp430f5529.h>


#define FLASH_BUFFER_SIZE 128
#define PHONE_ADDRESS (char *) 0x1900	// Address of phone number in memory.


/**
 * Erase flash segment pointed to by address.
 */
void flash_erase(char * address)
{
	_DINT();
	while(BUSY & FCTL3);
	FCTL1 = FWKEY + ERASE;
	FCTL3 = FWKEY;

	// Erase flash segment.
	*address = 0;

	while(BUSY & FCTL3);
	FCTL1 = FWKEY;
	FCTL3 = FWKEY + LOCK;
	_EINT();
}


/**
 * Write buffer to flash segment pointed to by address.
 */
void flash_write(char * address, char * buffer)
{
	_DINT();
	FCTL3 = FWKEY;
	FCTL1 = FWKEY + WRT;

	// Copy buffer into memory.
	int i;
	for (i = 0; i < FLASH_BUFFER_SIZE; ++i)
		*address++ = buffer[i];

	FCTL1 = FWKEY;
	FCTL3 = FWKEY + LOCK;
	_EINT();
}


/**
 * Write phone number to flash memory.
 */
void flash_write_phone_number(char * phone_number, unsigned char max_length)
{
	char buffer[FLASH_BUFFER_SIZE] = {0};

	// Copy phone number into buffer.
	int i;
	for (i = 0; i < max_length; ++i)
		buffer[i] = phone_number[i];

	flash_write(PHONE_ADDRESS, buffer);
}


#endif /* FLASH_H_ */
