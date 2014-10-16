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

#include "UMTSPhCh.h"
#include "UMTSL1FEC.h"
#include "AsnHelper.h"

#include "asn_system.h"
namespace ASN {
//#include "UL-DPCH-Info.h"
#include "UL-ChannelRequirement.h"
//#include "DL-DPCH-InfoCommon.h"
#include "DL-CommonInformation.h"
#include "DL-InformationPerRL-List.h"
};

namespace UMTS {
extern int gFecTestMode;

ChannelTree gChannelTree;	// The one and only ChannelTree object in the entire universe.

static int nPilot2Index(int npilot) {
	switch (npilot) {
	case 0: return 0;
	case 2: return 0;
	case 4: return 1;
	case 8: return 2;
	case 16: return 3;
	default: assert(0);
	}
}

// 25.211 table 11: [page 23] downlink DPDCH and DPCCH fields.
// Static Constructors with data fields in the order specified in
// the tables in the spec to make this easy:
static SlotFormat Table11(int format,int kbps,int sf,int bitsPerSlot,
	int data1,int data2,int tpc,int tfci,int pilot)
{
	SlotFormat result;
	result.mSlotFormat = format;
	//mBitRate = kbps;
	result.mSF = sf;
	result.mBitsPerSlot = bitsPerSlot;
	result.mNData1 = data1;
	result.mNData2 = data2;
	result.mNPilot = pilot;
	result.mPilotIndex = nPilot2Index(result.mNPilot);
	result.mNTfci = tfci;
	result.mNTpc = tpc;
	assert(result.mNData1 + result.mNData2 + result.mNPilot + result.mNTfci + result.mNTpc == result.mBitsPerSlot);
	return result;
};

SlotFormat SlotInfoDownlinkDch[17] = {
		// format	kbps	sf  	b/slot 	data1 	data2	tpc 	tfci	pilot
	Table11(0,		15,		512,	10,		0, 		4,		2,		0,		4 ),
	Table11(1,		15,		512,	10,		0, 		2,		2,		2,		4 ),
	Table11(2,		30,		256,	20,		2, 		14,		2,		0,		2 ),
	Table11(3,		30,		256,	20,		2, 		12,		2,		2,		2 ),
	Table11(4,		30,		256,	20,		2, 		12,		2,		0,		4 ),
	Table11(5,		30,		256,	20,		2, 		10,		2,		2,		4 ),
	Table11(6,		30,		256,	20,		2, 		8,		2,		0,		8 ),
	Table11(7,		30,		256,	20,		2, 		6,		2,		2,		8 ),
	Table11(8,		60,		128,	40,		6, 		28,		2,		0,		4 ),
	Table11(9,		60,		128,	40,		6, 		26,		2,		2,		4 ),
	Table11(10,		60,		128,	40,		6, 		24,		2,		0,		8 ),
	Table11(11,		60,		128,	40,		6, 		22,		2,		2,		8 ),
	Table11(12,		120,		64,	80,		12,		48,		4,		8,		8 ),
	Table11(13,		240,		32,	160,		28,		112,		4,		8,		8 ),
	Table11(14,		480,		16,	320,		56,		232,		8,		8,		16 ),
	Table11(15,		960,		8,	640,		120,		488,		8,		8,		16 ),
	Table11(16,		1920,		4,	1280,		248,		1000,		8,		8,		16 )
};

SlotFormat SlotInfoDownlinkDchREL99[17] = {
                // format       kbps    sf      b/slot  data1   data2   tpc     tfci    pilot
        Table11(0,              15,             512,    10,             2,              2,              2,              0,              4 ),
        Table11(1,              15,             512,    10,             0,              2,              2,              2,              4 ),
        Table11(2,              30,             256,    20,             2,              14,             2,              0,              2 ),
        Table11(3,              30,             256,    20,             0,              14,             2,              2,              2 ),
        Table11(4,              30,             256,    20,             2,              12,             2,              0,              4 ),
        Table11(5,              30,             256,    20,             0,              12,             2,              2,              4 ),
        Table11(6,              30,             256,    20,             2,              8,              2,              0,              8 ),
        Table11(7,              30,             256,    20,             0,              8,              2,              2,              8 ),
        Table11(8,              60,             128,    40,             6,              28,             2,              0,              4 ),
        Table11(9,              60,             128,    40,             4,              28,             2,              2,              4 ),
        Table11(10,             60,             128,    40,             6,              24,             2,              0,              8 ),
        Table11(11,             60,             128,    40,             4,              24,             2,              2,              8 ),
        Table11(12,             120,    	64,     80,             4,              56,             4,              8,              8 ),
        Table11(13,             240,    	32,     160,    	20,             120,    	4,              8,              8 ),
        Table11(14,             480,    	16,     320,    	48,             240,    	8,              8,              16 ),
        Table11(15,             960,    	8,      640,    	112,    	496,    	8,              8,              16 ),
        Table11(16,             1920,   	4,      1280,  		240,    	1008,   	8,              8,              16 )
};


// This is a special slot format used to test the encoder/decoders,
// for which we need a downlink channel the same width as the uplink channel.
//SlotFormat SlotInfoForTesting[1] = {
//	Table11(0,		30,		256,	20,		20, 	0,		0,		0,		0 )
//};

// These are the slot formats we have chosen to use for downlink DCH at each SF.
static unsigned SlotInfoDownlinkDchByTier[ChannelTree::sNumTiers] = {
	16,	// SF=4
	15,	// SF=8
	14,	// SF=16
	13,	// SF=32
	12,	// SF=64
	11,	// SF=128
	7	// SF=256
};

// Uplink data info table.
static SlotFormat Tabled(int format,int kbps,int sf,int bitsPerSlot)
{
	SlotFormat result;
	result.mSlotFormat = format;
	//mBitRate = kbps;
	result.mSF = sf;
	result.mBitsPerSlot = bitsPerSlot;
	result.mNData1 = bitsPerSlot;
	result.mNData2 = 0;
	result.mNPilot = result.mPilotIndex = 0;
	result.mNTfci = 0;
	result.mNTpc = 0;
	assert(result.mNData1 + result.mNData2 + result.mNPilot + result.mNTfci + result.mNTpc == result.mBitsPerSlot);
	return result;
}

// Uplink control info table.
static SlotFormat Tablec(int format,int kbps,int sf,int bitsPerSlot,int pilot,int tpc,int tfci,int fbi)
{
	SlotFormat result;
	result.mSlotFormat = format;
	//mBitRate = kbps;
	result.mSF = sf;
	result.mBitsPerSlot = bitsPerSlot;
	result.mNData1 = result.mNData2 = 0;
	result.mNPilot = pilot;
	result.mPilotIndex = 0;	// unused
	result.mNTfci = tfci;
	result.mNTpc = tpc;
	assert(fbi + result.mNData1 + result.mNData2 + result.mNPilot + result.mNTfci + result.mNTpc == result.mBitsPerSlot);
	return result;
}

// 3GPP 25.211 Table 1: uplink DPDCH [data] fields
// Obviously, this table provides no additional info, but keeping the same paradigm:
SlotFormat SlotInfoUplinkDpdch[7] = {
	// format,	kbps,	sf, 	b/slot
	Tabled(0, 	15, 	256, 	10),
	Tabled(1, 	30, 	128, 	20),
	Tabled(2, 	60, 	64, 	40),
	Tabled(3, 	120, 	32, 	80),
	Tabled(4, 	240, 	16, 	160),
	Tabled(5, 	480, 	8, 		320),
	Tabled(6, 	960, 	4, 		640)
};

// 3GPP 25.211 Table 2: uplink DPCCH [control] fields
SlotFormat SlotInfoUplinkDpcch[5] = {
	// format	kbs		sf		b/slot	pilot	tpc	tfci fbi
	Tablec(0,	15,		256,	10,		6,		2,	2,	0),
	Tablec(1,	15,		256,	10,		8,		2,	0,	0),
	Tablec(2,	15,		256,	10,		5,		2,	2,	1),
	Tablec(3,	15,		256,	10,		7,		2,	0,	1),
	Tablec(4,	15,		256,	10,		6,		4,	0,	0)
};

// 3GPP 25.211 Table 7: Random-access message control fields.
SlotFormat SlotInfoPrachControl[1] = {
	// format	kbs		sf		b/slot	pilot	tpc	tfci fbi
	Tablec(0, 	15, 	256, 	10,		8,		0,	2, 	0)
};

// 3GPP 25.211 Table 6: Random-access message data fields.
// Content identical to uplink dpdch
SlotFormat SlotInfoPrachData[4] = {
	// format,	kbps,	sf, 	b/slot
	Tabled(0, 	15, 	256, 	10),
	Tabled(1, 	30, 	128, 	20),
	Tabled(2, 	60, 	64, 	40),
	Tabled(3, 	120, 	32, 	80)
};

static SlotFormat Table18(int format,int kbps,int sf, int bitsPerSlot, int data, int pilot, int tfci)
{
	SlotFormat result;
	result.mSlotFormat = format;
	//mBitRate = kbps;
	result.mSF = sf;
	result.mBitsPerSlot = bitsPerSlot;
	result.mNData1 = data;
	result.mNData2 = 0;
	result.mNPilot = pilot;
	result.mPilotIndex = nPilot2Index(result.mNPilot);
	result.mNTfci = tfci;
	result.mNTpc = 0;
	assert(result.mNData1 + result.mNData2 + result.mNPilot + result.mNTfci + result.mNTpc == result.mBitsPerSlot);
	return result;
}


// table 10: SCCPCH fields.
// We will only use one for the forseeable future.
// Note that tfci is huge.  This implies that for DCH over FACH they expect you to
// use a big TFS and assign different TrCh for each DCH.
SlotFormat SlotInfoSccpch[18] = {
		// format	kbps	sf	  b/slot  data	  pilot	  tfci
	Table18(0,		30,		256,	20,		20,		0,		0),
	Table18(1,		30,		256,	20,		12,		8,		0),
	Table18(2,		30,		256,	20,		18,		0,		2),
	Table18(3,		30,		256,	20,		10,		8,		2),
	Table18(4,		60,		128,	40,		40,		0,		0),
	Table18(5,		60,		128,	40,		32,		8,		0),
	Table18(6,		60,		128,	40,		38,		0,		2),
	Table18(7,		60,		128,	40,		30,		8,		2),
	Table18(8,		120,	 64,	80,		72,		0,		8),
	Table18(9,		120,	 64,	80,		64,		8,		8),
	Table18(10,		240,	 32,	160,	152,	0,		8),
	Table18(11,		240,	 32,	160,	144,	8,		8),
	Table18(12,		480,	 16,	320,	312,	0,		8),
	Table18(13,		480,	 16,	320,	296,	16,		8),
	Table18(14,		960,	  8,	640,	632,	0,		8),
	Table18(15,		960,	  8,	640,	616,	16,		8),
	Table18(16,		1920,	  4,	1280,	1272,	0,		8),
	Table18(17,		1920,	  4,	1280,	1256,	16,		8)
};

// These are the slot formats we (would) choose to use for downlink SCCPCH DCH at each SF,
// if we wanted to support multiple SF, which we dont.
// So this is way overkill, but I put it in for symmetric beauty with DCH, which does need it.
static unsigned SlotInfoSccpchByTier[ChannelTree::sNumTiers] = {
	16,	// SF=4
	14,	// SF=8
	12,	// SF=16
	10,	// SF=32
	 8,	// SF=64
	 6,	// SF=128
	 2	// SF=256
	/**** 5-7-2012: Harvind says dont use pilot bits, so changed this:
	17,	// SF=4
	15,	// SF=8
	13,	// SF=16
	11,	// SF=32
	 9,	// SF=64
	 7,	// SF=128
	 3	// SF=256
	 ***/
};

static SlotFormat *getDlSlotFormat(PhChType chType,unsigned dlSF)
{
	int slotnum;
	switch (chType) {
	case DPDCHType:
		slotnum = SlotInfoDownlinkDchByTier[ChannelTree::sf2tier(dlSF)];
#ifndef RELEASE99
		return &SlotInfoDownlinkDch[slotnum];
#else
                return &SlotInfoDownlinkDchREL99[slotnum];
#endif
		break;
	case SCCPCHType:
		slotnum = SlotInfoSccpchByTier[ChannelTree::sf2tier(dlSF)];
		return &SlotInfoSccpch[slotnum];
		break;
	default: return NULL;	// Invalid phChType.
	}
}


// Bidirectional channel requires uplink scrambling code,
// and we will also map out the SF to the slot format now and remember it.
PhCh::PhCh(PhChType chType,
	unsigned wDlSF,		// downlink spreading factor
	unsigned wSpCode,	// downlink spreading [channel] code
	unsigned wUlSF,		// uplink spreading factors
	unsigned wSrCode,	// uplink scrambling code
	ARFCNManager *wRadio):
	mPhChType(chType), mDlSF(wDlSF), mDlSFLog2((int)log2(wDlSF)),mUlSF(wUlSF), mSpCode(wSpCode), mSrCode(wSrCode),mRadio(wRadio),
	mUlPuncturingLimit(gConfig.getNum("UMTS.Uplink.Puncturing.Limit")),
	mAllocated(0), mDlSlot(0), mUlDPCCH(0)
{
	switch (chType) {
	case DPDCHType:
		assert(mSpCode<mDlSF);
		mDlSlot = getDlSlotFormat(chType,wDlSF);
		break;
	case SCCPCHType:
		assert(mSpCode<mDlSF);
		assert(wUlSF == 0 && wSrCode == 0);	// downlink only channel
		mDlSlot = getDlSlotFormat(chType,wDlSF);
		gChannelTree.chReserve(wDlSF, wSpCode);
		break;
	case PCCPCHType:	// used for BCH
		assert(wDlSF==256 && wSpCode==1);	// Dictated by the UMTS gods.
		assert(wUlSF == 0 && wSrCode == 0);	// this is a downlink only channel
		// 11-2012: chReserve now checks for conflicting reservations so dont re-reserve it.
		// gChannelTree.chReserve(wDlSF, wSpCode);	// Redundant; this ch is fixed and we reserve it elsewhere too.
		break;
	case CPICHType:		// (pat) used for PICH which is a fixed synchronization pattern.
		// We dont allocate this, but this is what it would be if we did:
		assert(wDlSF==256 && wSpCode==0);	// Dictated by the UMTS gods.
		assert(wDlSF == 0 && wSrCode==0);	// this is a downlink only channel
		gChannelTree.chReserve(wDlSF, wSpCode);	// Redundant; this ch is fixed and we reserve it elsewhere too.
		break;
	case PRACHType:
		assert(wDlSF == 0 && wSpCode==0);	// uplink only channel
		// nothing to do.
		break;
	default: assert(0);	// Invalid phChType.
	}

        // TFCI and FBI settings automatically determing slot format
        // format 0:  2 TFCI bits, 0 FBI bits
        // format 1:  0 TFCI bits, 0 FBI bits
        // format 2:  2 TFCI bits, 1 FBI bit
        // format 3:  0 TFCI bits, 1 FBI bit
        // we are using TFCI but no FBI for uplink DPCCH
        if (chType==DPDCHType) mUlDPCCH = &SlotInfoUplinkDpcch[0];

	// The rach & fach channels are already the same size: 150 bits.
	//if (gFecTestMode) {
	//	// For testing, force the use of a dummy downlink channel
	//	// whose width matches the uplink ch.
	//	mDlSlot = &SlotInfoForTesting[0];
	//}
}

// Same result as PhCh::getDlRadioFrameSize but if you dont have a channel pointer handy.
unsigned getDlRadioFrameSize(PhChType chtype, unsigned sf)
{
	if (chtype == PCCPCHType) {
		// Special case, 18 bits/slot = 270 bits / frame.
		// And caller must not forget to multiply by 2 because it is always TTI=20ms.
		return 270;
	} 
	SlotFormat *slot = getDlSlotFormat(chtype,sf);
	return gFrameSlots * (slot->mNData1 + slot->mNData2);
}

// Dont forget to multiply this by TTI.
unsigned PhCh::getDlRadioFrameSize()
{
	if (phChType() == PCCPCHType) {
		// Special case, 18 bits/slot = 270 bits / frame.
		// And caller must not forget to multiply by 2 because it is always TTI=20ms.
		return 270;
	} else if (gFecTestMode && phChType() == DPDCHType) {
		// Return the same size for uplink and downlink channels so we can jumper
		// them together for testing.  Only works for DCH, other dl channels do not have a corresponding
		// ul channel, but RACH and FACH are already the same size anyway.
		return getUlRadioFrameSize();
	} else {
		return gFrameSlots * (mDlSlot->mNData1 + mDlSlot->mNData2);
	}
}

// This is the maximum frame size.  In each TTI the UE changes uplink SF to reduce
// the frame size as much as possible and still fit data in that TTI.
unsigned PhCh::getUlRadioFrameSize() {
	return gFrameLen / mUlSF;
}

// This gets the SF codes from the enum ASN::SpreadingFactor
AsnEnumMap sSpreadingFactor(ASN::asn_DEF_SpreadingFactor,ASN::SpreadingFactor_sf256);

// 25.331 10.3.6.88 Uplink DPDCH Info
// This is only used for DCH.  Send the physical parameters.
void PhCh::toAsnUL_DPCH_Info(ASN::UL_DPCH_Info *iep)
{
	// Sect. 8.6.6.11 of 25.331 says power control much be included when moving from CELL_FACH to CELL_DCH.
	// Define in 10.3.6.91
	// struct UL_DPCH_PowerControlInfo *ul_DPCH_PowerControlInfo   /* OPTIONAL */;
	iep->ul_DPCH_PowerControlInfo = RN_CALLOC(ASN::UL_DPCH_PowerControlInfo);
	iep->ul_DPCH_PowerControlInfo->present = ASN::UL_DPCH_PowerControlInfo_PR_fdd;
	iep->ul_DPCH_PowerControlInfo->choice.fdd.dpcch_PowerOffset = -3; //-6;/*-82 to -3*/ 
        iep->ul_DPCH_PowerControlInfo->choice.fdd.pc_Preamble = 0; /*long, 0-7*/
        iep->ul_DPCH_PowerControlInfo->choice.fdd.sRB_delay = 0; /*long, 0-7*/
        iep->ul_DPCH_PowerControlInfo->choice.fdd.powerControlAlgorithm.present = ASN::PowerControlAlgorithm_PR_algorithm2; 

	// v struct UL_DPCH_Info__modeSpecificInfo { } modeSpecificInfo;
	// . UL_DPCH_Info__modeSpecificInfo_PR present;
	iep->modeSpecificInfo.present = ASN::UL_DPCH_Info__modeSpecificInfo_PR_fdd;
	// Gotta love this...
	struct ASN::UL_DPCH_Info::
		UL_DPCH_Info__modeSpecificInfo::
		UL_DPCH_Info__modeSpecificInfo_u::
		UL_DPCH_Info__modeSpecificInfo__fdd *fdd = &iep->modeSpecificInfo.choice.fdd;
	// vv union UL_DPCH_Info__modeSpecificInfo_u { } choice;
	// vvv struct UL_DPCH_Info__modeSpecificInfo__fdd {...} fdd;
	// ... ScramblingCodeType_t     scramblingCodeType;
	// TODO: Which scambling code type should it be: short or long?
	//asn_long2INTEGER(&fdd->scramblingCodeType,ASN::ScramblingCodeType_longSC);
	fdd->scramblingCodeType = toAsnEnumerated(ASN::ScramblingCodeType_longSC);
	// ... UL_ScramblingCode_t  scramblingCode;
	fdd->scramblingCode = this->mSrCode;
	// ... NumberOfDPDCH_t *numberOfDPDCH  /* DEFAULT 1 */;
	// ... SpreadingFactor_t    spreadingFactor;
	fdd->spreadingFactor = sSpreadingFactor.toAsn(this->mUlSF);
	// ... BOOLEAN_t    tfci_Existence;
	// We always use tfci.
	fdd->tfci_Existence = 1;	// true.
	// ... NumberOfFBI_Bits_t  *numberOfFBI_Bits   /* OPTIONAL */;
	// ... PuncturingLimit_t    puncturingLimit;
	// The uplink puncturing limit is 0.4 to 1 by 0.04,
	// which we express as percent in the range 40..100
	// I am hard-coding the ASN enumeration here.
	int puncturing0to15 = ((this->mUlPuncturingLimit+2) - 40)/4;
	int puncturingEnumCode = RN_BOUND(puncturing0to15,0,15);
	fdd->puncturingLimit = toAsnEnumerated(puncturingEnumCode);

	// ^^^ struct UL_DPCH_Info__modeSpecificInfo__fdd {...} fdd;
	// ^^ union UL_DPCH_Info__modeSpecificInfo_u { } choice;
	// ^ struct UL_DPCH_Info__modeSpecificInfo { } modeSpecificInfo;
}

// This IE is not in 25.331, and has nothing useful in it except the pointer
// to UL_DPCH_Info.  I suspect it is just historical from a previous spec.
ASN::UL_ChannelRequirement *PhCh::toAsnUL_ChannelRequirement()
{
	if (this->mPhChType != DPDCHType) {return NULL;}	// This IE applies only to DPCH.

	ASN::UL_ChannelRequirement *result = RN_CALLOC(ASN::UL_ChannelRequirement);
	result->present = ASN::UL_ChannelRequirement_PR_ul_DPCH_Info;
	this->toAsnUL_DPCH_Info(&result->choice.ul_DPCH_Info);
	return result;
}

// 25.331 10.3.6.18 Downlink DPCH Info Common for all Radio Links
ASN::DL_DPCH_InfoCommon * PhCh::toAsnDL_DPCH_InfoCommon()
{
	ASN::DL_DPCH_InfoCommon *result = RN_CALLOC(ASN::DL_DPCH_InfoCommon);

	// v struct DL_DPCH_InfoCommon__cfnHandling { } cfnHandling;
	// . DL_DPCH_InfoCommon__cfnHandling_PR present;
	// You can never put in the 'NOTHING' option - the asn_CHOICE handler disallows.
	// This IE has something to do with handover.
	// I am just putting in the value with the least additional info required.
	result->cfnHandling.present = ASN::DL_DPCH_InfoCommon__cfnHandling_PR_maintain;

	// vv union DL_DPCH_InfoCommon__cfnHandling_u { } choice;
	// .. NULL_t   maintain;
	// vvv struct DL_DPCH_InfoCommon__cfnHandling__initialise {} initialise;
	// ... Cfntargetsfnframeoffset_t   *dummy  /* OPTIONAL */;
	// ^^^ struct DL_DPCH_InfoCommon__cfnHandling__initialise {} initialise;
	// ^^ union DL_DPCH_InfoCommon__cfnHandling_u { } choice;
	// ^ struct DL_DPCH_InfoCommon__cfnHandling { } cfnHandling;

	// vv struct DL_DPCH_InfoCommon__modeSpecificInfo { } modeSpecificInfo;
	// .. DL_DPCH_InfoCommon__modeSpecificInfo_PR present;
	result->modeSpecificInfo.present = ASN::DL_DPCH_InfoCommon__modeSpecificInfo_PR_fdd;
	// vv union DL_DPCH_InfoCommon__modeSpecificInfo_u { } fdd;
	// vvv struct DL_DPCH_InfoCommon__modeSpecificInfo__fdd { } choice;
	ASN::DL_DPCH_InfoCommon::
		DL_DPCH_InfoCommon__modeSpecificInfo::
		DL_DPCH_InfoCommon__modeSpecificInfo_u::
		DL_DPCH_InfoCommon__modeSpecificInfo__fdd *fdd = &result->modeSpecificInfo.choice.fdd;

	// TODO: What should all this power stuff be?
	// ... struct DL_DPCH_PowerControlInfo *dl_DPCH_PowerControlInfo   /* OPTIONAL */;

	// ... PowerOffsetPilot_pdpdch_t    powerOffsetPilot_pdpdch;
	// And I quote: "Power offset equals PPilot - PDPDCH, range 0..6 dB, in steps of 0.25 dB"
	fdd->powerOffsetPilot_pdpdch = 0;	// probably 0db.

	// ... struct Dl_rate_matching_restriction *dl_rate_matching_restriction   /* OPTIONAL */;
	// Ignored.  I suspect we do not need it if we used tfci.

	// ... SF512_AndPilot_t     spreadingFactorAndPilot;
	// Kinda verbose because we have to specify pilot bits only for SF=128 and 256,
	// but here it is:
	unsigned npilot = mDlSlot->mNPilot;
	switch (this->mDlSF) {
	case 4: fdd->spreadingFactorAndPilot.present = ASN::SF512_AndPilot_PR_sfd4; break;
	case 8: fdd->spreadingFactorAndPilot.present = ASN::SF512_AndPilot_PR_sfd8; break;
	case 16: fdd->spreadingFactorAndPilot.present = ASN::SF512_AndPilot_PR_sfd16; break;
	case 32: fdd->spreadingFactorAndPilot.present = ASN::SF512_AndPilot_PR_sfd32; break;
	case 64: fdd->spreadingFactorAndPilot.present = ASN::SF512_AndPilot_PR_sfd64; break;
	case 128:
		fdd->spreadingFactorAndPilot.present = ASN::SF512_AndPilot_PR_sfd128;
		switch (npilot) {
		case 4:
			fdd->spreadingFactorAndPilot.choice.sfd128 = toAsnEnumerated(ASN::PilotBits128_pb4);
			break;
		case 8:
			fdd->spreadingFactorAndPilot.choice.sfd128 = toAsnEnumerated(ASN::PilotBits128_pb8);
			break;
		default: assert(0);
		}
		break;
	case 256:
		fdd->spreadingFactorAndPilot.present = ASN::SF512_AndPilot_PR_sfd256;
		switch (npilot) {
		case 2:
			fdd->spreadingFactorAndPilot.choice.sfd256 = toAsnEnumerated(ASN::PilotBits256_pb2);
			break;
		case 4:
			fdd->spreadingFactorAndPilot.choice.sfd256 = toAsnEnumerated(ASN::PilotBits256_pb4);
			break;
		case 8:
			fdd->spreadingFactorAndPilot.choice.sfd256 = toAsnEnumerated(ASN::PilotBits256_pb8);
			break;
		default: assert(0);
		}
		break;
	case 512:	// We dont use SF-512.
	default: assert(0);
	}

	// ... PositionFixedOrFlexible_t    positionFixedOrFlexible;
	fdd->positionFixedOrFlexible = toAsnEnumerated(ASN::PositionFixedOrFlexible_fixed);

	// ... BOOLEAN_t    tfci_Existence;
	fdd->tfci_Existence = 1;	// true;

	// ^^^ struct DL_DPCH_InfoCommon__modeSpecificInfo__fdd { } choice;
	// ^^ union DL_DPCH_InfoCommon__modeSpecificInfo_u { } fdd;
	// ^^ struct DL_DPCH_InfoCommon__modeSpecificInfo { } modeSpecificInfo;
	return result;
}


// 25.331 10.3.6.24 Downlink Info Common for all Radio Links
ASN::DL_CommonInformation * PhCh::toAsnDL_CommonInformation()
{
	ASN::DL_CommonInformation *result = RN_CALLOC(ASN::DL_CommonInformation);
	result->dl_DPCH_InfoCommon = toAsnDL_DPCH_InfoCommon();
	result->modeSpecificInfo.present = ASN::DL_CommonInformation__modeSpecificInfo_PR_fdd;
	result->modeSpecificInfo.choice.fdd.defaultDPCH_OffsetValue = RN_CALLOC(ASN::DefaultDPCH_OffsetValueFDD_t);
	*(result->modeSpecificInfo.choice.fdd.defaultDPCH_OffsetValue) = 0;

	// v struct DL_CommonInformation__modeSpecificInfo__fdd {...} fdd;
	//   All of these are optional, so just ignore them:
	// . DefaultDPCH_OffsetValueFDD_t    *defaultDPCH_OffsetValue    /* OPTIONAL */;
	// . struct DPCH_CompressedModeInfo  *dpch_CompressedModeInfo    /* OPTIONAL */;
	// . TX_DiversityMode_t  *tx_DiversityMode   /* OPTIONAL */;
	// . struct SSDT_Information *dummy  /* OPTIONAL */;
	// ^ struct DL_CommonInformation__modeSpecificInfo__fdd {...} fdd;
	return result;
}

// 25.331 10.3.6.21 Downlink DPCH for each Radio Link.
ASN::DL_DPCH_InfoPerRL *PhCh::toAsnDL_DPCH_InfoPerRL()
{
	ASN::DL_DPCH_InfoPerRL *result = RN_CALLOC(ASN::DL_DPCH_InfoPerRL); 
	result->present = ASN::DL_DPCH_InfoPerRL_PR_fdd;

	// This is a boolean value:
	// PCPICH_UsageForChannelEst_t  pCPICH_UsageForChannelEst;
	int cpichUsage = gConfig.getNum("UMTS.PCPICHUsageForChannelEst");
	assert(cpichUsage == 0 || cpichUsage == 1);	// kinda harsh
	int asncuval = cpichUsage ?
		ASN::PCPICH_UsageForChannelEst_mayBeUsed : 
		ASN::PCPICH_UsageForChannelEst_shallNotBeUsed;
	result->choice.fdd.pCPICH_UsageForChannelEst = toAsnEnumerated(asncuval);

	// "Offset in number of chips between the beginning of the P-CCPCH frame
	// and the beginning of the DPCH frame" aka Tau(DPCH,n)
	// DPCH_FrameOffset_t   dpch_FrameOffset;
	int dpchFrameOffset = gConfig.getNum("UMTS.DPCHFrameOffset");
	assert(dpchFrameOffset >= 0 && dpchFrameOffset <= 38144);	// harsh
	assert(dpchFrameOffset % 256 == 0);
	result->choice.fdd.dpch_FrameOffset = dpchFrameOffset;


	// struct SecondaryCPICH_Info  *secondaryCPICH_Info    /* OPTIONAL */;

	// Eventually we are going to find the darned Channelization code in here.
	// DL_ChannelisationCodeList_t  dl_ChannelisationCodeList;
	ASN::DL_ChannelisationCode *one = RN_CALLOC(ASN::DL_ChannelisationCode); 

#define DORKINESS(spf) \
	case spf: \
		one->sf_AndCodeNumber.present = ASN::SF512_AndCodeNumber_PR_sf##spf; \
		one->sf_AndCodeNumber.choice.sf##spf = SpCode(); \
		break;

	switch (this->mDlSF) {
		DORKINESS(4)
		DORKINESS(8)
		DORKINESS(16)
		DORKINESS(32)
		DORKINESS(64)
		DORKINESS(128)
		DORKINESS(256)
		DORKINESS(512)
		default:assert(0);
	}
	ASN_SEQUENCE_ADD(&result->choice.fdd.dl_ChannelisationCodeList,one);

	// This is for multiple radio links, and we will never use it, so just default it:
	// TPC_CombinationIndex_t   tpc_CombinationIndex;
	result->choice.fdd.tpc_CombinationIndex = 0;	// a fine wonderful tpc index.

	// SSDT_CellIdentity_t *dummy  /* OPTIONAL */;
	// ClosedLoopTimingAdjMode_t   *closedLoopTimingAdjMode    /* OPTIONAL */;

	return result;
}

// 25.331 10.3.6.27 Downlink information for each radio link
ASN::DL_InformationPerRL_List * PhCh::toAsnDL_InformationPerRL_List()
{
	ASN::DL_InformationPerRL_List *result = RN_CALLOC(ASN::DL_InformationPerRL_List); 

	ASN::DL_InformationPerRL *one = RN_CALLOC(ASN::DL_InformationPerRL);

	one->modeSpecificInfo.present = ASN::DL_InformationPerRL__modeSpecificInfo_PR_fdd;
	// 10.3.6.60 Primary Scrambling Code (CPICH) in the range 0..511
	int primarySC = gConfig.getNum("UMTS.Downlink.ScramblingCode");
	assert(primarySC >= 0 && primarySC < 512);	// kinda harsh.
	one->modeSpecificInfo.choice.fdd.primaryCPICH_Info.primaryScramblingCode = primarySC;

	// struct DL_DPCH_InfoPerRL    *dl_DPCH_InfoPerRL  /* OPTIONAL */;
	one->dl_DPCH_InfoPerRL = toAsnDL_DPCH_InfoPerRL();

	// struct SCCPCH_InfoForFACH   *dummy  /* OPTIONAL */;

	ASN_SEQUENCE_ADD(&result->list,one);
	return result;
}

// Is this specific DCH available for allocation, ignoring the rest of the tree?
// This is an internal function to test a single spot in the tree;
// dont call this externally - you probably want chChooseByBW() or chChooseBySF().
bool ChannelTreeElt::available(bool checkOnlyReserved)
{
	if (checkOnlyReserved) { return !mReserved; }
	return ! mReserved && ! mAlsoReserved && mDch && ! mDch->phChAllocated();
}

bool ChannelTreeElt::active(void)
{
        return mReserved || !mDch || mDch->phChAllocated();
}


void ChannelTree::chConflict(Tier t1,unsigned ch1,Tier t2,unsigned ch2)
{
	LOG(ALERT) << "Attempt to reserve channel:"
		<<LOGVAR2("sf",tier2sf(t1))<<LOGVAR2("chcode",ch1)
		<<" which conflicts with reserved channel:"
		LOGVAR2("sf",tier2sf(t2))<<LOGVAR2("chcode",ch2);
	//assert(0);
}


// Reserve a downlink channel for something other than DCH.
// NOTE: A reservation only blocks a specific channel and unlike active() channels
// are not considered when allocating channels on other levels.
// The reservations block upwards only because chReserve reserves all the channels
// above the specified one in the tree.
// But for example, if you reserve ch(256,2), then ch(128,1) is also 'reserved',
// but that does not prevent you from allocating ch(256,3) which is
// also under ch(128,1) in the tree, which is the desired behavior.
// So be careful if you modify this code - use the test routines below.
void ChannelTree::chReserve(unsigned sf,unsigned chcode)
{
	// This causes all the channels above this one in the tree to be reserved as well.
	// It is not absolutely necessary to mark the ones above because the channel allocator would
	// notice the reserved channels and avoid them, but it is more efficient to mark them now,
	// and there is no point in allocating DCHFEC objects in places in the tree that can never be accessed.
	// 11-2012: Look for configuration conflicts of conflicting reserved channels:
	Tier t = sf2tier(sf);
	Tier badtier; unsigned badcode;	// To hold a conflicing reservation.
	printf("chReserve(%d,%d)\n",sf,chcode);
	if (! isTierFreeUpward(t,chcode,true,&badtier,&badcode) ||
		! isTierFreeDownward(t,chcode,1,true,&badtier,&badcode)) {
		chConflict(t,chcode,badtier,badcode);
	}
#if 0
	unsigned chcode2 = chcode;
	for (Tier t2 = t; t2 >= 0; t2--) {
		if (mTree[t2][chcode2].mReserved) { chConflict(sf,chcode,tier2sf(t2),chcode2); }
		chcode2 = chcode2 / 2;
	}
	for (Tier t3 = t; t3 <sNumTiers>= 0; t3++) {
		if (mTree[t3][chcode3].mReserved) { chConflict(sf,chcode,tier2sf(t3),chcode3); }
		chcode3 = chcode3 * 2;
	}
#endif

	// All ok.  Reserve this ch and also reserve everything above it.
	mTree[t][chcode].mReserved = true;
	chcode = chcode / 2;
	for (t--; t >= 0; t--) {
		mTree[t][chcode].mAlsoReserved = true;
		chcode = chcode / 2;
	}
}

unsigned ChannelTree::sf2tier(unsigned sf)		// Return the tree tier for a given SF.
{
	// This is not particularly efficient, but I could not bring myself to call log().
	switch (sf) {
		case   4: return 0;
		case   8: return 1;
		case  16: return 2;
		case  32: return 3;
		case  64: return 4;
		case 128: return 5;
		case 256: return 6;
		default:
			LOG(ERR) << "Invalid SF:"<<sf;
			assert(0);	// Oops!
	}
}

unsigned ChannelTree::tier2sf(Tier tier)		// Return the SF for a tree tier (0..7).
{
	const unsigned tiersf[] = { 4, 8, 16, 32, 64, 128, 256 };	// How wide is the tier?
	return tiersf[tier];
}

// Return the ChannelTree tier for the specified bandwidth.
// The peakthroughput in the QoS IE is quantized with values 256K, 128K, 64K, 32K, ... 4K, 2K, 1K
// which we will map to SF=4 (240K), SF=8 (120K) etc, even though they dont quite match.
// Ie, if they ask for 64000, we will return SF=16 at 60K, and very important, tell the phone it is 64K.
// If you really want the specified bandwidth, set the guaranteed flag.
// TODO: Need to subtract the overhead from the guaranteed bandwidths.
ChannelTree::Tier ChannelTree::bw2tier(unsigned ops, bool guaranteed)
{
	// This is kinda cheesy, but is not called often.
	if (guaranteed) {
		if (ops >= 120000) { return 0; }	// SF=4, actual == 240K, less overhead
		if (ops >= 60000) { return 1; }		// SF=8, actual == 120K
		if (ops >= 30000) { return 2; }		// SF=16, actual == 60K
		if (ops >= 15000) { return 3; }	// SF=32, actual == 30K
		if (ops >= 7500) { return 4; }	// SF=64, actual == 15K
		if (ops >= 3750) { return 5; }	// SF=128, actual == 7.5K
	} else {
		if (ops > 128000) { return 0; }	// SF=4, actual == 240K, less overhead
		if (ops > 64000) { return 1; }		// SF=8, actual == 120K
		if (ops > 32000) { return 2; }		// SF=16, actual == 60K
		if (ops > 16000) { return 3; }	// SF=32, actual == 30K
		if (ops > 8000) { return 4; }	// SF=64, actual == 15K
		if (ops > 4000) { return 5; }	// SF=128, actual == 7.5K
	}
	return 6;	// SF=256, actual == 3.75K
}

// Is anything allocated above this channel?
// We dont have to test reserved because reservations are bubbled up when they are made.
bool ChannelTree::isTierFreeUpward(Tier tier,unsigned startcode, bool checkOnlyReserved, Tier *badtier, unsigned *badcode)
{
	//unsigned code = startcode; (pat) 11-2012: this looks like a bug?
	unsigned code = startcode / 2;	// (pat) 11-2012: Should be this.
	for (Tier t = tier-1; t >= 0; t--) {
		//printf("isTierFreeUpward(%d %d %d)\n",t,code,checkOnlyReserved);
		// Harvind (3-11-13): Don't care if node above is "AlsoReserved", only care if its reserved or allocated
 		//	an "AlsoReserved" node has sub-trees w/ codes that are still permissible
		if (mTree[t][code].active()) { 
		//if (! mTree[t][code].available(checkOnlyReserved)) {
			if (badtier) { *badtier = t; }
			if (badcode) { *badcode = code; }
			return false;
		}
		code = code / 2;
	}
	return true;
}

// Is the sub-tree with apex at this tier and startcode free?
// On the first call width is 1, then it recursively descends to check the sub-tiers,
// increasing the width by 2 each time to cover the entire sub-tree.
bool ChannelTree::isTierFreeDownward(Tier tier,unsigned startcode, unsigned width, bool checkOnlyReserved, Tier *badtier, unsigned *badcode)
{
	if (tier >= sNumTiers) {return true;}
	//printf("isTierFreeDownward(%d %d %d %d)\n",tier,startcode,width,checkOnlyReserved);
	unsigned code = startcode;
	for (unsigned i = 0; i < width; i++, code++) {
		//redundant with available(): if (mTree[tier][code].mReserved) {return false;}						// permanently reserved.
		if (!mTree[tier][code].available(checkOnlyReserved)) {
			if (badtier) { *badtier = tier; }
			if (badcode ) { *badcode = code; }
			return false;				// in use.
		}
		if (!isTierFreeDownward(tier+1,2*startcode,2*width,checkOnlyReserved,badtier,badcode)) {return false;}// something underneath is in use.
	}
	return true;		// entire specified sub-tree is free.
}

// This function opens the channel before returning to prevent a race.
// TODO: If we cannot allocate a channel with the specified KBps, should we allocate a lower-bandwidth ch?
DCHFEC *ChannelTree::chChooseByTier(Tier tier)
{
	ScopedLock lock(mChLock);
	unsigned sf = tier2sf(tier);
	// For a channel to be free the sub-tree below and all channels above that chcode must be unused.
	for (unsigned chcode = 0; chcode < sf; chcode++) {
		// Harvind (3-11-13) upward search should only check reserved codes, don't care if upward codes are "also reserved"
		if (isTierFreeDownward(tier,chcode,1,false,0,0) && isTierFreeUpward(tier,chcode,true,0,0)) {
			DCHFEC *result = mTree[tier][chcode].mDch;
			result->phChOpen();
			return result;
		}
	}
	return NULL;
}

DCHFEC *ChannelTree::chChooseByBW(unsigned ops)	// octets per second
{
	return chChooseByTier(bw2tier(ops,true));
}

DCHFEC *ChannelTree::chChooseBySF(unsigned sf)
{
	return chChooseByTier(sf2tier(sf));
}

// Allocate the tree tiers, leave them empty.
ChannelTree::ChannelTree()
{
	unsigned width = tier2sf(0);	// Tier width is equal to spreading factor.
	for (Tier tier = 0; tier < sNumTiers; tier++, width*=2) {
		mTree[tier] = new ChannelTreeElt[width];
		//mTree[tier] = (ChannelTreeElt*)calloc(sizeof(ChannelTreeElt),tier);
	}
}

// Populate every non-reserved channel with pre-allocated DCHFEC.
// You should reserve the SCCPCH channels before calling this.
void ChannelTree::chPopulate(ARFCNManager *radio)
{
	const unsigned numscr = 16777216;	// Number of uplink scrambling codes = 2**24

	// The uplink scrambling codes should probably be chosen so as to maximize
	// the distance between adjacent cells using the same scrambing codes.
	// Currently we use a consecutive block of about 512 scrambling codes.
	// TODO: We could use just 256 scrambling codes by taking the scrambling code
	// for higher tiers in the tree from the lowest tier.
	unsigned ulScramblingCode = gConfig.getNum("UMTS.Uplink.ScramblingCode");

	// These two channels are reserved by order of the UMTS gods.  See 25.213 5.2.1
	chReserve(256,0);	// primary CPICH
	chReserve(256,1);	// primary CCPCH a.k.a. BCH.

	unsigned sf = 4;
	for (Tier tier = 0; tier < sNumTiers; tier++, sf *= 2) {
		for (unsigned chcode = 0; chcode < sf; chcode++) {
			if (ulScramblingCode >= numscr) { ulScramblingCode = 0; }
			// TODO: We may want to limit the uplink SF
			// TODO: The encoder/decoders cannot currently handle the large channels.
			// TODO: The uplink spreading factor is the maximum, not the actual that will be used.
                        DCHFEC *dch = new DCHFEC(sf,chcode,(sf==4) ? 4: (sf/2),ulScramblingCode,radio);
			//DCHFEC *dch = new DCHFEC(sf,chcode,(sf<16) ? 8 : (sf/2),ulScramblingCode,radio);
			//dch->setRadio(radio);
			mTree[tier][chcode].mDch = dch;
			ulScramblingCode+=37841;	// Next uplink scrambling code, please.
			ulScramblingCode = ulScramblingCode % numscr;
		}
	}
}

// Test allocate cnt channels at specified sf.
void ChannelTree::chTestAlloc(int sf, int cnt, std::ostream &os)
{
	int i;
	for (i = 0; i < cnt; i++) {
		if (! gChannelTree.chChooseBySF(sf)) break;
	}
	os << "Allocated " <<i<< " at sf=" <<sf<<"\n";
	os << gChannelTree << "\n";
}

// Test free cnt channels at specified sf.
void ChannelTree::chTestFree(int sf, int cnt, std::ostream &os)
{
	// See if we can find active channels at this tier and deallocate them.
	int tier = sf2tier(sf);
	int freed = 0;
	for (int chcode = 0; chcode < sf && freed < cnt; chcode++) {
		ChannelTreeElt *cte = &mTree[tier][chcode];
		if (!cte->mReserved && cte->mDch && cte->mDch->phChAllocated()) {
			cte->mDch->phChClose();
			freed++;
		}
	}
	os << "Freed " <<freed<< " at sf=" <<sf<<"\n";
	if (freed) { os << gChannelTree << "\n"; }
}

void ChannelTree::chTest(std::ostream &os)
{
	chTestAlloc(256,16,os);
	chTestAlloc(256,16,os);
	chTestAlloc(8,8,os);
	chTestAlloc(4,1,os);	// Should fail.
	chTestFree(4,8,os);
	chTestAlloc(4,1,os);
	// Free all channels.
	chTestFree(256,256,os);
	chTestFree(8,8,os);
	chTestFree(4,4,os);
	chTestAlloc(4,1,os);
	chTestAlloc(16,5,os);
}

std::ostream& operator<<(std::ostream& os, const ChannelTree&tree)
{
	unsigned sf = 4;
	for (int tier = 0; tier < tree.sNumTiers; tier++, sf *= 2) {
		for (unsigned chcode = 0; chcode < sf; chcode++) {
			ChannelTreeElt *cte = &tree.mTree[tier][chcode];
			bool active = cte->mDch->phChAllocated();
			bool reserved = cte->mReserved;
			bool alsoReserved = cte->mAlsoReserved;
			if (active || reserved) {
				os << "PhCh(sf=" <<sf<< ",ch=" <<chcode<< ")=";
				if (reserved) os <<"reserved";
				if (alsoReserved) os <<" alsoReserved";
				if (reserved&&active) os <<",";
				if (active) os <<"active";
				os <<" ";
			}
		}
	}
	return os;
}

};	// namespace UMTS
