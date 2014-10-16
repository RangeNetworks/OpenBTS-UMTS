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

#ifndef UMTSPHCH_H
#define UMTSPHCH_H
#include "Threads.h"
#include "UMTSCommon.h"
#include "TRXManager.h"

namespace ASN {
struct UL_DPCH_Info;
struct UL_ChannelRequirement;
struct DL_DPCH_InfoCommon;
struct DL_CommonInformation;
struct DL_DPCH_InfoPerRL;
struct DL_InformationPerRL_List;
};

//#define RELEASE99

namespace UMTS {
class DCHFEC;
unsigned getDlRadioFrameSize(PhChType chtype, unsigned sf);

// Note: This duplicates slot information that Harvind has added elsewhere.
// 3GPP 25.211
// table 6: [page 17] Random-access [aka PRACH] message data fields.
//		PRACH is different because the control and data parts are transmitted
//		on two carriers simultaneously; control always uses slot0.
// table 18: [page 36] Secondary CCPCH fields.
// table 1+2 [page 11] uplink DBDCH, DPCCH fields.
// table 11: [page 23] downlink DPDCH and DPCCH fields.
struct SlotFormat {
	int mSlotFormat;	// The index in this table; redundant - must match location in table.
	// int mBitRate;	// We dont need to keep this.
	int mSF;			// We dont need to keep this, but it is a handy comment.
	// int mBitsPerFrame;	// This is always bitsperslot*15.
	int mBitsPerSlot;
	int mNData1;				// may be data1+data2.
	int mNData2;				// if no data2 in table, then just 0.
	int mNPilot;
	int mPilotIndex;		// Index into sDlPilotBitPatternTable;
	int mNTfci;		// Number of raw tfci bits in the slot.
	int mNTpc;		// transmit power control, not used for CCPCH, always 0.

	// Currently the SlotForamt is used only by layer2, so I threw away the data1/data2 info and fbi,
	// but I put those in the tables so it is trivial to add in here.
	// transmitted slots per radio frame is always 15 because we do not use fractional.
	// Guess L2 doesnt care about TPC either, but whatever.
	//int mNData1;
	//int mNData2;	// not used for CCPCH, always 0.
	//int mNFbi;	// uplink dpcch only.  layer1 might be interested in this, but not layer2.
};

extern SlotFormat SlotInfoPrachControl[1];
extern SlotFormat SlotInfoPrachData[4];
extern SlotFormat SlotInfoSccpch[18];
extern SlotFormat SlotInfoUplinkDpcch[5];
extern SlotFormat SlotInfoUplinkDpdch[7];
extern SlotFormat SlotInfoDownlinkDch[17];
extern SlotFormat SlotInfoDownlinkDchREL99[17];

#if 0
// This class was defined in UMTSCommon.h but was unused, and the TrCHFEC classes
// redefined all these parameters using different names.
// I based the PhCh class on the TrCHFEC version, since that was in use, and this class was not.
// Here is the class preserved for posterity.
class PhyChanDesc {

	private: 
	
	unsigned int mScramblingCodeIndex;
	unsigned int mSpreadingCodeIndex;
	unsigned int mSpreadingFactor;
	PhyChanBranch mBranch;	

	public:

	PhyChanDesc(unsigned int wScramblingCodeIndex, unsigned int wSpreadingCodeIndex, 
		    unsigned int wSpreadingFactor, PhyChanBranch wBranch):
		    mScramblingCodeIndex(wScramblingCodeIndex),
		    mSpreadingCodeIndex(wSpreadingCodeIndex),
		    mSpreadingFactor(wSpreadingFactor),
		    mBranch(wBranch) {};

	unsigned int scramblingCode() { return mScramblingCodeIndex;}

	unsigned int spreadingCode() { return mSpreadingCodeIndex; }

	unsigned int spreadingFactor() { return mSpreadingFactor; }

	PhyChanBranch branch() { return mBranch; }

	friend bool operator<(const PhyChanDesc& a, const PhyChanDesc& b) {
		if (a.mScramblingCodeIndex < b.mScramblingCodeIndex) return true;
		if (a.mScramblingCodeIndex > b.mScramblingCodeIndex) return false;
                if (a.mSpreadingCodeIndex < b.mSpreadingCodeIndex) return true;
                if (a.mSpreadingCodeIndex > b.mSpreadingCodeIndex) return false;
                if (a.mSpreadingFactor < b.mSpreadingFactor) return true;
                if (a.mSpreadingFactor > b.mSpreadingFactor) return false;
                if (a.mBranch < b.mBranch) return true;
                if (a.mBranch > b.mBranch) return false;
		return false;
	}

};
#endif

// This is a physical channel in the ChannelTree.
// The primiary CPICH (sync) and PCCPCH (beacon) channels are reserved and non-programmable.
// The others are generally  associated with a SlotFormat from one of the tables.
class PhCh
{
	public:

	protected:
	PhChType mPhChType;	///< physical channel type
	// (pat) I suspect we are going to use assymetric SF on the SF=4 DCH, so I separated them.
	unsigned mDlSF;		///< downlink spreading factor
	unsigned mDlSFLog2;     ///< log 2 of downlink spreading factor
	unsigned mUlSF;		///< uplink spreading factor
	unsigned mSpCode;	///< downlink spreading code (pat) aka channel code, in the range 0..(SF-1)
	unsigned mSrCode;	///< uplink scrambling code
	ARFCNManager *mRadio;
	// Uplink puncturing limit expressed as percent in the range 40 to 100.
	// From sql "UMTS.Uplink.Puncturing.Limit"; default 100 (no puncturing); bounded if out of range.
	int mUlPuncturingLimit;
	bool mAllocated;	// Used by the ChannelTree
	// For SCCPCH and DPDCH we will save the downlink slot format here.
	SlotFormat *mDlSlot;

	// For uplink DPCCH, need slot format to determine pilot, TFCI, TPC, and FBI locations
	SlotFormat *mUlDPCCH;

	// For uplink channels there are two additional slot formats required for data and control parts.
	// The data slot format has no additional information for anyone.
	// The control slot format has no info needed by L2, and is probably a constant for all channels anyway,
	// so I left it out of here, although I did transcribe the data into the SlotFormat tables
	// in case anyone wants it.

	public:
	// Bidirectional channel requires two SF and uplink scrambling code:
	PhCh(PhChType chType,
		unsigned wDlSF,		// downlink spreading factor
		unsigned wSpCode,	// downlink spreading [aka channel] code
		unsigned wUlSf,		// maximum uplink spreading factor; the actual SF varies for each TFC.
		unsigned wSrCode,	// uplink scrambling code
		ARFCNManager *wRadio);

	SlotFormat *getDlSlot() {
		// This method should not be used for uplink channels (CPICH or PCCPCH or PRACH.)
		assert(mPhChType == SCCPCHType || mPhChType == DPDCHType);
		return mDlSlot;
	}

        SlotFormat *getUlDPCCH() {
                assert(mPhChType == SCCPCHType || mPhChType == DPDCHType);
                return mUlDPCCH;
        }

	// ASN interface:
	void toAsnUL_DPCH_Info(ASN::UL_DPCH_Info *iep);
	ASN::UL_ChannelRequirement *toAsnUL_ChannelRequirement();
	ASN::DL_DPCH_InfoCommon * toAsnDL_DPCH_InfoCommon();
	ASN::DL_CommonInformation * toAsnDL_CommonInformation();
	ASN::DL_DPCH_InfoPerRL *toAsnDL_DPCH_InfoPerRL();
	ASN::DL_InformationPerRL_List *toAsnDL_InformationPerRL_List();

	// Dont forget to multiply this by TTI.
	unsigned getDlRadioFrameSize();
	unsigned getUlRadioFrameSize();

	PhChType phChType() const { return mPhChType; }
	bool isDch() const { return mPhChType == DPDCHType; }
	bool isRach() const { return mPhChType == PRACHType; }
	unsigned getUlSF() const { return mUlSF; }
	unsigned getDlSF() const { return mDlSF; }
	unsigned getDlSFLog2() const { return mDlSFLog2; }
	unsigned SpCode() const { return mSpCode; }	// old name
	unsigned getSpCode() const { return mSpCode; }
	unsigned SrCode() const { return mSrCode; }	// old name
	unsigned getSrCode() const { return mSrCode; }
	ARFCNManager *getRadio() const { return mRadio; }

	// Uplink uses 2 bit tfci on both RACH and DCH.
	unsigned getUlNumTfciBits() { return 2; }
	unsigned getDlNumTfciBits() { return mDlSlot->mNTfci; }


	/**@name Ganged actions. */
	// NOTE: These are used by the ChannelTree to allocate the underlying
	// physical channel, which operations must be atomic.
	// The parent classes (eg: DCHFEC) have their own unrelated open() routines.
	// The final parent close() routine must call phChClose to release the
	// physical channel back tothe pool.
	//@{
	void phChOpen() { mAllocated = true; }
	void phChClose() { mAllocated = false; }
	//@}
	bool phChAllocated() { return mAllocated; }
};

// Downlink only channel has spreading factor and spreading [channel] code
struct PhChDownlink: PhCh {
	PhChDownlink(PhChType chType, unsigned wSF, unsigned wSpCode,ARFCNManager *wRadio):
	PhCh(chType,wSF,wSpCode,0,0,wRadio) {}
};

// Uplink only channel has scrambling code but no spreading factor.
// The uplink does not need an ARFCNManager pointer, so we set it to 0
struct PhChUplink: PhCh {
	PhChUplink(PhChType chType, unsigned wSF, unsigned wUplinkScramblingCode):
	PhCh(chType,0,0,wSF,wUplinkScramblingCode,(ARFCNManager*)0) {}
};


// An element in the channel tree.
// The channel allocation indication is not here,
// it is inside the DCHFEC class accessed by active() method.
struct ChannelTreeElt
{
	bool mReserved;	// This channel is reserved for something other than DCH.
	bool mAlsoReserved;	// This channel is above a reserved channel, so you cant use it either.
	DCHFEC *mDch;	// The DPDCH, although we could put the other PhChs in here too. (SCCPCH, PCCPCH, etc)
	bool available(bool checkOnlyReserved);
	bool active(void);
	ChannelTreeElt() : mReserved(0), mDch(0) {}
};


// The ChannelTree's primary purpose is to allocate DCH channels 
// from the pool for a SF chosen to meet a bandwidth criteria.
// DESIGN:
// The channels are in a tree, where each level is called a tier,
// with 4 channels at tier 0 (SF=4) and 256 channels at tier 7 (SF=256).
// We assume that we are going to allocate DCH physical channel objects to populate
// the entire tree on startup by the chPopulate() method.
// 
// CHANNEL ALLOCATION:
// Use the chChooseByBW() or chChooseBySF() methods to allocate a DCH channel.
// It is dynamic, so you can mix and match SF, no restrictions except what is intrinsic.
// There is no data stored in this structure about which channels are active,
// rather it calls the phChAllocated() method from the channel each time.
// So when a channel is deallocated, nothing is done in this tree at all.
// The chChoose functions currently open the channel before returning to make sure
// there is no race between two threads trying to allocate channels simultaneously.
// Dont know if that is possible because the callers dont yet exist :-)
// It is the callers responsibility to close the channel when finished.
//
// RESERVATIONS:
// Each physical channel that is used for some purpose other than a DCH 
// must be reserved in the tree by chReserve() prior to calling chPopulate()
// I added chReserve() calls to the RACH and FACH classes, but make darned sure
// you also call it for PICH, or whatever.
// And I quote, from 25.213 5.2.1:
// "The channelisation code for the Primary CPICH is fixed to C(ch,256,0)
// "and the channelisation code for the Primary CCPCH is fixed to C(ch,256,1).
// "The channelisation codes for all other physical channels are assigned by UTRAN."
// That implies that channel 0 is unusable in all tiers of the tree.
// Additionally, there are going to be some SCCPCH (for FACH and PCH),
// then we will fill the rest of the tree with DPDCH.
//
// QUALITY OF SERVICE:
// The bandwidth criteria comes from the QoS [Quality of Service] IE
// in the L3 PDP Context Setup message.
// In GPRS, I never saw anything but 'best effort' so we may have to
// map 'best effort' to something else, probably minimal effort.
// The bandwidth criteria in the QoS includes mean and peak throughputs,
// and an astonishing amount of other largely irrelevant information that
// mostly has to do with allocating capacity upstream.
// I assume we are only interested in peak throughput.
// The Peak bandwidth is discreetized into 9 classes: 1, 2, 4, 8, 16, 32, 64, 128 and 256 KB/s.
// (That's KiloBytes/s, not Kilobits/s.)  This does not exactly match the available bandwidth for
// our 7 available SF (SF=4 to SF=256) so we are going to be a little sloppy about that and
// just say that 256KB/s maps to SF=4, and so on down, so the bottom three tiers (1,2,4KB/s)
// all map to a SF=256 channel with a nominal bandwidth of 30kbs = 3.75KB/s 
class ChannelTree
{
	Mutex mChLock;				// channel choosing must be atomic.
	public:
	typedef int Tier;		// In the range 0..(sNumTiers-1) for SF=4 to SF=256.
							// Use 'int' because we have loops for (...; tier >= 0; tier--)
	static const int sNumTiers = 7;	// seven tree tiers for SF=4 to SF=256.

	private:
	ChannelTreeElt *mTree[sNumTiers];		// The tree itself is a pyramidal matrix.

	// These are internal functions of chChooseByTier and chReserve:
	bool isTierFreeUpward(Tier tier,unsigned chcode, bool checkOnlyReserved, Tier *badtier, unsigned *badcode);
	bool isTierFreeDownward(Tier tier,unsigned startcode, unsigned width, bool checkOnlyReserved, Tier *badtier, unsigned *badcode);
	void chConflict(Tier t1,unsigned ch1,Tier t2,unsigned ch2);
	DCHFEC *chChooseByTier(Tier tier);	// Choose a DCH specified by SF expressed as a Tier.

	public:
	ChannelTree();
	static unsigned sf2tier(unsigned sf);	// Return the tree tier for a given SF.
	static unsigned tier2sf(Tier tier);		// Return the SF for a tree tier (0..7).
	static Tier bw2tier(unsigned KBps, bool guaranteed);		// Return the tier needed for specified KBytes/s, approximately.
	DCHFEC *chChooseByBW(unsigned KBps);	// Choose one of the DCH channels by bandwidth in KBytes/s.
	DCHFEC *chChooseBySF(unsigned sf);		// Choose a DCH specified by SF.

	// Is this exact ch reserved?  This does not check above and below in the ChannelTree and is used
	// only to assert that we have correctly reserved a channel previously.
	bool isReserved(unsigned sf, unsigned code) { return mTree[sf2tier(sf)][code].mReserved; }

	// Permanently reserve the the specified channel in the tree for this channel.
	void chReserve(unsigned sf,unsigned chcode);
	// Populate the tree with DCH channels.
	// Call after reserving dedicated channels with chReserve()
	void chPopulate(ARFCNManager *downstream);

	void chTest(std::ostream &os);
	void chTestAlloc(int sf, int cnt, std::ostream &os);
	void chTestFree(int sf, int cnt, std::ostream &os);
	friend std::ostream& operator<<(std::ostream& os, const ChannelTree&);
};
std::ostream& operator<<(std::ostream& os, const ChannelTree&);

extern ChannelTree gChannelTree;	// And here it is.

};	// namespace UMTS
#endif
