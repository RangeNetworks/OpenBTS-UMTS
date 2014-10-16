/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef UMTSL1CONST_H
#define UMTSL1CONST_H
#include <stdint.h>

namespace UMTS {

unsigned findTfci(
	float *rawTfciAccumulator,	// Of size [gUlRawTfciSize]
	unsigned numTfcis);

class TrCHConsts {

	public:

	// 25.212, 4.2.1
	static const uint64_t mgcrc24 = 0x1800063;
	static const uint64_t mgcrc16 = 0x11021;
	static const uint64_t mgcrc12 = 0x180f;
	static const uint64_t mgcrc8 = 0x19b;
	static const unsigned sMaxTfci = 256;	// Maximum TFCI we will ever use is 8 bits.
	static const int inter1Columns[4];
	static const char inter1Perm[4][8];
	static const char inter2Perm[30];
	// TFCI can be up to 10 bits, but we wont use them all.
	static uint32_t sTfciCodes[sMaxTfci];	// Table for up to 8 bit tfci, plenty for us.
	static void initTfciCodes();

	// These are the pre-computed pilot patterns for Npilot =2,4,8,16.
	static uint16_t sDlPilotBitPattern[4][15];
	static const bool reedMullerTable[32][10];
	static void initPilotBitPatterns();
	static bool oneTimeInit;

	TrCHConsts();

};

};	// namespace
#endif
