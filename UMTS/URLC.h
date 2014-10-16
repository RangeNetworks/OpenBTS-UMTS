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

#ifndef URLC_H
#define URLC_H
#include "MemoryLeak.h"
#include "Utils.h"
#include "ByteVector.h"
#include "ScalarTypes.h"
#include "Threads.h"
#include "Interthread.h"
#include "GSMCommon.h"
#include "URRCTrCh.h"
#include "URRCRB.h"
#include "UMTSTransfer.h"
#include <list>
typedef GSM::Z100Timer Z100;

// Notes on this code:
// Pat Thompson 2/2012
//
// o The high end of RLC works on ByteVectors, but the low end of MAC (PHY) works on BitVectors.
// The conversion occurs on the way through MAC and RLC.
// The downlink from RLC->MAC is by ByteVector, and MAC does the conversion,
// which is more efficient because the MAC will know how many extra bits
// of header are required before it allocates the BitVector.
// The uplink MAC->RLC is by BitVector, and RLC does the conversion, which allows
// the RLC entitiy to throw the BitVector away without conversion if necessary.
// Note that TM allows transport block sizes that are not divisible by 8, which is irrelevant;
// in downlink the channel knows the exact size in bits, and in uplink, we are always
// passing full bytes anyway (either to the ASN decoder, or out to the internet)
// and the ByteVector in these cases has the final byte padded with 0 bits.
//
// o UMTS provides a huge set of RLC configuration parameters, and sadly,
// the example configurations, that we will probably follow, utilize a goodly set of them.
// As a way to reduce the complexity of the classes herein,
// the RLC Configuration classes are completely separated from the RLC engine classes.
// There is one configuration class for each type of engine class.
// Essentially, the code for channel setup is in the configuration classes, which
// are then invariant after RLC creation when the engine classes take over to process data.
//
// o There are two sets of RLC configuration structures - the ones in here and
// the ones in the RRC interface headers.  Why is that?
// The configuration parameters herein mostly follow the naming conventions
// of the RLC spec 25.322 sec 8: Primitives between RLC and Upper Layers,
// are segregated by the engine subsection (Am, Um, AmUm, or Tm) they control,
// and are designed for simplicity of use by the RLC engines.
// Some of the RLC working parameters are computed rather than specified.
// The RLC parameters specified in the RRC IEs in spec 25.331 are often wildly different,
// follow a bizarre layout that is designed for efficient transfer to the UE,
// and also vary based on the RRC release version.
// In fact, sometimes it is even hard to figure out what RRC param corresponds to what RLC param.
//
// o OK, in that case then why are there RLC config structures in the RRC header
// files at all, why didn't you just use ASN structures?  Several reasons.
// We need to specify the programming for Radio Bearers, then send
// that programming both to our own RLC/MAC/etc and to the UE.
// The programming examples in the specs (eg, 25.331 sec 13.7)
// use yet another syntax and naming convention that matches neither the RRC IE specs
// nor the RLC specs.  Rather than hand-code those examples each time (into what?),
// a process that I think would be extremely bug-prone,
// or create a bunch of flat functions to do it, I created classes with
// structure modelled closely on ASN, and added setup configuration methods
// matching the alternate syntax.
// Those structures are programmed using the setup functions and then become the
// master configuration, which can be converted to ASN to send to the UE or
// sent to this file to configure newly created RLC entities here.
//
// o For TM and UM the Trans and Recv classes can be allocated separately, in fact,
// the ubiquitous SRB0 uses TM uplink and UM downlink.
// However, the AM Trans and Recv classes are not allocated separately, you must
// allocate a single URlcAm entity which encompasses both Trans and Recv.
//
// o The URlcTransAmUm and URlcRecvAmUm engine classes, and their respective
// configuration classes, are not a strict union of the Am and Um classes.
// One of the biggest pieces of code here is the fillPduData function, which
// turns SDUs into PDUS and shares 95% functionality between the Am and Um cases.
// Rather than duplicating that huge function in the Am and Um code paths,
// it is placed in the TransAmUm class and has the functionality needed for both
// Am and Um paths, and I placed the configuration data needed by that function
// in the ConfigAmUm class, even though some of the configuration data is
// specific only to Am or Um mode.
//
// o For testing, use a dummy MAC that turns the vectors back around and sends
// them back up through RLC, then just send random vectors.

using namespace std;

namespace UMTS {
// URlcSN is intrinsically unsigned, but we often due arithmetic on them
// including subtraction, so use signed.
typedef Int_z URlcSN;	// A modulo mSNS number.
class UEInfo;
extern unsigned computeDlRlcSize(RBInfo *rb, RrcTfs *dltfs);
typedef void (*URlcHighSideFuncType)(ByteVector &sdu, RbId rbid);
void l2RlcStart();

// GSM 25.322 defines UMTS RLC.
// GSM 25.331 (RRC) see:
//		10.3.4.23 is the RLC setup message.
// 		10.3.4.21 is RB Mapping Info.
//			Specifies RLC sizes for each logical channel.
//		10.3.4.20 RB Information to setup.
//		10.3.5.23 Transport Format Set - this is where you configure
//			the RLC Size and the mapping of RB to logical channel (for RLC use)
//			and transport channel (for MAC use).
//			Note that there are multiple RLC sizes in this struct for different channels.


// 25.322 8.1 Parameters
// AM-DATA, UM-DATA TM-DATA, :
//	UE-ID type indicator: RNTI type, either U-RNTI or C-RNTI for this SDU.
//	DiscardReq
//	MUI - identifies SDU for use in confirmation or discard notification.
// AM-DATA only:
//	CNF - notify upper layers when SDU reception (ie, all PDUs making up the SDU) confirmed.
// General Config:
//	E/R	- Establishment, Re-establishment (na to TM mode), release or modification.
//	Stop (UM/AM)
//	Continue (UM/AM)
//	Ciphering Elements (UM/AM only)
//	SN_Delivery (UM only)
// AM_Parameters:
//		flexible PDU size
//		AMD PDU size or largest PDU size (depending on flexible size)
//		LI size
//		in-sequence delivery (to upper layers)
//		timer values
//		Use of a special value in HE field.
//		Protocol parameter values
//		Polling triggers
//		Status triggers
//		Periodic Status blocking configuration
//		SDU discard mode
//		Minimum WSN
//		Send MRW.
// (pat) These came up while coding:
//		HFN
// UM_Parameters:
//		Timer_Discard value
//		Alternative E-bit interpretation
//		largest UpLink UM PDU size
//		largest DownLoad RLC UM LI size
//		SN_delivery
//		There are more that are applicable only to the UE see paragraph 11.

static const int AmSNS = 4096;		// 12 bits wide
static const int UmSNS = 128;		// 7 bits wide



// This is the config as used by the URLC classes.
// The URLC Config is specified primarily in 25.322 8.2 Primitive Parameters,
// paragraph 7 for AM_parameters, paragraph 11 for UM_parameters,
// and paragraph 12 for TM_parameters.
// Parameters that are implemented in the common URlcTransAmUm or URlcRecvAmUm classes
// are defined in the ConfigCommon class even if they only apply to one mode.
struct URlcConfigAmUm
{
	// 25.331 RRC 10.3.4.25 Transmission RLC Discard IE
	///<@name RLC SDU Discard Mode 25.322 9.7.3
	// For TM and UM only TimerBasedWithoutExplicitSignaling, which is irrelevant
	// to us because our TM and UM implementation just ignores this field.
	// For AM the only AM mode we support is NoDiscard.
	// So this whole DiscardMode is currently irrelevant.
	//TransmissionRlcDiscard::SduDiscardMode mDiscardMode;
	TransmissionRlcDiscard mRlcDiscard;
	// For Am, the pdu size is specifed in the RlcInfo IE, and Um uses any of the
	// pdu sizes specified in the TransferFormatSet.
	// In REL-7, they added flexible pdu size to AM, which I think just makes it work like UM.
	// However, we are not going to support multiple PduSizes, so there is just one,
	// and here it is:
	// We dont use 'flexible size', so it is not even mentioned here.
	unsigned mDlPduSizeBytes;
	// The uplink and downlink LI size can be configured separately, several different ways.
	// AM mode is determined from the pduSize, or in REL-7, if flexiblePduSize is selected,
	// it is specified in the RlcInfo 25.331 10.3.4.23
	// For UM mode the downlink LISize is in RlcInfo, and the uplink LIsize is determined
	// indirectly from the largest PDU in the TransferFormatSet using rules
	// in 25.322 (RLC) 9.2.2.8 (Length Indicator)
	unsigned mUlLiSizeBytes, mUlLiSizeBits;
	unsigned mDlLiSizeBytes, mDlLiSizeBits;	// 9.2.2.8
	Bool_z mIsSharedRlc;	// This is the special RLC to run CCCH.

	URlcConfigAmUm(URlcMode rlcMode, URlcInfo &rlcInfo, RrcTfs &dltfs, unsigned dlPduSize);

	// These are UM-only parameters, but implemented in the URlcTransAmUm class, so they go here.
	struct {
		// This is post-REL-6 and is not in our ASN spec.
		// For AM mode, it should always be FALSE.
		Bool_z mSN_Delivery;	// This config option causes SN indications to be passed to higher layers
						// in UM mode, and further indicates not to concatenate SDUs in a PDU.
		// Must be FALSE for AM mode.
		Bool_z mAltEBitInterpretation;	// 9.2.2.5
	} UM;

	static const unsigned mMaxSduSize = 1502;
};
#if URLC_IMPLEMENTATION
URlcConfigAmUm::URlcConfigAmUm(URlcMode rlcMode, URlcInfo &rlcInfo, RrcTfs &dltfs, unsigned dlPduSize):
	mRlcDiscard(rlcInfo.mul.mTransmissionRlcDiscard)
{
	if (rlcMode == URlcModeAm) {
		//	We dont need this, and I dont think it is even defined in our REL of the ASN description.
		//if (mRlcPduSizeFlexible) {
		//	mDlPduSizeBytes = dltfs.getLargestPduSize();
		//	mUlLiSize = mDlLiSizeBits = rlcInfo.mdl.u.AM.mLengthIndicatorSize;
		//} else
		{
			// In the specs for the default RB configurations, the pdusize is sometimes specified
			// in the rlcInfo options, having been calculated by hand, but sometimes not.
			// We are going to figure out the RLC size directly
			// for AM and UM mode from the TB size and Mac header bits and pass it in
			// through the configuration constructors.
			//mDlPduSizeBytes = rlcInfo.mdl.u.AM.mDlRlcPduSize;
			mDlPduSizeBytes = dlPduSize;
			mUlLiSizeBits = mDlLiSizeBits = (mDlPduSizeBytes <= 126) ? 7 : 15;
			//mUlLiSizeBits = 7;
		}
	} else {
		mDlPduSizeBytes = dlPduSize;
		// TODO: Our version of ASN does not transmit mDlUmRlcLISize, and no one ever set it.
		// TODO: The RLC size should be determined from the TFS.
		//mDlLiSizeBits = rlcInfo.mdl.u.UM.mDlUmRlcLISize;
		// 8.6.4.9 of 25.331 says that Downlink UM LI is 7 bits unless explicitly indicated, regardless of PDU size
		mDlLiSizeBits = 7; //(mDlPduSizeBytes <= 125) ? 7 : 15;
		mUlLiSizeBits = (mDlPduSizeBytes <= 125) ? 7 : 15;
		//mUlLiSizeBits = 7;
	}
	switch (mDlLiSizeBits) {
		case 7: mDlLiSizeBytes = 1; break;
		case 15: mDlLiSizeBytes = 2; break;
		default: assert(0);
	}
	switch (mUlLiSizeBits) {
		case 7: mUlLiSizeBytes = 1; break;
		case 15: mUlLiSizeBytes = 2; break;
		default: assert(0);
	}
	//printf("mode=%s pdusize=%d dllibits=%d dllibytes=%d\n", rlcMode == URlcModeAm?"AM":"UM",
		//mDlPduSizeBytes,mDlLiSizeBits,mDlLiSizeBytes);
}
#endif

// 25.322 8.2 paragraph 7: Primitive parameters for AM.
// These are the primitive paramters as used by the RLC code.
// We create this struct when we create an RLC entity.
// The parameter names in the RLC spec do not match 25.321 RRC, and their configured values
// come from several places, including 10.4.3.23 RlcInfo, or may be computed from the
// TransportSet, for example the Max RLC pdu size.
// Therefore we will copy the subset of parameters that we actually use from the RRC IEs
// into this structure when we create an RLC entity.
struct URlcConfigAm : public /*virtual*/ URlcConfigAmUm	// AM only config stuff.
{
	bool mInSequenceDeliveryIndication;
	///<@name RLC Protocol Parameters, 3GPP 25.322 9.6
	unsigned mMaxDAT;
	//unsigned mPoll_Window;
	unsigned mMaxRST;	// Max-1 number of RESET PDUs, upper limit for VTRST
	unsigned mConfigured_Tx_Window_Size;
	unsigned mConfigured_Rx_Window_Size;
	//unsigned mMaxMRW;		// unimplemented
	///<@name RLC Timer values 25.322 9.5
	// Timer_Poll, Timer_Poll_Prohibit, Timer_Poll_Periodic - see PollingInfo
	// mTimer_Discard;	Not implemented yet, but we probably want it.
	// Timer_Status_Prohibit, Timer_Status_Periodic  - see RLC Status Triggers.
	unsigned mTimerRSTValue;	// 11.4.2
	// Timer_MRW not implemented.
	///<@name RLC Polling Triggers 25.322 9.7.1
	RrcPollingInfo mPoll;
	///<@name RLC Status Triggers 25.322 9.7.2
	bool mStatusDetectionOfMissingPDU;

	// TODO: mTimer_Status_Periodic
	///<@name RLC Periodical Status Blocking 25.322 9.7.2
	// TODO: mTimer_Status_Prohibit TODO
	///<@name RLC Minimum WSN 25.322 9.2.2.11.3
	// WONT DO: unsigned mMinWSN;
	///<@name RLC Send MRW.
	// WONT DO: unsigned mSendMRW;

	//unsigned mHFN;		

	URlcConfigAm(URlcInfo &rlcInfo, RrcTfs &dltfs, unsigned dlPduSize);
};
#if URLC_IMPLEMENTATION
URlcConfigAm::URlcConfigAm(URlcInfo &rlcInfo, RrcTfs &dltfs, unsigned dlPduSize) :
		URlcConfigAmUm(URlcModeAm,rlcInfo,dltfs,dlPduSize),
		mInSequenceDeliveryIndication(rlcInfo.mdl.u.AM.mInSequenceDelivery),
		mMaxDAT(rlcInfo.mul.mTransmissionRlcDiscard.mMaxDAT),
		mMaxRST(rlcInfo.mul.u.AM.mMaxRST),
		mConfigured_Tx_Window_Size(rlcInfo.mdl.u.AM.mReceivingWindowSize),
		mConfigured_Rx_Window_Size(rlcInfo.mul.u.AM.mTransmissionWindowSize),
		mTimerRSTValue(rlcInfo.mul.u.AM.mTimerRST),
		mPoll(rlcInfo.mul.u.AM.mPollingInfo),
		mStatusDetectionOfMissingPDU(rlcInfo.mdl.u.AM.mDownlinkRlcStatusInfo.mMissingPduIndicator)
		{}
#endif

// This contains both uplink and downlink config, although it is possible
// to use one without the other.
struct URlcConfigUm : public /*virtual*/ URlcConfigAmUm
{
	//unsigned largestUlUmdPduSize // 9.2.2.8  Computed from Transport Set  See mPduSize.
#if RLC_OUT_OF_SEQ_OPTIONS
	bool mOSD;	// Out of Sequence delivery.	// 11.2.3
	bool mOSR;	// Out of Sequence reception.	// 11.2.3.1

	//bool mUseOSD;	// UM downlink only, use out-of-sequence-sdu-delivery
	//unsigned mOSD_Window_Size;	// UM downlink only, only if UseOSD
	//Z100 mTimerOSD 	// UM downlink only, only if UseOSD, to delete stored PDUs see 11.2.3.2
	//bool SN_Delivery;	// REL-7 and up.  Used in OSD mode to indicate SDU SN to higher layers.
#endif

	// I dont understand why there are both Configured_Rx_Window_Size and OSD_Window_Size.
	// I think they are the same thing.
	//unsigned mConfigured_Rx_Window_Size;	// In UM only needed if UseOSD.
	//unsigned mOSD_Window_Size;	// UM downlink only, only if UseOSD.
	//bool mUseDAR;		// UM uplink only.
	//unsigned mDAR_Window_Size;  // UM uplink only, only if DAR.
	//Z100 mTimerDAR;  // UM uplink only, only if DAR.
	URlcConfigUm(URlcInfo &rlcInfo, RrcTfs &dltfs, unsigned dlPduSize) :
		URlcConfigAmUm(URlcModeUm, rlcInfo, dltfs, dlPduSize)
		// More To Do
	{}
};

class URlcBase
{
	// 25.322 9.7.6 RLC Stop mode.
	// The RLC_STOP mode is supposed to block the RLC at the low end, both incoming and outgoing,
	// but continues to accept SDUs at the high end.
	// This functionality is needed because of the dorky way they do the cell state transition
	// from CELL_FACH to CELL_DCH.
	// I only implemented outgoing, then stopped - stopping incoming would need to a special queue
	// and doesnt make any sense any way becaues the corresponding RLC entity in the UE
	// has already changed its state, so we need to process the incoming PDUs.
	public:
	enum RlcState {
		RLC_RUN,
		RLC_STOP,
		RLC_SUSPEND	// Not supported.
	} mRlcState;

	void rlcStop() { mRlcState = RLC_STOP; }
	void rlcResume() { mRlcState = RLC_RUN; }

	enum SufiType {	// Used for AM only.
		SUFI_NO_MORE = 0,
		SUFI_WINDOW = 1,
		SUFI_ACK = 2,
		SUFI_LIST = 3,
		SUFI_BITMAP = 4,
		SUFI_RLIST = 5,
		SUFI_MRW = 6, // Note: we dont need to use the MRW, implementing reset is sufficient.
		SUFI_MRW_ACK = 7,
		SUFI_POLL = 8
	};
	enum PduType {	// Used for AM only.
		PDUTYPE_STATUS = 0,
		PDUTYPE_RESET = 1,
		PDUTYPE_RESET_ACK = 2
	};

	URlcMode mRlcMode;

	unsigned mSNS;	// The Sequence Number Space for this RLC entity.

	UEInfo *mUep;	// The UE that owns us.  May be NULL for RLC for CCCH .
	RbId mrbid;	// The RadioBearer that we were created for.

	URlcBase(URlcMode wRlcMode, UEInfo *wUep, RBInfo *wRbp);

	// This initializer should never be called because we are a virtual class,
	// so only the most most derived (aka final) class needs an initializer,
	// and they are always provided.
	URlcBase() { assert(0); }

	// Modulo mSNS arithmetic functions.
	int deltaSN(URlcSN sn1, URlcSN sn2);
	URlcSN addSN(URlcSN sn1, URlcSN sn2);
	URlcSN minSN(URlcSN sn1, URlcSN sn2);
	URlcSN maxSN(URlcSN sn1, URlcSN sn2);
	void incSN(URlcSN &psn);
	// Currently this is used only for debug messages:
	virtual unsigned getRlcHeaderSize() { return 0; }	// If not over-ridden, return 0.
};
#if URLC_IMPLEMENTATION
	URlcBase::URlcBase(URlcMode wRlcMode, UEInfo *wUep, RBInfo *wRbp) :
		mRlcState(RLC_RUN), mRlcMode(wRlcMode), mUep(wUep), mrbid(wRbp->mRbId)
	{
		switch (wRlcMode) {
			case URlcModeTm: mSNS = 0; break;
			case URlcModeUm: mSNS = UmSNS; break;
			case URlcModeAm: mSNS = AmSNS; break;
			default: assert(0);
		}
	}
#endif


DEFINE_MEMORY_LEAK_DETECTOR_CLASS(URlcPdu,MemCheckURlcPdu)
// All purpose pdu between rlc and mac.
// Note that only TM (transparent mode) is allowed to be non-byte aligned.
class URlcBasePdu : public ByteVector, public MemCheckURlcPdu
{
	public:
	string mDescr;		// For debugging, description of content.
	URlcBasePdu(unsigned size, string &wDescr);
	URlcBasePdu(ByteVector &other, string &wDescr);
	URlcBasePdu(const BitVector &bits, string &wDescr);
};
#if URLC_IMPLEMENTATION
	URlcBasePdu::URlcBasePdu(ByteVector &other, string &wDescr) : ByteVector(other), mDescr(wDescr) {}
	URlcBasePdu::URlcBasePdu(unsigned size, string &wDescr) : ByteVector(size), mDescr(wDescr) {}
	URlcBasePdu::URlcBasePdu(const BitVector &bits, string &wDescr) :  ByteVector(bits), mDescr(wDescr) {}
#endif

DEFINE_MEMORY_LEAK_DETECTOR_CLASS(URlcDownSdu,MemCheckURlcDownSdu)
// The Downlink SDU takes possession of the ByteVector in the most efficient way.
// Note that there is no "priority" sent with the SDU because there are
// different RBs for different purposes and the priority is implicit
// in the RLC entity on which this is sent.
//struct URlcDownSdu : ByteVector, public MemCheckURlcDownSdu
struct URlcDownSdu : public URlcBasePdu
{
	ByteVector *sduData() { return this; }
	bool mDiscarded;	// Set if one or more SDUs were discarded at this SDU position.
	bool mDiscardReq;	// Discard request from upper layer.
	unsigned mMUI;		// SDU identifier, aka Message Unit Identifier.
	string mDescr;

	URlcDownSdu *mNext;	// The SDU can be placed in a SingleLinkedList
	URlcDownSdu *next() { return mNext; }
	void setNext(URlcDownSdu *next) { mNext = next; }


	URlcDownSdu(ByteVector &wData, bool wDR, unsigned wMUI, string wDescr) :
		URlcBasePdu(wData,wDescr), mDiscarded(0), mDiscardReq(wDR),
		mMUI(wMUI), mNext(0)
		{}

	// This class is always used by pointer and manually deleted, so no copy constructor
	// is needed.
	//void free() { if (mData) { delete mData; } delete this; }
	void free() { delete this; }
	size_t size() { return mDiscarded ? 0 : ByteVector::size(); }
};

// The Uplink SDU is just a ByteVector.
typedef ByteVector URlcUpSdu;

// May be any mode, and for AM, may be data or control pdu.
// PDU for UM and AM mode that use a common transmitter for reasons documented above,
// so we use the same PDU also.
// UM PDU:
//		SN:7;		// Used for reassembly
//		E:1
//		LI:7 or 15;
//		E:1;	// Meaning configured by upper layers.  Can be "normal" (below) or
//				// Alternative: 0 => next field is a complete SDU which
//				// is not segmented concatenated or padded
//				// 1=> next field is length indicator+E bit.
//		...
//		Data
//		Optional PAD
//class URlcPdu : public ByteVector, public Text2Str, public MemCheckURlcPdu
class URlcPdu : public URlcBasePdu
{	public:
	URlcBase *mOwner;	// The TM, UM or AM entity that owns this pdu.

	// For UM and AM data pdus:
	unsigned mPaddingStart;	// Location of padding or 0, used for piggybacked status.
	unsigned mPaddingLILocation;	// Location of LI indicator for padding, or 0.

	unsigned mVTDAT;	// For AM only, how many times PDU has been scheduled.
	bool mNacked;		// true if pdu has been negatively acknowledged.

	// Fields so this can be placed in a SingleLinkList:
	URlcPdu *mNext;	// The SDU can be placed in a SingleLinkedList
	URlcPdu *next() { return mNext; }
	void setNext(URlcPdu *next) { mNext = next; }

	URlcPdu(unsigned wSize, URlcBase *wOwner,string wDescr);
	URlcPdu(const BitVector &bits, URlcBase *wOwner,string wDescr);
	explicit URlcPdu(URlcPdu *other);

	// UM PDU fields:
	void setUmE(unsigned ebit) { setBit(7,ebit); }
	unsigned getUmE() const { return getBit(7); }
	void setUmSN(unsigned sn) { setField(0,sn,7); }
	unsigned getUmSN() const { return getField(0,7); }

	// AM PDU fields:
	void setAmSN(unsigned sn) { setField2(0,1,sn,12); }
	unsigned getAmSN() const { return getField2(0,1,12); }
	void setAmDC(bool isData) { setField2(0,0,isData,1); }
	int getAmDC() const { return getBit(0); }
	static const int sPollBit = 13;
	void setAmP(bool P) { setField2(1,(sPollBit-8),P,1); }
	unsigned getAmP(bool P) const { return getField2(1,(sPollBit-8),1); }
	unsigned getAmHE() const { return getField2(1,6,2); }	// but only the bottom bit of HE is used.
	void setAmHE(unsigned HE) { setField2(1,6,HE,2); }

	// Common functions:
	enum URlcMode rlcMode() const { return mOwner->mRlcMode; }
	void setSN(unsigned sn) { if (rlcMode() == URlcModeUm) setUmSN(sn); else setAmSN(sn); }
	unsigned getSN() const { return (rlcMode() == URlcModeUm) ? getUmSN() : getAmSN(); }
	void setEIndicator(bool ebit) { if (rlcMode() == URlcModeUm) setUmE(ebit); else setAmHE(ebit); }
	bool getEIndicator() const { return (rlcMode() == URlcModeUm) ? getUmE() : getAmHE(); }

	// For UM and AM data pdus:
	// E==1 implies there is a following LI+E (opposite of GPRS E definition.)
	void appendLIandE(unsigned licnt, unsigned E, unsigned lisize);

	// For debugging:
	unsigned getPayloadSize() const {
		return (mPaddingStart ? mPaddingStart : size()) - mOwner->getRlcHeaderSize();
	}
	void text(std::ostream &os) const;
};
#if URLC_IMPLEMENTATION
	URlcPdu::URlcPdu(unsigned wSize, URlcBase *wOwner, string wDescr)
		: URlcBasePdu(wSize,wDescr), mOwner(wOwner),
		mPaddingStart(0), mPaddingLILocation(0),
		mVTDAT(0), mNacked(0), mNext(0)
		{}

	URlcPdu::URlcPdu(const BitVector &bits, URlcBase *wOwner, string wDescr)
		: URlcBasePdu(bits, wDescr), mOwner(wOwner),
		mPaddingStart(0), mPaddingLILocation(0),
		mVTDAT(0), mNacked(0), mNext(0)
		{}
	URlcPdu::URlcPdu(URlcPdu *other)
		: URlcBasePdu(*other,other->mDescr), mOwner(other->mOwner),
		mPaddingStart(other->mPaddingStart),
		mPaddingLILocation(other->mPaddingLILocation),
		mVTDAT(other->mVTDAT), mNacked(other->mNacked), mNext(0)
		{}
	//URlcPdu::URlcPdu(ByteVector *other, string wDescr)	// Used to manufacture URlcPdu from URlcDownSdu for RLC-TM.
	//	: ByteVector(*other), mOwner(0), mDescr(wDescr),
	//	mPaddingStart(0),
	//	mPaddingLILocation(0),
	//	mVTDAT(0), mNacked(0), mNext(0)
	//	{}
#endif

//class URlcPduUm : public URlcPdu
//{	public:
//	URlcPduUm(unsigned wsize,unsigned wlisize) : URlcPdu(wsize,wlisize) {}
//	void setSN(unsigned sn) { setField(0,sn,7); }
//	unsigned getSN() { return getField(0,7); }
//};

// AM PDU:
//		DC:1;	1 for data pdu, 0 for control pdu
//		SN:12;		// Used for retransmission.
//		P:1;		// Polling bit: 1=> request a status report.
//		HE:2;		// Header Extension Type.
//					// 0 => succeeding octet contains data
//					// 1 => succeeding octet contains LI+E
//					// 2 => succeeding octet contains data and the last octet
//					//	of the PDU is last octet of SDU, but only if 
//					// "Use special value of HE field" is configured.
//					// 3 => reserved.
//		LI:7 or 15;	// Has special encoding, see 9.2.2.8
//		E:1;	// Always "Normal" E-bit interpretation, whcih is:
//				// 0 => next field is data, status or padding;
//				// 1=> next field is another length indicator + E bit.
//		...
//		Data
// 		Optional Piggybacked STATUS PDU.
//		Optional PAD
//class URlcPduAm : public URlcPdu
//{
//	void setSN(unsigned sn) { setField2(0,1,sn,12); }
//	unsigned getSN() { return getField2(0,1,12); }
//	void setDC(bool isData) { setField2(0,0,isData,1); }
//	void setP(bool P) { setField2(0,5,P,1); }
//	unsigned getP(bool P) { return getField2(0,5,1); }
//	unsigned getHE(unsigned HE) { return getField2(0,6,2); }
//	void setHE(unsigned HE) { setField2(0,6,HE,2); }
//	void setE(bool val) { setHE(val ? 1 : 0); }
//};

// AM Status PDU:  Size bounded by maximum RLC PDU size used by the logical channel.
//		DC:1;	0 for control pdu
//		PDUType:3;	// 0 = STATUS pdu type
//		SUFI(1) 	//SUFI defined 9.2.2.11
//		...
//		SUFI(k)
//		PAD	 to byte boundary, or if fixed size logical channel, to that PDU size.
//	RESET and RESET ACK PDU:
//		DC:1;
//		PDUType:3; 	//1 = RESET, 2 = RESET ACK
//		RSN:1;		// 9.2.2.13.  Dont understand. Indicates RESET is resent?
//		R1:3;		// Byte alignment, always 0.
//		HFNI:20;		// Indicates the Hyper Frame Number?
//		PAD

typedef SingleLinkList<URlcDownSdu> SduList_t;
typedef InterthreadQueue<URlcPdu, SingleLinkList<URlcPdu> > PduList_t;

// Any mode transmitter
class URlcTrans : public virtual URlcBase
{
	friend class URlcTransTm;
	friend class URlcTransAm;
	friend class URlcTransUm;
	friend class URlcTransAmUm;

	// The SduTxQ consists of the complete vectors in the list, plus mSplitSdu,
	// which is used only for UM and AM modes to save the SDU currently being processed.
	Mutex mQLock;		// Lock for SduTxQ.
	SduList_t mSduTxQ;
	URlcDownSdu *mSplitSdu;

	// mVTSDU incremeted for every SDU transmission when the first SDU segment
	// is scheduled to be transmitted the first time.
	// When == Poll_SDU, send a poll and set this to 0.
	// Used when for Am mode "poll every Poll_SDU" is configured.
	unsigned mVTSDU;

	// TODO: This count will be off if an sdu was deleted, because
	// am empty place-holder is left in the sduq.  Not worth worrying about now.
	unsigned getSduCnt();
	virtual bool pdusFinished() = 0;
	unsigned rlcGetSduQBytesAvail();

	// If exceeded we have to throw away some SDUs.
	// Where is this in the spec?
	unsigned mTransmissionBufferSizeBytes;
	string mRlcid;

	public:
	URlcTrans();

	virtual unsigned rlcGetBytesAvail() = 0;

	// Higher layer sends something to RLC. Same function for all modes:
	// put in the queue, but check for overflow.
	void rlcWriteHighSide(ByteVector &sdu, bool DR, unsigned MUI, string descr);

	// The mutex lock for both of these is in URlcTransAm::readLowSidePdu()
	virtual void rlcPullLowSide(unsigned amt) = 0;
	virtual URlcBasePdu *rlcReadLowSide() = 0;

	// There is no guarantee that all the PDUs are the same size when in TM.
	// These functions that interact only with the mPduOutQ do not need
	// further mutex protection beyond what the Q provides.
	virtual unsigned rlcGetPduCnt() = 0;
	virtual unsigned rlcGetFirstPduSizeBits() = 0;
	virtual unsigned rlcGetDlPduSizeBytes() { return 0; }	// Not defined for RLC-TM, so return 0.

	virtual void triggerReset() { }
	void textTrans(std::ostream &os);
	const char *rlcid() { return mRlcid.c_str(); }
	virtual void text(std::ostream &os) = 0;
};
#if URLC_IMPLEMENTATION
	URlcTrans::URlcTrans() : mSplitSdu(0), mVTSDU(0) {
		mTransmissionBufferSizeBytes = gConfig.getNum("UMTS.RLC.TransmissionBufferSize");
	}
	unsigned URlcTrans::rlcGetSduQBytesAvail() {
		ScopedLock lock(mQLock);
		unsigned partialsize = mSplitSdu ? mSplitSdu->size() : 0;
		return mSduTxQ.totalSize() +  partialsize;
	}
	unsigned URlcTrans::getSduCnt() {
		ScopedLock lock(mQLock);	// extra cautious
		return mSduTxQ.size() + (mSplitSdu?1:0);
	}
#endif

class URlcTransTm :
	public virtual URlcBase, public URlcTrans
{	public:
	URlcTransTm(RBInfo *rbInfo, UEInfo *uep) :
		URlcBase(URlcModeTm,uep,rbInfo)
		{}
	// There is only an sduq, not a pduq, and reading a pdu comes straight from the sduq.
	void rlcPullLowSide(unsigned amt) {}
	unsigned rlcGetPduCnt() { return getSduCnt(); }
	unsigned rlcGetFirstPduSizeBits();
	unsigned rlcGetBytesAvail();

	// Just turn the SDU into a PDU and send it along.
	URlcBasePdu *rlcReadLowSide();
	void text(std::ostream &os) { textTrans(os); }
	bool pdusFinished() { assert(mSplitSdu == NULL); return getSduCnt() == 0; }
};
#if URLC_IMPLEMENTATION
	unsigned URlcTransTm::rlcGetFirstPduSizeBits() {
		ScopedLock lock(mQLock);
		if (mRlcState == RLC_STOP) {return 0;}
		URlcDownSdu *sdu;
		for (sdu = mSduTxQ.front(); sdu; sdu = sdu->next()) {
			if (sdu->mDiscarded) { continue; }		// Ignoring deleted PDUs.
			return sdu->sizeBits();
		}
		return 0;
	}

	unsigned URlcTransTm::rlcGetBytesAvail() {
		if (mRlcState == RLC_STOP) {return 0;}
		return rlcGetSduQBytesAvail();
	}
#endif

class URlcTransAmUm : // Transmit common to AM and UM modes.
	public URlcTrans, public virtual URlcBase
{
	friend class URlcTransAm;
	friend class URlcTransUm;

	URlcConfigAmUm *mConfig;

	PduList_t mPduOutQ;

	// For Am and Um modes:
	int mLILeftOver;	// Special case flag carried over from previous PDU.
			// 1 => the previous sdu exactly filled the previous pdu.
			// 2 => the previous sdu was one byte short of filling previous pdu.

	// These vars are required for Am mode but we maintain them for all modes.
	unsigned mVTPDU;	// Used when for Am mode "poll every Poll_PDU" is configured.
			// Incremented for every PDU transmission of any kind.
			// When == Poll_PDU, send a poll and set this to 0.
			// mVTPDU is the absolute PDU count, not modulo arithmetic.


	bool fillPduData(URlcPdu *pdu, unsigned pduHeaderSize,bool*newPdu);		// Fill the pdu with sdu data.

	void transDoReset();

	virtual URlcPdu *readLowSidePdu() = 0;

	unsigned rlcGetBytesAvail();

	// Pull data through the RLC to fill the output queue, up to the specified amt,
	// which is the maximum amount needed for any Transport Format.
	void rlcPullLowSide(unsigned amt);
	unsigned rlcGetPduCnt() { return mPduOutQ.size(); }
	bool pdusFinished();

	public:
	// This class is not allocated alone; it is part of URlcTransAm or URlcTransUm.
	URlcTransAmUm(URlcConfigAmUm *wConfig) :
		mConfig(wConfig)
		{ transDoReset(); }

	// MAC reads the low side with this.
	URlcBasePdu *rlcReadLowSide();

	// Return the size of the top PDU, or 0 if none.
	unsigned rlcGetFirstPduSizeBits();
	void textAmUm(std::ostream &os);
};
#if URLC_IMPLEMENTATION
	unsigned URlcTransAmUm::rlcGetBytesAvail() {
		if (mRlcState == RLC_STOP) {return 0;}
		// Can the pdus move from one queue to the other in between the locks?
		return rlcGetSduQBytesAvail() + mPduOutQ.totalSize();
	}

	// Return the size of the top PDU, or 0 if none.
	unsigned URlcTransAmUm::rlcGetFirstPduSizeBits() {
		if (mRlcState == RLC_STOP) {return 0;}
		ByteVector *pdu = mPduOutQ.front();
		return pdu ? pdu->sizeBits() : 0;
	}
	// If mLILeftOver is non-zero then we still need to send another PDU.
	bool URlcTransAmUm::pdusFinished() { return getSduCnt() == 0 && !mLILeftOver; }
#endif

// Any mode receiver.
class URlcRecv :
	public virtual URlcBase
{
	friend class URlcRecvTm;
	friend class URlcRecvAm;
	friend class URlcRecvUm;
	friend class URlcRecvAmUm;

	URlcHighSideFuncType mHighSideFunc;
	string mRlcid;

	// This is where outgoing SDUs arrive.
	// The SDU is allocated and must be deleted eventually.
	void rlcSendHighSide(URlcUpSdu *sdu);

	public:
	// This is where pdus come in from the MAC via a routine in the UEInfo
	// to map to the approriate RLC entity based on the RbId.
	virtual void rlcWriteLowSide(const BitVector &pdu) = 0;

	// This is used for testing.
	void rlcSetHighSide(URlcHighSideFuncType wHighSideFunc) { mHighSideFunc = wHighSideFunc; }

	URlcRecv() : mHighSideFunc(0) {}
	const char *rlcid() { return mRlcid.c_str(); }
	virtual void text(std::ostream &os) = 0;
};

class URlcRecvTm :
	public virtual URlcBase, public URlcRecv
{	public:
	URlcRecvTm(RBInfo *rbInfo, UEInfo *uep);
	void rlcWriteLowSide(const BitVector &pdu);
	void text(std::ostream &os) {}
};
#if URLC_IMPLEMENTATION
	URlcRecvTm::URlcRecvTm(RBInfo *rbInfo, UEInfo *uep) :
		URlcBase(URlcModeTm,uep,rbInfo)
		{}
	// TM Messages just pass through.
	void URlcRecvTm::rlcWriteLowSide(const BitVector &pdu) {
		URlcUpSdu *sdu = new ByteVector((pdu.size() + 7)/8);
		pdu.pack(sdu->begin());
		rlcSendHighSide(sdu);
	}
#endif

class URlcRecvAmUm : // Receive common to AM and UM modes.
	public URlcRecv, public virtual URlcBase
{
	friend class URlcRecvAm;
	friend class URlcRecvUm;

	URlcConfigAmUm *mConfig;
	URlcUpSdu *mUpSdu;	// Partial SDU being assembled, or NULL.
	void sendSdu();						// Enqueue a completed SDU.

	bool mLostPdu;	// This is UM only, but easier to put in this class.
					// It is set only in UM mode to indicate a lost pdu,
					// so we have to continue to discard until we find the start of a new sdu.


	void addUpSdu(ByteVector &payload);	// Add to partially assembled SDU.
	void discardPartialSdu();
	void ChopOneByteOffSdu(ByteVector &payload);
	void parsePduData(URlcPdu &pdu, int headersize, bool Eindicator, bool statusOnly);

	void recvDoReset() { discardPartialSdu(); mLostPdu = false; }

	public:
	// This class is not allocated alone; it is part of URlcRecvAm or URlcRecvUm.
	URlcRecvAmUm(URlcConfigAmUm *wConfig) : mConfig(wConfig), mUpSdu(0) {}
	URlcRecvAmUm(): mUpSdu(0) {}
	void textAmUm(std::ostream &os);
};

class URlcAm;
class URlcRecvAm;

class URlcTransAm :
	public URlcTransAmUm	// UMTS RLC Acknowledged Mode Transmitter
{
	friend class URlcAm;
	friend class URlcRecvAm;
	friend class URlcTransAmUm;
	friend class URlcRecvAmUm;

	URlcConfigAm *mConfig;
	URlcPdu *readLowSidePdu();
	URlcPdu *readLowSidePdu2();

	// GSM25.322 9.4: AM Send State Variables
	URlcSN mVTS;	// SN of next AMD PDU to be transmitted for the first time.
	URlcSN mVTA;	// SN+1 of last in-sequence positively acknowledged pdu.
					// This is set by an incoming Ack SUFI 9.2.2.11.2

	// SN of upper edge of transmission window = VTA + VTWS.
	URlcSN VTMS()	{ return addSN(mVTA,mVTWS); }

	UInt_z mVTRST;	// Count number of RESET PDU sent before ack received.  See 11.4.2 and 11.4.5.1.
	// We will not use MRW.
	// unsigned mVTMRW;	// Count number of MRW command transmitted.
	URlcSN mVTWS;	// Window size.  Init to Configured_Tx_Window_size.
			// (pat) The window size is how many unacked blocks you can send before stalling.
			// Warning will robinson: it can theoretically be set up to SN-1,
			// which if allowed would cause deltaSN(), etc to fail.

	Z100 mTimer_Poll;		// Set when a poll is sent, and stopped when the poll is answered.
	URlcSN mTimer_Poll_VTS;	// Described in 25.322 9.5 paragraph a.
	//unsigned mTimer_Poll_SN;	// The value of VTS at the time poll was sent; timer is
			// stopped if ack is received for this and preceding SN.
	Z100 mTimer_Poll_Prohibit;
	//Z100 mTimer_Discard;			unimplemented
	Z100 mTimer_Poll_Periodic;	// How often to poll.
	//Z100 mTimer_Status_Prohibit;	// How often to send unsolicited (unpolled) status reports.
	//Z100 mTimer_Status_Periodic;	unimplemented
	Z100 mTimer_RST; // start when RESET PDU sent, stop when RESET ACK received.
					// Upon expiry, resend RESET PDU with same RSN
	//Z100 Timer_MRW - Resend MRW SUFI when expired.  We wont use MRW; it is unneeded.

	//URlcRecvAm *mRecv;	// Pointer to the paired receiving entity.

	URlcPdu* mPduTxQ[AmSNS];		// PDU array, saved for possible retransmission.
									// Note that only data pdus go in here, not control.

	// Variables pat added:
	bool mNackedBlocksWaiting;	// True if mNackVS is valid.
	URlcSN mVSNack;		// Next nacked block to be retransmitted.

	bool mPollTriggered;
	bool mStatusTriggered;
	bool mResetTriggered;	// This just triggers it.  An in-progress reset is indicated by mTimer_RST.active()
	bool resetInProgress() { return mTimer_RST.active(); }
	unsigned mResetTransRSN;		// RSN value we sent in our last reset pdu.
	unsigned mResetAckRSN;			// RSN value of last received reset pdu, saved to put in RESET_ACK message.
	//unsigned mResetTransCount;		// Total Number of resets transmitted.
	unsigned mResetRecvCount;		// Total Number of resets received, ever.
	//unsigned mResetAckRecvCount;	// Total number of reset ack received.
	bool mSendResetAck;

	unsigned mVTPDUPollTrigger;		// Next trigger for a poll if Poll_PDU option, or 0.
	unsigned mVTSDUPollTrigger;		// Next trigger for a poll if Poll_SDU option, or 0.

	public:
	void transAmReset();	// Happens whenever we get a reset PDU.
	void transAmInit();		// Happens once.
	private:
	URlcAm*parent();
	URlcRecvAm*receiver();

	bool stalled();
	void setNAck(URlcSN sn);	// Set the nack indicator for queued block with this sequence number.
	URlcPdu *getDataPdu();
	URlcPdu *getResetPdu(PduType type);
	URlcPdu *getStatusPdu();
	void advanceVTA(URlcSN newvta);
	void advanceVS(bool);
	void processSUFIs(ByteVector *vec);
	void processSUFIs2(ByteVector *vec, size_t rp);
	bool IsPollTriggered();
	unsigned rlcGetDlPduSizeBytes() { return mConfig->mDlPduSizeBytes; }

	public:
	// This class is not allocated alone; it is part of URlcAm.
	URlcTransAm(URlcConfigAm *wConfig) :
		URlcTransAmUm(wConfig),
		mConfig(wConfig)
		{
			mRlcid = format("AMT%d",mrbid);
		}
	void text(std::ostream &os);
	void triggerReset() { mResetTriggered = true; }	// for testing
};

class URlcRecvAm : // UMTS RLC Acknowledged Mode Receiver
	public URlcRecvAmUm
{
	friend class URlcAm;
	friend class URlcTransAm;
	friend class URlcRecvAmUm;

	URlcConfigAm *mConfig;
	// This class is not allocated alone; it is part of URlcAm.

	//URlcTransAm *mTrans;	// Pointer to the paired transmitting entity.

	// GSM25.322 9.4: AM Receive State Variables.
	// The range from mVRR to mVRH is what we need to acknowledge to the peer.
	// The LSN sent in the acknowledgment sufi is in the range VRR <= LSN <= VRH.
	URlcSN mVRR;	// SN of the "last" in-sequence PDU received + 1, meaning
					// that SN+1 is the first PDU not yet received.
	URlcSN mVRH;	// SN+1 of any PDU received or identified to be missing.
			// See 9.4 how to set it.  A PDU is "identified to be missing"
			// by the POLL SUFI, which sends the VTS from the peer entity.

	// If the PDU size is small we may not be able to fit all the missing
	// blocks in a single status report.  If you only send the oldest
	// status report over and over again, and there is a high PDU loss rate,
	// the total throughput is very slow, especially if the PDU containing
	// the poll bit is lost causing a wait for the mTimerPoll to expire for
	// each transaciton.  To fix that, if we have nothing else to send,
	// continue to send status reports until we have reported all the missing blocks.
	// This variable tells us where we are in the status reports.
	URlcSN mStatusSN;

	URlcSN VRMR() {
		// Maximum acceptable VRR: VRR + Configured_Rx_Window_size
		return addSN(mVRR,mConfig->mConfigured_Rx_Window_Size);
	}

	URlcPdu *mPduRxQ[AmSNS];		// PDU array for reassembly.
	// 11.4.3: Reception of RESET PDU resets all state variables to initial values except VTRST.
	public:
	void recvAmReset();	// Happens whenever we get a RESET PDU.
	private:
	void recvAmInit();	// Happens once.
	URlcAm*parent();
	URlcTransAm*transmitter();
	bool addAckNack(URlcPdu *pdu);
	bool isReceiverOk();

	public:
	URlcRecvAm(URlcConfigAm *wConfig) : URlcRecvAmUm(wConfig), mConfig(wConfig) {
		mRlcid = format("AMR%d",mrbid);
	}

	void rlcWriteLowSide(const BitVector &pdu);
	void text(std::ostream &os);
};

class URlcAm : public URlcTransAm, public URlcRecvAm	// UMTS RLC Acknowledged Mode Entity
{
	friend class URlcRecvAm;
	friend class URlcTransAm;

	void recvResetPdu(URlcPdu*pdu);
	void recvResetAck(URlcPdu*pdu);
	unsigned getRlcHeaderSize() { return 2; }

	URlcConfigAm mConfig;	// The one and only config for the entire class hierarchy.

	// Need a mutex only for RLC-AM.  The MAC pulls data from the transmitter
	// in one thread, and the receiver may be driven asynchronously
	// from the FEC classes by another thread, and status pdus may cause
	// activity in both transmitter and receiver, so need a lock.
	Mutex mAmLock;

	string mAmid;
	const char *rlcid() { return mAmid.c_str(); }

	public:
	URlcAm(RBInfo *rbInfo,RrcTfs *dltfs,UEInfo *uep,unsigned dlPduSize);
	// See 9.2.1.7 and 9.2.2.14
	// HFN defined in 25.331 8.5.8 - 8.5.10, for RRC Message Integrity Protection.
	UInt_z mULHFN;	// Security stuff.
	UInt_z mDLHFN;

	//URlcTransAm *transmitter() { return static_cast<URlcTransAm*>(this); }
	//URlcRecvAm *receiver() { return static_cast<URlcRecvAm*>(this); }

};
#if URLC_IMPLEMENTATION
	URlcAm::URlcAm(RBInfo *rbInfo,RrcTfs *dltfs,UEInfo *uep,unsigned dlPduSize) :
		URlcBase(URlcModeAm,uep,rbInfo),
		URlcTransAm(&mConfig),	// Warning, this is not set up yet, but gcc whines if you order correctly.
		URlcRecvAm(&mConfig),
		mConfig(*rbInfo,*dltfs,dlPduSize)
	{
		mAmid=format("AM%d",mrbid);
		transAmInit(); recvAmInit();
		if (mConfig.mMaxRST == 0) {
			LOG(WARNING) << "Max_RESET not configured";
		}
	}

	// Have to wait until both classes are defined before defining these:
	URlcAm* URlcTransAm::parent() { return static_cast<URlcAm*>(this); }
	URlcRecvAm* URlcTransAm::receiver() { return static_cast<URlcRecvAm*>(parent()); }
	URlcAm* URlcRecvAm::parent() { return static_cast<URlcAm*>(this); }
	URlcTransAm* URlcRecvAm::transmitter() { return static_cast<URlcTransAm*>(parent()); }
#endif

class URlcTransUm : // UMTS RLC Unacknowledged Mode Transmitter
	public virtual URlcBase,
	public URlcTransAmUm
{
	URlcConfigUm mConfig;
	// UM Send State Variables
	URlcSN mVTUS;	// SN of next UM PDU to be transmitted.
			// Note: For utran side initial value may not be 0?

	URlcPdu *readLowSidePdu();	// Return a PDU to lower layers, or NULL if queue empty.

	public:
	// We send the RBInfo to URlcConfigUm, but all it uses is the RlcInfo from it.
	URlcTransUm(RBInfo *rbInfo, RrcTfs *dltfs, UEInfo *uep,unsigned dlPduSize, bool isShared=0) :
		URlcBase(URlcModeUm,uep,rbInfo),
		URlcTransAmUm(&mConfig),
		mConfig(*rbInfo,*dltfs,dlPduSize),
		mVTUS(0)
		{mConfig.mIsSharedRlc = isShared;}
	unsigned getRlcHeaderSize() { return 1; }
	unsigned rlcGetDlPduSizeBytes() { return mConfig.mDlPduSizeBytes; }
	void text(std::ostream &os);
};

class URlcRecvUm : // UMTS RLC Unacknowledged Mode Receiver
	public virtual URlcBase,
	public URlcRecvAmUm
{	public:
	URlcConfigUm mConfig;
	URlcSN mVRUS; 	// SN+1 of last UM PDU received

	URlcRecvUm(RBInfo *rbInfo, RrcTfs *dltfs, UEInfo *uep) :
		URlcBase(URlcModeUm,uep,rbInfo),
		URlcRecvAmUm(&mConfig),
		mConfig(*rbInfo,*dltfs,0), // No downlink pdu size needed in uplink RLC.
		mVRUS(0)
		{}

	void rlcWriteLowSide(const BitVector &pdu);
#if RLC_OUT_OF_SEQ_OPTIONS
	//unsigned VRUDR;	// Expected next SN for DAR (duplicate avoidance and reordering.)
	//unsigned VRUDH;	// Highest received SN for DAR
	//unsigned VRUDT;	// Timer for DAR
	//Z100 mTimer_DAR;	// For UM duplicate avoidance and reordering. // see 9.7.10
	//unsigned VRUOH;	// Highest SN received.  Inited per 11.2.3.2
	// Only used for out-of-sequence-delivery
	unsigned VRUM() {	// SN of first UM PDU that shall be rejected.
		return addSN(mVRUS,mConfig->mConfigured_Rx_Window_Size);
	}
#endif
	void text(std::ostream &os);
};

// Rather than making the MAC delve into to the RrcMasterChConfig, and particularly,
// the RBMappingInfo (which we dont even use), we keep everything the MAC needs
// to know about this RB here in the RLC, which means we keep the TrCh id here too.
// The UMTS spec allows mapping of RLCs to different TrCh, but we will not, so it
// TrCh id is a single zero-based number, not a set.
struct URlcPair {
	class URlcTrans *mDown;
	class URlcRecv *mUp;
	TrChId mTcid;	// The transport channel to which this RLC is attached.
	URlcPair(RBInfo *rb, RrcTfs *dltfs, UEInfo *uep, TrChId tcid);
	~URlcPair();
};

}; // namespace UMTS

#endif
