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

#ifndef URRC_H
#define URRC_H 1
#include "UMTSConfig.h"
#include "UMTSTransfer.h"
#include "URRCTrCh.h"
#include "URRCRB.h"
#include "URLC.h"
#include "../SGSNGGSN/SgsnExport.h"
#include "asn_system.h"
#include "URRCMessages.h"
#include "IntegrityProtect.h"
namespace ASN {
//#include "InitialUE-Identity.h"
#include "UE-RadioAccessCapability.h"
struct UL_DCCH_Message;
};


// To keep it simple, we just use the logical channel id to index the RB array,
// which means 0 (for SRB0) is used only on CCCH, and SRB4 is never used.
// The UE is allowed to request data transfer on any of RB 5..15.
// When we start allowing simultaneous PS and CS connections, we may have to put the CS connect on 16,17,18.
const unsigned gsMaxRB = 16;	// SRB0 - SRB4 + data rb 5..15

namespace UMTS {
extern void rrcInitCommonCh(unsigned rachSF, unsigned fachSF);
class AsnUeId;
class DCCHLogicalChannel;

// The possible RRC specified UE states we support.
// This state is used by the MAC to decide which UEs want to send messages on FACH.
// There can be multiple simultaneous requests to establish DCH state... todo...
enum UEState {
	stUnused,
	stIdleMode,
	//stCELL_FACHPending,
	stCELL_FACH,
	//stCELL_DCHPending,
	stCELL_DCH,
	stCELL_PCH,	// we dont use thse yet
	stURA_PCH
};
const char *UEState2Name(UEState state);

struct UEDefs
{
	enum TransType {
		ttComplete,				// Transaction never used, or Transaction complete.
		ttRrcConnectionSetup,
		ttRrcConnectionRelease,
		ttRadioBearerSetup,
		ttRadioBearerRelease,
		ttCellUpdateConfirm,
		ttSecurityModeCommand
	};
	const char *TransType2Name(TransType ttype);

	// 4 because the transactionId is only 2 bits.
	static const unsigned sMaxTransaction = 4;
};

// Master Channel Config.
// Its just an RB and TrCh config combined.
// The RBMappingInfo can specify two mappings for each RB:
// one when mapped to DCH and one when mapped to RACH/FACH;
// therefore we could have two TrChConfig here.
class RrcMasterChConfig : public RrcDefs
{
	static const unsigned sMaxSRB = 3;	// We dont use SRB4.
	//static const unsigned sMaxDRB = 1;	// Will need to be 3 for AMR voice channels.
	static const unsigned sMaxTrCh = 2;	// Minimum 2 for DCCH+DTCH; Will need to be 4 for AMR voice.

	// This is indexed by the RB number, which means that position 0 is not used
	// except for SRB0 in the CCCH MasterChConfig.
	// old :Warning: the RBMappingInfo contains indicies into the TFS buried in the TrCh info
	// 		(but we dont use RBMappingInfo any more)
	RBInfo mRB[gsMaxRB];	// Has RLC info, RBMappingInfo

	public:
	TrChConfig mTrCh;	// This has lists of TrCh, their TFS, and the master TFCS.

	// One greater than the index of the maximum mRB defined; for cch it is only 1.
	// This is used just to limit the loops that look through them.
	unsigned mNumRB;

	RrcMasterChConfig():
		mNumRB(0)
		{}

	// The getRB works for RB or SRB.
	RBInfo *getRB(unsigned rbid) { assert(rbid<mNumRB); return &mRB[rbid]; }
	RBInfo *setSRB(unsigned rbid);
	RBInfo *setRB(unsigned rbid,CNDomainId domain);

	// These use 0 based numbering for TrCh:
	TrChInfo *getUlTrChInfo(TrChId tcid) { return mTrCh.ul()->getTrChInfo(tcid); }
	TrChInfo *getDlTrChInfo(TrChId tcid) { return mTrCh.dl()->getTrChInfo(tcid); }
	unsigned getUlNumTrCh() { return mTrCh.ul()->mNumTrCh; }
	unsigned getDlNumTrCh() { return mTrCh.dl()->mNumTrCh; }

	// TFS are per-TrCh.  TFCS are common for all TrCh.
	RrcTfs *getUlTfs(TrChId tcid = 0) { return mTrCh.ul()->getTfs(tcid); }
	RrcTfcs *getUlTfcs() { return mTrCh.ul()->getTfcs(); }
	RrcTfs *getDlTfs(TrChId tcid = 0) { return mTrCh.dl()->getTfs(tcid); }
	RrcTfcs *getDlTfcs() { return mTrCh.dl()->getTfcs(); }

	// Create a DCH channel with the a data channel on the specified RABid.
	void rrcConfigDchPS(DCHFEC *dch, int RabId, bool useTurbo);

	// Config DCH for CS (circuit-switched, ie, voice).
	// FIXME: Finish this.  I set up the RBInfo and TrChInfo, but we still
	// need to do the other stuff in rrcAllocateRabForPdp.
	void rrcConfigDchCS(DCHFEC *dch);

	// TODO: uplink function = none, RLC bypassed
};

extern RrcMasterChConfig *gRrcCcchConfig;	// Defines TrCh, SRB0 for UE in unconnected mode.
extern RrcMasterChConfig *gRrcDcchConfig;	// Defines TrCh, SRB for UE in connected mode.
extern RrcMasterChConfig *gRrcDchPSConfig;

class Rrc : public UEDefs
{
	UInt32_z mRrcRNTI;	// Next ue id.

	Thread mRrcUplinkHandler;

	//TrChConfig mRachFachTrCh;
	//Tfs *getRachTfs() { rrcDoInit(); return mRachFachTrCh.ul()->getTfs(1); }
	//Tfcs *getRachTfcs() { rrcDoInit(); return mRachFachTrCh.ul()->getTfcs(); }
	//Tfs *getFachTfs() { rrcDoInit(); return mRachFachTrCh.dl()->getTfs(1); }
	//Tfcs *getFachTfcs() { rrcDoInit(); return mRachFachTrCh.dl()->getTfcs(); }


	Bool_z inited;
	public:
	void rrcDoInit() {
		if (inited) { return; }
		inited = true;
		mRrcRNTI = 0xffff&time(NULL);
	}

	// List of UE we have heard from.
	Mutex mUEListLock;
	typedef std::list<UEInfo*> UEList_t;
	UEList_t mUEList;

	// If ueidtype is 0, look for URNTI, else CRNTI
	UEInfo *findUe(bool ueidtypeCRNTI,unsigned ueid);
	UEInfo *findUeByUrnti(uint32_t urnti) {return findUe(false,urnti);}
	UEInfo *findUeByAsnId(AsnUeId *ueid);
	void purgeUEs();
	void addUE(UEInfo *ue);

	// Dont init anything in the constructor to avoid an initialization race with UMTSConfig.
	Rrc() { mUEListLock.unlock();}

	// The crnti is 16 bits for UE id and the urnti is 12 bits for SRNC id and 20 bits for UE id.
	// We will use the same UE id for both.
	void newRNTI(uint32_t *urnti, uint16_t *crnti) {
		unsigned ueid = ++mRrcRNTI; // skip 0.  We use 0 sometimes to mean undefined UNRTI.
		if (mRrcRNTI >= 65536) { mRrcRNTI = 0; }
		*crnti = ueid;
		unsigned srnc = gNodeB.getSrncId();
		//printf("SRNC=%d ueid=%d\n",srnc,ueid);	// Something is wrong.
		*urnti = (srnc << 20) | ueid;
	}

	//URlcBase *RABFindURlc(RadioBearer *id);	// Return a pointer to the URlc object associated with a RB.

};

extern Rrc gRrc;


// When we send an RB setup message of some kind, we save the transaction here
// so that we know what to do when we receive the message reply.
// This is necessary for RadioBearerSetup because the UE may request multiple
// setups simultaneously, eg, might ask for two IP addresses at the same time,
// and we need to keep the responses straight.
struct UeTransaction : UEDefs
{
	TransType mTransactionType;	// Event type.
	UEState mNewState;	// If not idle, new state when transaction complete.
	unsigned mRabMask;	// The mask of RBs affected by this transaction.
	uint8_t mTransactionId;
	Timeval mTransTime;	// When the event occurred.

	UeTransaction(): mTransactionType(ttComplete),mNewState(stUnused) {}
	// The rabMask is just extra info saved with the transaction to be used
	// by the transaction response; it is currently a mask of the RABs affected
	// by a radiobearer modification, but conceptually it could be arbitrary info.
	UeTransaction(UEInfo *uep,TransType type, unsigned rabMask, unsigned transactionId,
		UEState nextState = stUnused);
	void transClose() { mTransactionType = ttComplete; }
	long elapsed();	// How long since the transaction, in ms?

	const char *name() { return TransType2Name(mTransactionType); }
	std::string str();
};


// TODO: After sending the RCC Connection Setup we dont know if the
// UE is in connected or unconnected mode until it returns the RRC Connection Complete message.
// In the meantime, we need to be prepared to receive messages on either CCCH or DCCH.
// We could punt and say that this UEInfo is on DCCH, and if we get another message,
// we will create a new UEInfo with a new RNTI.
//
// When switching from CELL_FACH to CELL_DCH state, RRC sends the RadioBearerSetup message
// in acknowledged mode, so RRC can know whether or not it was delivered.
// However, we still dont know what state the UE will be in, because it may not like the message,
// and stay in CELL_FACH state.  We dont know for sure until it sends the RadioBearerSetupComplete
// message, 8.2.2.4 and I quote:
// "If the new state is CELL_DCH or CELL_FACH, the response message shall be transmitted
// "using the new configuration after the state transition, and the UE shall:"
// if issued a new U-RNTI, or message included IE "Downlink Counter Synchronisation" or "SR-VCC",
// (and then later on) if the PDCP_SN_INFO variable is non-null (where this var is also
// related to counter re-synchronization...) then there is a long list of special cases, and finally we find:
// "The UE shall: resume data transmission on any suspended radio bearer and signalling
// radio bearer mapped on RLC-AM or RLC-UM;"
// So specifically for SRBs we need to listen to both the old and new config simultaneously.
// Now 8.6.4.9: RLC-Info: This specifies when to re-establish the RLC.  To paraphrase,
// if you change the size you re-establish it, otherwise you keep it.
// Therefore, for SRBs, the RRC MUST support two RLCs simultaneously for rlc-size changes,
// but if the size doesnt change, you need to copy the old RLC entity, including the state,
// a feature that RLC is supposed to also support for handover.
// In general, you cant use the same RLC entity even for the same-size case because you are
// permitted to change the RLC config in lots of other ways, even though they keep the internal state,
// however, for our purposes we could guarantee the config was the same and just point to the same RLC.
// In general, copying the RLCs would also require copying the table of PDUs in the RLC-AM entity,
// except I think we can guarantee that the RLC is completely empty at the time the UE changes state.
// Since the MAC also wants to know which RLC to route messages to, the easiest way for us
// is to have two sets of RLCs for state CELL_FACH and state CELL_DCH, so the MAC-c knows
// to route only to the CELL_FACH set.  And in fact, we dont care what state the UE is in -
// if it ever sends us anything on FACH, we want to hear about it.
// If in the future we need to change the SRBs, then we will need a THIRD set so that we
// can have two for CELL_DCH state - the old and the new.
// Moreover, to guarantee that MAC can tell the new SRB from the old SRB, you probably want to
// specify a new TrCh when you reconfigure the SRBs.
// Incredible.  They couldn't have made this any more complex.

#define RN_UE_FOR_ALL_RLC_DOWN(uep,rbid,rlcp) \
	URlcTrans *rlcp; \
	for (RbId rbid=0; rbid <=uep->mUeMaxRlc; rbid++) \
		if ((rlcp = uep->getRlcDown(rbid)))

#define RN_UE_FOR_ALL_RLC(uep,rbid,rlc) \
	URlcPair *rlc; \
	for (RbId rbid = 0; rbid <= uep->mUeMaxRlc; rbid++) \
		if ((rlc = uep->getRlc(rbid)))

#define RN_UE_FOR_ALL_DLTRCH(uep,tcid) \
	unsigned tcid##_numTC = uep->ueGetTrChConfig()->dl()->mNumTrCh; \
	for (unsigned tcid = 0; tcid < tcid##_numTC; tcid++)

// UE timers are described in 13.1 and default values are in 10.3.3.43 and 10.3.3.44
// The interesting timers and their default values are:
// T300: 1sec, start on RRC Connection Request, stop on RRC Connection Setup, retry.
// T302: 4secs, cell update retry.
// T305: 30minutes, periodic cell update in CELL_FACH, CELL_PCH or URA_PCH mode.
// T307: 30secs, dont understand start condition.  drop to idle mode if
//		T305 expired and no answer to periodic cell update?
// T312: 1sec, radio link no sync = failure on initial establishment of DCH
// T313: 3sec, radio link no sync = failure otherwise.
// T314: 12sec, used for CS connection, timeout to idle mode after radio link failure.
// T315: 180sec, used for PS connection, timeout to idle mode after radio link failure.
// T319: unspecified, when to start DRX mode after entering CELL_PCH or URA_PCH state.
static int sNextUeDebugId = 1;	// Each UE gets a human-readable id for log messages.
class UEInfo : public SGSN::MSUEAdapter, public UEDefs 
{
	int mUeDebugId;
	UEState mUeState;

	// There are two sets of RLCs - explanation in the big comment above.
	// We use the rbid directly for the rlc index, so SRB1 uses 1, etc.
	// They wont all be used simultaneously, but its just easier.
	// The SRB0 is never used because that rlc is in the Mac-c.
	URlcPair *mRlcsCF[gsMaxRB];		// The RLCs used in CELL_FACH state.
	URlcPair *mRlcsCDch[gsMaxRB];	// The RLCs used in CELL_DCH state.
	public:
	// TODO: Decrement mUeMaxRlc when deleting RBs.
	UInt_z mUeMaxRlc;	// Max index of any allocated rlc for this ue.

	// I am not yet sure if we need to keep the RrcMasterChConfig for this UE around
	// after we program it.  Specifically, the RBs that are connected vary for each UE
	// depending on which PDPs the UE requests.
	// The MAC layer only needs the TrCh config part out of the RrcMasterChConfig,
	// and that will be invariant at least per SF+intended use.
	RrcMasterChConfig mUeDchConfig;	// The config used to connect this UE in DCH mode.
	RrcMasterChConfig *ueGetConfig();
	TrChConfig *ueGetTrChConfig();

	// We need to remember which rbs have been used.
	// While we are at it, we will remember everything we told the SGSN so if
	// we get a duplicate request we can return identical information.
	SGSN::RabStatus mConfiguredRabs[gsMaxRB];


	private:
	float mTimingError;
	float mRSSI;

	// 25.331 13.4.27 - same TRANSACTIONS table in RRC and UE:
	UeTransaction mTransactions[sMaxTransaction];
	friend class UeTransaction;
	UeTransaction *getTransaction(unsigned transId, TransType ttype, const char *help);
	public:
	UeTransaction *getTransactionRaw(unsigned transId, const char *help);
	UeTransaction *getLastTransaction();

	Mutex mUeLock;
	Timeval mHelloTime;
	Timeval mActivityTime;	// When was last activity?
	AsnUeId mUid;			// Self-inits.
	// We use neither dch nor mac pointers while running, but we keep
	// the pointers so we can de-allocate everything at the end.
	DCHFEC *mUeDch;	// If allocated.
	MacdBase *mUeMac;	// If allocated a DCH, this is the Mac-d entity.
	DCCHLogicalChannel *mGsmL3;	// Somewhere to send L3 messages for the GSM stack.
	
        ASN::UE_RadioAccessCapability *radioCapability; // UE capabilities, usually from a RRC Conn. Setup Complete msg.
			
	uint32_t mURNTI; 	// Note: The mURNTI is 12 bits SRNC id + 20 bits UE id.
	uint16_t mCRNTI;	// Used by mac on the phy interface.

	IntegrityProtect integrity;

	// The external interface is by rbid - do not access the rlcs directly because
	// they may get destroyed when the UE switches states.
	// The rlc-down is always picked based on the UE state.
	// In uplink we need to be able to receive messages from both states simultaneously
	// during the cell state transition, so the MAC must specify the state.
	URlcPair *getRlc(RbId rbid);
	URlcTrans *getRlcDown(RbId rbid);
	URlcRecv *getRlcUp(RbId rbid, UEState state);
	friend class MacWithTfc;
	friend class MaccSimple;
	friend class MacdSimple;
	protected:

	UInt_z mNextTransactionId;	// Next Transaction Id.

	// The UE does not need to point downstream at all for functional purposes,
	// because the MAC pulls data from the RLCs as needed using the pointer to the UE,
	// but if we are in DCH state we will keep a downstream pointer past mac
	// all the way to the physical channel, for reporting purposes only.

	Thread* mThread;

	// Stupid language.
	private: void _initUEInfo() {
		mUeDebugId = sNextUeDebugId++;
		mUeState = stIdleMode;
		mUeDch = NULL;
		mUeMac = NULL;
		mGsmL3 = allocateLogicalChannel();
		memset(mRlcsCF,0,sizeof(mRlcsCF));
		memset(mRlcsCDch,0,sizeof(mRlcsCDch));
		mHelloTime.now();
		mActivityTime = mHelloTime;
		mURNTI = 0;	// We expect these to be set immediately, but cant be too cautious.
		mCRNTI = 0;
		radioCapability = NULL;
	}

	public:
	DCCHLogicalChannel *allocateLogicalChannel();

	UEInfo(AsnUeId *wUid) : mUid(*wUid)
	{
		_initUEInfo();
		// Allocate a RNTI for this new UE.
		gRrc.newRNTI(&mURNTI,&mCRNTI);
		//connectUeRlc(gRrcCcchConfig);	// Not necessary
		gRrc.addUE(this);
	}

	UEInfo(uint32_t urnti) {
		// OK to leave AsnUeId empty.
		_initUEInfo();
		mURNTI = urnti;
		mCRNTI = (urnti & 0xffff);	// We use the same id for both; see newRNTI().
		gRrc.addUE(this);
	}

	~UEInfo() {
		ueDisconnectRlc(stCELL_FACH);
		ueDisconnectRlc(stCELL_DCH);
	}

	// Write bytes to the high side of the rlc on rbid.
	void ueWriteHighSide(RbId rbid, ByteVector &sdu, string descr);
	// Write bits to the low side of the rlc on rbid for the two possible channels.
	// There are two functions because we have two sets or RLCs simultaneously
	// when changing the UE state - see gigantic comment above.
	void ueWriteLowSide(RbId rbid, const BitVector &pdu, UEState state);

	// This user data sdu popped out of the top of the RLC, and now wants to go somewhere.
	void ueRecvData(ByteVector &sdu, RbId rbid) {
		ueRegisterActivity();
		// Currently all it can be is PS data because we do not support CS.
		LOG(INFO) << "Sending SDU to SGSN: " << sdu;
		sgsnWriteLowSide(sdu,mURNTI,rbid);
	}
	void ueRecvDcchMessage(ByteVector &bv,unsigned rbid);
	void ueRecvL3Msg(ByteVector &msgframe, UEInfo *uep);
	void ueRecvStatusMsg(ASN::UL_DCCH_Message *msg1);

	void ueRecvRadioBearerSetupResponse(unsigned transId, bool success, const char *msgname);
	void ueRecvRadioBearerReleaseResponse(unsigned transId, bool success, const char *msgname);
	void ueRecvRrcConnectionSetupResponse(unsigned transId, bool success, const char *msgname);
	void ueRecvRrcConnectionReleaseResponse(unsigned transId, bool success, const char *msgname);

	// Connect a UE to some RLCs.
	void ueConnectRlc(RrcMasterChConfig *config, UEState nextState);
	void ueDisconnectRlc(UEState state); // Destroy the RLC entities.
	void reestablishRlcs(); // re-establish all the AM RLCs.
	bool mStateChange;

	// Try to pull amtBytes through the RLC on every channel.
	void uePullLowSide(unsigned amtBytes);
	// Read one PDU from the RLC on the specified rb.
	ByteVector *ueReadLowSide(RbId rbid);

	void uePeriodicService();
	void ueRegisterActivity();

	// 25.331 10.3.3.36 The transaction id in the messages is only 2 bits.
	unsigned newTransactionId() { return (mNextTransactionId++) % sMaxTransaction; }

	void ueSetState(UEState newState);
	UEState ueGetState() const { return mUeState; }

	// Serving RNC [Radio Network Controller] Id.  This returns what is saved in the UEInfo structure,
	// but it must match the cell id broadcast in SIB2, except they are different sizes,
	// so only the top 12 bits are considered.
	unsigned getSrncId() { return (mURNTI>>20) & 0xfff; }
	// 20 bits SRNTI = SRNC-RNTI = Serving RNC - Radio Network Temporary Id.  Identifies the UE within this cell.
	unsigned getSRNTI() { return mURNTI & 0xfffff; }

	void setPhy(float wRSSI, float wTimingError) { mRSSI = wRSSI; mTimingError = wTimingError; }
	float RSSI() { return mRSSI; }
	float timingError() { return mTimingError; }

	// MAC Interface:
	// Return the number of bytes waiting in the highest priority queue for this UE.
	unsigned getDlDataBytesAvail(unsigned *uePriority);

	// Return the size of the waiting pdu, and how many pdus.
	// Note that for TM entities, not all pdus may be the same size.
	//unsigned peekPduSize(unsigned rbid, unsigned *pducnt) {
	//	URlcPair *rlc = mRlc[rbid];
	//	if (!rlc) return 0;
	//	*pducnt = rlc->mDown->rlcGetPduCnt();
	//	return rlc->mDown->rlcGetFirstPduSize();
	//}

	string ueid() const;	// A human readable name for the UE.
	string msid() const { return ueid(); }	// Used by the SGSN.

	void msWriteHighSide(ByteVector &dlpdu, uint32_t rbid, const char *descr);	// Called by SGSN
	void msDeactivateRabs(unsigned rabMask);	// Called by SGSN.
	uint32_t msGetHandle() { return mURNTI; }
};
std::ostream& operator<<(std::ostream& os, const UEInfo*uep);

ByteVector* sendDirectTransfer(UEInfo* uep, ByteVector &dlpdu, const char *descr, bool psDomain);

};	// namespace UMTS
#endif
