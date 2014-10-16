/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2013, 2014 Range Networks, Inc.
 * 
 * This software is distributed under the terms of the GNU General Public 
 * License version 3. See the COPYING and NOTICE files in the current
 * directory for licensing information.
 * 
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#include "rnrad1Core.h"

/*
	TODO : lots of copied bits from RAD1CMD and RAD1SN, could definitely be improved
		by simplifying the hex<->dec parsing. Comments from rad1_setup.sh inline.
*/
class FactoryCalibration {

	private:

	rnrad1Core * core;
	unsigned int sdrsn;
	unsigned int rfsn;
	unsigned int band;
	unsigned int freq;
	unsigned int rxgain;
	unsigned int txgain;

	static int hexval (char ch);
	static unsigned char * hex_string_to_binary(const char *string, int *lenptr);
	bool i2c_write(int i2c_addr, char *hex_string);
	std::string i2c_read(int i2c_addr, int len);
	unsigned int hex2dec(std::string hex);

	public:

	unsigned int getValue(std::string name);
	void readEEPROM();
};
