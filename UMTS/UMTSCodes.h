/**@file Objects for generating UMTS channelization, scrambling and sync codes, from 3GPP 25.213 Sections 4 & 5. */

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

#ifndef UMTSCODES_H
#define UMTSCODES_H

#include <BitVector.h>
#include "UMTSCommon.h"

namespace UMTS {

/**
  This class provides a global table of OVSF codes.
  This is a singleton; you only need one in the system.
  As unsigned char, the values are 0/1.
  3GPP 25.213 4.3.1.
*/
class OVSFTree {

	protected:

	int8_t** mCodeSets[10];		//< Each point in this array is another array storing a code set.


	public:

	/** Generate the code tree. */
	OVSFTree(void);

	//TODO -- We need a proper destructor for this to deallocate the memory.

	/** Return the OVSF code for the given spreading factor and channel index. */
	const int8_t* code(unsigned SFI, unsigned index);


	protected:

	/** From the code set at SFI-1, generate the code set for SFI. */
	void branch(unsigned SFI);

	/** Create the first branch for SF=0. */
	void seed();
};

extern UMTS::OVSFTree gOVSFTree;


/**
  This class provides a global table of Hadamard8 codes.
  This is a singleton; you only need one in the system.
  As unsigned char, the values are 0/1.
  3GPP 25.213 4.3.1.
*/
class Hadamard8 {

	protected:

	int8_t** mCodeSets[9];		//< Each point in this array is another array storing a code set.


	public:

	/** Generate the code tree. */
	Hadamard8(void);

	//TODO -- We need a proper destructor for this to deallocate the memory.

	/** Return the H_8 for the given spreading factor and channel index. */
	const int8_t* code(unsigned index);

	protected:

	/** From the code set at H_(N-1), generate the code set H_N. */
	void branch(unsigned dim);

	/** Create the first branch for N=0. */
	void seed();
};

extern UMTS::Hadamard8 gHadamard8;


/**
	Common scrambling code framework.
*/
class ScramblingCode {

	protected:

	SequenceGenerator32 mXGenerator;
	unsigned char *mXFBCode;		///< feedback path from LSB
	unsigned char *mXFFCode;		///< feed-forward path

	SequenceGenerator32 mYGenerator;
	unsigned char *mYFBCode;		///< feedback path from LSB
	unsigned char *mYFFCode;		///< feed-forward path

	int8_t *mICode;
	int8_t *mQCode;

	public:

	ScramblingCode(unsigned xCoeff, unsigned yCoeff, unsigned order, unsigned len = gFrameLen);

	~ScramblingCode();

	const int8_t* ICode() const { return mICode; }
	const int8_t* QCode() const { return mQCode; }

	protected:

	void generateXYSubcodes(SequenceGenerator32& gen, unsigned readMask, unsigned char* codeFB, unsigned char* codeFF, unsigned len = gFrameLen);

	void sumCodes(const unsigned char *codeX, const unsigned char* codeY, int8_t* codeC, unsigned len = gFrameLen);

};



/**
	Downlink scrambling codes.
	3GPP 25.213 5.2.2

	X coeff: 0,7: ...0 1000 0001: 0x081
	X read mask: 4,6,15: .... 0000 1000 0000 0101 0000: 0x08050

	Y coeff: 0,5,7,10: ...0100 1010 0001: 0x04a1
	Y read mask: 5,6,8-15: ....0 1111 1111 0110 0000: 0x0ff60
*/
class DownlinkScramblingCode : public ScramblingCode {

	public:

	DownlinkScramblingCode(unsigned N);

};





/**
	Uplink long scrambling codes.
	3GPP 25.213 4.3.2

	X coeff: 3,0: ...0001001: 0x09
	X read mask: 4,7,18: .... 0100 0000 0000 1001 0000: 0x040090

	Y coeff: 3,2,1,0: ...0001111: 0x0F
	Y read mask: 4,6,17: ....0010 0000 0000 0101 0000: 0x020050
*/
class UplinkScramblingCode : public ScramblingCode {


	public:

	UplinkScramblingCode(unsigned N);

	~UplinkScramblingCode();

};



/**
	Promary synchronization code.
	3GPP 25.213 5.2.3
*/
class PrimarySyncCode {

	protected:

	int8_t mCode[256];

	public:

	PrimarySyncCode();

	const int8_t* code() const { return mCode; }


};




/**
	Secondary synchronization code.
	The secondary sync code gets the rows of H8 from gHadamard8.
	3GPP 25.213 5.2.3
*/
class SecondarySyncCode {

	protected:

	int8_t mCode[256];

	public:

	SecondarySyncCode(unsigned N);

	const int8_t* code() const { return mCode; }


};


} // namespace UMTS




#endif
