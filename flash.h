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
 * Always call this before flash_write or flash_write_phone_number.
 */
void flash_erase(char * address);

/**
 * Write buffer to flash segment pointed to by address.
 * Buffer is the size of an information memory segment.
 */
void flash_write(char * address, char * buffer);

/**
 * Write phone number to flash memory.
 * The segment written to is specified by PHONE_ADDRESS.
 * max_length sets the maximum number of digits for a phone number.
 */
void flash_write_phone_number(char * phone_number, unsigned char max_length);


#endif /* FLASH_H_ */
