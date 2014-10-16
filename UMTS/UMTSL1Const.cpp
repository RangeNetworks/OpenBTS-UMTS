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

#include "Defines.h"
#include "URRCDefs.h"
#include "UMTSL1Const.h"
#include "UMTSCommon.h"
namespace UMTS {


unsigned TTICode2NumFrames(TTICodes ttiCode)	// return 1,2,4,8
{
	switch (ttiCode) {
	case TTI10ms: return 1;
	case TTI20ms: return 2;
	case TTI40ms: return 4;
	case TTI80ms: return 8;
	default: assert(0);
	}
}
unsigned TTICode2TTI(TTICodes ttiCode)			// return 10,20,40,80
{
	return TTICode2NumFrames(ttiCode)*10;
}

TTICodes TTI2TTICode(unsigned tti)
{
	switch (tti) {
	case 10: return TTI10ms;
	case 20: return TTI20ms;
	case 40: return TTI40ms;
	case 80: return TTI80ms;
	default:
		LOG(ERR) <<"Invalid TTI:"<<tti;
		assert(0);	// kinda harsh, but it should be fixed before we proceed.
	}
}

void TrCHConsts::initPilotBitPatterns()
{
	// 25.211 5.3.2 Table 12: Pilot bit patterns for downlink DPCCH with Npilot = 2, 4, 8 and 16
	//
	//      Npilot=2 Npilot=4   Npilot=8          Npilot=16
	//Symbol   0       0  1    0  1  2  3     0  1  2  3  4  5  6  7
	//   #
	//Slot #0  11     11 11   11 11 11 10    11 11 11 10 11 11 11 10
	//   1     00     11 00   11 00 11 10    11 00 11 10 11 11 11 00
	//   2     01     11 01   11 01 11 01    11 01 11 01 11 10 11 00
	//   3     00     11 00   11 00 11 00    11 00 11 00 11 01 11 10
	//   4     10     11 10   11 10 11 01    11 10 11 01 11 11 11 11
	//   5     11     11 11   11 11 11 10    11 11 11 10 11 01 11 01
	//   6     11     11 11   11 11 11 00    11 11 11 00 11 10 11 11
	//   7     10     11 10   11 10 11 00    11 10 11 00 11 10 11 00
	//   8     01     11 01   11 01 11 10    11 01 11 10 11 00 11 11
	//   9     11     11 11   11 11 11 11    11 11 11 11 11 00 11 11
	//  10     01     11 01   11 01 11 01    11 01 11 01 11 11 11 10
	//  11     10     11 10   11 10 11 11    11 10 11 11 11 00 11 10
	//  12     10     11 10   11 10 11 00    11 10 11 00 11 01 11 01
	//  13     00     11 00   11 00 11 11    11 00 11 11 11 00 11 00
	//  14     00     11 00   11 00 11 11    11 00 11 11 11 10 11 01

	// These are from columns 1,3,5,7 for nPilot==16; all the others columns are just copies.
	// Table 19: Pilot Symbol Pattern for SCCPCH is identical.
	static uint8_t sDlPilotBitPatternTable[4][gFrameSlots] = {
		{ 3, 0, 1, 0, 2, 3, 3, 2, 1, 3, 1, 2, 2, 0, 0 },	// column 1
		{ 2, 2, 1, 0, 1, 2, 0, 0, 2, 3, 1, 3, 0, 3, 3 },	// column 3
		{ 3, 3, 2, 1, 3, 1, 2, 2, 0, 0, 3, 0, 1, 0, 2 },	// column 5
		{ 2, 0, 0, 2, 3, 1, 3, 0, 3, 3, 2, 2, 1, 0, 1 }		// column 7
		};

	for (unsigned slot = 0; slot < gFrameSlots; slot++) {
		uint16_t col1 = sDlPilotBitPatternTable[0][slot];
		uint16_t col3 = sDlPilotBitPatternTable[1][slot];
		uint16_t col5 = sDlPilotBitPatternTable[2][slot];
		uint16_t col7 = sDlPilotBitPatternTable[3][slot];
		uint16_t pat;
		// Npilot=2
		sDlPilotBitPattern[0][slot] = col1;
		// Npilot=4
		sDlPilotBitPattern[1][slot] = pat = (3<<2)|col1;
		// Npilot=8
		sDlPilotBitPattern[2][slot] = pat = (pat<<4)|(3<<2)|col3;
		// Npilot=16
		sDlPilotBitPattern[3][slot] = (pat<<8)|(3<<6)|(col5<<4)|(3<<2)|col7;
	}
}

const int TrCHConsts::inter1Columns[4] = {
		1, // TTI = 10ms
		2, // TTI = 20ms
		4, // TTI = 40ms
		8 // TTI = 80ms
	};

	// 25.212 4.2.5.2 table 4: Inter-Column permutation pattern for 1st interleaving:
const char TrCHConsts::inter1Perm[4][8] = {
		{0}, // TTI = 10ms
		{0, 1}, // TTI = 20ms
		{0, 2, 1, 3}, // TTI = 40ms
		{0, 4, 2, 6, 1, 5, 3, 7} // TTI = 80ms
	};

	// 25.212 4.2.11 table 7: Inter-Column permutation pattern for 2nd interleaving:
const char TrCHConsts::inter2Perm[30] = {0,20,10,5,15,25,3,13,23,8,18,28,1,11,21,
		6,16,26,4,14,24,19,9,29,12,2,7,22,27,17};

	// (pat) 25.212 4.3.3 table 8: Magic for TFCI code encoding.
	// The codes are 32 words of 10 bits each, but I am generating it
	// form the original binary table in the spec (below)
	// to avoid any transcription errors.
const bool TrCHConsts::reedMullerTable[32][10] = {
		// i Mi,0 Mi,1 Mi,2 Mi,3 Mi,4 Mi,5 Mi,6 Mi,7 Mi,8 Mi,9
		/*0*/ {   1,   0,   0,   0,   0,   1,   0,   0,   0,   0 },
		/*1*/ {   0,   1,   0,   0,   0,   1,   1,   0,   0,   0 },
		/*2*/ {   1,   1,   0,   0,   0,   1,   0,   0,   0,   1 },
		/*3*/ {   0,   0,   1,   0,   0,   1,   1,   0,   1,   1 },
		/*4*/ {   1,   0,   1,   0,   0,   1,   0,   0,   0,   1 },
		/*5*/ {   0,   1,   1,   0,   0,   1,   0,   0,   1,   0 },
		/*6*/ {   1,   1,   1,   0,   0,   1,   0,   1,   0,   0 },
		/*7*/ {   0,   0,   0,   1,   0,   1,   0,   1,   1,   0 },
		/*8*/ {   1,   0,   0,   1,   0,   1,   1,   1,   1,   0 },
		/*9*/ {   0,   1,   0,   1,   0,   1,   1,   0,   1,   1 },
		/*10*/ {  1,   1,   0,   1,   0,   1,   0,   0,   1,   1 },
		/*11*/ {  0,   0,   1,   1,   0,   1,   0,   1,   1,   0 },
		/*12*/ {  1,   0,   1,   1,   0,   1,   0,   1,   0,   1 },
		/*13*/ {  0,   1,   1,   1,   0,   1,   1,   0,   0,   1 },
		/*14*/ {  1,   1,   1,   1,   0,   1,   1,   1,   1,   1 },
		/*15*/ {  1,   0,   0,   0,   1,   1,   1,   1,   0,   0 },
		/*16*/ {  0,   1,   0,   0,   1,   1,   1,   1,   0,   1 },
		/*17*/ {  1,   1,   0,   0,   1,   1,   1,   0,   1,   0 },
		/*18*/ {  0,   0,   1,   0,   1,   1,   0,   1,   1,   1 },
		/*19*/ {  1,   0,   1,   0,   1,   1,   0,   1,   0,   1 },
		/*20*/ {  0,   1,   1,   0,   1,   1,   0,   0,   1,   1 },
		/*21*/ {  1,   1,   1,   0,   1,   1,   0,   1,   1,   1 },
		/*22*/ {  0,   0,   0,   1,   1,   1,   0,   1,   0,   0 },
		/*23*/ {  1,   0,   0,   1,   1,   1,   1,   1,   0,   1 },
		/*24*/ {  0,   1,   0,   1,   1,   1,   1,   0,   1,   0 },
		/*25*/ {  1,   1,   0,   1,   1,   1,   1,   0,   0,   1 },
		/*26*/ {  0,   0,   1,   1,   1,   1,   0,   0,   1,   0 },
		/*27*/ {  1,   0,   1,   1,   1,   1,   1,   1,   0,   0 },
		/*28*/ {  0,   1,   1,   1,   1,   1,   1,   1,   1,   0 },
		/*29*/ {  1,   1,   1,   1,   1,   1,   1,   1,   1,   1 },
		/*30*/ {  0,   0,   0,   0,   0,   1,   0,   0,   0,   0 },
		/*31*/ {  0,   0,   0,   0,   1,   1,   1,   0,   0,   0 }
		};

void TrCHConsts::initTfciCodes()
{
	// Pre-compute the tfci code for each possible tfci.
	// Implements 4.3.3 verbatim.
	// Yes, I realize we could turn the table sideways and do this quickly,
	// but it is only done once, ever.
	for (unsigned tfci = 0; tfci < sMaxTfci; tfci++) {
		uint32_t result = 0;
		for (unsigned i = 0; i <= 31; i++) {
			unsigned bi = 0;
			for (unsigned n = 0; n <= 9; n++) {
				unsigned an = (tfci >> n) & 1;		// a0 is the lsb of tfci.
				bi += (an & reedMullerTable[i][n]);	// b0 is the lsb of the result.
			}
			result |= ((bi&1) << i);
		}
		sTfciCodes[tfci] = result;
	}
}

// This is the wonderfully redundant redundant C++ way to declare declare static members.
bool TrCHConsts::oneTimeInit = false;
uint16_t TrCHConsts::sDlPilotBitPattern[4][15];
uint32_t TrCHConsts::sTfciCodes[sMaxTfci];	// Table for up to 8 bit tfci, plenty for us.

//TrCHConsts::TrCHConsts(TTICodes wTTImsDiv10Log2) :mTTImsDiv10Log2(wTTImsDiv10Log2)
TrCHConsts::TrCHConsts()
{
	/**
	static uint16_t reedMullerCode[32];	// They are 10 bits long.
	for (unsigned w = 0; w < 32; w++) {
		unsigned code = 0;
		for (unsigned b = 0; b < 10; b++) {
			code = (code << 1) | !!reedMullerTable[w][b];
		}
		ReedMullerCode[w] = code;
	}
	**/

	if (!oneTimeInit) {
		oneTimeInit = true;
		initPilotBitPatterns();
		initTfciCodes();
	}

	//mInter1Columns = inter1Columns[mTTImsDiv10Log2];
	//mInter1Perm = inter1Perm[mTTImsDiv10Log2];
}

TrCHConsts gDummyTrCHConsts; // need to be declared somewhere once to fill up static members

// In uplink each slot has 2 TFCI bits which are concatenated to form 30 bits,
// from which we attempt to retrieve the original tfci.
// TODO: Probably want a proper Reed-Muller soft decoder
unsigned findTfci(
	float *rawTfciAccumulator,	// Of size [gUlRawTfciSize]
	unsigned numTfcis)
{
	float *bits = rawTfciAccumulator;

	unsigned bestTfci = 0;
	float bestMatch = 0;
	assert(numTfcis <= TrCHConsts::sMaxTfci);
	for (unsigned tfci = 0; tfci < numTfcis; tfci++) {
		uint32_t tfciCode = TrCHConsts::sTfciCodes[tfci];
		float thisMatch = 0;
		for (unsigned b = 0; b < 30; b++) {
			// The bits are transmitted in the slots LSB first, so start with LSB of tfciCode.
			unsigned wantbit = tfciCode & 1; tfciCode >>= 1;
			float havebit = RN_BOUND(bits[b],0.0,1.0);
			if (wantbit) {
				thisMatch += havebit;
			} else {
				thisMatch += 1.0 - havebit;
			}
		}
		// A perfect match would be 30.
		if (thisMatch > bestMatch) {
			bestMatch = thisMatch;
			bestTfci = tfci;
		}
	}
	return bestTfci;	// TODO: Regardless of how poor it is?
}

}; // namespace
