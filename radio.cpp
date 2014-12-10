/*
 * radio.cpp
 *
 *  Created on: Dec 10, 2014
 *      Author: Lukas
 */

#include <msp430.h>
#include "nRF24L01.h"
#include "spi.h"

void _writeReg(unsigned char addr, unsigned char val)
{

	spi_csl();
	spi_xfer_byte(RF24_W_REGISTER | addr);
	spi_xfer_byte(val);
	spi_csh();
}

int read_reg (unsigned char addr){
	spi_csl();
	spi_xfer_byte(RF24_R_REGISTER | addr);
	int out = spi_xfer_byte(RF24_NOP);
	spi_csh();
	return out;
}

void _writeRegMultiLSB(unsigned char addr, unsigned char *buf, int len)
{
	unsigned char i;

	spi_csl();
	spi_xfer_byte(RF24_W_REGISTER | addr);
	for (i=0; i<len; i++) {
		spi_xfer_byte(buf[len-i-1]);
	}
	spi_csh();
}

void sendMsg(unsigned char msg){
	spi_csl();
	spi_xfer_byte(RF24_W_TX_PAYLOAD);
	int out = spi_xfer_byte(msg);
	spi_csh();

}

int recvMsg(){
	spi_csl();
	spi_xfer_byte(RF24_R_RX_PAYLOAD);
	int out = spi_xfer_byte(RF24_NOP);
	spi_csh();
	return out;

}
