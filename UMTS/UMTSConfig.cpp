/**@file Central organizing object for UMTS Uu state. */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2011, 2012, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include "UMTSConfig.h"
#include "UMTSTransfer.h"
#include "UMTSLogicalChannel.h"
#include <ControlCommon.h>
#include <Logger.h>
#include <Ggsn.h>
#include "AsnHelper.h"
//#include "asn_system.h"	// Dont let other includes land in namespace ASN.
namespace ASN {

#include "asn_SEQUENCE_OF.h"
//#include "SystemInformation-BCH.h"
#include "CompleteSIBshort.h"

#include "MasterInformationBlock.h"
#include "SysInfoType1.h"
#include "SysInfoType2.h"
#include "SysInfoType3.h"
#include "SysInfoType4.h"
#include "SysInfoType5.h"
#include "SysInfoType6.h"
#include "SysInfoType7.h"
#include "SysInfoType8.h"
#include "SysInfoType9.h"
#include "SysInfoType10.h"
#include "SysInfoType11.h"
#include "SysInfoType12.h"
#include "SysInfoType13.h"
#include "SysInfoType14.h"
#include "SysInfoType15.h"
#include "SysInfoType16.h"
#include "SysInfoType17.h"
#include "SysInfoType18.h"
#include "SIB-Type.h"
#include "SystemInformation-BCH.h"
};
#include "URRC.h"

#define PAT_TEST 0 


using namespace std;
using namespace UMTS;
using namespace ASN;	// Historical: this module written before namespace ASN added.

static const unsigned sBeaconStartInvalid = 1;	// Impossible value.
static unsigned sPrevBeaconStart = sBeaconStartInvalid;
static const unsigned sMaxSibId = 18;


/** An array of the SIB types we support. */
//const SIBSb_TypeAndTag_PR UMTSConfig::SIBTypes[] = {
//	SIBSb_TypeAndTag_PR_sysInfoType1,
//	SIBSb_TypeAndTag_PR_sysInfoType2,
//	SIBSb_TypeAndTag_PR_sysInfoType3,
//	SIBSb_TypeAndTag_PR_sysInfoType5,
//	SIBSb_TypeAndTag_PR_sysInfoType7,
//	SIBSb_TypeAndTag_PR_sysInfoType11,
//};
//const unsigned UMTSConfig::numSIBTypes = sizeof(UMTSConfig::SIBTypes)/sizeof(SIBSb_TypeAndTag_PR);

// (pat) The plan for SIB scheduling.  I didnt write a scheduler, so must be done manually.
// If we discover that we dont know the number of segs ahead of time, then
// it will be time to write a scheduler.
// Note that only even Frame Numbers are used.
// Uses terminology from 8.1.1.1.5.
struct SibInfo_t {
	unsigned mSibIndex;	// Index into mSibPhase1, 0..mNumSibTypes-1.
	const char *mSibId;	// User printable name: MIB or SIB block type id to which this plan applies.
	ASN::e_SIB_Type mSibAsnType;	// The ASN enum value for this SIB id.
	ASN::SIBSb_TypeAndTag_PR mTypeTag;	// Another ASN enum value needed for this SIB.
	unsigned mSegCount;	// Number of segs.
	unsigned mSibPos;	// The position (phase?) of the first segment within one cycle.
	unsigned mSibRep; 	// Repetition period.
	// mSibOff - Documentation sucks so badly for this that I am just using the default,
	//	which is that all segments are consecutive, even though other layouts might
	//  have been more clever (like subsequent segments in same slot in next cycle.)
};

class BeaconConfig {

	ASN::MasterInformationBlock mMIB;
	ASN::SysInfoType1 mSIB1;
	ASN::SysInfoType2 mSIB2;
	ASN::SysInfoType3 mSIB3;
	ASN::SysInfoType3 mSIB4;
	ASN::SysInfoType5 mSIB5;
	ASN::SysInfoType6 mSIB6;
	ASN::SysInfoType7 mSIB7;
	ASN::SysInfoType8 mSIB8;
	ASN::SysInfoType9 mSIB9;
	ASN::SysInfoType10 mSIB10;
	ASN::SysInfoType11 mSIB11;
	ASN::SysInfoType12 mSIB12;
	ASN::SysInfoType13 mSIB13;
	ASN::SysInfoType14 mSIB14;
	ASN::SysInfoType15 mSIB15;
	ASN::SysInfoType16 mSIB16;
	ASN::SysInfoType17 mSIB17;
	ASN::SysInfoType18 mSIB18;

	/**@ Beacon paramters. */
	//@{
	unsigned mMIBValueTag;

	UInt_z mNumSibTypes;
	SibInfo_t mSibInfo[sMaxSibId+1];

	//static const ASN::SIBSb_TypeAndTag_PR SIBTypes[];
	static const unsigned sMaxSibSegments = 4;	// Max segments required by a SIB.
	//@}

	// This lock protects mSibPhase1, which is regnerated and used asynchronously.
	mutable Mutex mBeaconLock;		///< multithread access control
	// ByteVectors to hold the output of the phase1 SIB encoding.
	ByteVector *mSibPhase1[sMaxSibId+1];	// +1 for MIB

	/**@ Beacon scheduling table. */
	//@{
	// Only every other frame is used because the beacon uses TTI 20ms
	// and MIB is broadcast every 8th frame, so every 8 frames we can
	// broadcast 8/2-1 = 3 SIBs.   We currently broadcast 6 SIBs and
	// one of them is segmented, so we need a minimum repeat period of 32.
	static const unsigned msSibRepeat = 32;

	// (pat) Only the even values of mSIBSched are used, because TTI 20ms beacon,
	// so we dont bother saving the others.
	TransportBlock *mSibSched[msSibRepeat/2];
	//@}

	/** Encode an SIB to a transpoint block and populate it into the scheduling table. */
	void encodeSIBPhase1(
		unsigned sibIndex,	// Index into mSibInfo.
		const char *sibId,		// The RRC spec SIB type number used only for user readable messages, may be double in future.
		struct ASN::asn_TYPE_descriptor_s *type_descriptor,
		void *struct_ptr);	/* Structure to be encoded */
	void encodeSI(const char *sibId, ASN::SystemInformation_BCH &si, TransportBlock *trb, unsigned sfn);
	void encodeSIBPhase2( SibInfo_t *plan, unsigned sfn);

	void addSibPlan(const char *wSibId,		// user printable name for SIB type id (1,2,..) or 0 for MIB.
		unsigned wSibPos,	// The position, of the first segment within one cycle.
		unsigned wSegCount,	// Number of segs.
		unsigned wSibRep, 	// Repetition period.
		ASN::e_SIB_Type wSibASNType,	// The ASN enum value for this SIB id.
		ASN::SIBSb_TypeAndTag_PR wTypeTag);	// Another ASN enum value needed for this SIB.

	public:
	void beaconInit();
	BeaconConfig();
	void regenerate();
	void encodePhase2(unsigned sfn);
	TransportBlock *getSITB(unsigned sfn) {	// Get System Information Transport Block.
		assert(!(sfn&1));
		return mSibSched[(sfn % msSibRepeat)/2];
	}
};
static class BeaconConfig sBeacon;

BeaconConfig::BeaconConfig()
	: mMIBValueTag(0),mNumSibTypes(0)
{
	memset(mSibPhase1,0,sizeof(mSibPhase1));	// overkill - be safe
	memset(mSibInfo,0,sizeof(mSibInfo));		// overkill - be safe
	memset(mSibSched,0,sizeof(mSibSched));		// overkill - be safe
}

// TODO: This code should not be in a constructor because it implicitly references BitVector.
void BeaconConfig::beaconInit()
{
	for (unsigned i = 0; i < msSibRepeat/2; i++) {
		// Note: The TransportBlock on BCH is all payload.
		// If BCCH is mapped to FACH, then there is a 2 bit header, but we dont do that.
		mSibSched[i] = new TransportBlock(sSIBTrBlockSize);
		RN_MEMLOG(TransportBlock,mSibSched[i]);
	}

#define SIB_TAGS(n) \
	ASN::SIB_Type_systemInformationBlockType##n, ASN::SIBSb_TypeAndTag_PR_sysInfoType##n

	// First entry is for the MIB, transmitted on an 8 frame cycle.
	// The final argument is ignored, but we need to provide something that C++ accepts.
	addSibPlan("MIB",0,1,8,ASN::SIB_Type_masterInformationBlock, SIBSb_TypeAndTag_PR_NOTHING);
	addSibPlan("SIB1",2,1,16,SIB_TAGS(1));	// SIB 1 at pos 2
	addSibPlan("SIB2",4,1,16*2,SIB_TAGS(2));	// SIB 2 at pos 4
	addSibPlan("SIB3",6,1,16*2,SIB_TAGS(3));	// SIB 3 at pos 6; pos 8 reserved for MIB.
	// SIB 5 at pos 10, with 2 segments (ie 2 TransportBlocks)
	addSibPlan("SIB5",10,3,16,SIB_TAGS(5));
	// SIB 7 at pos 14 in 32-frame cycle.
	addSibPlan("SIB7",20,1,32,SIB_TAGS(7));
	// SIB 11 shares slot with SIB7 in a 32 frame cycle.
	addSibPlan("SIB11",22,1,32,SIB_TAGS(11));
	//addSibPlan("SIB12",62,1,64,SIB_TAGS(12));

	// (pat) Note: You cant regenerate the beacon in this constructor because
	// the other class constructors have not been called yet.
}

static int getConfigSccpchSF()
{
	return gConfig.getNum("UMTS.SCCPCH.SF");
}

static int getConfigPrachSF()
{
	return gConfig.getNum("UMTS.PRACH.SF");
}





UMTSConfig::UMTSConfig():
	// (pat) The radiomodem does not get the downlink scrambling code from here,
	// so I am removing it from this constructor as too confusing.
	// (pat) The class initializations should be finished before allocating
	// the FECs, so I am changing mBCH to a pointer and allocating it later.
	//mBCH(gConfig.getNum("UMTS.Downlink.ScramblingCode")),
	mInited(0),
	mBand((UMTSBand)gConfig.getNum("UMTS.Radio.Band")),
	mStartTime(::time(NULL))
{
}


void UMTSConfig::init(ARFCNManager *downstream)
{
	if (gConfig.defines("UMTS.Debug")) { rrcDebugLevel = gConfig.getNum("UMTS.Debug.RRC"); }
	// The default constructor for mBCH is fine.
	mBCH = new BCHFEC(downstream);


	// The channels 256,0 and 256,1 are reserved by UMTS.
	// TODO: Modify the beacon to get the fec classes.
	int chcode = gConfig.getNum("UMTS.SCCPCH.SpreadingCode");	// Use two consecutive ch starting here.
	unsigned fachSF = getConfigSccpchSF();
	unsigned rachSF = getConfigPrachSF();
	rrcInitCommonCh(rachSF,fachSF);
	unsigned ulScCode = gConfig.getNum("UMTS.PRACH.ScramblingCode");
	RrcTfs *tfs = gRrcCcchConfig->getDlTfs(0);
	mFachFec = new FACHFEC(fachSF,chcode,tfs->getPB(),tfs->getTBSize(0),tfs->getTTICode(),downstream);
	mFachFec->fecConfig(gRrcCcchConfig->mTrCh);
	mFachFec->open();
	tfs = gRrcCcchConfig->getUlTfs(0);
	//mPchFec = new PCHFEC(fachSF,chcode+1,downstream);	Not implemented yet, so dont bother.
	mRachFec = new RACHFEC(rachSF,ulScCode,tfs->getPB(),tfs->getTBSize(0),tfs->getTTICode());
	mRachFec->fecConfig(gRrcCcchConfig->mTrCh);
	mRachFec->open();
	macHookupRachFach(mRachFec,mFachFec,true);

	mInited = true;
	sBeacon.beaconInit();
	regenerateBeacon();	// (pat) This is the second call; we ignored the first - see apps/OpenBTS-UMTS.cpp
	// TODO: Right now regenerateBeacon is reserving channels.
	// That should be done above, but no matter.  We can populate the channel tree now.
	gChannelTree.chPopulate(downstream);

	/***
	// For debugging purposes: test the ChannelTree...
	std::cout << "Testing ChannelTree\n";
	gChannelTree.chTest(std::cout);
	exit(1);
	***/

	// This encodePhase2 is redundant, but causes this code to be tested during
	// initialization, even if there is no transciever.
	// Use UMTSConfig::time() to make sure we fail if overloaded UMTSConfig::time() is renamed
	// we dont get time() from the standard library.
	sBeacon.encodePhase2(UMTSConfig::time());

	/***
	// Debug: do it a few more times.
	regenerateBeacon();
	sBeacon.encodePhase2(0);
	regenerateBeacon();
	sBeacon.encodePhase2(1000);
	regenerateBeacon();
	sBeacon.encodePhase2(2000);
	regenerateBeacon();
	sBeacon.encodePhase2(3000);
	***/
}


void UMTSConfig::start(ARFCNManager* C0)
{
	sPrevBeaconStart = sBeaconStartInvalid;	// Force beacon re-encoding
	//mPowerManager.start();
	mPager.start();
	//mBCH->setDownstream(C0);	(pat) Moved this to the constructor.
	mBCH->start();
	// mFachFec->setDownstream(C0);	(pat) Moved this to the constructor.
	// Do not call this until AGCHs are installed.
	//mAccessGrantThread.start(RRC::AccessGrantServiceLoop,NULL);
	SGSN::Ggsn::start();
	l2RlcStart();	// Start the handler for the high side of RLCs.
}



void BeaconConfig::addSibPlan(const char *wSibId,	// user printable SIB type id (1,2,..) or 0 for MIB.
	unsigned wSibPos,	// The position, always even, of the first segment within one cycle.
	unsigned wSegCount,	// Number of segs.
	unsigned wSibRep, 	// Repetition period.
	ASN::e_SIB_Type wSibAsnType,	// The ASN enum value for this SIB id.
	ASN::SIBSb_TypeAndTag_PR wTypeTag)	// Another ASN enum value needed for this SIB.
{
	unsigned ind = mNumSibTypes;
	mSibInfo[ind].mSibIndex = mNumSibTypes;	// Used as index into mSibPhase1.
	mSibInfo[ind].mSibId = wSibId;	// user printable name
	mSibInfo[ind].mSibPos = wSibPos;
	mSibInfo[ind].mSegCount = wSegCount;
	mSibInfo[ind].mSibRep = wSibRep;
	mSibInfo[ind].mSibAsnType = wSibAsnType;
	mSibInfo[ind].mTypeTag = wTypeTag;
	// Create the buffer for the phase1 encoder:
	mSibPhase1[ind] = new ByteVector(1 + (sMaxSibSegments * sSIBTrBlockSize)/8);
	mSibPhase1[ind]->fill(0);	// unnecessary but neat.
	RN_MEMLOG(ByteVector,mSibPhase1[ind]);
	mNumSibTypes = mNumSibTypes + 1;
}

//unsigned SIBOffset(unsigned i)
//{
//	// The pattern is 2, 4, 6, 10, 12, 14, 18, 20, 22 ...
//	// because the MIB goes at position 0, 8, 16, ...
//	// The times 2 is because the result is an SFN [frame number] but the beacon
//	// is TTI 20ms, ie, 2 frames per SIB.
//	unsigned m3 = i%3;
//	unsigned d3 = i/3;
//	return d3*8 + (m3+1)*2;
//}

// We zero out the entire byte that endBit falls in.
// Unused, untested.
//static void zeroBitPad(char *buf, unsigned startBit, unsigned lengthBits)
//{
//	unsigned pos = startBit/8;
//	unsigned tailBits = startBit%8;
//	if (tailBits) {	// Zero the tail of the last byte.
//		unsigned mask = (1 << (8-tailBits)) - 1;	// Mask of bits we want to zero.
//		buf[pos++] &= ~mask;
//	}
//	while (pos < (lengthBits+7)/8) { buf[pos++] = 0; }
//}

// Encode the SIB into an ASN struct into the corresponding mSibPhase1[] buffer.
void BeaconConfig::encodeSIBPhase1(
	unsigned sibIndex,
	const char *sibId,
	struct asn_TYPE_descriptor_s *type_descriptor,
	void *struct_ptr)	/* Structure to be encoded */
{
	bool rn_asn_debug_beacon = gConfig.getNum("UMTS.Debug.ASN.Beacon");
	if (rn_asn_debug_beacon) { printf("=== Phase1 Encoding SIB %s\n",sibId); }
	assert(0 == strcmp(sibId,mSibInfo[sibIndex].mSibId));
	ByteVector *perBuf = mSibPhase1[sibIndex];
	perBuf->resetSize();

	asn_enc_rval_t rval = uper_encode_to_buffer(
		type_descriptor,struct_ptr,(void*)perBuf->begin(),perBuf->size());
	int numBits = rval.encoded;	// Must be an int to detect less than zero!!
	if (numBits<0) {
		LOG(ERR) << "failed to encode SIB " << sibId <<" into buf size="<<perBuf->size();
		assert(0);
	}
	perBuf->setSizeBits(numBits);
	printf("=== Phase1 Encoded SIB %s size %u blocks %g\n",sibId,numBits,numBits/226.0);
	if (rn_asn_debug_beacon) {
		cout << "SIB" << sibIndex << ": bytes: " << *perBuf << endl;
	}
	fflush(stdout);
}

// Encode the System Information message into a TransportBlock.
void BeaconConfig::encodeSI(const char *sibId, SystemInformation_BCH &si, TransportBlock *trb, unsigned sfn)
{
	// Second step encoding of the complete System Information message, as per 25.331 12.1.3-2
	const unsigned bufSize = 1+ 1 + sSIBTrBlockSize/8;
	unsigned char siPerBuf[bufSize];
	// (10-5-2012 pat) The encoder output appears to have interjected randomness, so lets try pre-zeroing out the buffer:
	// Update: that did the trick.
	memset(siPerBuf,0,bufSize);
	// (pat) result.encoded is number of bits, despite documentation
	// at asn_enc_rval_t that claims it is in bytes.
	//rn_asn_debug = gConfig.getNum("UMTS.Debug.ASN.Beacon");
	asn_enc_rval_t siRval = uper_encode_to_buffer(&asn_DEF_SystemInformation_BCH,&si,(void*)siPerBuf,bufSize);
	if (siRval.encoded<0) {
		LOG(ERR) << "failed to encode SIB " << sibId;
		assert(0);
	}
	int sizeBits = siRval.encoded;
	if (sizeBits > (int)trb->size()) {
		LOG(ERR) << "SIB " << sibId << " size " << sizeBits << " does not fit BCH transfer block " << trb->size();
	}
	// TODO: Just zero out the tail, but I didnt want to risk changing this while Harvind
	// is busily debugging it.
	trb->zero();	// Pat added.
	trb->unpack(siPerBuf);
	trb->setSchedule(sfn);
	trb->mDescr = (string)sibId;	// TODO: make sibId a string so this does not bother to allocate memory.
}

// Put the SIBs, which were previously phase1 encoded into PER strings, into
// one or more System Information messages inside TransportBlocks to go in the beacon.
// We have to run this anew for each beacon because the blocks include the SFN.
// See 10.2.48 SYSTEM INFORMATION message.
// See also: 8.1.1.1.3 "Segmentation and concatenation of system information blocks",
// and there is a picture at Figure 12.1.3-2: "Padding for System Information."
void BeaconConfig::encodeSIBPhase2(
	SibInfo_t *plan,
	unsigned sfn)
{
	SystemInformation_BCH si;
	memset(&si,0,sizeof(si));
	assert(!(sfn&1));	// sfn is even.
	si.sfn_Prime = sfn/2;
	unsigned pos = sfn % msSibRepeat;	// Position in the beacon.

	ByteVector *in = mSibPhase1[plan->mSibIndex];
       
#if 0
	// DIETER messages 
	/*if (plan->mSibIndex == 1) { // SIB1
                const char dufus[] = "C4020188000C10001802019C035680";
                BitVector tmp(15*8);
                tmp.unhex(dufus);
                in = new ByteVector(tmp);
	}*/
        /*if (plan->mSibIndex == 3) { // SIB3
                const char dufus[] = "800000001B0C00080000969BC0";
                BitVector tmp(13*8);
                tmp.unhex(dufus);
                in = new ByteVector(tmp);
        }*/
        /*if (plan->mSibIndex == 3) { // SIB5
                const char dufus[] = "256C3AFFFF43FFFC5210F0290C0A8018000C8FF7B17EE10FF000003F3647FF03202038101608100088642990A50018088B9581000F18C108B6D850021844A05880318400";
                BitVector tmp(68*8);
                tmp.unhex(dufus);
                in = new ByteVector(tmp);
        }*/
	/*if (plan->mSibIndex == 6) { // SIB11
		const char dufus[] = "019FC10609B20696B29201020A2A80A80BB005411400049F600A00";
		BitVector tmp(27*8);
		tmp.unhex(dufus);
		in = new ByteVector(tmp);
	}*/
#endif

	//LOG(INFO) << "sfn: " << sfn << " ix: " << plan->mSibIndex;

	// (pat) 25.331 12.1.3-2 "Padding for System Information" is difficult reading.
	// Translated: you use the exact bit-length from the first per encoder step above,
	// which is equivalent to "ignoring the padding added to achieve octet alignment".
	// Then if you land in one of the fixed length variants below (eg, completeSIB)
	// you must zero-bit-pad to the fixed length of the buffer, eg 226.

	int numBits = in->sizeBits();
	if (numBits <= 214) {
		// Short variant: 10.2.48.7.  214 is a magic number from the IE description.
		// Note: the IE description says "upto 214 bits" but they mean inclusive.
		assert(plan->mSegCount == 1);
		si.payload.present = SystemInformation_BCH__payload_PR_completeSIB_List;
		CompleteSIBshort_t sibShort;
		memset(&sibShort,0,sizeof(sibShort));
        asn_long2INTEGER(&sibShort.sib_Type, plan->mSibAsnType);
		// We use the exact bit length here, which is equivalent
		// to truncating the "encoder added padding".
		setAsnBIT_STRING(&sibShort.sib_Data_variable,in->begin(),numBits);
		ASN_SEQUENCE_ADD(&si.payload.choice.completeSIB_List,&sibShort);
		encodeSI(plan->mSibId, si, getSITB(pos),sfn);
	} else if (numBits <= 226) {
		// Long variant: 10.2.48.6  226 is the magic maximum length number from the IE description.
		assert(plan->mSegCount == 1);
		si.payload.present = SystemInformation_BCH__payload_PR_completeSIB;
		// In this case the bit string is zero padded out to length 226.
		// 12.1.3. says the worst case padding needed is 17, so 32 should be plenty.
		in->setField(numBits,0,32);
        asn_long2INTEGER(&si.payload.choice.completeSIB.sib_Type, plan->mSibAsnType);
		// (10-2012 pat) The 226 is longer than in.size but the underlying ByteVector is large enough to
		// hold the maximum size payload so we are not reading beyond the end of the ByteVector storage.
		setAsnBIT_STRING(&si.payload.choice.completeSIB.sib_Data_fixed,in->begin(),226);
		encodeSI(plan->mSibId, si, getSITB(pos),sfn);
	} else {
		// The first segment holds 222 bits.
		// The second segment uses "Last segment (short)" format up to 214 bits.
		// There are longer formats but we dont need them yet.
		assert(plan->mSegCount > 1);
		// First Segment 10.2.48.1
		int i = 0;
		do {
			if (i == 0) {
				si.payload.present = SystemInformation_BCH__payload_PR_firstSegment;
				FirstSegment_t *firstSeg = &si.payload.choice.firstSegment;
        			asn_long2INTEGER(&firstSeg->sib_Type, plan->mSibAsnType);
        			firstSeg->seg_Count = plan->mSegCount;
				//LOG(INFO) << "SIB5: " << *in;
				setAsnBIT_STRING(&firstSeg->sib_Data_fixed,in->begin(),222);
				encodeSI(plan->mSibId, si, getSITB(pos), sfn);
			} else {
                                // Harvind was here...
                                si.payload.present = SystemInformation_BCH__payload_PR_subsequentSegment;
                                SubsequentSegment_t *subSeg = &si.payload.choice.subsequentSegment;
                                asn_long2INTEGER(&subSeg->sib_Type, plan->mSibAsnType);
                                subSeg->segmentIndex = i;
                                //LOG(INFO) << "SIB5: " << *in;
                                setAsnBIT_STRING(&subSeg->sib_Data_fixed,in->begin(),222);
                                encodeSI(plan->mSibId, si, getSITB(pos), sfn);
			}

			// We need to chop off the first 222 bits.  222 == 27*8+6.
			// todo: make this more efficient.
			numBits = numBits-222;
#if PAT_TEST
	// TODO: Try to test this:
			ByteVector *in2 = new ByteVector((numBits+7)/8);
			for (int j = 0; j < numBits; j++) {
				in2->setBit(j,in->getBit(j+222));
			}
			in = in2;
#else
			BitVector tmp1(numBits +6+8);
			tmp1.unpack(in->begin()+27);
			BitVector tmp2(tmp1.tail(6));
			unsigned char buf[numBits/8+2];
			tmp2.pack(buf);
			if (i > 0) delete in;
			in = new ByteVector(buf,numBits/8+2);					// convert back to byte array.
			RN_MEMLOG(ByteVector,in);
#endif

			// Next segment
			sfn += 2;
			pos = sfn % msSibRepeat;	// Position in the beacon.
			memset(&si,0,sizeof(si));
			si.sfn_Prime = sfn/2;
			i++;
		} while (numBits > 222);

		if (numBits <= 214) {
			si.payload.present = SystemInformation_BCH__payload_PR_lastSegmentShort;
			LastSegmentShort_t *lastSegShort = &si.payload.choice.lastSegmentShort;
	        	asn_long2INTEGER(&lastSegShort->sib_Type, plan->mSibAsnType);
			// This is the 0 based segment index, but since it does not appear in
			// the first block, it is range 1..15
			lastSegShort->segmentIndex = i;
			setAsnBIT_STRING(&lastSegShort->sib_Data_variable,in->begin(),numBits);
			encodeSI(plan->mSibId, si, getSITB(pos), sfn);
		}
		else { 
                        si.payload.present = SystemInformation_BCH__payload_PR_lastSegment;
                        LastSegment_t *lastSeg = &si.payload.choice.lastSegment;
                        asn_long2INTEGER(&lastSeg->sib_Type, plan->mSibAsnType);
			lastSeg->segmentIndex = i;
#if PAT_TEST
						if (numBits < 222) { in->setField(numBits,0,222 - numBits); } // (pat) zero pad the tail.
#endif
                        setAsnBIT_STRING(&lastSeg->sib_Data_fixed,in->begin(),222); //numBits);
                        encodeSI(plan->mSibId, si, getSITB(pos), sfn);
		}
		delete in;
	} /*else {
		LOG(ERR) << "SIB " << plan->mSibId << " size " << numBits << " does not fit SI block";
		assert(0);
	}*/
}




// (pat) And just where are we supposed to put the additional segments for multi-segment SIBs?
// The documentation for this totally sucks.
// In 8.1.1.1.5, the intro comment indicates they wanted to have short-rep period blocks
// interspersed with "blocks with segmentation over many frames".
// Well, first there is no necessary reason why rep-period and segmentation are
// mutually exclusive so the comment is intrinsically slightly nonsensical.
// Elsewhere in this paragraph the word "frame" means a radio frame;
// I think they meant to say "cycle" to mean a group of rep-period [eg 16] frames.
// Later there is a formula that specifies non-uniform offsets, implying that
// the segments can be scattered over long distances, as is their intention.
// But SIB_OFF is an indexed variable in the repetition formula.  Sigh.
// The real clue is in a comment from 10.3.8.16, and I quote:
// "SIB_POS Offset Info: The default value is that all segments are
// consecutive, i.e., that the SIB_OFF = 2 for all
// segments except when MIB segment/complete MIB is
// scheduled to be transmitted in between segments
// from same SIB. In that case, SIB_OFF=4 in between
// segments which are scheduled to be transmitted at
// SFNprime = 8 *n-2 and 8*n + 2, and SIB_OFF=2 for the rest of the segments."
// Ah ha!  So the SIB_POS Offset IE affects the segment position in a way
// that is not documented precisely.  But apparently we can ignore all this and
// just use the default value so the consecutive segments be, well, consecutive.
//void UMTSConfig::fillMIBSchedule()
//{
//	// Encode MIB into transport block.
//	encodeSIB(SIB_Type_masterInformationBlock, &asn_DEF_MasterInformationBlock,&mMIB,0);
//	// Put pointers to MIB in SIB scheduling table at 0, 8, 16, ...
//	// For FDD MIB, per table 8.1.1: SIB_POS = 0, SIB_REP = 8, SIB_OFF = 2.
//	unsigned FN = 0;
//	while (FN<mSIBRepeat) {
//		mSibSched[FN] = tbMIB;
//		FN += 8;
//	}
//
//}

long UMTSConfig::getULInterference() const
{
	// TODO UMTS -- This needs to really calculate something.
	return -70;
}

static long *newlong(long value) {
	long *result = RN_CALLOC(long);
	*result = value;
	return result;
}

// This fills in the mMIB and mSIB* structures.
void UMTSConfig::regenerateBeacon()
{
	// The BeaconConfig::regenerate itself calls gConfig, which may try
	// to do a recursive call through here; we prevent that with noRecursionPlease
	static int noRecursionPlease = 0;
	// This check for mInited is needed because regenerateBeacon()
	// is called from purgeconfig in apps/OpenBTS-UMTS.cpp before
	// the UMTSConfig class has been initialized.
	if (!mInited) {return;}
	ScopedLock lock(mLock);	// First wait to lock the configuration.
	if (noRecursionPlease) return;
	noRecursionPlease++;
	sBeacon.regenerate();
	noRecursionPlease--;
}

static SCCPCH_SystemInformation *generateSCCPCH(unsigned fachChCode, bool addPICH,unsigned pichChCode)
{
	// (pat) See 10.3.6.72: Secondary CCPCH Info IE.
	SCCPCH_SystemInformation *sCCPCH_SI = RN_CALLOC(ASN::SCCPCH_SystemInformation);
	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.present = SecondaryCCPCH_Info__modeSpecificInfo_PR_fdd;
	asn_long2INTEGER(&(sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.dummy1),PCPICH_UsageForChannelEst_mayBeUsed);

	// FIXME -- Figure out what STTD is and be sure we don't use it.
	// (pat) See 10.3.6.86: STTD is one of the TX Diversity modes.
	// SCCPCH Secondary Common Control Physical Channel is the one carrying FACH and PCH.
	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sttd_Indicator = false;
	unsigned scc_sf = getConfigSccpchSF();	// (pat) Not currently programmable.
	switch (scc_sf) {
		case 256:
			sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf256;
			break;
		case 128:
						sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf128; //256;
			break;
		case 64:
						sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf64; //256;
			break;
		case 32:
						sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf32; //256;
			break;
		case 16:
						sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf16; //256;
			break;
		case 8:
						sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf8; //256;
			break;
		case 4:
						sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf4; //256;
			break;
		default:
			break;
	}
	// We are assigning to choice.sf256 but it is the same variable location for all SFs.
	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.choice.sf256 = fachChCode;

	// (pat) We must reserve this spot in the ChannelTree.
	// This code is redundant with the one that creates the channel in FACHFEC(),
	// but cant be too careful.
	// (pat) If scc_sf is not 256, then the other FACH/PICH channels are going to collide with this in the
	// tree of spreading codes and need to be changed.
	//gChannelTree.chReserve(scc_sf,fachChCode);	// (pat) This channel may not be used for DCH.

	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.pilotSymbolExistence = false;
	// (pat)  No tfci implies "blind" transport format selection.
	// It can be used if you are not multiplexing channels so you can use the number of bits
	// to imply the transport format selected.

	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.tfci_Existence = true;
	asn_long2INTEGER(&(sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.positionFixedOrFlexible),PositionFixedOrFlexible_fixed);
	// (pat) First FACH TFS+TFCS is mandatory.
	// I believe the subsequent ones are optional and will use the same one if not specified.
	FACH_PCH_Information *fachPchInfo = RN_CALLOC(ASN::FACH_PCH_Information);
#if 1 //PAT_TEST
	// (pat) It is 1, not 3.
	fachPchInfo->transportChannelIdentity = gRrcDcchConfig->getDlTrChInfo(0)->mTransportChannelIdentity;		// Range is 1..32
	assert(fachPchInfo->transportChannelIdentity == 1);
#else
	fachPchInfo->transportChannelIdentity = 1+1;
#endif

	// We are leaving CTCH indicator false.
	// PICH_info is optional - needed only if PCH is multiplexed.
	gRrcCcchConfig->getDlTfs()->toAsnTfs(&fachPchInfo->transportFormatSet,true);

	gRrcCcchConfig->getDlTfcs()->toAsnTfcs((sCCPCH_SI->tfcs = RN_CALLOC(ASN::TFCS)),TrChFACHType);

	sCCPCH_SI->fach_PCH_InformationList = RN_CALLOC(ASN::FACH_PCH_InformationList);
	ASN_SEQUENCE_ADD(&sCCPCH_SI->fach_PCH_InformationList->list,fachPchInfo); 
	// FIXME: Adding second FACH to SIB5, though it doesn't really exist.  Phone assumes first FACH is PCH and won't go into the RACH procedure
#if 0	// unused code
	/*FACH_PCH_Information *fachPchInfo2 = RN_CALLOC(ASN::FACH_PCH_Information);
	memcpy(fachPchInfo2,fachPchInfo,sizeof(ASN::FACH_PCH_Information));
	fachPchInfo2->transportChannelIdentity = 2;
		ASN_SEQUENCE_ADD(&sCCPCH_SI->fach_PCH_InformationList->list,fachPchInfo2);*/
		/*FACH_PCH_Information *fachPchInfo3 = RN_CALLOC(ASN::FACH_PCH_Information);
		memcpy(fachPchInfo3,fachPchInfo,sizeof(ASN::FACH_PCH_Information));
		fachPchInfo3->transportChannelIdentity = 3;
		ASN_SEQUENCE_ADD(&sCCPCH_SI->fach_PCH_InformationList->list,fachPchInfo3);*/
#endif
	if (addPICH) {	// Add PICH
		PICH_Info_t *pichInfo = RN_CALLOC(ASN::PICH_Info);
		pichInfo->present = PICH_Info_PR_fdd;
		assert(pichChCode < scc_sf);	// Configuration error if this happens.
		pichInfo->choice.fdd.channelisationCode256 = pichChCode;
		asn_long2INTEGER(&(pichInfo->choice.fdd.pi_CountPerFrame),PI_CountPerFrame_e18);
		pichInfo->choice.fdd.sttd_Indicator = false;
		sCCPCH_SI->pich_Info = pichInfo;
	}
	return sCCPCH_SI;
}

// This macro is from ASN/.../constr_TYPE.h
#undef ASN_STRUCT_FREE_CONTENTS_ONLY
#define ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF,ptr) { \
		rn_asn_debug = asn_debug_free; \
		(asn_DEF).free_struct(&(asn_DEF),ptr,1); \
		rn_asn_debug = asn_debug_beacon; \
		}

void BeaconConfig::regenerate()
{
	bool asn_debug_beacon = gConfig.getNum("UMTS.Debug.ASN.Beacon");
	bool asn_debug_free = gConfig.getNum("UMTS.Debug.ASN.Free");
	rn_asn_debug = asn_debug_beacon;
	// Note: we already locked UMTSConfig.
	// Currently, the only other locker is encodePhase2 which is brief.
	// But make sure you dont insert a race condition here.
	// (pat) NOTE: The mutex we use is recursive, so it does not prevent the same
	// thread from going right through here.
	ScopedLock lock(mBeaconLock);

	sPrevBeaconStart = sBeaconStartInvalid;	// Force beacon re-encoding, but hardly matters.

	// Update everything from the configuration.
	LOG(NOTICE) << "regenerating system information messages";

	// TODO: Update LAI.
	// TODO: We are throwing away the memory allocated in sub-structures, which is
	// easy to fix, but the amount is small so defer.
	// Note: You must clear the mMIB and mSIB structs before using them to make
	// sure that the lists are inited to 0 size.  Otherwise they just get bigger
	// every time through this loop.

	// Generate the MIB.
	// 3GPP 25.331 10.2.48.8.1
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_MasterInformationBlock,&mMIB);
	// Value Tag, 10.3.8.1
	memset(&mMIB,0,sizeof(mMIB));
	mMIB.mib_ValueTag = mMIBValueTag%8 + 1;

	// PLMN parameters, 10.3.1.12, 10.3.1.11
	mMIB.plmn_Type.present = PLMN_Type_PR_gsm_MAP;
	const string MCC = gConfig.getStr("UMTS.Identity.MCC");
	setASN1SeqOfDigits(
		(void*)&mMIB.plmn_Type.choice.gsm_MAP.plmn_Identity.mcc.list,
		MCC.c_str());
	const string MNC = gConfig.getStr("UMTS.Identity.MNC");
	setASN1SeqOfDigits(
		(void*)&mMIB.plmn_Type.choice.gsm_MAP.plmn_Identity.mnc.list,
		MNC.c_str());

	// SIB reference list, 10.3.8.14
	for (unsigned i=1; i<mNumSibTypes; i++) {	// Skip 0, which is for the MIB
		SibInfo_t *plan = &mSibInfo[i];
		// (david) A bit of a hack - We are putting the MOB value tag everywhere.
		// (pat) There are different versions of this list for different releases of RRC spec.
		// We are using the oldest one, which uses IE 10.3.18a instead of other variants.
		// List of: Scheduling Information IE 10.3.8.16 + SIB & SB Type IE 10.3.8.18a.
		SchedulingInformationSIBSb *SIBSb = RN_CALLOC(ASN::SchedulingInformationSIBSb);
		SIBSb->sibSb_Type.present = plan->mTypeTag;
		//printf("SIB %d type %d\n",i,(int)plan->mTypeTag);
		switch (plan->mTypeTag) {
			case SIBSb_TypeAndTag_PR_sysInfoType1:
				// type is PLMN-ValueTag, 1..256
				SIBSb->sibSb_Type.choice.sysInfoType1 = mMIBValueTag%256 + 1;
				break;
			case SIBSb_TypeAndTag_PR_sysInfoType2:
				// type is CellValueTag, 1..4
				SIBSb->sibSb_Type.choice.sysInfoType2 = mMIBValueTag%4 + 1;
				break;
			case SIBSb_TypeAndTag_PR_sysInfoType3:
				// type is CellValueTag, 1..4
				SIBSb->sibSb_Type.choice.sysInfoType3 = mMIBValueTag%4 + 1;
				break;
			case SIBSb_TypeAndTag_PR_sysInfoType5:
				// type is CellValueTag, 1..4
				SIBSb->sibSb_Type.choice.sysInfoType5 = mMIBValueTag%4 + 1;
				break;
			case SIBSb_TypeAndTag_PR_sysInfoType7:
				// type is NULL
				break;
			case SIBSb_TypeAndTag_PR_sysInfoType11:
				// type is CellValueTag, 1..4
				SIBSb->sibSb_Type.choice.sysInfoType11 = mMIBValueTag%4 + 1;
				break;
                        case SIBSb_TypeAndTag_PR_sysInfoType12:
                                // type is CellValueTag, 1..4
                                SIBSb->sibSb_Type.choice.sysInfoType12 = mMIBValueTag%4 + 1;
                                break;
			default:
				LOG(ERR) << "uncoded sys info type " <<plan->mSibId <<" asn type "<<(int)plan->mTypeTag;
				assert(0);
		}

		// (pat) 10.3.8.16 indicates the position is "INTEGER (0 ..Rep-2 by step of 2)",
		// but the constraints in rrc.asn1 for SchedulingInformation sib-Pos are:
		// rep8 INTEGER (0..3),
		// rep16 INTEGER (0..7),
		// rep32 INTEGER (0..15),
		// In other words, we are supposed to pre-divide it by 2.  Gotta love it.
		switch (plan->mSibRep) {
		case 8:
			SIBSb->scheduling.scheduling.sib_Pos.present = SchedulingInformation__scheduling__sib_Pos_PR_rep8;
			SIBSb->scheduling.scheduling.sib_Pos.choice.rep8 = plan->mSibPos/2;
			break;
		case 16:
			SIBSb->scheduling.scheduling.sib_Pos.present = SchedulingInformation__scheduling__sib_Pos_PR_rep16;
			SIBSb->scheduling.scheduling.sib_Pos.choice.rep16 = plan->mSibPos/2;
			break;
		case 32:
			SIBSb->scheduling.scheduling.sib_Pos.present = SchedulingInformation__scheduling__sib_Pos_PR_rep32;
			SIBSb->scheduling.scheduling.sib_Pos.choice.rep32 = plan->mSibPos/2;
			break;
                case 64:
                        SIBSb->scheduling.scheduling.sib_Pos.present = SchedulingInformation__scheduling__sib_Pos_PR_rep64;
                        SIBSb->scheduling.scheduling.sib_Pos.choice.rep64 = plan->mSibPos/2;
                        break;
		default:
			assert(0);	// To fix just add the case you need.
		}
		if (plan->mSegCount != 1) {
			SIBSb->scheduling.scheduling.segCount = RN_CALLOC(ASN::SegCount_t);
			*SIBSb->scheduling.scheduling.segCount = plan->mSegCount;
#if PAT_TEST
	// Try leaving this out.  It is supposed to default correctly.
	// The TEMS phone worked even when this was encoded incorrectly.
	// The Samsung phone works without it.
#else
			SIBSb->scheduling.scheduling.sib_PosOffsetInfo = RN_CALLOC(struct SibOFF_List);
			// (pat) This is an array of enumerated values, not simple integers.
			for (int j = 0 ; j < (int)plan->mSegCount-1; j++) {
				//SibOFF* sibOff = RN_CALLOC(ASN::SibOFF);
				SibOFF_t* sibOff = RN_CALLOC(ASN::SibOFF_t);
				//*sibOff = SibOFF_so4; //FIXME: Why isn't this SibOFF_so2?
				// (pat) Because is it is an ASN "ENUMERATED" value, a simple int, so we were
				// putting the entirely wrong value in here.  Amazing it worked at all.
				asn_long2INTEGER(sibOff,SibOFF_so2);
				ASN_SEQUENCE_ADD(&SIBSb->scheduling.scheduling.sib_PosOffsetInfo->list,sibOff);
			}
#endif
		}

		ASN_SEQUENCE_ADD(&mMIB.sibSb_ReferenceList.list,SIBSb);
	}
	//fillMIBSchedule();
	mMIBValueTag++;

	// The SIBs themselves.
	// Behold the glory that is UMTS!!

	// Type 1
	// 3GPP 25.331 10.2.48.8.4
	// ASN SysInfoType1
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SysInfoType1,&mSIB1);
	memset(&mSIB1,0,sizeof(mSIB1));
	// cn_CommonGSM_MAP_NAS_SysInfo
	// See 3GPP 24.008 10.5.1.12.1.  It's just the LAC.
	// Let's see how many lines of C it takes to encode it!
	uint16_t *LAC = (uint16_t*)calloc(1,sizeof(*LAC));
	*LAC = htons(gConfig.getNum("UMTS.Identity.LAC"));
	// LAC is an OCTET_STRING
	mSIB1.cn_CommonGSM_MAP_NAS_SysInfo.buf = (uint8_t*)LAC;
	mSIB1.cn_CommonGSM_MAP_NAS_SysInfo.size = 2;
	// cn_DomainSysInfoList
	// See 3GPP 24.008 10.5.1.12.2-3.  Domain-specific NAS sysinfo
	// For now, we are advertising PS domain only.
	CN_DomainSysInfo *dsi = RN_CALLOC(ASN::CN_DomainSysInfo);
        asn_long2INTEGER(&(dsi->cn_DomainIdentity), CN_DomainIdentity_ps_domain);
	dsi->cn_Type.present = CN_DomainSysInfo__cn_Type_PR_gsm_MAP;

	// RAC is a 2 byte OCTET_STRING: first byte is RAC, second byte is:
	//	0 => NMO I, 1 => NMO II, top 7 bits in that byte unused.
	// 23.060 6.5.2:
	// "A GPRS-attached MS makes an IMSI attach via the SGSN with the combined RA / LA update
	// procedure if the network operates in mode I. If the network operates in mode II,
	// or if the MS is not GPRS-attached, the MS makes a normal IMSI attach.
	// An IMSI-attached MS engaged in a CS connection shall use the (non-combined)
	// GPRS Attach procedure when it performs a GPRS attach."
	// Pat says: These NMO are different than GPRS.
	// We will try to use NMO I, combined attach.
	// The SGSN supports NMO I, without security, and without actually doing anything
	// with the CS connection imsi-attach, which currently has no meaning for UMTS anyway.
	// This also hopefully allows us to bypass UMTS security.
	// Update: no it did not.
	int nmo = gConfig.getNum("GPRS.NMO");	// config value is 1 based
	uint8_t *RAC = (uint8_t*)calloc(2,sizeof(char));
	RAC[0] = gConfig.getNum("GPRS.RAC");
	RAC[1] = RN_BOUND((nmo-1),0,1);
	dsi->cn_Type.choice.gsm_MAP.buf = RAC;		// It is an OCTET_STRING_T
	dsi->cn_Type.choice.gsm_MAP.size = 2;
	dsi->cn_DRX_CycleLengthCoeff = gConfig.getNum("UMTS.CN-DSI.CycleLengthCoeff"); 	// FIXME -- What does this mean?!
	// add the element to the list
	ASN_SEQUENCE_ADD(&mSIB1.cn_DomainSysInfoList.list,dsi);

#if PAT_TEST
	if (1) {
		// Advertise a CS domain also.  Pat put this back in 10-23-2012
		CN_DomainSysInfo *dsi2 = RN_CALLOC(CN_DomainSysInfo);
        	asn_long2INTEGER(&(dsi2->cn_DomainIdentity), CN_DomainIdentity_cs_domain);
        	dsi2->cn_Type.present = CN_DomainSysInfo__cn_Type_PR_gsm_MAP;
        	uint8_t *CS_domain_specific_info = (uint8_t*)calloc(2,sizeof(char));
		// octet 0 is T3212 in deci-hours.
		CS_domain_specific_info[0] = gConfig.getNum("UMTS.Timer.T3212");
		// octet 1 is attach-detach allowed flag: 1 means "MSs shall apply IMSI attach and detach procedure".
		CS_domain_specific_info[1] = 1;
		dsi2->cn_Type.choice.gsm_MAP.buf = CS_domain_specific_info;
        	dsi2->cn_Type.choice.gsm_MAP.size = 2;
		// DRX_CycleLengthCoeff: "A coefficient in the formula to count the paging occasions to be used by a specific UE (specified
		//	in  3GPP TS 25.304: "UE Procedures in Idle Mode and Procedures for Cell Reselection in Connected Mode".)
        	dsi2->cn_DRX_CycleLengthCoeff = gConfig.getNum("UMTS.CN-DSI.CycleLengthCoeff");        // FIXME -- What does this mean?!
        	ASN_SEQUENCE_ADD(&mSIB1.cn_DomainSysInfoList.list,dsi2);             
	}
#endif

	// (pat) I tried taking this out but was not sure if it changed the blackberry SIB1 report or not.
	// Everything else is optional, so ignore it.
	UE_ConnTimersAndConstants *connTimers = RN_CALLOC(ASN::UE_ConnTimersAndConstants);
        connTimers->t_301 = RN_CALLOC(ASN::T_301_t);	asn_long2INTEGER(connTimers->t_301,ASN::T_301_ms2000);
	connTimers->n_301 = RN_CALLOC(ASN::N_301_t);	*(connTimers->n_301) = 2;
        connTimers->t_302 = RN_CALLOC(ASN::T_302_t);	asn_long2INTEGER(connTimers->t_302,ASN::T_302_ms4000);
        connTimers->n_302 = RN_CALLOC(ASN::N_302_t);	*(connTimers->n_302) = 3;
        connTimers->t_304 = RN_CALLOC(ASN::T_304_t);	asn_long2INTEGER(connTimers->t_304,ASN::T_304_ms2000);
        connTimers->n_304 = RN_CALLOC(ASN::N_304_t);	*(connTimers->n_304) = 2; 
        connTimers->t_305 = RN_CALLOC(ASN::T_305_t);	asn_long2INTEGER(connTimers->t_305,ASN::T_305_m30);
        connTimers->t_307 = RN_CALLOC(ASN::T_307_t);	asn_long2INTEGER(connTimers->t_307,ASN::T_307_s30);
        connTimers->t_308 = RN_CALLOC(ASN::T_308_t);	asn_long2INTEGER(connTimers->t_308,ASN::T_308_ms320);
        connTimers->t_309 = RN_CALLOC(ASN::T_309_t);	*(connTimers->t_309) = 5;
        connTimers->t_310 = RN_CALLOC(ASN::T_310_t);	asn_long2INTEGER(connTimers->t_310,ASN::T_310_ms160);
        connTimers->n_310 = RN_CALLOC(ASN::N_310_t);	*(connTimers->n_310) = 4;
        connTimers->t_311 = RN_CALLOC(ASN::T_311_t);	asn_long2INTEGER(connTimers->t_311,ASN::T_311_ms2000);
        connTimers->t_312 = RN_CALLOC(ASN::T_312_t);	*(connTimers->t_312) = 1;
        connTimers->n_312 = RN_CALLOC(ASN::N_312_t);	asn_long2INTEGER(connTimers->n_312,ASN::N_312_s1);
        connTimers->t_313 = RN_CALLOC(ASN::T_313_t);	*(connTimers->t_313) = 3;
        connTimers->n_313 = RN_CALLOC(ASN::N_313_t);	asn_long2INTEGER(connTimers->n_313,ASN::N_313_s20);
        connTimers->t_314 = RN_CALLOC(ASN::T_314_t);	asn_long2INTEGER(connTimers->t_314,ASN::T_314_s0);
        connTimers->t_315 = RN_CALLOC(ASN::T_315_t);	asn_long2INTEGER(connTimers->t_315,ASN::T_315_s0);
        connTimers->n_315 = RN_CALLOC(ASN::N_315_t);	asn_long2INTEGER(connTimers->n_315,ASN::N_315_s1);
        connTimers->t_316 = RN_CALLOC(ASN::T_316_t);	asn_long2INTEGER(connTimers->t_316,ASN::T_316_s30);
        connTimers->t_317 = RN_CALLOC(ASN::T_317_t);	asn_long2INTEGER(connTimers->t_317,ASN::T_317_infinity1);
        UE_IdleTimersAndConstants *idleTimers = RN_CALLOC(ASN::UE_IdleTimersAndConstants_t);
	/* need to fill in idleTimers parameters? */
	mSIB1.ue_ConnTimersAndConstants = connTimers;
	mSIB1.ue_IdleTimersAndConstants = idleTimers;
	asn_long2INTEGER(&idleTimers->t_300,T_300_ms2000);
	idleTimers->n_300 = 5;
	asn_long2INTEGER(&idleTimers->n_312,N_312_s1);
	idleTimers->t_312 = 10;

	// Type 2
	// 3GPP 25.331 10.2.48.8.5
	// ASN SysInfoType2
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SysInfoType2,&mSIB2);
	memset(&mSIB2,0,sizeof(mSIB2));
	// ura_IdentityList
	// URA is "UTRAN registration area".
	// FIXME -- Where in the spec is that defined??
	// (pat) See 25.331 8.1.1.6.2: I think this is only used if we put the UE in URA_PCH mode, which we dont.
	// It looks like just another organizational aggregation of RNCs within a PLMN and I think we can ignore it.
	URA_Identity_t *urai = (URA_Identity_t*)calloc(1,sizeof(*urai));
	// urai is a BIT_STRING.
	uint16_t *uraval = (uint16_t*)calloc(1,sizeof(*uraval));
	*uraval = htons(gConfig.getNum("UMTS.Identity.URAI"));
	setAsnBIT_STRING(urai,(uint8_t*)uraval,16);
	ASN_SEQUENCE_ADD(&mSIB2.ura_IdentityList.list,urai);

	// Type 3
	// 3GPP 25.331 10.2.48.8.6
	// ASN SysInfoType3
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SysInfoType3,&mSIB3);
	memset(&mSIB3,0,sizeof(mSIB3));
	// sib4indicator, 0 if there's no SIB4.
	mSIB3.sib4indicator = 0;
	// cellIdentity, 10.3.2.2
	// This field is implementation specific, up to 28 bits.
	// We will implement it as a 16-bit value, but we have to encode all 28 bit.
	// From 25.401: The Cell identifier (C-Id) is used to uniquely identify a cell within an RNS/BSS.
	// [pat - RNS = one RNC + one or more Node-Bs]
	// The Cell-Id together with the identifier of the controlling RNC/BSS (CRNC-Id) constitutes the UTRAN/GERAN
	// Cell Identity (UC-Id) and is used to identify the cell uniquely within UTRAN/GERAN Iu mode.
	// UC-Id or C-Id is used to identify a cell in UTRAN Iub and Iur interfaces or Iur-g interface: UC-Id = RNC-Id + C-Id.
	// (pat) This last does not apply to us because we dont use the Iub/Iur interfaces.
	uint32_t *cellID = RN_CALLOC(uint32_t);
	// (pat) TODO: Is this right?  network order is high byte first.
	// Luckily, hardly matters.
	*cellID = gConfig.getNum("UMTS.Identity.CI");
	// UMTS protocol specifies that the cell ID is a 28 bit number, but it is casted into a 32 bit value.  The 4 LSB bits are considered padding, so the "real" data must be shifted about this padding.
	*cellID = *cellID << 4;
	*cellID = htonl(*cellID);
	// cellID is a 28-bit BIT_STRING
	setAsnBIT_STRING(&mSIB3.cellIdentity,(uint8_t*)cellID,28);
	// ASN CellSelectReselectInfoSIB_3_4 cellSelectReselectInfo, 10.3.2.3
	//  mappingInfo - optional - skip it
	//  cellSelectQualityMeasure
	mSIB3.cellSelectReselectInfo.cellSelectQualityMeasure.present =
		CellSelectReselectInfoSIB_3_4__cellSelectQualityMeasure_PR_cpich_Ec_N0;
	mSIB3.cellSelectReselectInfo.cellSelectQualityMeasure.choice.cpich_Ec_N0.q_HYST_2_S = new long(1);
	//  modeSpecificInfo, skipping optional parts
	//mSIB3.cellSelectReselectInfo.modeSpecificInfo.present=CellSelectReselectInfoSIB_3_4__modeSpecificInfo_PR_NOTHING;
	mSIB3.cellSelectReselectInfo.modeSpecificInfo.present=CellSelectReselectInfoSIB_3_4__modeSpecificInfo_PR_fdd;
	mSIB3.cellSelectReselectInfo.modeSpecificInfo.choice.fdd.q_QualMin=-24; //gConfig.getNum("UMTS.CellSelect.QualMin", -12);
	mSIB3.cellSelectReselectInfo.modeSpecificInfo.choice.fdd.q_RxlevMin=-58; //gConfig.getNum("UMTS.CellSelect.Q-RxlevMin",-20);
	mSIB3.cellSelectReselectInfo.modeSpecificInfo.choice.fdd.s_Intrasearch = new long(-16);
        mSIB3.cellSelectReselectInfo.modeSpecificInfo.choice.fdd.s_Intersearch = new long(8); //long(0);

	//  q_Hyst_l_S
	mSIB3.cellSelectReselectInfo.q_Hyst_l_S = 1; //gConfig.getNum("UMTS.CellSelect.Q-Hyst1s",6);
	//  t_Reselection_S
	mSIB3.cellSelectReselectInfo.t_Reselection_S = 5; //gConfig.getNum("UMTS.CellSelect.T-Reselection-S",6);
	//  hcs_ServingCellInformation - optional - skip it
	//  maxAllowedUL_TX_Power
	mSIB3.cellSelectReselectInfo.maxAllowedUL_TX_Power = gConfig.getNum("UMTS.CellSelect.MaxAlloweddUL-TX-Power");
	// cellAccessRestriction
	mSIB3.cellAccessRestriction.cellBarred.present = CellBarred_PR_notBarred;
	asn_long2INTEGER(&mSIB3.cellAccessRestriction.cellReservedForOperatorUse,ReservedIndicator_notReserved);
	asn_long2INTEGER(&mSIB3.cellAccessRestriction.cellReservationExtension,ReservedIndicator_notReserved);
        mSIB3.cellAccessRestriction.accessClassBarredList = RN_CALLOC(ASN::AccessClassBarredList);
	for (int q=0; q< 16; q++) {
          AccessClassBarred_t *acb = RN_CALLOC(ASN::AccessClassBarred_t);
          asn_long2INTEGER(acb,ASN::AccessClassBarred_notBarred); 
          ASN_SEQUENCE_ADD(&mSIB3.cellAccessRestriction.accessClassBarredList->list,acb);
	}

	// Type 4
	// 3GPP 25.331 10.2.48.8.7
	// We're skipping this one, using Type 3 to fill its defaults.

	// Type 5
	// 3GPP 25.331 10.2.48.8.8
	//ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SysInfoType5,&mSIB5);
	memset(&mSIB5,0,sizeof(mSIB5));
	mSIB5.sib6indicator = 0;
	mSIB5.pich_PowerOffset = gConfig.getNum("UMTS.PICH.PICH-PowerOffset");
	// modeSpecificInfo
	mSIB5.modeSpecificInfo.present = SysInfoType5__modeSpecificInfo_PR_fdd;
	mSIB5.modeSpecificInfo.choice.fdd.aich_PowerOffset = gConfig.getNum("UMTS.AICH.AICH-PowerOffset");

	// (pat) The only thing primaryCCPH_Info has in it for FDD is a boolean Tx diversity flag.
	// primaryCCPCH_Info - optional - skip  (pat) Lets try putting it in.
#if PAT_TEST
	mSIB5.primaryCCPCH_Info = RN_CALLOC(PrimaryCCPCH_Info);
	mSIB5.primaryCCPCH_Info->present = ASN::PrimaryCCPCH_Info_PR_fdd;
	mSIB5.primaryCCPCH_Info->choice.fdd.tx_DiversityIndicator = 0;
#endif
	mSIB5.primaryCCPCH_Info = RN_CALLOC(ASN::PrimaryCCPCH_Info);
	mSIB5.primaryCCPCH_Info->present = PrimaryCCPCH_Info_PR_fdd;
	mSIB5.primaryCCPCH_Info->choice.fdd.tx_DiversityIndicator = false;

	{ // start of 10.3.6.55 prach_SystemInformationList, with one entry
	PRACH_SystemInformation *prach_SI = RN_CALLOC(PRACH_SystemInformation);
	//  10.3.6.52 PRACH-RACH-Info
	prach_SI->prach_RACH_Info.modeSpecificInfo.present = PRACH_RACH_Info__modeSpecificInfo_PR_fdd;
	uint16_t * PRACHSigs = RN_CALLOC(uint16_t);
	//*PRACHSigs = htons(0x08000 >> gConfig.getNum("UMTS.PRACH.Signature"));
	// (pat) tried: *PRACHSigs = htons(0xffff);
	*PRACHSigs = htons(0x0001 << gConfig.getNum("UMTS.PRACH.Signature"));
	setAsnBIT_STRING(&prach_SI->prach_RACH_Info.modeSpecificInfo.choice.fdd.availableSignatures,(uint8_t*)PRACHSigs,16);
	//   spreading factor
        unsigned rachSF = gConfig.getNum("UMTS.PRACH.SF");
        switch (rachSF) {
                case 256:
                        asn_long2INTEGER(&(prach_SI->prach_RACH_Info.modeSpecificInfo.choice.fdd.availableSF),SF_PRACH_sfpr256);
			break;
                case 128:
                        asn_long2INTEGER(&(prach_SI->prach_RACH_Info.modeSpecificInfo.choice.fdd.availableSF),SF_PRACH_sfpr128);
                        break;
                case 64:
                        asn_long2INTEGER(&(prach_SI->prach_RACH_Info.modeSpecificInfo.choice.fdd.availableSF),SF_PRACH_sfpr64);
                        break;
                case 32:
                        asn_long2INTEGER(&(prach_SI->prach_RACH_Info.modeSpecificInfo.choice.fdd.availableSF),SF_PRACH_sfpr32);
                        break;
                default:
                        break;
	}
	//   scrambling code
	prach_SI->prach_RACH_Info.modeSpecificInfo.choice.fdd.preambleScramblingCodeWordNumber = gConfig.getNum("UMTS.PRACH.ScramblingCode");
	// (pat) 25.212 4.2.7.1.1 indicates that the SF for uplink is chosen based on the
	// number of bits needed for the TF after mutilation by puncturing, so it looks
	// like the PL Puncturing Limit indirectly selects the SF for the TF.  What a mess.
	// There is a hint in the Change History of the Layer 1 spec that indicates that
	// the Punturing Limit was moved around in 2000.
	// The Puncturing Limit from 10.3.6.52: PRACH info specifies PunturingLimit is 
	// Real(0.4..1.0 by step of 0.4) which gets ASN encoded as an enumeration
	// called, are you ready?  PuncturingLimit.  I am setting it to a value of PL=1.00
	// which means no puncturing, whose enumeration name is PunturingLimit_pl1 == 15

	// Tried for testing, did not help samsung:
	// asn_long2INTEGER(&(prach_SI->prach_RACH_Info.modeSpecificInfo.choice.fdd.puncturingLimit),PuncturingLimit_pl0_80);
	asn_long2INTEGER(&(prach_SI->prach_RACH_Info.modeSpecificInfo.choice.fdd.puncturingLimit),PuncturingLimit_pl1);

	//   subchannel number
	uint16_t * PRACHSubChan = RN_CALLOC(uint16_t);
	*PRACHSubChan = htons(0x0010 << gConfig.getNum("UMTS.PRACH.Subchannel"));
	// (pat) tried: *PRACHSubChan = htons(0xffff);
	setAsnBIT_STRING(&prach_SI->prach_RACH_Info.modeSpecificInfo.choice.fdd.availableSubChannelNumbers,(uint8_t*)PRACHSubChan,12);
	// transportChannelIdentity
	// FIXME -- I don't understand the encoding of this one.
	// (pat) Yes, the spec doesnt really explain what TrCh id is for.
	// There can be multiple PRACH, each one serving some subset of UEs,
	// so it would be possible to download a single Transport Format Set and then steer
	// each PRACH onto a different Transport Format by specifying a different TrCh id.
	// If they were going to allow voice channels on RACH, then they would need to specify
	// three transport channels, not just one, so it makes no sense for that.

	// Pat says: Whatever this is, it is not a config option - it is determined by the TFS setup.
	// prach_SI->transportChannelIdentity = gConfig.getNum("UMTS.PRACH.TransportChannel",2);
#if PAT_TEST
	// (pat) It is 1, not 3.
	prach_SI->transportChannelIdentity = gRrcDcchConfig->getUlTrChInfo(0)->mTransportChannelIdentity;		// Range is 1..32
	assert(prach_SI->transportChannelIdentity == 1);
#else
	prach_SI->transportChannelIdentity = 3;
#endif


	// (pat) The RACH TFS and TFCS are MD, meaning they are only optional after the first one.
	// Note: the description and picture of the MAC-s/ch/m imply there is no TFCS on RACH, but there is.
	// The 10.3.5.23 TFS for RACH differs from a normal TFS only in the rlcsize encoding.  Geesh.
	// Note that the RACH includes an "ASC" code that replaces logical channel.
	// (pat) First RACH TFS+TFCS is mandatory.
	// If there are multiple PRACH channels, subsequent ones are optional: all use first TFS.
	gRrcCcchConfig->getUlTfs()->toAsnTfs((prach_SI->rach_TransportFormatSet = RN_CALLOC(ASN::TransportFormatSet)));
	gRrcCcchConfig->getUlTfcs()->toAsnTfcs((prach_SI->rach_TFCS = RN_CALLOC(ASN::TFCS)),TrChRACHType);

	// 10.3.6.53 prach_Partitioning - optional (pat) It defaults to all ASC are available, which is what we want.
	// (pat) If you take out prach_partitioning, the Samsung does not RACH, or if it does, something is wrong.
        prach_SI->prach_Partitioning = RN_CALLOC(ASN::PRACH_Partitioning);
	prach_SI->prach_Partitioning->present = PRACH_Partitioning_PR_fdd;
	// 10.3.6.6 ASC Setting 
	AccessServiceClass_FDD_t *asc = RN_CALLOC(ASN::AccessServiceClass_FDD_t);
	// We are only using one signature
	asc->availableSignatureStartIndex = 0; //gConfig.getNum("UMTS.PRACH.Signature");
	asc->availableSignatureEndIndex = 0; //gConfig.getNum("UMTS.PRACH.Signature");
        uint8_t *assignedSubChan = RN_CALLOC(uint8_t);
		// (pat) This is a very weird bit mask defined in 25.331 8.6.6.29.
		// The 4 bits are duplicated in the mask to cover the 12 sub-channels.
        *assignedSubChan = 0x0f << 4;
        setAsnBIT_STRING(&asc->assignedSubChannelNumber,assignedSubChan,4);
        ASCSetting_FDD_t *ptt = RN_CALLOC(ASN::ASCSetting_FDD_t);
	ptt->accessServiceClass_FDD = asc;
	ASN_SEQUENCE_ADD(&prach_SI->prach_Partitioning->choice.fdd.list,ptt);

	// persistenceScalingFactorList - optional
 
	// (pat) According to of 10.3.6.55 ac_to_ASC mapping is not optional,
	// but it is not very interesting.
	// Mapping described in 8.5.13, and more in some other document, and if you 
	// find it put a reference here.  They can be used to prioritize, ie,
	// emergency services.
	// I dont think we need any values other than 0.
	// But TODO: Figure out how the other values work.
	// Update: The rrc.asn1 specifies that if you supply these, then you
	// must supply a sequence of 7 values, which specifically
	// contradicts RRC sec 10.3.6.53, that says that they default after the first one.

	// AC_To_ASC_Mapping is list of AC_To_ASC_Mapping.
	prach_SI->ac_To_ASC_MappingTable = RN_CALLOC(ASN::AC_To_ASC_MappingTable);
	for (unsigned i = 0; i < 7; i++) {
		long *ac = newlong(0); //i;	// Must be in the range 0..7
		ASN_SEQUENCE_ADD(&prach_SI->ac_To_ASC_MappingTable->list,ac);
	}

	//  mode specific information - all of its elements are optional but we have to pick a type
	//  Not convinced this is optional...adding AICH parameters was required to make phone work.
	prach_SI->modeSpecificInfo.present = PRACH_SystemInformation__modeSpecificInfo_PR_fdd;
	prach_SI->modeSpecificInfo.choice.fdd.primaryCPICH_TX_Power = RN_CALLOC(ASN::PrimaryCPICH_TX_Power_t);
        *prach_SI->modeSpecificInfo.choice.fdd.primaryCPICH_TX_Power = 10; //dBm
	prach_SI->modeSpecificInfo.choice.fdd.constantValue = RN_CALLOC(ASN::ConstantValue_t);
        *prach_SI->modeSpecificInfo.choice.fdd.constantValue = -10;
	prach_SI->modeSpecificInfo.choice.fdd.prach_PowerOffset = RN_CALLOC(ASN::PRACH_PowerOffset);
	prach_SI->modeSpecificInfo.choice.fdd.prach_PowerOffset->powerRampStep = 1;
	prach_SI->modeSpecificInfo.choice.fdd.prach_PowerOffset->preambleRetransMax = 64;
	prach_SI->modeSpecificInfo.choice.fdd.rach_TransmissionParameters = RN_CALLOC(ASN::RACH_TransmissionParameters);
	prach_SI->modeSpecificInfo.choice.fdd.rach_TransmissionParameters->mmax = 32;
	prach_SI->modeSpecificInfo.choice.fdd.rach_TransmissionParameters->nb01Min = 0; 
	prach_SI->modeSpecificInfo.choice.fdd.rach_TransmissionParameters->nb01Max = 50;
	prach_SI->modeSpecificInfo.choice.fdd.aich_Info = RN_CALLOC(ASN::AICH_Info);
	// AICH Info 10.3.6.2
#if PAT_TEST
	prach_SI->modeSpecificInfo.choice.fdd.aich_Info->channelisationCode256 = cAICHSpreadingCodeIndex;
		// (pat) This ch does not need to be reserved here for the uplink channel - chReserve is for downlink
		// channels only; the uplink channels use a completely different underlying differentiation
		// mechanism, namely scrambling codes, not spreading codes.
		// Harvind probably put this to fix a bug, but the bug was that channel 2 was not being
		// reserved in the FACH setup code below.
#else
        gChannelTree.chReserve(256,2);     // (harvind) This channel may not be used for DCH.
		prach_SI->modeSpecificInfo.choice.fdd.aich_Info->channelisationCode256 = 2;
#endif
        prach_SI->modeSpecificInfo.choice.fdd.aich_Info->sttd_Indicator = false;
	// NOTE: we want AICH timing to be e1 (5 slots) to give us a little more time to respond
	if (cAICHRACHOffset == 5) {
		asn_long2INTEGER(&(prach_SI->modeSpecificInfo.choice.fdd.aich_Info->aich_TransmissionTiming),AICH_TransmissionTiming_e1);
	} else {
		assert(0);	// Only value 5 is supported.
	}
	
	// insert the PRACH_SystemInformation into the list.
	ASN_SEQUENCE_ADD(&mSIB5.prach_SystemInformationList.list,prach_SI);
	//asn_fprint(stdout,&ASN::asn_DEF_PRACH_SystemInformation, prach_SI);
	} // end of 10.3.6.55 prach_SystemInformationList

	// (pat) This is the SCCPCH [FACH] channel code.  TODO: Why a lower case 's'?
	// (pat) Clean this up a little.  Harvind is allocating three downlink channels for PICH and two FACH.
	// (pat) channels 0,sf=256 and 1,sf=256 are reserved so channels ch=0,sf=64 is also reserved.
	unsigned fachChCode = gConfig.getNum("UMTS.SCCPCH.SpreadingCode");
	unsigned pichChCode = fachChCode + 2;

	{ // start of sCCPCH_SystemInformationList
#if 1 //PAT_TEST
	unsigned pchChCode = fachChCode + 1;
	// 25.331 10.3.6.72 note says that the first one is PCH if PCH exists.  Only PCH has a PICH.
	SCCPCH_SystemInformation *sCCPCH_SI = generateSCCPCH(pchChCode,true,pichChCode);
	unsigned scc_sf = getConfigSccpchSF();	// (pat) Not currently programmable.
	gChannelTree.chReserve(scc_sf,pchChCode);	// (pat) This channel may not be used for DCH.

	// (pat) This code may be redundant with a reservation to create the FEC, but cant be too careful.
	// TODO: The 256 should be scc_sf.
	gChannelTree.chReserve(256,pichChCode);	// (pat) This channel may not be used for DCH.

	// FIXME: Try taking out pch...
	// Doesnt seem to matter if this is here or not for either Samsung or Blackberry.
	ASN_SEQUENCE_ADD(&mSIB5.sCCPCH_SystemInformationList.list,sCCPCH_SI);

	SCCPCH_SystemInformation *sCCPCH_SI2 = generateSCCPCH(fachChCode,false,0);
	// TODO: The 256 should be scc_sf.
	gChannelTree.chReserve(scc_sf,fachChCode);	// (pat) This channel may not be used for DCH.
	ASN_SEQUENCE_ADD(&mSIB5.sCCPCH_SystemInformationList.list,sCCPCH_SI2);

#else
	// (pat) See 10.3.6.72: Secondary CCPCH Info IE.
	SCCPCH_SystemInformation *sCCPCH_SI = RN_CALLOC(ASN::SCCPCH_SystemInformation);
	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.present = SecondaryCCPCH_Info__modeSpecificInfo_PR_fdd;
	asn_long2INTEGER(&(sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.dummy1),PCPICH_UsageForChannelEst_mayBeUsed);
	
	// FIXME -- Figure out what STTD is and be sure we don't use it.
	// (pat) See 10.3.6.86: STTD is one of the TX Diversity modes.
	// SCCPCH Secondary Common Control Physical Channel is the one carrying FACH and PCH.
	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sttd_Indicator = false;
	unsigned scc_sf = getConfigSccpchSF();	// (pat) Not currently programmable.
	switch (scc_sf) {
		case 256:
			sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf256;
			break;
		case 128:
                        sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf128; //256;
			break;
		case 64:
                        sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf64; //256;
			break;
		case 32:
                        sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf32; //256;
			break;
		case 16:
                        sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf16; //256;
			break;
		case 8:
                        sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf8; //256;
			break;
		case 4:
                        sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.present = SF256_AndCodeNumber_PR_sf4; //256;
			break;
		default:
			break;
	}
	// We are assigning to choice.sf256 but it is the same variable location for all SFs.
	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.choice.sf256 = fachChCode+1;

	// (pat) We must reserve this spot in the ChannelTree.
	// This code is redundant with the one that creates the channel in FACHFEC(),
	// but cant be too careful.
	// (pat) If scc_sf is not 256, then the other FACH/PICH channels are going to collide with this in the
	// tree of spreading codes and need to be changed.
	// (pat) This is wrong - should be fachChCode+1 as above.
	gChannelTree.chReserve(scc_sf,fachChCode);	// (pat) This channel may not be used for DCH.

	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.pilotSymbolExistence = false;
	// (pat)  No tfci implies "blind" transport format selection.
	// It can be used if you are not multiplexing channels so you can use the number of bits
	// to imply the transport format selected.

	sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.tfci_Existence = true;
	asn_long2INTEGER(&(sCCPCH_SI->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.positionFixedOrFlexible),PositionFixedOrFlexible_fixed);
	// (pat) First FACH TFS+TFCS is mandatory.
	// I believe the subsequent ones are optional and will use the same one if not specified.
	FACH_PCH_Information *fachPchInfo = RN_CALLOC(ASN::FACH_PCH_Information);
#if PAT_TEST
	// (pat) It should be 1, not 3.
	fachPchInfo->transportChannelIdentity = gRrcDcchConfig->getDlTrChInfo(0)->mTransportChannelIdentity;		// Range is 1..32
	assert(fachPchInfo->transportChannelIdentity == 1);
#else
	fachPchInfo->transportChannelIdentity = 1+1;
#endif

	// We are leaving CTCH indicator false.
	// PICH_info is optional - needed only if PCH is multiplexed.
	gRrcCcchConfig->getDlTfs()->toAsnTfs(&fachPchInfo->transportFormatSet,true);
	gRrcCcchConfig->getDlTfcs()->toAsnTfcs((sCCPCH_SI->tfcs = RN_CALLOC(ASN::TFCS)),TrChFACHType);

	sCCPCH_SI->fach_PCH_InformationList = RN_CALLOC(ASN::FACH_PCH_InformationList);
	ASN_SEQUENCE_ADD(&sCCPCH_SI->fach_PCH_InformationList->list,fachPchInfo); 
	// FIXME: Adding second FACH to SIB5, though it doesn't really exist.  Phone assumes first FACH is PCH and won't go into the RACH procedure
#if 0	// unused code
	/*FACH_PCH_Information *fachPchInfo2 = RN_CALLOC(ASN::FACH_PCH_Information);
	memcpy(fachPchInfo2,fachPchInfo,sizeof(ASN::FACH_PCH_Information));
	fachPchInfo2->transportChannelIdentity = 2;
        ASN_SEQUENCE_ADD(&sCCPCH_SI->fach_PCH_InformationList->list,fachPchInfo2);*/
        /*FACH_PCH_Information *fachPchInfo3 = RN_CALLOC(ASN::FACH_PCH_Information);
        memcpy(fachPchInfo3,fachPchInfo,sizeof(ASN::FACH_PCH_Information));
        fachPchInfo3->transportChannelIdentity = 3;
        ASN_SEQUENCE_ADD(&sCCPCH_SI->fach_PCH_InformationList->list,fachPchInfo3);*/
#endif
	
	PICH_Info_t *pichInfo = RN_CALLOC(ASN::PICH_Info);
	pichInfo->present = PICH_Info_PR_fdd;
	pichInfo->choice.fdd.channelisationCode256 = pichChCode;

	// (pat) This code may be redundant with a reservation to create the FEC, but cant be too careful.
	// TODO: The 256 should be scc_sf.
	gChannelTree.chReserve(256,pichChCode);	// (pat) This channel may not be used for DCH.

	asn_long2INTEGER(&(pichInfo->choice.fdd.pi_CountPerFrame),PI_CountPerFrame_e18);
	pichInfo->choice.fdd.sttd_Indicator = false;
	sCCPCH_SI->pich_Info = pichInfo;
	ASN_SEQUENCE_ADD(&mSIB5.sCCPCH_SystemInformationList.list,sCCPCH_SI);

        SCCPCH_SystemInformation *sCCPCH_SI2 = RN_CALLOC(ASN::SCCPCH_SystemInformation);
	memcpy(sCCPCH_SI2,sCCPCH_SI,sizeof(ASN::SCCPCH_SystemInformation));
        sCCPCH_SI2->secondaryCCPCH_Info.modeSpecificInfo.choice.fdd.sf_AndCodeNumber.choice.sf256 = fachChCode;
	gChannelTree.chReserve(scc_sf,fachChCode);
	//sCCPCH_SI2->tfcs = RN_CALLOC(ASN::TFCS);
	//memcpy(sCCPCH_SI2->tfcs,sCCPCH_SI->tfcs,sizeof(ASN::TFCS));
	sCCPCH_SI2->pich_Info = NULL;
        FACH_PCH_Information *fachPchInfo2 = RN_CALLOC(ASN::FACH_PCH_Information);
        gRrcCcchConfig->getDlTfs()->toAsnTfs(&fachPchInfo2->transportFormatSet,true);
        gRrcCcchConfig->getDlTfcs()->toAsnTfcs((sCCPCH_SI2->tfcs = RN_CALLOC(ASN::TFCS)),TrChFACHType);
	//memcpy(fachPchInfo2,fachPchInfo,sizeof(ASN::FACH_PCH_Information));
	// (pat) We already set the transportChannelIdentity above, and it is the same.
	// These are different channels.  They all use the transport channel id in the TFS.  Same TFS is used for all.
	fachPchInfo2->transportChannelIdentity = 1;
        sCCPCH_SI2->fach_PCH_InformationList = RN_CALLOC(ASN::FACH_PCH_InformationList);
        ASN_SEQUENCE_ADD(&sCCPCH_SI2->fach_PCH_InformationList->list,fachPchInfo2);
        ASN_SEQUENCE_ADD(&mSIB5.sCCPCH_SystemInformationList.list,sCCPCH_SI2);

#endif
	} // end of sCCPCH_SystemInformationList

	// (pat) This print has been superceded by asn2String dumping all the SIBs to the log at end of function below.
	if (gConfig.getNum("UMTS.Debug.SIB")) {
		printf("Dumping SIB5...\n");
		asn_fprint(stdout,&ASN::asn_DEF_SysInfoType5, &mSIB5);	// Dump it all.
	}
        
	// cbs_DRX_Level1Information - optional - skip

	// Type 6
	// Optional, so skip it.

	// Type 7
	// 3GPP 25.331 10.2.48.8.10
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SysInfoType7,&mSIB7);
	memset(&mSIB7,0,sizeof(mSIB7));
	mSIB7.modeSpecificInfo.present = SysInfoType7__modeSpecificInfo_PR_fdd;
	mSIB7.modeSpecificInfo.choice.fdd.ul_Interference = gNodeB.getULInterference();
	// SIB5 dynamic persistence level list
	//DynamicPersistenceLevel_t *dpl = (DynamicPersistenceLevel_t*)calloc(1,sizeof(*dpl));
	//*dpl = gConfig.getNum("UMTS.PRACH.DynamicPersistenceLevel");	// TODO UMTS -- what does this mean?
	DynamicPersistenceLevel_t *dpl = newlong(gConfig.getNum("UMTS.PRACH.DynamicPersistenceLevel"));
	ASN_SEQUENCE_ADD(&mSIB7.prach_Information_SIB5_List.list,dpl);
	mSIB7.expirationTimeFactor = newlong(1);

	// Type 8-10 are obsolete.

	// Type 11
	// 3GPP 25.331 10.2.48.8.14
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SysInfoType11,&mSIB11);
	memset(&mSIB11,0,sizeof(mSIB11));
	mSIB11.sib12indicator = false;
	mSIB11.fach_MeasurementOccasionInfo = NULL;
	//mSIB11.fach_MeasurementControlSysInfo = NULL;//RN_CALLOC(ASN::FACH_MeasurementOccasionInfo);
	mSIB11.measurementControlSysInfo.use_of_HCS.present = MeasurementControlSysInfo__use_of_HCS_PR_hcs_not_used;
	mSIB11.measurementControlSysInfo.use_of_HCS.choice.hcs_not_used.cellSelectQualityMeasure.present = MeasurementControlSysInfo__use_of_HCS__hcs_not_used__cellSelectQualityMeasure_PR_cpich_Ec_N0;
	// Type 12
	// optional, skip it

	// Type 13
	// not applicable in GSM-MAP UTRAN

	// Type 14
	// TDD only, skip it

	// Type 15
	// 3GPP 25.331 10.2.48.8.18
#if 0
Comment this out until we are further along.
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SysInfoType15,&mSIB15);
	memset(&mSIB15,0,sizeof(mSIB15));
	float refLat = gConfig.getFloat("GSM.RRLP.SEED.LATITUDE");
	EllipsoidPointAltitudeEllipsoide__latitudeSign latSign;
	if (refLat<0.0F) latSign = EllipsoidPointAltitudeEllipsoide__latitudeSign_south;
	else latSign = EllipsoidPointAltitudeEllipsoide__latitudeSign_south;
	mSIB15.ue_positioning_GPS_ReferenceLocation.ellipsoidPointAltitudeEllipsoide.latitudeSign = latSign;
	long latVal = (long)(0.5F + 8388608.0 * (fabs(refLat)/90.0F));
	mSIB15.ue_positioning_GPS_ReferenceLocation.ellipsoidPointAltitudeEllipsoide.latitude = latVal;
	float refLong = gConfig.getFloat("GSM.RRLP.SEED.LONGITUDE");
	long longVal = (long)(0.5F + 8388608.0 * (fabs(refLat)/180.0F));
	// There are lots of uncertainty fields here that we are defaulting to 0.
	mSIB15.ue_positioning_GPS_ReferenceLocation.ellipsoidPointAltitudeEllipsoide.confidence = 50;
#endif
	
#if 0
Comment this out until we are further along.
	// Type 16
	// 3GPP 25.331 10.2.48.8.19
	// If we don't do handover yet, do we need this?
	// (pat) If you look at 8.1.1.6.16, looks like these are used to setup pre-defined
	// RAB/RB/TrCh configurations, which are optional.
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_SysInfoType16,&mSIB16);
	memset(&mSIB16,0,sizeof(mSIB16));
	// preDefinedRadioConfiguration
	mSIB16.preDefinedRadioConfiguration.ul_DPCH_InfoPredef.present = UL_DPCH_PowerControlInfoPredef_PR_fdd;
	mSIB16.preDefinedRadioConfiguration.ul_DPCH_InfoPredef.choice.fdd.powerControlAlgorithm.present = PowerControlAlgorithm_PR_algorithm2;
#endif

	// Type 17
	// TDD only, skip

	// Type 18
	// All parts are optional, so do we need it?

	LOG(INFO) << asn2string(&ASN::asn_DEF_MasterInformationBlock, &mMIB);
	LOG(INFO) << asn2string(&ASN::asn_DEF_SysInfoType1, &mSIB1);
	LOG(INFO) << asn2string(&ASN::asn_DEF_SysInfoType2, &mSIB2);
	LOG(INFO) << asn2string(&ASN::asn_DEF_SysInfoType3, &mSIB3);
	LOG(INFO) << asn2string(&ASN::asn_DEF_SysInfoType5, &mSIB5);
	LOG(INFO) << asn2string(&ASN::asn_DEF_SysInfoType7, &mSIB7);
	LOG(INFO) << asn2string(&ASN::asn_DEF_SysInfoType11, &mSIB11);
	cout << asn2string(&ASN::asn_DEF_SysInfoType11, &mSIB11);
        //LOG(INFO) << asn2string(&ASN::asn_DEF_SysInfoType12, &mSIB12);

	// Phase1 encoding - just turns them into a single buffer apiece.
	encodeSIBPhase1(0, "MIB", &asn_DEF_MasterInformationBlock,&mMIB);
	encodeSIBPhase1(1, "SIB1", &asn_DEF_SysInfoType1,(void*)&mSIB1);
	encodeSIBPhase1(2, "SIB2", &asn_DEF_SysInfoType2,(void*)&mSIB2);
	encodeSIBPhase1(3, "SIB3", &asn_DEF_SysInfoType3,(void*)&mSIB3);
	encodeSIBPhase1(4, "SIB5", &asn_DEF_SysInfoType5,(void*)&mSIB5);
	encodeSIBPhase1(5, "SIB7", &asn_DEF_SysInfoType7,(void*)&mSIB7);
	encodeSIBPhase1(6, "SIB11",&asn_DEF_SysInfoType11,(void*)&mSIB11);
        //encodeSIBPhase1(7, "SIB12",&asn_DEF_SysInfoType12,(void*)&mSIB12);
	// We are done with the SIB structs; we could free them now.

	rn_asn_debug = gConfig.getNum("UMTS.Debug.ASN");
}

// Unfortunately the System Information messages in the beacon
// include the SFN, so we must re-encode them, at least the phase2 part, for each beacon.
void BeaconConfig::encodePhase2(unsigned sfn)
{
	// Re-run the phase2 encoding for the beacon at the start of every beacon cycle,
	// to bring the SFN up-to-date in the System Information messages.
	// We are doing it this way instead of testing SFN % mSibRepeat == 0 because
	// we may start in the middle of a cycle.

	// back up to the start of the beacon cycle:
	sfn = (sfn / msSibRepeat) * msSibRepeat;
	if (sfn == sPrevBeaconStart) { return; }	// Already encoded this beacon cycle.

	// BeaconLock makes sure we dont do this at the same time as regenerateBeacon
	// Note: Do not lock the UMTSConfig here or you will create a deadlock race condition.
	ScopedLock lock(mBeaconLock);
	sPrevBeaconStart = sfn;

	// Debug test: clear out the scheduling from the TransportBlocks.
	for (unsigned j = 0; j < sizeof(mSibSched)/sizeof(TransportBlock*); j++) {
		mSibSched[j]->mScheduled = false;
	}

	for (unsigned i=0; i<mNumSibTypes; i++) {
		SibInfo_t *plan = &mSibInfo[i];

		// Now rerun the phase2 encoder to generate transport blocks
		// for each location in the beacon where this SIB goes.
		for (unsigned pos = plan->mSibPos; pos < msSibRepeat; pos += plan->mSibRep) {
			//LOG(INFO) << "i: " << i << " sfn: " << sfn << " pos: " << pos;
			encodeSIBPhase2(plan,sfn+pos);
		}
	}

	for (unsigned j = 0; j < sizeof(mSibSched)/sizeof(TransportBlock*); j++) {
		assert(mSibSched[j]->scheduled());
	}
}


template <class ChanType> ChanType* getChan(vector<ChanType*>& chanList)
{
#if CS_SERVICES_ENABLED
	// (pat) I am removing this until we need CS channels.
	// For PS channels, see: ChannelTree.chChooseBySF()

	const unsigned sz = chanList.size();
	if (sz==0) return NULL;
	// Start the search from a random point in the list.
	//unsigned pos = random() % sz;
	// HACK -- Try in-order allocation for debugging.
	for (unsigned i=0; i<sz; i++) {
		ChanType *chan = chanList[i];
		//ChanType *chan = chanList[pos];
		if (chan->recyclable()) return chan;
		//pos = (pos+1) % sz;
	}
#endif
	return NULL;
}


// Pat removed.  For UMTS, DCCH is not in a pool. The DCCH is SRB3 of a UEInfo.
//DCCHLogicalChannel *UMTSConfig::getDCCH()
//{
//	ScopedLock lock(mLock);
//	DCCHLogicalChannel *chan = getChan<DCCHLogicalChannel>(mDCCHPool);
//	if (chan) chan->open();
//	return chan;
//}


//DTCHLogicalChannel *UMTSConfig::getDTCH()
//{
//	ScopedLock lock(mLock);
//	DTCHLogicalChannel *chan = getChan<DTCHLogicalChannel>(mDTCHPool);
//	if (chan) chan->open();
//	return chan;
//}


template <class ChanType> size_t chanAvailable(const vector<ChanType*>& chanList)
{
	size_t count = 0;
	for (unsigned i=0; i<chanList.size(); i++) {
		if (chanList[i]->recyclable()) count++;
	}
	return count;
}


//size_t UMTSConfig::DCCHAvailable() const
//{
//	ScopedLock lock(mLock);
//	return chanAvailable<DCCHLogicalChannel>(mDCCHPool);
//}

// 9-2012 (pat) TODO: This needs lots more work.
// First, the DCH available for voice calls have to plucked
// from the channel tree at a particular SF, probably 256.
// Second, we dont currently maintain gActiveDCH.
// No one currently calls open()/close() for any DCH.
// We need to keep a list of DCH that have been used for voice calls,
// and keep them somewhere until they can be recycled.
size_t UMTSConfig::DTCHAvailable() const		// size_t?
{
	return 1;	// for now punt, but tell the caller to try to allocate.
//	ScopedLock lock(mLock);
//	return chanAvailable<DTCHLogicalChannel>(mDTCHPool);
}


template <class ChanType> unsigned countActive(const vector<ChanType*>& chanList)
{
	unsigned active = 0;
	const unsigned sz = chanList.size();
	// Start the search from a random point in the list.
	for (unsigned i=0; i<sz; i++) {
		if (!chanList[i]->recyclable()) active++;
	}
	return active;
}


//unsigned UMTSConfig::DCCHActive() const
//{
//	return countActive(mDCCHPool);
//}

unsigned UMTSConfig::DTCHActive() const
{
	return countActive(mDTCHPool);
}


void UMTSConfig::hold(bool val)
{
	ScopedLock lock(mLock);	// currently irrelevant
	mHold = val;
}

bool UMTSConfig::hold() const
{
	ScopedLock lock(mLock);	// currently irrelevant
	return mHold;
}

const TransportBlock* UMTSConfig::getTxSIB(unsigned SFN)
{
	// Note: This may take some time...
	sBeacon.encodePhase2(SFN);
	//LOG(INFO) << "SFN: " << SFN << ", length: " << *(sBeacon.getSITB(SFN));
	return sBeacon.getSITB(SFN);
}


// vim: ts=4 sw=4
