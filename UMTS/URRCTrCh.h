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

#ifndef URRCTRCH_H
#define URRCTRCH_H 1
#include "UMTSCommon.h"
#include "Logger.h"

#include "ScalarTypes.h"
#include "URRCDefs.h"
#include <assert.h>
#include "asn_system.h"	// Dont let other includes land in namespace ASN.
namespace ASN {
#include "TransportFormatSet.h"
#include "TFCS.h"
#include "RB-MappingInfo.h"
};

namespace UMTS {
class TrChInfo;
class RACHFEC;
class FACHFEC;
class DCHFEC;

// 3GPP 25.331 RRC protocol spec.  Messages and IEs below are transmitted to the UE using ANS.
// sec 10.2.40: Message RRC Connection Setup.
// sec 10.2.33: Message Radio Bearer Setup
// sec 10.3.4.24 Signalling RB Info to setup.
// (pat) RB (Radio Bearer) setup.  The programming options for the RB is huge, with tons of stuff
// we will never use, and additional complication is inserted into the specs to more closely match
// the constraints imposed by using ANS.
// To attack this, I am pulling out those parts of RB setup that we will use into C++ classes.
// Our job is to program our own RLC+MAC+L1+PHY with these directly, and the UE with
// matching (though not identical, because uplink and downlink are assymetric) parameters using ANS.
// We will probably not create generic "write" routines to convert these classes
// into ANS structures to send to the UE for two reasons:
// 1. We will use pre-defined UE setups (sec 13.7) as much as possible, and we dont
// need to transmit those, we only need to program our own RRC to match.
// 2. The ANS complexity is high, we only program a tiny subset of it,
// and we will only make a couple of Radio Bearer setups (maybe only 1), ever.
// It will be easier to custom program the ANS for the few that we use, using
// the info in these C++ classes.

//	RB Information Elements
// TrCH Information Elements
//		UL Transport Channel Info common for all Transport Channels 10.3.5.24
//		Added or Reconfigured UL TrCH info 10.3.5.2
//		DL Transport Channel Info common for all Transport Channels 10.3.5.6
//		Added or Reconfigured DL TrCH info 10.3.5.1
// PhyCH Info Elements
//	...
//		

typedef unsigned RBIdentity;	// 1..32.  1-4 reserved for SRB1 - SRB4.
struct TrChList;


struct RrcTfsRlcSize {
	//unsigned mValue;  Used to do it this way.
	static void toAsnBitMode(ASN::BitModeRLC_SizeInfo*result,unsigned rlcsize);	// Used for dedciated ch (DCH)
	static void toAsnOctetMode1(ASN::OctetModeRLC_SizeInfoType1 *result,unsigned rlcsize);
	static void toAsnOctetMode2(ASN::OctetModeRLC_SizeInfoType2*result,unsigned rlcsize);	// Used for common ch (RACH/FACH)
};

struct RrcTfsNumberOfTransportBlocks {
	//unsigned mValue;	// 0 .. 51
	static ASN::NumberOfTransportBlocks *toAsn(unsigned numtb);
};

struct RrcSemiStaticTFInfo : public virtual RrcDefs 	// 10.3.5.11
{
	// Note: The ASN encodes the static TTI in the tti CHOICE in the enclosing 
	// CommonTransChTfs or DedicatedTransChTfs.  Note that it is still a single tti choice,
	// it is just a non-obvious place to put it.
	unsigned mTTI;	// 10, 20, 40, 80, or "dynamic" which we dont use
	CodingType mTypeOfChannelCoding;
	CodingRate mCodingRate;
	unsigned mRateMatchingAttribute;
	unsigned mCRCSize;
	void toAsnSSTF(ASN::SemistaticTF_Information *result);
	void text(std::ostream &os) { os <<LOGVAR(mTTI)<<LOGVAR(mTypeOfChannelCoding)<<LOGVAR(mRateMatchingAttribute)<<LOGVAR(mCRCSize); }
};

// 25.331 10.3.5.23 Transport Format Set. Defines the Transport Formats for one TrCh.
// The first CHOICE is DCH or non-DCH.  We may not need the non-DCH at all,
// because for us that will only be FACH, and if we only use that for signalling,
// there will only be one message size and one transport option and we will just
// hard-code it into the MAC.
// There are two kinds of these: for DCH or not, but the info we use is identical for both.
class RrcTfs : public virtual RrcDefs  // 10.3.5.23 Transport Format Set
{
	TrChInfo *mTrChPtr;	// Our owner.  The only reason we need this so far is for the multiplexed option
						// flag needed to compute the confusing rlc_size from the Transport Block size.
	UInt_z mMaxTfSize;	// Computed
	UInt_z mMaxTbSize;	// Computed

	// This is from ASN, representing: CHOICE Transport channel type.
	// The two choices for us represent DCH or RACH/FACH.
	// I have not broken out the cases separately below, because RACH/FACH is the same as DCH
	// but with logical channels left out.
	// You must call setDedicatedCh or setCommonCh
	enum ASN::TransportFormatSet_PR mPresent;
	public:
	RrcTfs(TrChInfo *wtrch) : mTrChPtr(wtrch) {}
	RrcTfs* setDedicatedCh() { mPresent = ASN::TransportFormatSet_PR_dedicatedTransChTFS; return this; }
	RrcTfs* setCommonCh() { mPresent = ASN::TransportFormatSet_PR_commonTransChTFS; return this; }

	// And here is our ultra-light-weight RBMappingInfo data:
	// We dont use logical-channel-mapping (the rbid is the logical channel id)
	// and the RB mapping is invariant, so we can put it in the TFS permanently.
	// Either the TrCh is MAC multiplexed, or it carries a single RbId;
	// the latter case will probably only ever be used for voice channels
	// which map the AMR data channels to dedicated TrCh 1,2,3, and put
	// all the SRB signalling on multiplexed TrCh 4.
	bool isMacMultiplexed();
	RbId getNonMultiplexedRbId();

	//bool isDedicated() {
	//	switch (mPresent) {
	//		case ASN::TransportFormatSet_PR_dedicatedTransChTFS: return true;
	//		case ASN::TransportFormatSet_PR_commonTransChTFS: return false;
	//		default: assert(0);	// oops!
	//	}
	//}

	private:
	RrcSemiStaticTFInfo mSemiStaticTFInfo;


	// In the spec this structure is confusingly compressed to save space,
	// by encoding the "Number of TBs and TTI List" as a sub-array, even though
	// there is really one Tfs array.
	// I am expanding that out, however, note that a TF can have TBs of only one size.
	// The spec method makes sense because for AM and UM mode there should only be one TB size,
	// or sometimes there are two and the second one is zero.
	// For TM mode, there can be lots of different block sizes, and the MAC is supposed
	// to pick a TFS to match them.
	struct DynamicTFInfo {
		// 25.331 8.6.5.1: the "RLC-Size" in these structures is a magic term defined as follows:
		// For common [RACh/FACH] TrCh, TB size = "RLC-Size".
		// For dedicated [DCH] TrCh, TB size = "RLC-Size" + MAC header (unless RLC-Size == 0, then TB size = 0)
		// In other words, for (fach, rach) the "RLC-Size" is not the rlc payload size,
		// which must be figured differently for each rb-id (aka logical channel) depending on the
		// the MAC header needed for that channel, which is different for CCCH (SRB0), DCCH (SRB1) and others.
		// There is a very useful example at 34.108 6.10.2.4.3.2.1.2.
		// We dont save the "RLC-Size" as defined above, but rather that Transport Block Size,
		// and then compute the "RLC-Size" needed to be sent to the MS in the IE when we create the ASN.
		unsigned mTBSize;			// Size of the transport block in bits.  This is NOT the 'rlc_size' in the TFS IE.
		//unsigned mTTI;		// Only if semi-static TTI is "dynamic", which it wont be.
		unsigned mNumTB;

		// We always just put the 'ALL' option in the ASN.
		// CHOICE Logical Channel List:

		unsigned getTBSize() { return mTBSize; }
		unsigned getNumTB() { return mNumTB; }
		// Total size of all Transport Blocks in this one TF, in bits.
		unsigned getTfTotalSize() { return mTBSize * mNumTB; }

		// This struct can be converted to two different asn structures:
		// They are the same except for the rlcSize encoding:
		// the dedicated channel TFS allows bit-aligned rlcSize control,
		// while common channels are restricted to octet-aligned when using FDD.
		ASN::CommonDynamicTF_Info *toAsnCommon1(ASN::CommonDynamicTF_Info *result, bool isDownlink);
		ASN::DedicatedDynamicTF_Info *toAsnDedicated1(RrcTfs*tfs,ASN::DedicatedDynamicTF_Info *result);
		void text(std::ostream &os) {
			os <<"TF("<<LOGVAR(mTBSize)<<LOGVAR(mNumTB)<<")";
		}
	};

	struct DynamicTFInfoList {
		UInt_z mNumTF;
		DynamicTFInfo mDynamicTFInfo[maxTFS];
		void addTF(unsigned tbsize, unsigned numBlocks) {
			assert(mNumTF < maxTFS);
			mDynamicTFInfo[mNumTF].mTBSize = tbsize;
			mDynamicTFInfo[mNumTF].mNumTB = numBlocks;
			mNumTF++;
		}
		// This can be converted to two different asn structures:
		void toAsnCommon(ASN::CommonDynamicTF_InfoList *list, bool isDownlink);
		void toAsnDedicated(RrcTfs*tfs,ASN::DedicatedDynamicTF_InfoList *list);
		void text(std::ostream &os) {
			for (unsigned i=0;i<mNumTF;i++) { mDynamicTFInfo[i].text(os); os <<" ";}
		}
	} mDynamic;


	public:

	RrcSemiStaticTFInfo *getSemiStatic() { return &mSemiStaticTFInfo; }
	unsigned getTBSize(int tfnum) { return mDynamic.mDynamicTFInfo[tfnum].getTBSize(); }
	//unsigned getCodedBlockSize(int tfnum);
	unsigned getTfTotalSize(int tfnum) { return mDynamic.mDynamicTFInfo[tfnum].getTfTotalSize(); }
	unsigned getNumTB(int tfnum) {
		return mDynamic.mDynamicTFInfo[tfnum].getNumTB();
	}
	unsigned getNumTf() { return mDynamic.mNumTF; }
	TrChInfo *getTrCh() { return mTrChPtr; }

	unsigned getPB() { return mSemiStaticTFInfo.mCRCSize; }
	unsigned getRM() { return mSemiStaticTFInfo.mRateMatchingAttribute; }
	TTICodes getTTICode() { return TTI2TTICode(mSemiStaticTFInfo.mTTI); }
	unsigned getTTINumFrames() { return mSemiStaticTFInfo.mTTI / 10; }	// Return 1,2,4,8
	bool getTurboFlag() { return mSemiStaticTFInfo.mTypeOfChannelCoding == Turbo; }

	// Used for AM and UM mode RLC, but we dont really expect there to be different
	// TB sizes, only that there may be a zero-valued one that we want to ignore.
	unsigned getMaxTBSize() {	// in bits.
		if (mMaxTbSize == 0) {
			for (unsigned i = 0; i < mDynamic.mNumTF; i++) {
				unsigned tmp = getTBSize(i);
				if (tmp > mMaxTbSize) mMaxTbSize = tmp;
			}
		}
		return mMaxTbSize;
	}

	unsigned getMaxTfSize() {	// Return max number of bytes in any single TF.
		if (mMaxTfSize == 0) {
			for (unsigned i = 0; i < mDynamic.mNumTF; i++) {
				unsigned tmp = getTfTotalSize(i);
				if (tmp > mMaxTfSize) mMaxTfSize = tmp;
				//LOG(NOTICE) << format("getMaxTfSize i=%d tmp=%d max=%d",i,tmp,(int)mMaxTfSize);
			}
		}
		return mMaxTfSize;
	};

	// Configuration Functions:
	RrcTfs *setSemiStatic(unsigned tti,CodingType ct, CodingRate cr, unsigned rm, unsigned crcSize) {
		mSemiStaticTFInfo.mTTI = tti;
		mSemiStaticTFInfo.mTypeOfChannelCoding = ct;
		mSemiStaticTFInfo.mCodingRate = cr;
		mSemiStaticTFInfo.mRateMatchingAttribute = rm;
		mSemiStaticTFInfo.mCRCSize = crcSize;
		return this;
	}


	// Note: We specify the Transport Block Size, not 'rlc-size' used in the TFS IE.
	// If you transcribe these from an RRC spec, you may have to adjust the size.
	RrcTfs *addTF(unsigned tbsize, unsigned numBlocks) {
		mDynamic.addTF(tbsize,numBlocks);
		return this;
	}

	void toAsnTfs(ASN::TransportFormatSet *result, bool isDownlink = false);
	unsigned getNumTF() { return mDynamic.mNumTF; }
	void text(std::ostream &os) {
		os <<LOGVAR(mPresent)<<LOGVAR(mMaxTfSize)<<LOGVAR(mMaxTbSize)<<
			LOGVAR2("multiplexed",isMacMultiplexed());
		mSemiStaticTFInfo.text(os);
		os << " ";
		mDynamic.text(os);
	}
};

// This is a handle on a TransportFormat; applies to one TrCh.
// The TF is part of a TFS, which has shared (semi-static) and non-shared (dynamic) parts,
// so it is a pointer to the TFS plus the index into the dynamic part.
// It also serves as a cheap iterator object.
struct RrcTf
{
	RrcTfs *mTfs;	// The owner of this TF.
	TfIndex mTfi;

	RrcTf() : mTfs(0) {}
	RrcTf(RrcTfs *wTfs,TfIndex wTfi) : mTfs(wTfs),mTfi(wTfi) {}
	unsigned getNumTB() {
		if (mTfs == 0) { return 0; }
		return mTfs->getNumTB(mTfi);
	}
	unsigned getTBSize() {	// in bits
		if (mTfs == 0) { return 0; }
		return mTfs->getTBSize(mTfi);
	}
	unsigned getTfTotalSize() { return getTBSize() * getNumTB(); }
	RrcSemiStaticTFInfo *getSemiStaticInfo() {
		if (mTfs == 0) { return 0; }
		return mTfs->getSemiStatic();
	}

	// Cheap iterator for TF in TFS.  Does not iterate TF in TFCS:
	// Return the first TF.
#if CURRENTLYUNUSED
	void firstTf(RrcTfs *tfs) {
		mTfs = tfs;
		mTfi = 0;
	}
	bool valid() { return mTfs && mTfi < mTfs->getNumTf(); }
	// Advance to the next TF in the TFS.
	void nextTf() {
		mTfi++;	// May be rendered invalid; caller must check.
	}
#endif
};


struct PowerOffset {	// 10.3.5.8
	Bool_z mPresent;	// Has this been specified?
	unsigned char mGainFactorBetaC;
	PowerOffset() { mPresent = false; }
	PowerOffset(unsigned char val) { mPresent = true; mGainFactorBetaC = val; }
};

// One Transport Format Combination, saves info for one CTFC.
class RrcTfc : public virtual RrcDefs
{
	// Each TrCh [Transport Channel] has a TFS [Transport Format Set] which specifies
	// the legal TF [Transport Formats] allowed for that TrCh.
	// The physical channel encodes CCTrCh consisting of 1 or more TrCh.
	// The TFC specifies the complete set of TB [Tranport Block] that can be transmitted
	// for all TrCh simultaneously for the PHY by picking one TF from each TrCh.
	// The spec compresses this info into the CTFC, but we also keep a handle
	// to the TF specified for each TrCh, which is not part of the IE.
	TrChList *mTrChList;	// Who owns us, same pointer duplicated in all TFC in this TFCS.
							// Note that it will be either a downlink or uplink one.
							// It is inited not by constructor, but when the RrcTf are.
	RrcTf mTfList[maxTrCh];
	public:
	unsigned mCTFC;			// This is what goes out over the PHY in the special spot in the RadioSlot.
	PowerOffset mPowerOffset;
	unsigned mTfcsIndex;		// The index of this TFC in the TFCS.

	// A way to get back to the TrChInfo if you have a handle to the TFC, but
	// not to the TrChConfig or MasterChConfig in which it resides.
	TrChInfo *getTrChInfo(TrChId tcid);
	unsigned getTfcSize();

	RrcTf *getTf(TrChId trchid);
	unsigned getTfcsIndex() { return mTfcsIndex; }
	unsigned getTfIndex(TrChId trchid) { return getTf(trchid)->mTfi; }
	void setTfc(TrChList *chlist, TfIndex tfa, TfIndex tfb=0, TfIndex tfc=0, TfIndex tfd=0);
	unsigned getSize();
	unsigned getNumTrCh() const;
};

// 10.3.5.15  TFCS Reconfiguration/Addition Information.
// TFC set is a list where each element selects one of the pre-defined TF for each TrCh.
// Therefore the TFCS is a matrix whose width is the
// number of TrCh and whose length is the max number of TFC.
// They compress the rows into a single number called CTFC as per 14.10.
// Here is an example, L and P defined in 14.10:
// L, P indexed by TrCh numbered 1..i, Transport Formats numbered 0..L(i)-1
// TrCh0: has tf0, tf1, tf2.  L(1)=3; P(1)=1;
// TrCh1: has tf0, tf1.		L(2)=2; P(2)=3;
// TrCh2: has tf0, tf1.		L(3)=2; P(3)=6;
// Each row here is a TFC, selecting one tf for each TrCh.
// CTFC(tf0,tf0,tf0) = 0*1 + 0*3 + 0*6 = 0;
// CTFC(tf2,tf0,tf0) = 2*1 + 0*3* + 0*6 = 2;
// CTFC(tf1,tf1,tf1) = 1*1 + 1*3  + 1*6 = 10;
struct RrcTfcs : public virtual RrcDefs 	// 10.3.5.15 Transport Format Combination Set
{
	unsigned mCtfcSize;	// number of bits, chosen from: 2,4,6,8,12,16.
	unsigned mNumTfc;
	RrcTfc mTfcList[maxTfc];
	RrcTfcs() : mCtfcSize(0), mNumTfc(0) {
		for (unsigned i = 0; i < maxTfc; i++) { mTfcList[i].mTfcsIndex = i; }
	}

	//void setCTFCSize(unsigned val) { mCtfcSize = val; }

	// Add a new TFC and set to the specified TF Indicies for up to four TrChs.
	RrcTfcs *addTFC2(TrChList *chlist, TfIndex tfa, TfIndex tfb=0, TfIndex tfc=0, TfIndex tfd=0);
	// Set the PowerOffset for the most recently defined CTFC
	void setPower(PowerOffset po) {
		mTfcList[mNumTfc-1].mPowerOffset = po;
	}

	// If the spec gives a ctfc, make sure we calculate it properly:
	// Check the most recent CTFC to see if it matches ctfc.
	void checkCTFC(unsigned ctfc) {
		assert(mTfcList[mNumTfc-1].mCTFC == ctfc);
	}
	//void toAsnTfcs(ASN::TFCS *result, bool powerInfo = true);
	void toAsnTfcs(ASN::TFCS *result, TrChType tctype);

	RrcTfc *getTfc(unsigned tfci) { return &mTfcList[tfci]; }

	//unused: bool isTrivialTfcs();

	unsigned getNumTfc() { return mNumTfc; }
	RrcTfc *iterBegin() { return &mTfcList[0]; }
	RrcTfc *iterEnd() { return &mTfcList[mNumTfc]; }
	/***
	class iter {
		unsigned pos;
		iter(unsigned wpos) : pos(wpos) {} 
		bool end() { return pos == mNumTfc; }
		iter operator++() { pos++; return *this; }
	};
	***/
};

// 10.3.5.13 TFCS Explicit Configuration.
// also 10.3.5.20 - TFCS, which consists entirely of one TFCS struct.
// All this contains is a TFCS to be reconfigured, added, removed or replaced
//struct TfcsExplicitConfiguration {	// 10.3.5.13, Explicit TFCS configuration
	//TFCS ...;
//};


struct TfcSubset {
	// todo?
};

// Container for DlTrChInfo or UlTrChInfo
struct TrChInfo : public virtual RrcDefs
{
	TrChType mTransportChType;
	UInt_z mTransportChannelIdentity;	// We use TrCh id 1..4, so 0 means uninitialized.

	// Normally the RBMappingInfo specifies the mapping of rb->trch.
	// We do that statically, so the RBMappingInfo appears in two places:
	// The RBInfo specifies the trch and the TrChInfo (here) specifies
	// the multiplexing option, and if not multiplexed, the single rbid
	// to be sent on that channel.
	// The multiplexing is true if multiple logical channels (RBs) are mapped onto
	// this TrCh, and is used to specify if the MAC uses the CT field for logical channel mapping.
	// Multiplexing is almost always true.
	// The major exception is voice channels where the AMR codec data channels
	// are on rbid 1,2,3 mapped to non-multiplexed trch 1,2,3, and 
	// everything else (SRB1,2,3) are mapped to multiplexed trch 4
	// It would also be possible to use multiple trch for PS channels,
	// but we are not doing that in the initial implementation.
	// Normally, the RBMappingInfo also specifies the rbid to logical channel mapping,
	// (where the logical channel is what is sent in the TransportBlock to
	// indicate the rbid for multiplexed TrCh) but we just use 1-to-1 for that,
	// so the logical channel mapping appears nowhere in this code base.
	// Technically, the RBMappingInfo could reference the same TFS for different
	// underlying channel types (DCH, FACH) in which case the mMacMultiplexed flag could change,
	// but we would use a different TFS for each PhCh in that case.
	bool mTcIsMultiplexed;
	// If not multiplexed, this is the rbid.
	// 0 means undefined, as SRB0 is never one of the options here.
	RbId mTcRbId;

	void setTrCh(TrChType type, unsigned trchid1based, bool mMultiplexed, RbId rbid=0) {
		assert(mMultiplexed || rbid);
		mTransportChType = type;
		mTransportChannelIdentity = trchid1based;
		mTcIsMultiplexed = mMultiplexed;
		mTcRbId = rbid;
	}

	virtual RrcTfs *getTfs() = 0;
	//virtual TrChInfo *bler_QualityValue(double val);	// Only in dl.
	//virtual RrcTfs *setSemiStatic(unsigned tti,CodingType ct, CodingRate cr, unsigned rm, unsigned crcSize);
};

// 10.3.5.24: UL TrCh info common for all DCH transport channels.
// By which they mean all the transport channels in this DCH.
// (Because the TFCS selects a set of TFS for all TrCh simultaneously.)
// I also use this structure for the PRACH TFC subset.
// Note there is a PRACH Tfcs "omitted in this version of spec"
// Rather the PRACH informaion is in SIB5 or SIB6
struct UlTrChInfoCommon
{
	// CHOICE mode = FDD:
	struct RrcTfcs mTfcs;
	TfcSubset mTfcSubset;	// MD - Mandatory with default value. (gotta love that) In different location for REL-4 or REL-10
	struct RrcTfcs *getTfcs() { return &mTfcs; }
};

struct DlTrChInfoCommon // 10.3.5.6: DL TrCh common for all TrCh
{
	// SCCPCH TFCS - omitted in this version (v4-v10) of the spec.
	// CHOICE mode = FDD:
	//union DlParameters_t {  This is a union, but C++ does not allow it, so oh well...
	enum ChoiceDlParameters { eExplicit, eSameAsUL } mChoiceDlParameters;
	struct Explicit {
		RrcTfcs mTfcs;
	} mExplicit;
	//} DlParameters;
	struct RrcTfcs *getTfcs() { return &mExplicit.mTfcs; }
};

struct UlTrChInfo : TrChInfo // 10.3.5.2: Added or Reconfigured UL TrCh information
{
	RrcTfs mTfs;
	RrcTfs *getTfs() { return &mTfs; }
	UlTrChInfo() : mTfs(this) { }
};

class UlTrChList;
struct DlTrChInfo  : TrChInfo // 10.3.5.1: Added or Reconfigured DL TrCh information
{
	//union {	// C++ cant initialize unions.
	enum ChoiceDlParameters { eExplicit, eSameAsUL } mChoiceDlParameters;
	//struct {
		RrcTfs mTfs;		// This is used for Explicit TFS.
	//} mExplicit;
	struct {
		// NOT IMPLEMENTED!!!!
		// And not going to be; this just saves room in the setup message, and who cares.
		TrChType mChType;
		unsigned mChId;
	} mSameAsUL;
	//} DlParameters;
	double mDCHQualityTarget;	// Only in the DL TrChInfo

	// TODO: If it is not explicit, we would fish TFS out of the UL info.
	RrcTfs *getTfs() { return &mTfs; }
	DlTrChInfo *bler_QualityValue(double val);
	DlTrChInfo *copyUplink(UlTrChList *ul, unsigned trch1based);
	DlTrChInfo() : mTfs(this) {}
};
#if URRC_IMPLEMENTATION
	DlTrChInfo *DlTrChInfo::bler_QualityValue(double val) { mDCHQualityTarget = val; return this; }
#endif

// Container for "TrCh Information Elements" part of 10.2.50: Transport Channel Reconfiguration.
// or 10.2.33: Radio Bearer Setup.
// It contains a list of transport channels.
// Each channel has a RrcTfs; the TFCS applies to the entire list.
struct TrChList : public virtual RrcDefs, public Text2Str
{
	//virtual unsigned getNumTF(unsigned TrChNum0) =0;
	virtual RrcTfcs *getTfcs() =0;
	virtual RrcTfs *getTfs(TrChId tcid) =0;
	// If macMultiplexed is false, rbid must be specified, and not otherwise.
	virtual TrChInfo *defineTrCh(TrChType type, unsigned id, bool macMultiplexed, RbId rbid=0) =0;
	virtual unsigned getNumTrCh() const = 0;
	virtual TrChInfo *getTrChInfo(TrChId tcid) =0;

	void text(std::ostream &os) const {
		for (TrChId i = 0; i < getNumTrCh(); i++) {
			os << "TrCh "<<i<<":";
			const_cast<TrChList*>(this)->getTfs(i)->text(os); // unconst foo bar.
			os << "\n";
		}
	}

	// Return the maximum size in bits of any transport format on any single TrCh.
	// Note that the entire Transport Format Combination could be bigger.
	unsigned getMaxAnyTfSize() {
		unsigned maxtfsize = 0;
		for (TrChId tcid = 0; tcid < getNumTrCh(); tcid++) {
			unsigned tfsize = getTfs(tcid)->getMaxTfSize();
			if (tfsize > maxtfsize) {maxtfsize = tfsize;}
			//LOG(NOTICE)<<"getMaxAnyTfSize"<<LOGVAR(tfsize)<<LOGVAR(maxtfsize);
		}
		return maxtfsize;
	}

	// Max of 4 transport channels for this function:
	TrChList *addTFC(TfIndex tfa, TfIndex tfb=0, TfIndex tfc=0, TfIndex tfd=0) {
		getTfcs()->addTFC2(this,tfa,tfb,tfc,tfd);
		return this;
	}
	TrChList *setCTFCSize(unsigned size) { getTfcs()->mCtfcSize = size; return this; }
	TrChList *checkCTFC(unsigned ctfc) { getTfcs()->checkCTFC(ctfc); return this; }
	TrChList *setPower(PowerOffset po) { getTfcs()->setPower(po); return this; }

	unsigned iterTrChBegin() { return 0; }
	unsigned iterTrChEnd() { return getNumTrCh(); }

	// Derived accessors:
	unsigned getTTINumFrames(TrChId tcid) { return getTfs(tcid)->getTTINumFrames(); }
	TTICodes getTTICode(TrChId tcid) { return getTfs(tcid)->getTTICode(); }
	unsigned getRM(TrChId tcid) { return getTfs(tcid)->getRM(); }
	//unsigned getTFCSize(TrChId tcid, TfcId j) { return getTfcs()->? }
};

// ASC Access Service Class - included in RACH.
// One ASC is assigned by RRC for initial channel assignment.
// Related IEs:
// 10.3.6.1 AC-to-ASC mapping
//	used in 10.3.6.55 PRACH system information list,
//		but default is PRACH System Information List in SIB5

// David set tfci to false in SIB5, implying no transport format set for RACH/FACH.
// So where does the pdu size come from, then?
// Note on FACH: it is carried on SCCPCH channel, whose Tfs and Tfcs may be set up via:
// 10.3.6.70 SCCPCH Info for FACH
// 10.3.6.72 Secondary CCPCH system information
// 10.3.6.13 CPCH set info - may be broadcast in SIB or assigned by SRNC, but it is pseudo-static in a cell.,
//		used in 10.2.8 Cell Update Confirm, 10.2.16a: Handover
// 		10.2.22 Physical Channel Reconfiguration, 10.2.27 Radio Bearer Reconfig
// 		10.2.40 RRC Connection Setup, 10.2.50 Transport Channel Reconfig
// 		10.2.48.8.11: SIB 8.
// Also see 10.3.6.88: Uplink DPCH info 10.3.6.30: Downlink PDSCH info

// The below does not correspond to a specific IE.
// It represents the uplink TrCh Information Elements part
// of 10.2.50: Transport Channel Reconfiguration.
// or 10.2.33: Radio Bearer Setup.
// or 10.2.40: RRC Connection Setup, which includes:
//		10.3.4.24 Signalling RB info to setup.
//			10.3.4.23 RLC Info 
//		10.3.4.0a Default config for CELL_FACH (rel-8)
//			If present use default config 0.
//		10.3.5.24 UL TrChInfo common for all transport channels.
//			PRACH TFCS "should not be included in this version of spec"
//			TFC subset
//			DCH TFCS
//		10.3.5.2 Added UL TrCh info (for DCH only, but mandatory - must be empty)
//		10.3.4.6 DL TrChInfo common for all transport channels.
//			SCCPCH TFCS "should not be included in this version of spec"
//			DCH TFCS.
//		10.3.5.1 Added DL TrCh info (for DCH only)
//		Default or preconfiguration options (rel-5)
//		10.3.6.36 Freq Info
//			ARFCN
//	
// NOTE: If SIB6 is present then:
// SIB6 specifies PRACH and SCCPCH for UE in connected mode.
// SIB5 specifies PRACH and SCCPCH for UE in idle mode.
// Above discussed in 8.5.17 and 8.5.19.
// SIB5 includes:
//		10.3.6.72 Secondary CCPCH system information
//			includes FACH TFS and FACH TFCS.
// 		10.3.6.55 PRACH system information list
//			includes RACH TFS, RACH TFCS, 
//			Additional RACH TFS for CCCH (rel-6).
//			Additional RACH TFCS for CCCH (rel-6) - includes only
//				PowerOffsetInformation 10.3.5.8.
//			
// For DCH, they put the TFCS and TFC subset in:
// UlTrChInfoCommon: "UL Tranport Channel Information for all Transport Channels",
// which is used, for example, by 10.2.50: Transport Channel Reconfiguration.
// For RACH/PRACH (or FACH/SCCPCH) the physical channels do not support
// multiple transport channels, so the TFCS is in the main structure,
// for example: 10.3.6.55: PRACH System Information list.

// The MAC-d can route logical channels to the MAC-s/sh/m to be sent on RACH/FACH, but I suspect
// that we are really only expected to use this facility for SRBs, not data RBs.  Not sure.
// Anyway, all the TrCh multiplexing stuff applies ONLY to DCH.
// Note that 8.6.5: Transport Channel Information Setup starts out saying if the Tfcs is
// received for a RACH channel in SIB5 or SIB6, but those are in IEs only applicable to TDD,
// for example, there is a Tfcs buried in TDD only 10.3.6.46: PDSCH system info.
struct UlTrChList : public TrChList, public UlTrChInfoCommon
{
	UInt_z mNumTrCh;
	UlTrChInfo mChInfo[maxTrCh];	// Only 1 for RACH/PRACH

	// TODO CPCH set id ?  OP optional
	// TODO: Added or Reconfigured TrCh information for DRAC list.  OP optional
	// The TrChId is 0 based.
	RrcTfs *getTfs(TrChId tcid) { assert(tcid < getNumTrCh()); return mChInfo[tcid].getTfs(); }
	RrcTfcs *getTfcs() { return UlTrChInfoCommon::getTfcs(); }
	unsigned getNumTrCh() const { return mNumTrCh; }
	UlTrChInfo *getTrChInfo(TrChId tcid) { return &mChInfo[tcid]; }

	// This is the TrCh config function, which uses TrCh id numbered starting with 1.
	// The arrays where we save TrCh stuff are 0 based.
	// The complicated RBMappingInfo never changes for us, so we save
	// it in the TrChInfo and RBInfo, which must also be configured to refer
	// back to this TrCh.
	// The multiplexed variable is whether the MAC is going to multiplex multiple logical channels
	// on this TrCh.  This is not in the TFS in the spec - normally you would have to
	// figure it out after applying the RBMappingInfo, but we dont do that, we
	// specify it statically when the TrChInfo is set up.
	// More comments at mRlcSize in DynamicTFInfo.
	// If macMultiplexed is false, rbid must be specified, and not otherwise.
	// The rbid is 1 for SRB1, etc, so 5 is the first available user rbid.
	UlTrChInfo *defineTrCh(TrChType type, unsigned trchid1based, bool macMultiplexed, RbId rbid=0) {
		UlTrChInfo *result = &mChInfo[trchid1based-1];
		if (result->mTransportChannelIdentity) { assert(0); }	// Already defined.
		result->setTrCh(type,trchid1based,macMultiplexed,rbid);
		if (trchid1based > mNumTrCh) { mNumTrCh = trchid1based; }
		return result;
	}
};

// This does not correspond to a specific IE.
// It represents the downlink TrCh Information Elements part of 10.2.50: Transport Channel Reconfiguration.
struct DlTrChList : public TrChList, public DlTrChInfoCommon
{
	UInt_z mNumTrCh;
	DlTrChInfo mChInfo[maxTrCh];	// Only one for FACH/SCCPCH

	RrcTfs *getTfs(TrChId tcid) { assert(tcid < getNumTrCh()); return mChInfo[tcid].getTfs(); }
	RrcTfcs *getTfcs() { return DlTrChInfoCommon::getTfcs(); }
	unsigned getNumTrCh() const { return mNumTrCh; }
	DlTrChInfo *getTrChInfo(TrChId tcid) { return &mChInfo[tcid]; }

	// See comments for identical function in UlTrChList.
	DlTrChInfo *defineTrCh(TrChType type, unsigned trchid1based, bool macMultiplexed,RbId rbid=0) {
		DlTrChInfo *result = &mChInfo[trchid1based-1];
		if (result->mTransportChannelIdentity) { assert(0); }	// Already defined.
		result->setTrCh(type,trchid1based,macMultiplexed,rbid);
		if (trchid1based > mNumTrCh) { mNumTrCh = trchid1based; }
		return result;
	}
};

// This defines the set TrCh for one physical channel, either RACH, FACH, UL-DCH or DL-DCH.
// It provides the info for the "TrCh Information Elements" part of
// 10.2.50: Transport Channel Reconfiguration.
// TODO: Simplify the ChList by getting rid of the Common struct.
struct TrChConfig : public virtual RrcDefs
{
	//todo? TfcSubset
	// How many transport formats defined for TrCh, numbered starting at 0.
	//unsigned getNumTF(unsigned TrChNum0) { return mChInfo[TrChNum0].getNumTF(); }
	UlTrChList mUlTrChs;
	DlTrChList mDlTrChs;

	// The following parameters are needed by MAC.
	// In a normal UMTS system they would be derived from the
	// RBMappingInfo as per 25.331 8.5.21, but we may just set them.
	// We are not using RLC subset.

	UlTrChList *ul() { return &mUlTrChs; }
	DlTrChList *dl() { return &mDlTrChs; }

	// It will be FACH or DCH.  All the TrCh are the same type, so just return the first one. 
	// unused...
	//TrChType getDlTrChType() {
	//	return dl()->getTrChInfo(0)->mTransportChType;
	//}

	// Just dump TrCh 0.
	void tcdump() {
		TrChId chid = 0;
		std::cout << format("UL:%p tfcs=%p mNumTfc=%d numTF=%d\n",ul(),ul()->getTfcs(),
			ul()->getTfcs()->getNumTfc(),ul()->getTfs(chid)->getNumTf());
		std::cout << format("DL:%p tfcs=%p mNumTfc=%d numTF=%d\n",dl(),dl()->getTfcs(),
			dl()->getTfcs()->getNumTfc(),dl()->getTfs(chid)->getNumTf());
	}

	// TODO: this goes where now?
	// bool mSameAsUplink;	// Only in downlink, means just use the uplink.

	// These are generic TrCh configuration.
	void configRachTrCh(int ulSF,TTICodes ulTTICode, int ulPB, int TBSize);
	void configFachTrCh(int ulSF,TTICodes ulTTICode, int ulPB, int TBSize);
	bool configDchPS(DCHFEC *dch, TTICodes tticode, unsigned pb, bool useTurbo, unsigned ulTBSize, unsigned dlTBSize);

	// From 25.331 13.7: Parameter values for default radio configurations.
	void defaultConfig3TrCh();	// Default config number 3, used for voice.
	//void defaultConfig1TrCh();	// Default config number 1 for low-rate signalling.
};

// And I quote: "RB Mapping Info: A multiplexing option for each possible transport channel
// this RB can be multiplexed on."
// Pats opinion is that the primary purpose of this is to allow the DCCH and DTCH to be
// sent over either FACH or DCH, so if the UE changes state to CELL_FACH
// you can still send it messages.
// It seems like quite alot of work to avoid just sending a new configuration when that happens.
// This is not really in either layer 1 and layer 2, but defines a logical
// mapping to hook them together.
// It specifies the TrCh and mac logical channel to use for the RB.
// We only use a small subset of this, and here it is:
	// From 25.331 6.3 and I quote: "Additionally, RBs whose identities shall be set
	// between 5 and 32 may be used as signalling radio bearer for the
	// RRC messages on the DCCH sent in RLC transparent mode (RLC-TM)."
	// Lets just leave the SRB mapping options out for now.

// There can be two mappings: one for RACH or FACH and one for DCH.
// For RACH/FACH you need:
//		LogicalChId, TFS index to use, mac priority.
// For DCH  you need:
//		TrCh id, logicalChId if multiplexed, mac priority.
//		optional list of TFSs
// So we could specify this in the TrCh setup, by specifying the logical channels
// that are allowed to use each TF in addTF().
#if USE_RBMAPPINGINFO
// RBMappingInfo - see RBInfo and defaultRBMappingInfoToAsn
// We can derive it all of the mapping info from the TrCh, the RbId, and these variables:
// o mTcIsMultiplexed in the TrCh.
// o mTrChAssigned in the RBInfo.
// Which implies that we have to download new RBInfos when we change
// the UE state from CELL_FACH to CELL_DCH or vice versa.  And so what.
// TODO: Are multiple mapping options necessary?
// Which I think is the same question as: Does the phone change state without our orders?
// I guess it would if it gets lost, but that should be ok because
// it can just rach in again and reestablish channels from scratch.
// But if it becomes necessary to support two RBMappingInfo options, all we have
// to do is add two mTrChAssigned to RBInfo - one for RACH/FACH and one for DCH.
// We dont need this messy confusing RBMappingInfo structure.
struct RBMappingInfo : public virtual RrcDefs 	// 10.3.4.21
{
	// "ALL" means the entire TFS, "Configured" means the RLC sizes
	// configured for this logical channel in the TFS.
	// For RACH, it must be explicit, and the mRlcSizeIndex below
	// is the 1-based index of the TF in the TFS for the RACH TrCh.
	// Very easy to goof up the configuration here.
	enum ChoiceRlcSizeList { eAll, eConfigured, eExplicitList };

	unsigned mNumberOfUplinkRlcLogicalChannels;	// 1..2  This is unused historic nonsense.
	struct ulRBMappingInfo_s {
		// TODO
		// RLC logical channel mapping indicator is always TRUE, so not included here.
		TrChType mUplinkTransportChType;
		int mUlTransportChannelIdentity;	// 1..31,  sec 10.3.5.18  One-based, not Zero-based!!
		// logical channel 1..15. Used to distinguish logical chans by MAC on TrCh.
		// It is optional because if there is no logical channel mapping by MAC it is unneeded.
		int mUlLogicalChannelIdentity;
		enum ChoiceRlcSizeList mChoiceRlcSizeList;
		//union {
		struct ExplicitList_t {
			// This is always just one integer, so dont bother making it an array.
			//unsigned mRlcSizeIndex[maxTFS];	// If choice == ExplicitList.
			unsigned mRlcSizeIndex;	// If choice == ExplicitList.  Which it wont.
		} ExplicitList;
		//} RlcSizeList;
		// E-DCH stuff skipped.
		unsigned mMacLogicalChannelPriority;	// 1..8

		ulRBMappingInfo_s() :
			mUplinkTransportChType(TrChInvalid),	// Must specify this
			mUlTransportChannelIdentity(1),			// Default is almost always used
			mUlLogicalChannelIdentity(-1),			// Must specify this.
			mChoiceRlcSizeList(eAll),				// Default is ok.
			mMacLogicalChannelPriority(1)			// Default is ok.
			{}
	} ul[maxRBMuxOptions];

	unsigned mNumberOfDownlinkRlcLogicalChannels;	// 1..2  This is unused nonsense.
	struct dlRBMappingInfo_s {
		TrChType mDownlinkTransportChType;
		unsigned mDlDchTransportChannelIdentity;	// 1..31
		int mDlLogicalChannelIdentity;	// 1..15. Optional. Used to distinguish logical chans by MAC on transport chan
		dlRBMappingInfo_s() :
			mDownlinkTransportChType(TrChInvalid),	// Must specify this.
			mDlDchTransportChannelIdentity(1),		// Default is almost always used
			mDlLogicalChannelIdentity(-1)			// Must specify this
		{}
	} dl[maxRBMuxOptions];

	RBMappingInfo() :
		mNumberOfUplinkRlcLogicalChannels(1),
		mNumberOfDownlinkRlcLogicalChannels(1)
		{}

	// Write this IE out to an RRC Message:
	void toAsnRB_MappingOption(ASN::RB_MappingOption *thing);

	// Default Config setup methods:

	//bool parse_ul;
	void UL_LogicalChannelMappings(unsigned val) { mNumberOfUplinkRlcLogicalChannels = val; }
	// I'm not sure I have this option mapped properly, but it doesnt
	// matter because it is always just 1:
	void MappingOption(unsigned val) { mNumberOfDownlinkRlcLogicalChannels = val; }
	void ul_TransportChannelType(TrChType type) { /*parse_ul=true;*/ ul->mUplinkTransportChType = type; }
	void ul_transportChannelIdentity(unsigned id) {ul->mUlTransportChannelIdentity = id;}
	void dl_transportChannelIdentity(unsigned id) {dl->mDlDchTransportChannelIdentity = id;}
	void ul_logicalChannelIdentity(unsigned id) {ul->mUlLogicalChannelIdentity = id;}
	void dl_logicalChannelIdentity(unsigned id) {dl->mDlLogicalChannelIdentity = id;}

	//void transportChannelIdentity(unsigned id) {
	//	if (parse_ul) mUlTransportChannelIdentity = id;
	//	else mDlDchTransportChannelIdentity = id;
	//}
	//void logicalChannelIdentity(unsigned id) {
	//	if (parse_ul) mUlLogicalChannelIdentity = id;
	//	else mDlLogicalChannelIdentity = id;
	//}
	// rlc_SizeList(configured);
	void mac_LogicalChannelPriority(unsigned val) { ul->mMacLogicalChannelPriority = val; }
	void dl_TransportChannelType(TrChType type) { dl->mDownlinkTransportChType = type; }
	void rlc_SizeList(ChoiceRlcSizeList choice) {
		ul->mChoiceRlcSizeList = choice;
	}
	void rlc_SizeIndex(int val) {
		assert(ul->mChoiceRlcSizeList == eExplicitList);
		ul->ExplicitList.mRlcSizeIndex = val;
	}
};
#endif

// 10.3.4.22: RB with PDCP info, used in ActiveSetUpdate, ActiveSetUpdateComplete,
//		CellUpdateConfirm, PhysicalChannelReconfiguration, PhysicalChannelReconfigurationComplete,
// 		RadioBearerSetup, RadioBearerSetupComplete, RadioBearerReconfigurationComplete, RadioBearerRelease,
//		TransportChannelReconfiguration, TransportChannelReconfigurationComplete, URAUpdateConfirm
// todo?  I dont think we will ever need this for anything because it has to do with SRRC handoff.

void defaultRbMappingInfoToAsn(TrChInfo *tcul,TrChInfo *tcdl,RbId rbid,ASN::RB_MappingInfo_t *asnmsg);
int quantizeRlcSize(bool common, int tbsize);

}; // namespace UMTS

#endif
