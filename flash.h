/*
 * flash.h
 *
 *  Created on: Feb 21, 2016
 *      Author: Pat
 */

#ifndef FLASH_H_
#define FLASH_H_


#include <msp430.h>


#define PHONE_ADDRESS (char *)0xE000


// __DINT() is in IAR workbench
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


void flash_write(char * address, char * buffer) {
	_DINT();                             // Disable interrupts.
	int i = 0;
	FCTL2 = FWKEY + FSSEL_1 + FN0;       // Clk = SMCLK/4
	FCTL3 = FWKEY;                       // Clear Lock bit
	FCTL1 = FWKEY + WRT;                 // Set WRT bit for write operation

	for (i = 0; i < 64; ++i)
		*address++ = buffer[i];         // copy value to flash

	FCTL1 = FWKEY;                        // Clear WRT bit
	FCTL3 = FWKEY + LOCK;                 // Set LOCK bit
	_EINT();
}


#endif /* FLASH_H_ */
