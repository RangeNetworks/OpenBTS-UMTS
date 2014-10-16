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

#ifndef URB_H
#define URB_H 1
#include "URRCTrCh.h"
#include "AsnHelper.h"
#include "asn_system.h"
namespace ASN { //extern "C" {
#include "RLC-Info.h"
#include "UL-RLC-Mode.h"
#include "DL-RLC-Mode.h"
//}
};

namespace UMTS {

// These are the timer definitions that are implemented as enumerations in ASN.
// We suck the enumerated values out of the ASN description.
#if URRC_IMPLEMENTATION
#define ASN_ENUM(basename,minval,maxval) \
	AsnEnumMap s##basename(ASN::asn_DEF_##basename,ASN::basename##_##maxval)
#else
#define ASN_ENUM(basename,minval,maxval) \
	extern AsnEnumMap s##basename
#endif

#if 0
class cMaxDAT : public AsnEnumMap {
	long mValue;
	cMaxDAT(long wValue) : mValue(wValue);
	void toAsn(ENUMERATED_t *ref) { asn_long2INTEGER(pasnval,findEnum(actual)); }
};
#endif

//AsnEnumMap sMaxDatEnum(ASN::asn_DEF_MaxDAT,ASN::MaxDAT_dat40);
//AsnEnumMap sTimerMRWEnum(ASN::asn_DEF_TimerMRW,ASN::TimerMRW_te900);
//AsnEnumMap sMaxMRWEnum(ASN::asn_DEF_MaxMRW,ASN::MaxMRW_mm32);
//AsnEnumMap sNoExplicitDiscardEnum(ASN::asn_DEF_NoExplicitDiscard,ASN::NoExplicitDiscard_dt100);
//AsnEnumMap sTimerDiscardEnum(ASN::asn_DEF_TimerDiscard,ASN::TimerDiscard_td7_5);
//AsnEnumMap sTransmissionWindowSizeEnum(ASN::asn_DEF_TransmissionWindowSize,15);
//AsnEnumMap sTimerRST(ASN::asn_DEF_TimerRST,ASN::TimerRST_tr1000);

ASN_ENUM(MaxDAT,dat1,dat40);
ASN_ENUM(TimerMRW,te50,te900);
ASN_ENUM(MaxMRW,mm1,mm32);
// This is a timer too, despite the poor name.
ASN_ENUM(NoExplicitDiscard,dt10,dt100);

// TODO: The actual values in this one have underbars in them.
// It still works but ignoring the part after the underbar so precision is low.
// The underlying timer will have to be converted to msecs or maybe a float.
ASN_ENUM(TimerDiscard,td0_1,td7_5);
ASN_ENUM(TransmissionWindowSize,tw1,tw4095);
ASN_ENUM(TimerRST,tr50,tr1000);
ASN_ENUM(MaxRST,rst1,rst32);
ASN_ENUM(TimerPollProhibit,tpp10,tpp1000);
ASN_ENUM(TimerPoll,tp10,tp1000);
ASN_ENUM(Poll_PDU,pdu1,pdu128);
ASN_ENUM(Poll_SDU,sdu1,sdu64);
ASN_ENUM(PollWindow,pw50,pw99);
ASN_ENUM(TimerPollPeriodic,tper100,tper2000);
ASN_ENUM(ReceivingWindowSize,rw1,rw4095);
ASN_ENUM(TimerStatusProhibit,tsp10,tsp1000);
ASN_ENUM(TimerEPC,te50,te900);
ASN_ENUM(TimerStatusPeriodic,tsp100,tsp2000);


enum URlcMode {
	URlcModeTm, URlcModeUm, URlcModeAm
};
extern const char*URlcMode2Name(URlcMode mode);

//extern enum ASN::UL_RLC_Mode_PR mapAsn_UL_RLC_Mode_PR[3];

//extern enum ASN::DL_RLC_Mode_PR mapAsn_DL_RLC_Mode_PR[3];


// 25.331 RRC: 10.3.4.4 Polling Info.
// None or all of the types of polling may be present.
// We will use a value of 0 to indicate the timer is not configured.
struct RrcPollingInfo {	// For RLC
	UInt_z mTimerPollProhibit;	// implemented.
	UInt_z mTimerPoll;				// implemented.	// 9.7.1 paragraph 3. and 9.5
	UInt_z mPollPdu;				// implemented.		// 9.7.1 paragraph 4.
	UInt_z mPollSdu;				// implemented.		// 9.7.1 paragraph 5.
	Bool_z mLastTransmissionPduPoll;	// implemented.		// 9.7.1 paragraph 1.
	Bool_z mLastRetransmissionPduPoll;	// implemented.		// 9.7.1 paragraph 2.
	UInt_z mPollWindow;			// not implemented		// 9.7.1 paragraph 6.
	UInt_z mTimerPollPeriodic;	// implemented.		// 9.7.1 paragraph 7.
	void toAsnPollingInfo(ASN::PollingInfo *pi)
	{
		if (mTimerPollProhibit) {
			pi->timerPollProhibit = sTimerPollProhibit.allocAsn(mTimerPollProhibit);
		}
		if (mTimerPoll) {
			pi->timerPoll = sTimerPoll.allocAsn(mTimerPoll);
		}
		if (mPollPdu) { pi->poll_PDU = sPoll_PDU.allocAsn(mPollPdu); }
		if (mPollSdu) { pi->poll_SDU = sPoll_SDU.allocAsn(mPollSdu); }
		pi->lastTransmissionPDU_Poll = mLastTransmissionPduPoll;
		pi->lastRetransmissionPDU_Poll = mLastRetransmissionPduPoll;
		if (mPollWindow) { pi->pollWindow = sPollWindow.allocAsn(mPollWindow); }
		if (mTimerPollPeriodic) {
			pi->timerPollPeriodic = sTimerPollPeriodic.allocAsn(mTimerPollPeriodic);
		}
	}
};

// Only used in for MBMS
struct UMDuplicationAvoidanceAndReorderingInfo { // 10.3.4.26
	// TODO
	UInt_z mTimerDAR;
	UInt_z mWindowSizeDAR;
};
// Only used in for MBMS
struct UMOutOfSequenceDeliveryInfo {	// 10.3.4.27
	UInt_z mTimerOSD;
	UInt_z mWindowSizeOSD;	// 8,16,32,40,48,56, or 64.
};

// There is a discrepancy between the RLC spec 25.322 9.7.3.5, which says:
// "If SDU discard has not been configured for a UM or TM entity SDUs in the transmitter
// shall not be discarded unless the transmission buffer is full."
// and the RRC spec 25.331 10.3.4.25, which says:
// "For UM and TM, only "TimerBasedWithoutExplicitSignaling" is allowed."
struct TransmissionRlcDiscard  // 10.3.4.25
{
	enum SduDiscardMode {
		// See 9.7.3.5.  An AM entity is always configured.
		// For UM and TM, "Not Configured" is one of the options
		// and implies discard without signaling.
		// To represent it here, I am using ...PR_NOTHING, but you cannot
		// put that value in the ASN, rather, the ASN represents by the
		// transmissionRLC_Discard being absent from, eg, struct UL_UM_RLC_Mode.
		NotConfigured = ASN::TransmissionRLC_Discard_PR_NOTHING,
		// Implies MRW after timer.
		TimerBasedWithExplicitSignaling = ASN::TransmissionRLC_Discard_PR_timerBasedExplicit,
		// Section 11.3.4.3 does not document how this
		// mode works, but we wont use it so who cares.
		TimerBasedWithoutExplicitSignaling = ASN::TransmissionRLC_Discard_PR_timerBasedNoExplicit,
		// Implies MRW after MaxDAT retransmissions.
		DiscardAfterMaxDatRetransmissions = ASN::TransmissionRLC_Discard_PR_maxDAT_Retransmissions,
		// Implies Reset after MaxDAT retransmissions.
		NoDiscard = ASN::TransmissionRLC_Discard_PR_noDiscard
	} mSduDiscardMode;
	TransmissionRlcDiscard(): mSduDiscardMode(NotConfigured) {}
	// See 10.3.4.25 for the cases of SduDiscardMode under which these are valid:
	UInt_z mTimerMRW;	// For TimerBasedWithExplicit, DiscardAfterMaxDAT
	UInt_z mMaxMRW;	// For TimerBasedWithExplicit, DiscardAfterMaxDat
	UInt_z mTimerDiscard;	// For TimerBased*, but note that values allowed in enumeration differ.
	UInt_z mMaxDAT;	// For DiscardAfterMaxDat or NoDiscard: Max num transmissions of AMD PDU.
	// For mandatory IE, pass address of IE.
	// For optional IE, pass a 0 ptr and this will return an allocated one,
	// unless the value is NotConfigured, in which case it returns NULL
	// as the value of ASN transmissionRLC_Discard.
	ASN::TransmissionRLC_Discard *toAsnTRD(ASN::TransmissionRLC_Discard*ptr) {
		if (mSduDiscardMode == NotConfigured) {
			// This is a special case in ASN represented by absense of this IE
			// for RLC-UM or RLC-TM.  But if ptr is specified, it is RLC-AM
			// and discard mode must always be configured.
			assert(ptr == NULL);
			return NULL;
		}

		if (ptr == 0) ptr = RN_CALLOC(ASN::TransmissionRLC_Discard);
		ptr->present = (ASN::TransmissionRLC_Discard_PR) mSduDiscardMode;
		switch (mSduDiscardMode) {
		case TimerBasedWithExplicitSignaling:
			sTimerMRW.cvtAsn(ptr->choice.timerBasedExplicit.timerMRW,mTimerMRW);
			sTimerDiscard.cvtAsn(ptr->choice.timerBasedExplicit.timerDiscard,mTimerDiscard);
			sMaxMRW.cvtAsn(ptr->choice.timerBasedExplicit.maxMRW,mMaxMRW);
			break;
		case TimerBasedWithoutExplicitSignaling:
			sNoExplicitDiscard.cvtAsn(ptr->choice.timerBasedNoExplicit, mTimerDiscard);
			break;
		case DiscardAfterMaxDatRetransmissions:
			// Alternate way:
			//AsnEnum(sMaxDAT,mMaxDAT).toAsn(ptr->choice.maxDAT_Retransmissions.maxDAT);
			sMaxDAT.cvtAsn(ptr->choice.maxDAT_Retransmissions.maxDAT, mMaxDAT);
			sTimerMRW.cvtAsn(ptr->choice.maxDAT_Retransmissions.timerMRW,mTimerMRW);
			sMaxMRW.cvtAsn(ptr->choice.maxDAT_Retransmissions.maxMRW,mMaxMRW);
			break;
		case NoDiscard:
			// Yes, the maxDAT value goes in the ASN variable called noDiscard. barfo
			sMaxDAT.cvtAsn(ptr->choice.noDiscard,mMaxDAT);
			break;
		case NotConfigured:
			assert(0);	// handled above.
		}
		return ptr;
	}
};

struct DownlinkRlcStatusInfo { // 10.3.4.1
	UInt_z mTimerStatusProhibit;
	Bool_z mMissingPduIndicator;
	UInt_z mTimerStatusPeriodic;
	void toAsnRSI(ASN::DL_RLC_StatusInfo *si) {
		if (mTimerStatusProhibit) {
			si->timerStatusProhibit = sTimerStatusProhibit.allocAsn(mTimerStatusProhibit);
		}
		si->missingPDU_Indicator = mMissingPduIndicator;
		if (mTimerStatusPeriodic) {
			si->timerStatusPeriodic = sTimerStatusPeriodic.allocAsn(mTimerStatusPeriodic);
		}
	}
};

struct URlcInfo 	// 10.3.4.23
{
	struct ul_t {
		enum URlcMode mRlcMode;
		// For UM and TM, only "TimerBasedWithoutExplicitSignaling" is allowed.
		TransmissionRlcDiscard mTransmissionRlcDiscard;	// For AM, UM, TM
		struct ulu_t {	// Its a union, but C++ does not allow them.
			struct ulam_t {
				UInt_z mTransmissionWindowSize;	// For AM only
				UInt_z mTimerRST;	// For AM only
				UInt_z mMaxRST;	// For AM only
				RrcPollingInfo mPollingInfo;	// For AM only
			} AM;
			struct ultm_t {
				// From 25.322 9.2.2.9: If TRUE:
				//	- all pdus carrying segments of sdu shall be sent in one TTI.
				//	- only pdus carrying segments from a single sdu shall be sent one TTI.
				// 	  (So what good is it?  Maybe used with 2ms slotted HS Phy channels?)
				// otherwise:
				//	- PDU size is fixed within a single TTI and equal to SDU size.
				Bool_z mSegmentationIndication;	// For TM only.  TRUE => segment Sdus.
			} TM;
		} u;
		// Note: no Pdu size in uplink.  You can infer the largest pdu size from the Transport Format Set.
		// The inferred largest pdu size implies the LISize as specified in 25.322
	} mul;	// uplink parameters
	struct dl_t {
		enum URlcMode mRlcMode;
		struct dlu_t {	// Its a union, but C++ does not allow them.
			struct dlam_t {
				// Note that 25.332 9.2.2.9 says that flexible size and pdu size are
				// for both AM and UM, because it is post-REL-7.
				// The flexible size option does not exist in our ASN, which is r6,
				// and the option in the 25.331 is marked REL-7.
				Bool_z mRlcPduSizeFlexible;	// REL-7
				UInt_z mDlRlcPduSize;		// AM mode only.  in REL-7, only if not rlcPduSizeFlexible.
											// UM mode infers pdu size from TFS.
				// The LI size was added to AM mode in REL-7 and does not exist in our ASN description.
				UInt_z mLengthIndicatorSize;		// REL-7 Length Indicator: 7 or 15.
								// REL-6: LISize only applicable to UM, AM specifies based on pdusize
								// NOTE: The UM uplink LISize is different - see above.
							
				Bool_z mInSequenceDelivery;
				UInt_z mReceivingWindowSize;	// AM only.
				DownlinkRlcStatusInfo	mDownlinkRlcStatusInfo;
			} AM;
			struct dlum_t {
				// ReceptionWindowSize is for UM, ReceivingWindowSize is for AM.
				// Their use is similar but the ranges of possible values of the two are different.
				// NOT USED because we are using an earlier version of ASN.
				UInt_z mDlUmRlcLISize;		// REL-5 Length Indicator: 7 or 15.
				//UInt_z mReceptionWindowSize;	// REL-6 UM only.
			} UM;
			struct dltm_t {
				Bool_z mSegmentationIndication;	// TM only
			} TM;
		} u;
		// These options are outside the union, even though they apply to only one mode.  Whatever.
		Bool_z mOneSidedRlcReEstablishment;	// TM only, not implemented
		//Bool_z mAlternativeEBitInterpretation;	// UM only.
		//Bool_z mUseSpecialValueOfHEField;	// AM only.
	} mdl;	// downlink parameters

	enum URlcMode getUlRlcMode() { return mul.mRlcMode; }
	enum URlcMode getDlRlcMode() { return mdl.mRlcMode; }

	// Default Config setup methods:
	//bool parse_ul;	// Indicates whether we are setting ul or dl options now.

	void ul_RLC_Mode(URlcMode mode) { /*parse_ul=true;*/ mul.mRlcMode = mode; }
	void transmissionRLC_DiscardMode(TransmissionRlcDiscard::SduDiscardMode mode) {
		mul.mTransmissionRlcDiscard.mSduDiscardMode = mode;
	}
	void maxDat(unsigned val) { mul.mTransmissionRlcDiscard.mMaxDAT = val; }
	void transmissionWindowSize(unsigned val) {
		mul.u.AM.mTransmissionWindowSize = val;
	}
	void timerRST(unsigned val) { mul.u.AM.mTimerRST = val; }
	void max_RST(unsigned val) { mul.u.AM.mMaxRST = val; }
	void TimerPoll(unsigned val) { mul.u.AM.mPollingInfo.mTimerPoll = val; }
	void timerPollProhibit(unsigned val) { mul.u.AM.mPollingInfo.mTimerPollProhibit = val; }
	void timerPollPeriodic(unsigned val) { mul.u.AM.mPollingInfo.mTimerPollPeriodic = val; }
	void PollSDU(int val) { mul.u.AM.mPollingInfo.mPollSdu = val; }
	void PollPDU(int val) { mul.u.AM.mPollingInfo.mPollPdu = val; }
	void lastTransmissionPDU_Poll(bool val) { mul.u.AM.mPollingInfo.mLastTransmissionPduPoll = val; }
	void lastRetransmissionPDU_Poll(bool val) { mul.u.AM.mPollingInfo.mLastRetransmissionPduPoll = val; }
	void PollWindow(bool val) { mul.u.AM.mPollingInfo.mPollWindow = val; }

	void timerStatusProhibit(unsigned val) { mdl.u.AM.mDownlinkRlcStatusInfo.mTimerStatusProhibit = val; }
	void timerStatusPeriodic(unsigned val) { mdl.u.AM.mDownlinkRlcStatusInfo.mTimerStatusPeriodic = val; }

	void dl_RLC_Mode(URlcMode mode) { /*parse_ul=false;*/ mdl.mRlcMode = mode; }
	//void dl_RLC_PDU_size(unsigned size) { mdl.u.AM.mDlRlcPduSize = size; }
	void dl_RLC_PDU_size(unsigned size) { assert(0); }	// Let the RLC figure out its own sizes.
	void missingPDU_Indicator(bool val) {
		mdl.u.AM.mDownlinkRlcStatusInfo.mMissingPduIndicator = val;
	}
	// Multiple names for the same thing:
	void dl_UM_RLC_LI_size(unsigned val) { mdl.u.UM.mDlUmRlcLISize = val; }
	void dl_LengthIndicatorSize(unsigned val) { mdl.u.UM.mDlUmRlcLISize = val; }
	void rlc_OneSidedReEst(bool val) { mdl.mOneSidedRlcReEstablishment = val; }

	void ul_segmentationIndication(bool val) { mul.u.TM.mSegmentationIndication = val; }
	void dl_segmentationIndication(bool val) { mdl.u.TM.mSegmentationIndication = val; }
	void inSequenceDelivery(bool val) { mdl.u.AM.mInSequenceDelivery = val; }
	void receivingWindowSize(unsigned val) { mdl.u.AM.mReceivingWindowSize = val; }
	
	void toAsnRLC_Info(ASN::RLC_Info *rp) {
		rp->ul_RLC_Mode = RN_CALLOC(ASN::UL_RLC_Mode);
		switch (mul.mRlcMode) {
		case URlcModeAm: {
			rp->ul_RLC_Mode->present = ASN::UL_RLC_Mode_PR_ul_AM_RLC_Mode;
			ASN::UL_AM_RLC_Mode *pulam = &rp->ul_RLC_Mode->choice.ul_AM_RLC_Mode;
			mul.mTransmissionRlcDiscard.toAsnTRD(&pulam->transmissionRLC_Discard);
			sTransmissionWindowSize.cvtAsn(pulam->transmissionWindowSize,mul.u.AM.mTransmissionWindowSize);
			sTimerRST.cvtAsn(pulam->timerRST,mul.u.AM.mTimerRST);
			sMaxRST.cvtAsn(pulam->max_RST,mul.u.AM.mMaxRST);
			pulam->pollingInfo = RN_CALLOC(ASN::PollingInfo);
			mul.u.AM.mPollingInfo.toAsnPollingInfo(pulam->pollingInfo);
			}
			break;
		case URlcModeUm: {
			rp->ul_RLC_Mode->present = ASN::UL_RLC_Mode_PR_ul_UM_RLC_Mode;
			ASN::UL_UM_RLC_Mode *pulum = &rp->ul_RLC_Mode->choice.ul_UM_RLC_Mode;
			pulum->transmissionRLC_Discard = mul.mTransmissionRlcDiscard.toAsnTRD(0);
			}
			break;
		case URlcModeTm: {
			rp->ul_RLC_Mode->present = ASN::UL_RLC_Mode_PR_ul_TM_RLC_Mode;
			ASN::UL_TM_RLC_Mode *pultm = &rp->ul_RLC_Mode->choice.ul_TM_RLC_Mode;
			pultm->transmissionRLC_Discard = mul.mTransmissionRlcDiscard.toAsnTRD(0);
			pultm->segmentationIndication = mul.u.TM.mSegmentationIndication;
			}
			break;
		}

		// Now the downlink.
		rp->dl_RLC_Mode = RN_CALLOC(ASN::DL_RLC_Mode);
		switch (mdl.mRlcMode) {
		case URlcModeAm: {
			rp->dl_RLC_Mode->present = ASN::DL_RLC_Mode_PR_dl_AM_RLC_Mode;
			ASN::DL_AM_RLC_Mode *pdlam = &rp->dl_RLC_Mode->choice.dl_AM_RLC_Mode;
			pdlam->inSequenceDelivery = mdl.u.AM.mInSequenceDelivery;
			sReceivingWindowSize.cvtAsn(pdlam->receivingWindowSize,mdl.u.AM.mReceivingWindowSize);
			mdl.u.AM.mDownlinkRlcStatusInfo.toAsnRSI(&pdlam->dl_RLC_StatusInfo);

			// Our version ASN does not support:
			//bool mRlcPduSizeFlexible;	// REL-7
			//unsigned mDlRlcPduSize;		// in REL-7
			//unsigned mLengthIndicatorSize;		// REL-7 Length Indicator: 7 or 15.
			}
			break;
		case URlcModeUm:
			rp->dl_RLC_Mode->present = ASN::DL_RLC_Mode_PR_dl_UM_RLC_Mode;
			// ASN is completely empty unless you use at least REL-5.
			//ASN::DL_UM_RLC_Mode *pdlum = &rp->dl_RLC_Mode->choice.dl_UM_RLC_Mode;
			break;
		case URlcModeTm: {
			rp->dl_RLC_Mode->present = ASN::DL_RLC_Mode_PR_dl_TM_RLC_Mode;
			ASN::DL_TM_RLC_Mode *pdltm = &rp->dl_RLC_Mode->choice.dl_TM_RLC_Mode;
			pdltm->segmentationIndication = mdl.u.TM.mSegmentationIndication;
			}
			break;
		}
	}
};


// Note: In the RLC Protocol Parameters, these have different names as follows:
// Configured_Tx_Window_Size == uplink.TransmissionWindowSize
// Configured_Rx_Window_Size == downlink.ReceivingWindowSize
// The ReceptionWindowSize is for UM mode and the ReceivingWindowSize 
// SN_Delivery is found where?

	// The "Largest UL UM PDU size" is one of the primitive params
	// as per 25.322 8.2 paragraph 11, but it is not specified in this structure,
	// it is inferred from the RB Mapping Info, which is the next struct above this one.
	// I am adding the value here for use by the RLC, but this is not transmitted in ASN.
	//unsigned cLargestUlUmPduSize;

// 3GPP 24.331 10.3.4.2 PDCP Info.
// It is most likely included in 10.3.4.18 RB Information to Reconfigure,
// which is most likely included in 10.2.33 Radio Bearer Setup Message.
// There does not need to be anything in this structure if we dont support compression.
struct PdcpInfo
{
	static const bool mSrnsSupport = false;
	static const bool mPdcpPduHeader = false;	// absent;
	static const unsigned mAlgorithmType = 0;	// No compression.
};



// There are several nearly identical IEs:
// 10.3.4.18: RB information to reconfigure, used in Cell Update Confirm.
// 10.3.4.20: RB information to Set up is used in:
//		10.3.4.10 RAB Information for setup, which is used in 10.2.33: Radio Bearer Setup
//		in 10.3.4.7 Predefined RB configuration, we wont use.
// 		SRNS Relocation Info, we wont use.
// 3-27-2012: Throw away RBMappingInfo and use a default mapping based on the TrCh.
struct RBInfo : public URlcInfo, public PdcpInfo, public virtual RrcDefs
{
	// from 10.3.4.18

	// Unneeded: PDCPSNInfo; it is optional; used for lossless SRNS relocation.

	int mRbId;	// logical channel id 10.3.4.16, specified as 1..32 in the spec,
			// but I use the same struct for SRB0, in which case id==0.
	// For 10.3.4.20 only, the Rlc info can come from some other RB.
	enum ChoiceRLCInfoType { eRlcInfo, eSameAsRB } mChoiceRlcInfoType;
	// If mChoiceRlcInfo == RlcInfo, use the URlcInfo above; otherwise setup from this RB:
	//struct {
	//	unsigned mRbId2;
	//} SameAsRB;
	enum RBStopContinue { Stop, Continue } mRBStopContinue;	// unused yet.

	// We need to know which RBs are RABs for the Radio Bearer Setup Message.
	// Only applies to RB-id >= 5, which we assume is a RAB (or sub-rab-flow.)
	// We dont allow mapping SRBs with id >= 5.
	// If it is CS domain, that rbid 5,6,7 constituate a RAB for the AMR codec.
	// You dont have to set mPsCsDomain for SRBs (which have rbid <= 4.)
	CNDomainId mPsCsDomain; // CS or PS domain.
	bool isCsDomain() {
		assert(mRbId >= 5); // Only call this function on RB-ids >= 5.
		assert(mPsCsDomain == CSDomain || mPsCsDomain == PSDomain);
		return mPsCsDomain == CSDomain;
	}

	// Normally the RBMappingInfo specifies the mapping of rb->trch.
	// We do that statically, so the RBMappingInfo appears in two places:
	// The RBInfo specifies the trch (here) and the TrChInfo specifies
	// the multiplexing option, and if not multiplexed, the single rbid
	// to be sent on that channel.
	TrChId mTrChAssigned;	// The trch id (0-based) that carries this logical channel.
	void setTransportChannelIdentity(unsigned tcid1based) {	// incoming is 1-based.
		assert(tcid1based >= 1 && tcid1based < 32);
		mTrChAssigned = tcid1based-1;
	}

	RBInfo(): mRbId(-1), mPsCsDomain(UnconfiguredDomain), mTrChAssigned(0) {}
	bool valid() { return mRbId >= 0; }

	// Default Config Setup Functions:
	// If it is an srb, the domain need not be specified.
	void rb_Identity(unsigned rbid, CNDomainId domain=UnconfiguredDomain) {
		assert(rbid <= 4 || domain != UnconfiguredDomain);
		mRbId = rbid;
		mPsCsDomain = domain;
	}

	// This is a generic RLC-AM config, from defaultConfig0CFRb below.
	void defaultConfigSrbRlcAm();
	void defaultConfigRlcAmPs();	// One for packet-switched RLCs.

	// The first one applies to us.
	// Some are from 34.108: Common Test Environments for UE testing, list
	// in sec 6.10.2, with the RAB and RBs defined immediately before.
	// Some are from 25.993: Typical Examples of RABs and RBS supported by UTRA.
	// 25.331 RRC 13.6: RB information parameters for SRB0
	void defaultConfigSRB0();

	// 25.331 RRC 13.8: Default configuration 0 when using CELL_FACH.
	// This is the RB part; the TrCh part goes in TrChConfig
	void defaultConfig0CFRb(unsigned rbn);

	// 25.331 RRC 13.7: Other Default configurations
	void defaultConfig3Rb(unsigned rbn);
};

// I am using the RB-id for the RAB-id, eg, 5.
// 10.3.4.8
// struct RabInfo : public virtual RrcDefs {
// 	unsigned mRabIdentity;	// 10.3.1.14: 8 bits
// 	// MBMSServiceIdentity;
// 	// MBMSSessionIDentity;
// 	CNDomainId mCNDomainIdentity;	// 10.3.1.1
// 	unsigned mNASSyncIndicator;	// 10.3.4.12: 4 bits, and I quote: "A containter for non-access stratum information to be transferred transparently through utran.
// 	ReestablishmentTimer mReestablishmentTimer;	// 10.3.3.30
// };


// 10.3.4.10.
// closely resembles Rab Info to reconfig
// Currently unused:
//struct RabInfoSetup : public virtual RrcDefs {
//	RabInfo info;	// The RAB id.  We dont really need this because we dont have an Iu interface.
//	// RAB info to replace 10.3.4.11a
//	// RB information to setup list
//	unsigned mNumRb;	// Number of guys in below.
//	RBInfo mRbInfo[maxRBperRAB];	// The important stuff.
//};

// 10.2.33: I dont need to make this a seperate structure; asn can be created on demand.
//struct RadioBearerSetup {
	// SRBs...
	// RABs...
	// RBs...
	// TrCh Information Elements.
//};

}; // namespace UMTS
#endif
