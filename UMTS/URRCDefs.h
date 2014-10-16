/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef URRCDEFS_H
#define URRCDEFS_H

#include <assert.h>		// Must include before namespace ASN
#include "asn_system.h"	// Dont let other includes land in namespace ASN.
namespace ASN {
#include "CodingRate.h"
#include "ChannelCodingType.h"
};

namespace UMTS {
extern int rrcDebugLevel;

// This macro allows deletion of the current var from the list being iterated,
// because itr is advanced to the next position at the beginning of the loop,
// and list iterators are defined as keeping their position even if elements are deleted.
#define RN_FOR_ALL(type,list,var) \
	for (type::iterator itr = (list).begin(); \
		itr == (list).end() ? 0 : ((var=*itr++),1);)

// Permissible to call list.erase(itr)
// Do not advance the itr inside the loop, it is done by the macro.
#define RN_FOR_ITR(type,list,itr) \
	if ((list).size()) for (type::iterator itr,next = (list).begin(); \
		(itr = next++) != (list).end(); itr=next)

//#define PATLOG(level,msg) if (level & rrcDebugLevel) std::cout << msg <<"\n";
#define PATLOG(level,msg) if (level & rrcDebugLevel) LOG(INFO) << msg;

#ifndef RN_CALLOC
#define RN_CALLOC(type) ((type*)calloc(1,sizeof(type)))
#endif
#ifndef RN_BOUND
// Bound value between min and max values.
#define RN_BOUND(value,min,max) ( (value)<(min) ? (min) : (value)>(max) ? (max) : (value) )
#endif

typedef unsigned RbId;		// Numbered starting at 0 for SRB0, so SRB1 is 1.
typedef unsigned TrChId;	// Numbered starting at 0 herein.
typedef unsigned TfcId;		// TFC (Transport Format Combination) id, numbered 0 .. 
typedef unsigned TfIndex;	// TF index, specifies which TF in a TFS for a single TrCh.

// Since the initial RRC connection setup is on SRB0, and subsequently we use SRB2
// for everything, one wonders what would ever be send on SRB1?
// It appears to be used for the HS-DSCH channels that use a bunch of extra
// junk inside MAC that has a hard time getting synchronized.
const RbId SRB0 = 0;
const RbId SRB1 = 1;
const RbId SRB2 = 2;
const RbId SRB3 = 3;
const RbId SRB4 = 4;
class RrcDefs
{
	public:
	//enum CodingRate { CodingRate_half, CodingRate_third };	// These are from ASN
	//enum CodingType { NoCoding, Convolutional, Turbo };
	typedef ASN::ChannelCodingType_PR CodingType;	// Borrow from ASN
	typedef ASN::CodingRate CodingRate;
	static const CodingType Convolutional = ASN::ChannelCodingType_PR_convolutional;
	static const CodingType Turbo = ASN::ChannelCodingType_PR_turbo;
	static const int CodingRate_half = ASN::CodingRate_half;
	static const int CodingRate_third = ASN::CodingRate_third;
	// Max size of coded blocks from 25.212 4.2.2.2
	static const int ZConvolutional = 504;
	static const int ZTurbo = 5114;
	enum CNDomainId { UnconfiguredDomain, CSDomain, PSDomain };	// 10.3.1.1
	//enum TransportChType { DCH, RACH, FACH };	// See TrChType in UMTSCommon.h
	enum ReestablishmentTimer { useT314, useT315 };	// 10.3.3.30
	struct URNTI {
		unsigned SrncId:12;
		unsigned SRnti:20;
	};
	typedef unsigned CRNTI; // 16 bits
	static const unsigned maxRBMuxOptions = 2;
	static const unsigned maxRBperRAB = 2;	// Is this right?  What about having 3 sub-rab flows for voice?
	static const unsigned maxCtfcBits = 5;	// Up to 32; 16 is not enough for tbsize=340 at SF=4, 
	static const unsigned maxTfc = (1<<maxCtfcBits);
	static const unsigned maxTfPerTrCh = maxTfc;	// Max Transport Formats per TrCh.
	static const unsigned maxTrCh = 4;		// Max TrCh per RB.
	static const unsigned maxTbPerTrCh = 32;
	static const unsigned maxTFS = maxTbPerTrCh;	// Max number of transport formats per TrCh, not per TFC.
	static const unsigned maxRBid = 32;	// From 10.3.4.16;  numbered 1..32 with 1-4 reserved for SRBs
	typedef uint32_t RbSet;	// A set of RbIds, numbered starting from 0.
	static const uint32_t RbSetDefault = 0xffffffff;	// all of them.
	static const unsigned maxTrChid = 32;	// From 10.3.5.18;  numbered 1-32.

	// Encoder/Decoder block sizes for the various encoder types.
	// 25.212 4.2.3 These are the output size [Yi] of encoding size input bits [Ki]
	// although there are special cases if Ki gets too small.
	// The inline prevents gcc from whining or emitting unused code.
	// 4.2.3 Input/Output Size of Convolutional and Turbo Coding, and I quote:
	// "Ki is the number of bits in each code block.  Yi is the number of encoded bits."
	// "convolutional coding with rate 1/2: Yi = 2*Ki + 16; rate 1/3: Yi = 3*Ki + 24;"
	// "turbo coding with rate 1/3: Yi = 3*Ki + 12."
	static unsigned R2EncodedSize(unsigned Xi, unsigned *codeInBkSz=NULL, unsigned *fillBits=NULL);   // rate 1/2
	static unsigned R3EncodedSize(unsigned Ki);      // rate 1/3
	static unsigned TurboEncodedSize(unsigned Ki, unsigned *codeInBkSz=NULL, unsigned *fillBits=NULL); // Turbo
	static unsigned R2DecodedSize(unsigned Yi);     // rate 1/2
	static unsigned R3DecodedSize(unsigned Yi);     // rate 1/3
	static unsigned TurboDecodedSize(unsigned Yi);  // Turbo

	static unsigned encodedSize(CodingType ct, CodingRate cr, unsigned Yi);

};

enum TTICodes {
	TTI10ms=0,
	TTI20ms=1,
	TTI40ms=2,
	TTI80ms=3
};
extern unsigned TTICode2NumFrames(TTICodes ttiCode);	// return 1,2,4,8
extern unsigned TTICode2TTI(TTICodes ttiCode);			// return 10,20,40,80
extern TTICodes TTI2TTICode(unsigned tti);				// Return TTICodes for numbers 10,20,40,80

};	// namespace
#endif
