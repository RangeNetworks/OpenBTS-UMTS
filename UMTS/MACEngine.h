/**@file UMTS MAC, 3GPP 25.321. */

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

#ifndef L2MACENGINE_H
#define L2MACENGINE_H


#include <Interthread.h>
//#include <Configuration.h>
#include <ByteVector.h>
#include <list>
#include <Defines.h>
#include "URRCDefs.h"
#include "UMTSTransfer.h"
#include "UMTSCommon.h"	// For L1FEC_t
#define USE_CCCH_Q 0

#if 0
Questions:
o Rate Matching - UL can zero-pad messages, but DL cannot.
	Without rate matching CCTrCh must be exact radio size or 0.
o Who is doing the USIM L3 negotiation?
#endif



namespace UMTS {


class MacEngine;
class UEInfo;
class MaccBase;
class MacdBase;
class RrcTfc;
class RrcTfcs;
class RrcMasterChConfig;
class URlcTransUm;
class DCHFEC;
class RACHFEC;
class FACHFEC;
typedef DCHFEC DCHFEC_t;


extern unsigned macHeaderSize(TrChType trch, ChannelTypeL3 ltype, bool multiplexing);
extern unsigned macHeaderSize(TrChType trch, int rbid, bool multiplexing);
extern void macHookupRachFach(RACHFEC *rach, FACHFEC *fach, bool useForCcch);
extern void macHookupDch(DCHFEC_t *dch, UEInfo *uep);
extern void macUnHookupDch(UEInfo *uep);


class MacTbDl : public TransportBlock
{
	public:
	MacTbDl(unsigned wTBSize) : TransportBlock(wTBSize) {}
};

// Note: If BCCH were sent of FACH, then a special header would be added, but we dont do that.
// We send BCCH over BCH, and there is no header at all, so there is no class here;
// it just uses TransportBlock directly, and bypasses RLC and MAC entirely.

// Used for CCCH, which may be sent only on Mac-c: common channel SCCPCH.
struct MaccTbDlCcch : public MacTbDl
{
	MaccTbDlCcch(unsigned trbksize,ByteVector *pdu);
};

// Used for DCCH or DTCH when sent on Mac-c: common channel SCCPCH.
struct MaccTbDl : public MacTbDl
{
	MaccTbDl(unsigned trbksize,ByteVector *pdu, UEInfo *uep, RbId rbid);
};

// Used for DCCH or DTCH when sent on Mac-d: dedicated channel DCH.
struct MacdTbDl : public MacTbDl
{
	MacdTbDl(unsigned trbksize, ByteVector *pdu, RbId rbid, bool multiplexed);
};

// Uninteresting class, but the name itself is documentation about which way it is traveling.
struct MacTbUl : public TransportBlock
{
	MacTbUl(const TransportBlock &tb) : TransportBlock(tb) {}	// great language
	MacTbUl(const MacTbUl &tb) : TransportBlock(static_cast<TransportBlock>(tb)) {}
};

// This accumulates the information for a Transport Format Combination match
// when the MAC is trying to find a TFC that matches the data available to send in a UE.
struct TfcMap
{
	RrcTfc *mtfc;
	struct TcMap {
		unsigned mNumTbAvail;
		unsigned mChIdMap[RrcDefs::maxTbPerTrCh]; // Logical channel where TBs are coming from.
	} mtc[RrcDefs::maxTrCh];
	unsigned getNumTbAvail(TrChId tcid) { return mtc[tcid].mNumTbAvail; }
	void tfcMapClear() {
		mtfc = 0;
		for (unsigned i = 0; i < RrcDefs::maxTrCh; i++) {mtc[i].mNumTbAvail = 0;}
	}
	// Return true if the map is full so the caller can stop looking.
	bool tfcMapAdd(TrChId tcid,int lcid,int numTB) {
		TcMap *ptc = &mtc[tcid];
		for (int i = 0; i < numTB; i++) {
			if (ptc->mNumTbAvail >= RrcDefs::maxTbPerTrCh) { return true; }
			ptc->mChIdMap[ptc->mNumTbAvail++] = lcid;
		}
		return false;
	}
};

// The MAC implementation has four variations arranged in a 2x2 matrix.
// There are two channel types supported:
// 1. MAC for DCH (logical DCCH+DTCH) dedicated channels,
// 2. MAC for PRACH/SCCPCH (logical RACH/FACH) common channels.
// For each of the above, there are two variations:
// 1. Simple version - simple version sends single TransportBlock to Layer1.
// 2. WithTfc version - complex version sends MacTbs to Layer1.
// Whoever sets up the stack should glue an appropriate MAC class to the top
// of the Layer 1 TrCHFEC class.

// The Simple version is a simplified MAC implementation that assumes
// a single TrCh and sends only a single TransportBlock down to the PHY layer class TrCHFEC.
// That means either:
// o there is no CTFC broadcast on the channel, meaning that blind detection is employed.
// Blind detection could be based on either presence/absence of the data
// (meaning just one tbsize) or optionally multiple rlc sizes using a method
// so complicated that we will probably never use it.
// o or there could be a CTFC selected by the PHY layer based solely on the TransportBlock size,
// however, that would be a weird partitioning of the duties between MAC and L1.
//
// The WithTfc version is a full-bore implementation of everything.
// There were lots of other sub-possibilities of MAC implementations, but once
// you are doing TFCS selection, it is no extra trouble to implement the whole
// shebang here at the MAC level and let the PHY layer only pick out the data it wants,
// which it knows will be correct because we control the TFS setups via the RRC setup classes.

// The MacTbs is what MAC sends to the PHY channel for the full-bore implementation.
// Most generally, the TBS consists of a TFCS that selects a TransportFormat for each TrCh,
// plus a set of TransportBlocks for each TrCh channel.
//

// A TBS sent on Mac-C for FACH.
// I dont think all the possible TfcMap possibilities are legal,
// but this just does whatever is programmed by RRC.
class MaccTbs : public MacTbs
{	public:
	// Fill the TBS with data from this UE.
	// The logical channel from which to send data is in the map.
	MaccTbs(UEInfo *uep, TfcMap &map);
};

// A TBS consisting of a single TransportBlock on channel 0.
// Used for CCCH.
class MacTbsSingle : public MacTbs
{	public:
	MacTbsSingle(RrcTfc *tfc, TransportBlock *tb);
};
#if MAC_IMPLEMENTATION
	MacTbsSingle::MacTbsSingle(RrcTfc *tfc, TransportBlock *tb) : MacTbs(tfc) {
		addTb(tb);
	}
#endif

// A TBS sent on Mac-D for DCH.
class MacdTbs : public MacTbs
{	public:
	// Fill the TBS with data from this UE.
	// The logical channel from which to send data is in the map.
	MacdTbs(UEInfo *uep, TfcMap &map);
};


// The low side of MAC uses TransportBlocks (possibly inside MacTbs) for uplink and downlink.
// The high side of MAC uses ByteVectors for downlink (except for BCH, which bypasses MAC)
// and BitVectors for uplink.  See URLC.h for explanation.
// The data channels are all pulled from below on demand, with the exception of CCCH messages
// which bypass RLC.  

// gMacSwitch is the only MacSwitch object.
class MacSwitch
{
	typedef std::list<MaccBase*> CchList_t;	// For common channels.
	//typedef std::list<MacdBase*> DchList_t;	// For dedicated channels.

	// A list of all the MAC entities that want service, and a lock for the list.
	typedef std::list<MacEngine*> MacList_t;
	Mutex mMacListLock;
	MacList_t mMacList;

	Bool_z mStarted;	// Is the macServiceLoop running?
	Thread macThread;	// The mac service loop thread.
	static void *macServiceLoop(void *arg);


	CchList_t mCchList;	// Common channels, ie, one RACH/FACH.  Might be only one.
						// This has to be an ordered list so we can pick the proper
						// FACH channel based on URNTI.
	//DchList_t mDchList;	// Not used. Dedicated channels, ie, associated with a UE.

	public:

	// Add, remove, macs from the list of active macs.
	void addMac(MacEngine *mac, bool useForCcch);
	void rmMac(MacEngine *mac);

	MaccBase *pickFachMac(unsigned urnti);

	void macWriteLowSideRach(const MacTbUl&tb);

	//void writeHighSideBch(ByteVector *msg);  // Not used - MAC bypassed entirely.
	void writeHighSideCcch(ByteVector &sdu, const std::string descr); // Goes out on a FACH channel.
	// There is no writeHighSideDCCH or writeHighSideDTCH here.
	// Those messages go directly to an RLC entitiy in the UEInfo
	// Dont think we will use this here either:
	//void writeLowSideDch(const MacTbUl &tb,UEInfo *uep);
};
extern MacSwitch gMacSwitch;


// These little tiny MAC classes are for David to glue on to the top of the TrChFEC classes.
// Note that the whole conceptual point of MAC is to switch data between different channels,
// so the bulk of MAC is in the MacSwitch.
// Various notes:
// o For RACH, if there are multiple ones we dont care
// which one sent us the TransportBlock, they are all equivalent.
// o For FACH, if there may be muliple ones we pick one based on URNTI.
// o I recommended that we have different types of RACH/FACH channels for different PDU sizes.
// o Control information for a UE may switch from FACH to DCH.
// o For DCH, tying MAC to a particular TrChFEC is ok, but the RLC engines are per-UE.

// Defines the downstream interface to L1, a base class for TrCHFEC and L1CCTrCh
//class MacL1FecInterface {
//	public:
//	virtual unsigned getNumRadioFrames() const;
//	virtual void writeHighSide(const TransportBlock& frame);
//	virtual unsigned getDlTrBkSz() const;
//};

static void shut_up_gcc(int) {}

// This is the class referenced in class TrCHFEC.
class MacEngine
{
	protected:
	// Temporary hack: Use two downstream pointers while we are debugging the L1CCTrCh, and use whichever is non-NULL.
	TrCHFEC * mDownstream;
	L1CCTrCh * mccDownstream;
	public:
	MacEngine() : mDownstream(0), mccDownstream(0) { }
	void macSetDownstream(TrCHFEC *wDownstream) { mDownstream = wDownstream; }
	void macSetDownstream(L1CCTrCh *wccDownstream) { mccDownstream = wccDownstream; }
	unsigned macGetDlNumRadioFrames() const;
	void macWriteHighSide(MacTbDl &tb);
	unsigned macGetDlTrBkSz();

	// There are two different writeLowSide methods depending on MAC complexity.
	// They are only defined in their respective final classes.
	virtual void macWriteLowSideTb(const TransportBlock&,TrChId tcid=0) { assert(0); shut_up_gcc(tcid); }
	virtual void macService(int fn) = 0;
};

// Sends/receives TransportBlocks of just one size.
class MacSimple : public virtual MacEngine
{
	public:
	// redundant: virtual void macWriteLowSideTb(const TransportBlock&tb) = 0;
	void sendDownstreamTb(MacTbDl &tb);
};

// Sends/receives MacTbs [Transport Block Set] which includes a TFC [Transport Format Combination]
// Also supports multiple TrCh, which was no extra effort.
class MacWithTfc : public virtual MacEngine
{	protected:
	void findTbAvail(UEInfo *uep, TfcMap *map);
	bool matchTfc(RrcTfc *tfc, UEInfo *uep, TfcMap *match);
	public:
	bool findTfcForUe(UEInfo *uep,TfcMap *result);
	RrcTfc *findTfcOfTbSize(RrcTfcs *tfcs, TrChId tcid, unsigned tbsize);
	void sendDownstreamTbs(MacTbs &tbs);
};


// MAC-d handles DCH.
// The only point of it is to associate a UEInfo with an L1 TrCh processor.
// (pat) TODO: This is not fully implemented yet.
class MacdBase : public virtual MacEngine
{	protected:
	UEInfo *mUep;
	//bool macMultiplexed[RrcDefs::maxTrCh];
	//RbId macRbId[RrcDefs::maxTrCh];		// If multiplexed == false this is the rbid.
	//Thread macThread;

	// Flush any TransportBlocks in the UE associated with this DCH.
	virtual bool flushUE() = 0;

	public:
	MacdBase(UEInfo *wUep) : mUep(wUep) {
		//memset(macMultiplexed,0,sizeof(macMultiplexed));
		//memset(macRbId,0,sizeof(macRbId));
	}

	// Same service loop works for both descendent classes.
	//static void *macServiceLoop(void*);
	//void macStart();
	void macService(int fn);
};

class MacdSimple : public MacdBase, public MacSimple
{
	// This class handles just a single TrCh.
	// Either it is multiplexed or it is assigned to a specific rb.
	// (pat) macRbId is not used.  If we are not multiplexed we will be using class MacdWithTfc.
	bool flushUE();
	bool macMultiplexed;	// We always use this multiplexed, so this is redundant.
	RbId macRbId;			// This is not used.
	public:
	MacdSimple(UEInfo *wUep, bool wMultiplexed, RbId wrbid) : MacdBase(wUep)
		{ macMultiplexed = wMultiplexed; macRbId = wrbid; }
	void macWriteLowSideTb(const TransportBlock&tb, TrChId tcid=0);
};

class MacdWithTfc :  public MacdBase, public MacWithTfc
{	public:
	bool flushUE();
	MacdWithTfc(UEInfo *wUep) : MacdBase(wUep) {}
	void macWriteLowSideTb(const TransportBlock&tb, TrChId tcid=0);
};

class MaccBase : public virtual MacEngine
{
	protected:
	// Where is the RLC for CCCH messages?  Not clear.
	// The MAC spec says that RLC is connected to the top of MAC
	// per logical channel, which would imply there is one UM-RLC entity per RNC.
	// However, CCCH can switched to multiple different FACH, so that doesnt really make sense.
	// UEs that are listening to different FACH would not know or care if there were
	// other FACHs around, so I am implementing with one RLC per Macc engine
	// for the CCCH messages sent on that FACH, and here it is:
	URlcTransUm *mCcchRlc;

	//Thread macThread;
	virtual bool flushUE()=0;
	virtual bool flushQ()=0;
	//static void *macServiceLoop(void*);
	//void macStart();
	void macService(int fn);

#if USE_CCCH_Q // Not using this now - using mCcchRlc instead.
	// PDUs waiting to be sent normally sit in the queue at the high end of an RLC
	// and wait until they get pulled.
	// CCCH is an exception - they are pushed through RLC and wait here,
	// and this Q is written first before we look for other data.
	// CCCH is special in several ways:
	// It is not associated with any UE, and if there are several FACH,
	// data may be broadcast in several CCCH queues simultaneously.
	// The MAC header is prepended before adding to the queue, in other words,
	// these are TransportBlocks all ready to go.
	InterthreadQueue<MaccTbDlCcch> mTxCcchQ;
#endif

	public:
	// TODO: Constructor?

	// Write a CCCH message to this channel.
	//void writeHighSideCcch(MaccTbDlCcch *tb);
	void writeHighSideCcch(ByteVector &sdu, const std::string descr);
};

// MAC-c handles RACH or FACH.
// The only point of it is to put a handle on a TrCHFEC used for RACH/FACH.
// Uplink messages go to a common queue for processing.
//
// If there are multiple FACH, downlink messages go to a FACH based on
// (UE-id mod (number of FACH channels)).
// For connected mode, 25.331 8.5.19 says UE-id is URNTI.
// For unconnected mode, ie, for the RRC Connection Request
// and RRC Connection Setup messages on CCCH, 25.304 8.2 defines UE-id
// based on the "IntialUE_Identity" and has many different cases.
// Currently We just blast the message out on all FACH in use for unconnected mode.
//
// CCCH messages do not have much of a MAC header, but they still must use a TFC if the
// same phy channel is used for any other tbsizes.  In the latter case, they
// use the exact same processing as any other MAC channel.
//
// The simplified MaccSimple assumes all pdus are the same size, ignores priority.
// It may have a TFC with just two entries to indicate presence or absence.
class MaccSimple : public MaccBase, public MacSimple
{
	bool flushUE();
	bool flushQ();
	void macWriteLowSideTb(const TransportBlock&tb, TrChId tcid=0);
	public:
	MaccSimple(unsigned trbksize);
};

class MaccWithTfc : public MaccBase, public MacWithTfc
{
	bool flushUE();
	bool flushQ();

	public:
        MaccWithTfc(unsigned trbksize);
	// Check all the UEs that can use this FACH to see if they have something to send.
        void macWriteLowSideTb(const TransportBlock&tb, TrChId tcid=0);
	void macWriteLowSideTbs(const MacTbs&/*tbs*/) {
		// TODO.  We may never use  this.
	}
};


// We dont need a Mac-b class - BCH bypasses MAC entirely.
//class MACbEngine : public virtual MacEngine {
//	virtual void writeHighSide(const RLCSDU&);
//	void macWriteLowSide(const TransportBlock&) { assert(0); }
//};

} // namespace UMTS

// Pats in-progress now-obsolete notes below.
// I kept these while I was trying to figure this out, and they are probably
// useless now, but I loathe discarding any information whatever.
//
// GSM 25.331: RRC Protocol Spec.
//	10.3.5.1: Added or reconfigured DL TrCH info
//		The UL and DL can be the same or different.
//		UL & DL Type (DSCH)
//		UL & DL Transport Channel ID 10.3.5.18
//		TFS Transport Format Set 10.3.5.23
//		UL & DL TrCH Id
//		MAC header type (choice of ...)
//		DCH Quality Target. 10.3.5.10  real(-6.3..0)
//
// 10.3.5.24: UL Transport Ch Info common for All TrCh.
//		Looks unused in this version of the protocol.
//		TFC subsets for various channels and TFCS ids.
//
// 10.3.5.11: Semi-Static Transport Format Info
//		TTI integer(10,20,40,80)
//		ChannelCoding enum(None,Convolutional, Turbo)
//		CodingRate enum(1/2,1/3)
//		RateMatching Attribute integer(1..hi RM)
//		CRCsize integer(0,8,12,16,24)
// 10.3.5.15 TFCS Complete Reconfig/Addition Information
//		It is a big union depending on TFCI size (number of bits in TFC, maybe 4,8,1,6,32)
//		Power Offset Info 10.3.5.8 (needed only for UL physical channels)

// MAC does not perform segmentation on DCH, only only on MAC-e/es, MAC-hs/ehs, MAC-i/is
//	Same with HARQ - Hybrid Automatic Repeat Request.
//
// Notes:
//		RABs are 8 bits, used within a CN (Core Network) domain.
//		RB ids are 1..32, used with a single RNC I guess.
//		Logical channel ids are 1..15 "Used to distinguish logical channels multiplexed
//			by MAC on a transport  channel."

// 10.3.5.23: Tranport Format Set
//		GSM25.212 says:
//		"Uplink [and Downlink] Dedicated Channel (DCH):
//		The maximum value of the number of TrCHs I in a CCTrCH,
//		the maximum value of the number of transport blocks Mi
//		on each transport channel, and the maximum value of the
//		number of DPDCHs P are given from the UE capability class."

// CHOICE(transport channel type)
// if Transport Channel Type == DCH
//	Dynamic Transport Format Info: 1 to maxTF
//		RLC Size integer(16..5000 by 8)
//		TTI integer(10,20,40,80)
//			(note: presence of TTY in dynamic info controlled by
//			dynamic-TTI-usage opiton in semi-static transport format info
//		Number of TBs integer(0..512)
//			GSM25.212 says: Max number of TrCH in CCTrCH and max
//		Logical channel List - logical channels allowed to use this RLC size.
//			(Note, logical channels in list identified by RB id)
//	Semi-static Transport Format Info see 103.5.11
// if Transport Channel Type == Common Transport channels
//		much the same as above.


// 10.2.39 RRC Connection Request

// 10.2.40 RRC Connection Setup

// 10.2.33 RB Setup
//		todo

// 10.3.4.21: RB Mapping Info
//		todo

// RRC Connection Establishment Procedure 8.1.3
//	RRC Connection Request includes IE "Domain Indicator" for PS or CS
//	UTRAN returns RRC Connection Setup message.


// Primitives:
// TrCH id 10.3.5.18 integer(1..32)
// TFC integer(0..1023)
// 10.3.35a RRC State Indicator: enum(CELL_DCH,CELL_FACH,CELL_PCH,URA_PCH)
//		indicates to a UE the RRC state to be entered.
// Default config for CELL_FACH 10.3.4.0a: Reserved for future versions of spec.
// RAB Identity 10.3.1.14 - string(8) identifies RAB to CN.
// No: This is used only for MAC-hs:
//		MAC-d Flow Id 25.331 10.3.5.7b : integer(0..7)  Used in 10.3.4.21 RB Mapping Info for MAC-hs only.


// RANAP GSM25.413:
//	RANAP functions:
//		Relocate serving RNC.
//		Overall RAB management - set up, modify, release.
//		Paging - provides CN capability to page UE
//		and management of the Iu interface.
// From GSM 25.413 (RANAP): "The RAB ID shall uniquely identify the RAB
// for the specific CN domain and for the particular UE,
// which makes the RAB ID unique over the Iu connection on which
// the RAB ASSIGNMENT REQUEST message is received. When a
// RAB ID already in use over that particular Iu instance is used,
// the procedure is considered as modification of that RAB.


#endif
