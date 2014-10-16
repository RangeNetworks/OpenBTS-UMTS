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

#define MAC_IMPLEMENTATION 1
#include "UMTSCommon.h"
#include "URRCDefs.h"
#include "URRCMessages.h"
#include "URRC.h"
#include "URRCTrCh.h"
#include "UMTSL1FEC.h"
#include "MACEngine.h"
#include "URLC.h"
#include <Logger.h>

namespace UMTS {
MacSwitch gMacSwitch;

//void MACbEngine::writeHighSide(const RLCSDU& sdu)
//{
//	assert(mDownstream);
//	assert(sdu.size()==246);
//	mDownstream->writeHighSide(TransportBlock(sdu));
//}

// How many bits are in the MAC header?
unsigned macHeaderSize(TrChType tctype, ChannelTypeL3 lch, bool multiplexing)
{
	// From 25.321 9.2.1.4 [MAC spec]
	// Table 9.2.1.4: Coding of the tctf [Target Channel Type Field] on RACH for FDD
		// CCCH => 2 bits
		// DCCH or DTCH => 2 bits
	// Table 9.2.1.2: Coding of tctf [Target Channel Type Field] on FACH for FDD:
		// BCCH => 2 bits
		// CCCH => 8 bits
		// DCCH or DTCH => 2 bits
	switch (lch) {
	case BCCHType:
		switch (tctype) {
		case TrChFACHType: return 2;
		case TrChBCHType: return 2;
		default: assert(0);
		}
		break;
	case CCCHType:
		// CCCH on RACH/FACH adds tctf only.
		// (Because this is used only for the initial connection setup message
		// in which the UE id encoded is in the message instead of the MAC header.)
		switch (tctype) {
		case TrChRACHType: return 2;
		case TrChFACHType: return 8;
		default: assert(0);
		}
		break;
	case DCCHType:
	case DTCHType:
		switch (tctype) {
		case TrChDlDCHType:
		case TrChUlDCHType:
			// From 25.321 9.2.1.1 case (a) no multiplexing or (b) multiplexing.
			// 'multiplexing' means multiple logical channels on the same TrCh, which will
			// likely always be true for us except for voice channels.
			return multiplexing ? 4 : 0;
		case TrChRACHType:
		case TrChFACHType:
			// From 25.321 9.2.1.1 case (c) DTCH+DCCH mapped to RACH/FACH: tctf+ueidtype+ueid+c/t
			// 2 bits for tctf from Table 9.2.1.2
			// 2 bits for ue id type
			// 16 bits for ue id of type C-RNTI
			// 4 bits for C/T field (which is Radio Bearer id)
			return 2+2+16+4;
		default: assert(0);
		}
	default: assert(0);
	}
}


// For all channels except BCCH: 
// Since we pass the macMutiplexed flag explicitly there are only two different
// cases of logical channel to worry about, CCCH or not-CCCH, so if it
// is not-CCCH pass DCCH, which has same mac header size as DTCH.
// If we start sending BCCH on FACH (why would we ever?) then this needs to change.
unsigned macHeaderSize(TrChType tctype, int rbid, bool multiplexing)
{
	return macHeaderSize(tctype, rbid == 0 ? CCCHType : DCCHType, multiplexing);
}

unsigned MacEngine::macGetDlNumRadioFrames() const
{
	if (mDownstream) return mDownstream->l1GetDlNumRadioFrames(0);
	if (mccDownstream) return mccDownstream->l1GetDlNumRadioFrames(0);
	assert(0);
}
void MacEngine::macWriteHighSide(MacTbDl &tb)
{
	if (mDownstream) mDownstream->l1WriteHighSide(tb);
	if (mccDownstream) mccDownstream->l1WriteHighSide(tb);
}

unsigned MacEngine::macGetDlTrBkSz()
{
	if (mDownstream) return mDownstream->l1GetDlTrBkSz();
	if (mccDownstream) return mccDownstream->l1GetDlTrBkSz();
	assert(0);
}

MaccSimple::MaccSimple(unsigned trbksize)
{
	// Set up the CCCH downlink rlc entity.  Needs a dummy RBInfo for SRB0, but the SRB0
	// info is not dummied - it comes from the RRC spec sec 13.6.
	// The SRB0 
	RBInfo rbtmp;
	rbtmp.defaultConfigSRB0();

	int dlmacbits = macHeaderSize(TrChFACHType, CCCHType, true);
	assert(dlmacbits == 8);	// We could have just said: 8.

	mCcchRlc = new URlcTransUm(&rbtmp,
		0,	// no TFS required, and in fact, this param is no longer used and should be elided.
		0,	// no associated UE.
		(trbksize - dlmacbits)/8,
		true);	// This is the shared RLC for Ccch.
}

MaccWithTfc::MaccWithTfc(unsigned trbksize)
{
        // Set up the CCCH downlink rlc entity.  Needs a dummy RBInfo for SRB0, but the SRB0
        // info is not dummied - it comes from the RRC spec sec 13.6.
        // The SRB0 
        RBInfo rbtmp;
        rbtmp.defaultConfigSRB0();

        int dlmacbits = macHeaderSize(TrChFACHType, CCCHType, true);
        assert(dlmacbits == 8); // We could have just said: 8.

        mCcchRlc = new URlcTransUm(&rbtmp,
                0,      // no TFS required, and in fact, this param is no longer used and should be elided.
                0,      // no associated UE.
                (trbksize - dlmacbits)/8,
                true);  // This is the shared RLC for Ccch.
}


// There can be multiple FACH.  Each FACH can be used for:
// UE unconnected mode (CCCH messages, defined in SIB5),
// UE connected mode (DCCH messages, defined in SIB6) or both.
// We assume all our FACH use the simplified MAC for now.
void macHookupRachFach(RACHFEC *rach, FACHFEC *fach, bool useForCcch)
{
	// This MAC will be immortal, like the FACH to which it is connected.
	//printf("MAC-C: Fach pdu size = %d\n",fach->l1GetDlTrBkSz());
	//MaccSimple *mac = new MaccSimple(fach->l1GetDlTrBkSz());
	MaccWithTfc *mac = new MaccWithTfc(fach->l1GetDlTrBkSz());

	// Hook it  up.
	mac->macSetDownstream(fach);
	rach->l1SetUpstream(mac);
	
	gMacSwitch.addMac(mac,useForCcch);
	//gMacSwitch.addMac(macR,useForCcch);

	// The high side of mac does not need to be explictly connected.
	// All Macc entities are hooked on the high side to the same places based on RB-id of the message.
}

// This extremely important pair of functions hooks and unhooks together
// the UEInfo, the DCHFEC and the MAC entity.
void macHookupDch(DCHFEC_t *dch, UEInfo *uep)
{
	MacdBase *mac;
#if USE_OLD_DCH
		mac = new MacdSimple(uep,true,0);	// Assume multiplexed.
#else
	// TODO: We should remove this old code as soon as the new is demonstrably working.
	if (dch->l1dl()->isTrivial() && dch->l1ul()->isTrivial()) {
		mac = new MacdSimple(uep,true,0);	// Assume multiplexed.
	} else {
		mac = new MacdWithTfc(uep);
	}
#endif

	// TODO: If we are not using logical channel multiplexing this needs to change:
	mac->macSetDownstream(dch);
	dch->l1SetUpstream(mac);
	dch->open();		// adds the channel to the gActiveDch, starts upstream communication.
	gMacSwitch.addMac(mac,false);	// starts downstream communication.
	uep->mUeMac = mac;	// Remember these for macUnHookupDch.
	uep->mUeDch = dch;
}

// TODO: This may need to be thread-protected?  From whom?
// The channel can be reallocated immediately.
// Note: We can get a macUnHookupDch without ever having had macHookupDch called; it happens
// if the UE sends DeActivatePdpContextAccept before having finished the ActivatePdpContext procedure,
// so the radiobearer setup never occurred.
void macUnHookupDch(UEInfo *uep)
{
	DCHFEC_t *dch = uep->mUeDch;
	MacdBase *mac = uep->mUeMac;
	if (mac) { gMacSwitch.rmMac(mac); }	// stops downstream communcation.
	if (dch) {
        	printf("unhooking DCH %0x\n",dch);

		dch->close();		// closes physical channel and removes from gActiveDch.
						// Note: Channel can be reallocated immediately.
		dch->l1SetUpstream(0);
	}
	uep->mUeMac = 0;
	uep->mUeDch = 0;
	if (mac) { delete mac; }
}


MaccTbDlCcch::MaccTbDlCcch(unsigned trbksize, ByteVector *pdu) :
	MacTbDl(trbksize)
{
	assert(trbksize >= pdu->sizeBits() + 8);	// Must be room for MAC header.
	size_t wp = 0;
	writeField(wp,0x40,8);	// set TCTF for CCCH.
	segment(wp,pdu->sizeBits()).unpack(pdu->begin());	// add the pdu data.
	// Fill the tail with zeros.
	wp += pdu->sizeBits();
	tail(wp).zero();
}

MaccTbDl::MaccTbDl(unsigned trbksize,ByteVector *pdu, UEInfo *uep, RbId rbid) :
	MacTbDl(trbksize)
{
	assert(trbksize >= pdu->sizeBits() + (2+2+16+4));	// Must be room for MAC header.
	size_t wp = 0;
	writeField(wp,0x3,2);	// TCTF is DCCH or DTCH over FACH for FDD.
	writeField(wp,1,2);		// UE id-type is C-RNTI.
	writeField(wp,uep->mCRNTI,16);
	writeField(wp,rbid-1,4);	// Logical channel id.
	segment(wp,pdu->sizeBits()).unpack(pdu->begin());	// add the pdu data.
	// Fill the tail with zeros.
	wp += pdu->sizeBits();
	tail(wp).zero();
}

MacdTbDl::MacdTbDl(unsigned trbksize,ByteVector *pdu, RbId rbid,bool multiplexed) :
	MacTbDl(trbksize)
{
	//printf("trbksize: %u, pduSize: %u, multiplexed: %d\n",trbksize,pdu->sizeBits(),multiplexed);
	assert(trbksize >= pdu->sizeBits() + (multiplexed?4:0));
	size_t wp = 0;
	if (multiplexed) {writeField(wp,rbid-1,4);}	// Logical channel id.
	//LOG(INFO) << "sizeBits: " << pdu->sizeBits();
	segment(wp,pdu->sizeBits()).unpack(pdu->begin());
	wp += pdu->sizeBits();
	tail(wp).zero();
	//LOG(INFO) << "vector: " << *((BitVector *) this);
}

// The TBS construction is always the same, but the constituent
// TB header is different for MAC-C or MAC-D.  We could possibly push
// this method into MacTbs by encapsulating the TB construction
// in a virtual method.
MaccTbs::MaccTbs(UEInfo *uep, TfcMap &map) : MacTbs(map.mtfc)
{
	unsigned numTrCh = mTfc->getNumTrCh();
	for (TrChId tcid = 0; tcid < numTrCh; tcid++) {
		RrcTf *tf = mTfc->getTf(tcid);
		unsigned numTB = tf->getNumTB();
		assert(numTB <= RrcDefs::maxTbPerTrCh);
		unsigned tbSize = tf->getTBSize();
		for (unsigned tbn = 0; tbn < numTB; tbn++) {
			RbId rbid = map.mtc[tcid].mChIdMap[tbn];
			ByteVector *vec = uep->ueReadLowSide(rbid);
                        if (!vec) {continue;}
			MaccTbDl *out = new MaccTbDl(tbSize,vec,uep,rbid);
			RN_MEMLOG(MaccTbDl,out);
			addTb(out, tcid);
			delete vec;
		}
	}
}

// Create the TBS for the TFC from this TfcMap.
// The caller has already checked that all the TBs specified in this TFC exist,
// so all we do is go get them from the RLCs and stick them in the TBS.
MacdTbs::MacdTbs(UEInfo *uep, TfcMap &map) : MacTbs(map.mtfc)
{
	unsigned numTrCh = mTfc->getNumTrCh();
	for (TrChId tcid = 0; tcid < numTrCh; tcid++) {
		bool multiplexed = mTfc->getTrChInfo(tcid)->mTcIsMultiplexed;
		RrcTf *tf = mTfc->getTf(tcid);
		unsigned numTB = tf->getNumTB();
		assert(numTB <= RrcDefs::maxTbPerTrCh);
		unsigned tbSize = tf->getTBSize();
		for (unsigned tbn = 0; tbn < numTB; tbn++) {
			RbId rbid = map.mtc[tcid].mChIdMap[tbn];
			//LOG(INFO) << "ueReadLowSide rb " << rbid << " at time " << gNodeB.clock().get();
			ByteVector *vec = uep->ueReadLowSide(rbid);
                        //LOG(INFO) << "ueReadLowSide rb " << rbid << " done at time " << gNodeB.clock().get();
			if (!vec) {continue;}
			MacdTbDl *out = new MacdTbDl(tbSize,vec,rbid,multiplexed);
			RN_MEMLOG(MaccTbDl,out);
			addTb(out,tcid);
			delete vec;
		}
	}
}

void MacSwitch::addMac(MacEngine *mac, bool useForCcch)
{
	mMacListLock.lock();
	mMacList.push_back(mac);
	if (useForCcch) { mCchList.push_back(dynamic_cast<MaccBase*>(mac)); }
	mMacListLock.unlock(); // Must release the mMacListLock before macServiceLoop to avoid deadlock.

	// If we have not started the service loop yet, its time.
	if (!mStarted) {
		mStarted = true;
		macThread.start(macServiceLoop,this);
	}
}

// The CCCH Mac is never removed, so we dont worry about removing from mCchList.
void MacSwitch::rmMac(MacEngine *mac)
{
	ScopedLock lock(mMacListLock);
	mMacList.remove(mac);
}

// Receive a RACH from the FEC via the MacSwitch.
// Decode the header and send CCCH messages to RRC directly and DCCH messages
// to their UE, which will route them to one of the RLC entities in the UE.
void MacSwitch::macWriteLowSideRach(const MacTbUl&tb)
{
	// See table 9.2.1.4 "Coding of the Target Channel Type Field on RACH for FDD"
	size_t rp = 0;
	unsigned tctf = tb.readField(rp,2);
	if (tctf == 0) {
		// It is CCCH.  No other MAC fields present.  Always uses SRB0,
		// which just means the messages are for RRC itself.
		// Uses RLC TM, which just means we bypass RLC entirely.
		BitVector msg(tb.tail(rp));
		rrcRecvCcchMessage(msg,0);
	} else if (tctf == 1) {
		// It is DCCH or DTCH.  Send it off to a UE.
		unsigned ueid;
		unsigned ueidtype = tb.readField(rp,2);
		if (ueidtype == 0) {
			ueid = tb.readField(rp,32);	// URNTI
		} else if (ueidtype == 1) {
			ueid = tb.readField(rp,16);	// CRNTI
		} else {
			LOG(ERR) << "Invalid UE idtype in RACH TransportBlock";
			return;
		}
		// The ct field is the logical channel id for this transport channel,
		// but we always map it directly to RbId.
		unsigned ct = tb.readField(rp,4)+1;
		UEInfo *uep = gRrc.findUe(ueidtype,ueid);
		if (uep == NULL) {
			LOG(INFO) << "Could not find UE with id "<< ueid;
			return;
		}
		BitVector msg(tb.tail(rp));
		uep->ueWriteLowSide(ct,msg,stCELL_FACH);
	} else {
		// "PDUs with this coding will be discarded"
	}
}

// This transport block arrived on a DCHFEC for a specific UE.
// We are also assuming there is only one trch, so no mapping.
// Encoding is in 25.321 table 9.2.1.1
void MacdSimple::macWriteLowSideTb(const TransportBlock &tb, TrChId unused)
{
	UEInfo *uep = mUep;	// From MacdBase.
	if (macMultiplexed) {
		size_t rp = 0;
		RbId rbid = tb.readField(rp,4)+1;
		BitVector msg(tb.tail(rp));
		// This needs to be prepared for a garbage rbid.
		uep->ueWriteLowSide(rbid,msg,stCELL_DCH);
	} else {
		// This code path is not used.
		uep->ueWriteLowSide(macRbId,tb,stCELL_DCH);
	}
}


// This transport block arrived on a DCHFEC for a specific UE.
// We are also assuming there is only one trch, so no mapping.
// Encoding is in 25.321 table 9.2.1.1
void MacdWithTfc::macWriteLowSideTb(const TransportBlock &tb, TrChId tcid)
{
	UEInfo *uep = mUep;	// From MacdBase.
	// Dig the mac multiplexing info out of the RRC config for the UE.
	TrChInfo *info = uep->mUeDchConfig.mTrCh.ul()->getTrChInfo(tcid);
	if (info->mTcIsMultiplexed) {
		size_t rp = 0;
		RbId rbid = tb.readField(rp,4)+1;
		BitVector msg(tb.tail(rp));
		// This needs to be prepared for a garbage rbid.
		uep->ueWriteLowSide(rbid,msg,stCELL_DCH);
	} else {
		// This code path is not used.
		uep->ueWriteLowSide(info->mTcRbId,tb,stCELL_DCH);
	}
}

#if 0	// (pat) 10-2012 This is not how it works.  The TransportBlocks arrive one by one to macWriteLowSideTb.
// The caller takes care of deallocated the tbs.
void MacdWithTfc::macWriteLowSideTbs(const MacTbs &tbs)
{
	// We need a TFC to describe what is going on inside this MacTbs.
	// FIXME: The mTfc in MacTbs needs to be set from the TFI bits

	UEInfo *uep = mUep;	// From MacdBase.
	RrcTfc *tfc = tbs.mTfc;		// FIXME: Set this in L1FEC
	unsigned numTrCh = tfc->getNumTrCh();
	for (TrChId tcid = 0; tcid < numTrCh; tcid++) {
		TrChInfo *tcinfo = tfc->getTrChInfo(tcid);
		bool multiplexed = tcinfo->mTcIsMultiplexed;
		RrcTf *tf = tfc->getTf(tcid);
		unsigned numTB = tf->getNumTB();
		assert(numTB <= RrcDefs::maxTbPerTrCh);
		for (unsigned tbn = 0; tbn < numTB; tbn++) {
			TransportBlock *tb = tbs.getTB(tbn,tcid);
			if (multiplexed) {
				size_t rp = 0;
				RbId rbid = tb->readField(rp,4)+1;
				BitVector msg(tb->tail(rp));
				uep->ueWriteLowSide(rbid,msg,stCELL_DCH);
			} else {
				uep->ueWriteLowSide(tcinfo->mTcRbId,*tb,stCELL_DCH);
			}
		}
	}
}
#endif

//void writeHighSideBch(ByteVector *msg);  Not used - MAC bypassed entirely.


// Run the message through the RLC-UM entity for CCCH.
void MaccBase::writeHighSideCcch(ByteVector &sdu, const string descr)
{
	PATLOG(1,"MaccBase::writeHighSideCcch sizebits:"<<sdu.sizeBits()<<" "<<descr);
	mCcchRlc->rlcWriteHighSide(sdu,0,0,descr);
#if USE_CCCH_Q // No, dont bother:
	{
		// Suck the sdu through the rlc right now, for historical reasons.
		URlcBasePdu *pdu;
		while ((pdu = rlc->rlcReadLowSide())) {
			PATLOG(1,"rlcReadLowSide returned size:"<<pdu->size());
			gMacSwitch.writeHighSideCcch(pdu,this);
			MaccTbDlCcch *tb = new MaccTbDlCcch(sdu);
			tb->mDescr = pdu->mDescr;
			RN_MEMLOG(MaccTbDlCcch,tb);
			mTxCcchQ.write(tb);
			delete pdu;
		}
	}
#endif
}

// Which FACH does CCCH use?
// We have to pick one based on 25.304 8.2, which is too hard.
// For now, just send out this message on all FACH.
// Even when we do support multiple FACH, the CCCH messages are few and far between,
// so I think it is low priority to fix this.
void MacSwitch::writeHighSideCcch(ByteVector &sdu,const string descr)
{
	MaccBase *mc;
	RN_FOR_ALL(CchList_t, mCchList,mc) {
		// Goes out on FACH, with 8 bit TCTF = 0x40.
		//MacTbDl *tb = new MacTbDl(8*sdu->size() + 8,sdu->size());
		//tb->fillField(0,0x40,8);				// set TCTF for CCCH.
		//tb->segment(8,sdu->sizeBits()).unpack(sdu->begin());	// add the sdu data.

		//mc->writeHighSideCcch(new MaccTbDlCcch(sdu));
		mc->writeHighSideCcch(sdu,descr);
	}
	//delete sdu;
}

MaccBase *MacSwitch::pickFachMac(unsigned urnti)
{
	int fachnum = urnti % mCchList.size();
	for (CchList_t::iterator itr = mCchList.begin(); itr != mCchList.end(); itr++) {
		if (fachnum-- == 0) return *itr;
	}
	assert(0);
}

//void MacSwitch::writeHighSideFach(TransportBlock &tb, UEInfo *uep)
//{
//	// TODO: If multiple fach, pick one based on ue.
//	pickFachMac(uep->mUrnti)->
//}

#if 0	// This is not right - the messages have to go through the UE RLC.
// This may go to FACH or a dedicated channel depending on the UE state.
//void MacSwitch::writeHighSideDcch(ByteVector *sdu, UEInfo *uep, RbId rbid)
//{
	//MacTbDl tb(sdu);
	//writeHighSideFach(tb,uep);
	//delete sdu;
//}
#endif


void MacSimple::sendDownstreamTb(MacTbDl &tb)
{
   UMTS::Time now = gNodeB.clock().get();       // (harvind) <<<<< add me
   tb.time(now);                                                   // (harvind) <<<<< add me
   tb.mScheduled = true;
   macWriteHighSide(tb);	// blocks until sent.  Probably the caller should pull.
}

// The TB size for an RLC can only vary if it is in transparent mode;
// the standard voice-channel setup does use variable size TBs on TrCh 1.
//unsigned rlc2TbSize(UEInfo *uep,RbId rbid) //TrChInfo *dltc
//{
//	URlcTrans *rlc = uep->getRlcDown(rbid);
//	// The mac header size is a constant for a particular allocated rlc, since they apply
//	// to only one UE state, so we dont need to recompute all the time, but its cheap:
//	TrChInfo *dltc = uep->ueGetConfig()->l1GetDlTrChInfo(tcid);
//	unsigned dlmacbits = macHeaderSize(dltc->mTransportChType,rlc->mrbid,dltc->mTcIsMultiplexed);
//	int rlctbsize = rlc->rlcGetFirstPduSizeBits() + dlmacbits;
//	return rlctbsize;
//}

void MacWithTfc::sendDownstreamTbs(MacTbs &tbs)
{
   unsigned numTrCh = tbs.mTfc->getNumTrCh();
   LOG(INFO) << "numTrCh: " << numTrCh;
   unsigned totalTb = 0;
   for (TrChId tcid = 0; tcid < numTrCh; tcid++) {
	LOG(INFO) << "numTb: " << tbs.getNumTb(tcid) << " tcid: " << tcid;
	unsigned numTb = tbs.getNumTb(tcid);
	LOG(INFO) << "Sched. Tbs of " << numTb << " for time " << gNodeB.clock().get();
   	for (unsigned tbIx = 0; tbIx < numTb; tbIx++) {
		TransportBlock *tb = tbs.getTB(tbIx,tcid);
		UMTS::Time now = gNodeB.clock().get();       // (harvind) <<<<< add me
   		tb->time(now);                                                   // (harvind) <<<<< add me
   		tb->mScheduled = true;
		totalTb++;
	}
   }
   //LOG(INFO) << "Sched writing high side at time " << gNodeB.clock().get();
   tbs.mTime = gNodeB.clock().get();
   if (totalTb > 0) mccDownstream->l1WriteHighSide(tbs);
   //LOG(INFO) << "Sched done at time " << gNodeB.clock().get();
}

// Get the number of each TB [Transport Block] size that is ready to send.
// TfcMap is the result.
// Technically, the term TBS [Transport Block Set] applies only to uplink,
// but the concept applies to downlink so we use the same term.
// As a simplifying assumption, all TB on a TrCh are the same size.
void MacWithTfc::findTbAvail(UEInfo *uep, TfcMap *map)
{
	RN_UE_FOR_ALL_DLTRCH(uep,tcid) {
		RN_UE_FOR_ALL_RLC(uep,rbid,rlc) {
			// For simplicity we are assuming there is only one TB size per TrCh,
			// and therefore any rlc PDUs must fit it in it, by design.
			// This assumption will not be true for voice channels, but they dont
			// send multiple TB at a time, so I think we will handle
			// that as a special case when we get there.
			if (rlc->mTcid == tcid) {
				map->tfcMapAdd(tcid,rbid,rlc->mDown->rlcGetPduCnt());
			}
		} // for each logical channel
	} // for each trch
}

bool MacWithTfc::matchTfc(RrcTfc *tfc, UEInfo *uep, TfcMap *map)
{
	//for (TrChId tcid = 0; tcid < mMasterConfig->mTrCh.dl()->getNumTrCh(); tcid++)
	// All TrCh have to match for the tfc to match.
	RN_UE_FOR_ALL_DLTRCH(uep,tcid) {
		RrcTf *tf = tfc->getTf(tcid);	// The Transport Format for this TrCh in this TFCS.
		int tbSize = tf->getTBSize();
		unsigned numTB = tf->getNumTB();
		if (tbSize == 0 || numTB == 0) { continue; }	// 0 matches anything.

		if (map->getNumTbAvail(tcid) < numTB) { return false; }

	} // for each trch
	return true;	// tfc matches
}

// Find a TFC for the data waiting in this UE, or return NULL on failure.
// Also return the logIdMap which is the logical channel to be applied to each TrCh.
// Note: A failure indicates the data in the UE did not match any TFC.
// In the general case, this could happen because because data is not ready on
// synchronized channels, for example, for AMR data, there is data in one
// sub-rab flow but not the others.
// However, for us, this is probably a bug.
bool MacWithTfc::findTfcForUe(UEInfo *uep,TfcMap *result)
{
	// Select TFC based on number of blocks in each TrCh/logical channel.
	// There could be multiple matches, including one that is all zeros, so we want
	// to select the TFC with the most bytes.
	// We are ignoring many complicating factors, including:
	//  o each logical channel could be mapped to multiple TrCh: we do not support this.
	//  o logical channel -> rbid mapping: we use 1-to-1 mapping.
	//	o variable TB sizes: we assume only one TB size per RLC; more comments on this elsewhere.
	result->tfcMapClear();
	unsigned bestTfcSize = 0;			// Number of bytes in bestTfc
	TrChConfig *config = uep->ueGetTrChConfig();

	// Step 1: The RLC data is pulled on demand, so start by sucking data through the RLCs.
	// up to a maximum number of bytes defined in any TF for any TrCh in the TFS,
	// which is overkill but its ok.
	// Twould be better to add a function to check the RLC high-side queues
	// and compute this, but quite complicated for RLC-AM and this was easier.
	uep->uePullLowSide(config->dl()->getMaxAnyTfSize()/8);

	// Step 2: How many TBs avail on each TrCh in this UE?
	findTbAvail(uep,result);

	// Step 3: Find a TFC to match the avail TB.
	RrcTfcs *tfcs = config->dl()->getTfcs();
	// For each TFC [Transport Format Combination] in the TFCS [TFC Set]
	//for (unsigned tfcid = 0; tfcid < tfcs->mNumTFC; tfcid++)
	for (RrcTfc *tfc = tfcs->iterBegin(); tfc != tfcs->iterEnd(); tfc++) {
		if (matchTfc(tfc,uep,result)) {
			// We will keep the tfc with the largest size.
			unsigned tfcsize = tfc->getTfcSize();
			if (tfcsize >= bestTfcSize) {
				bestTfcSize = tfcsize;
				result->mtfc = tfc;
			}
		}
	} // for each tfc.

	return result->mtfc != 0;
}

// Simplified version:
// Assuming there is only one transport channel,
// just look for a TFC with the specified transport block size.
RrcTfc *MacWithTfc::findTfcOfTbSize(RrcTfcs *tfcs, TrChId tcid, unsigned tbsize)
{
	for (RrcTfc *tfc = tfcs->iterBegin(); tfc != tfcs->iterEnd(); tfc++) {
		RrcTf *tf = tfc->getTf(tcid);	// The Transport Format for this TrCh in this TFCS.
		if (tbsize == tf->getTBSize() && 1 == tf->getNumTB()) {
			return tfc;
		}
	}
	return NULL;
}

// Just look for a pdu, any pdu, and send it.
bool MaccSimple::flushUE()
{
	UEInfo *uep;
	//printf("BEFORE LOCK\n");
	ScopedLock lock(gRrc.mUEListLock);
	//printf("AFTER LOCK\n");
	//printf("LIST size=%d\n",gRrc.mUEList.size());
	RN_FOR_ALL(Rrc::UEList_t,gRrc.mUEList,uep) {
		if (uep->ueGetState() != stCELL_FACH) {continue;}
		if (gMacSwitch.pickFachMac(uep->mURNTI) != this) {continue;}
		// Look in each logical channel.
		// The ones that might have something in them are 1,2,3.
		// Currently we dont hook up 5 and above in CELL_FACH state,
		// but we'll just check all anyway.  This may be wrong.
		RN_UE_FOR_ALL_RLC_DOWN(uep,rbid,rlcp) {
			ByteVector *pdu = rlcp->rlcReadLowSide();
			if (! pdu) {continue;}
                	LOG(INFO) << "Found RLC pdu on rb: " << rbid << "pdu: " << *pdu;
			// Format up a TransportBlock and send it off.
			// For this case we send a reference instead of a pointer to allocated.
			MaccTbDl tb(macGetDlTrBkSz(),pdu,uep,rbid);
			sendDownstreamTb(tb);
			delete pdu;
			return true;
		}
	}
	return false;
}

bool MaccSimple::flushQ()
{
#if USE_CCCH_Q
	MacTbDl *tb;
	if ((tb = mTxCcchQ.readNoBlock())) {
		PATLOG(1,"MaccSimple::flushQ size:"<<tb->size());
		sendDownstreamTb(*tb);
		delete tb;
		return true;
	}
#else
	// Now we can treat the ccch rlc like any other.
	ByteVector *pdu = mCcchRlc->rlcReadLowSide();
	if (pdu) {
		MaccTbDlCcch tb(macGetDlTrBkSz(),pdu);
		sendDownstreamTb(tb);
		delete pdu;
		return true;
	}
#endif
	return false;
}

void MaccSimple::macWriteLowSideTb(const TransportBlock&tb, TrChId tcid)
{
	gMacSwitch.macWriteLowSideRach(MacTbUl(tb));
}


void MaccWithTfc::macWriteLowSideTb(const TransportBlock&tb, TrChId tcid)
{
        gMacSwitch.macWriteLowSideRach(MacTbUl(tb));
}

bool MaccWithTfc::flushUE()
{
	// Step 1: Pick the UE that is going to use this FACH.
	// Step 1a: First find the UE with the highest priority message waiting.
	// Step 1b: Among UEs from step 1a, it would be nice to pick the one with the most
	//		data ready to go.
	UEInfo *chosenUE = 0;
	TfcMap chosenMap;
	unsigned chosenPriority = 100;	// In this case, low priority is better.
	unsigned chosenSize = 0;
	UEInfo *uep = 0;	// unused init to shut up gcc.

	{
		gRrc.mUEListLock.lock();
		RN_FOR_ALL(Rrc::UEList_t,gRrc.mUEList,uep) {
			if (uep->ueGetState() != stCELL_FACH) {continue;}
			if (gMacSwitch.pickFachMac(uep->mURNTI) != this) {continue;}

			unsigned uePriority = 10000;
        		uep->uePullLowSide(1);
			unsigned ueBytesAvail = uep->getDlDataBytesAvail(&uePriority);
			// Check ueBytesAvail just to avoid wasting effort if we already have a non-empty TFC.
			if (( chosenUE == 0 || ueBytesAvail) && uePriority <= chosenPriority) {
				TfcMap tmpMap;
				if (! findTfcForUe(uep,&tmpMap)) {
					// No TFC match for data waiting in UE.
					// Once we start using MAC to synchronize TrCh, this may be expected,
					// but for us now this is probably a bug.
					LOG(WARNING) << "mac-c: No tfc matched available data in UE";
					continue;
				}
				unsigned tmpSize = tmpMap.mtfc->getTfcSize();
				// Is the TFC either higher priority or have more bytes than the chosen one?
				if (uePriority < chosenPriority || tmpSize > chosenSize) {
					chosenUE = uep;
					chosenPriority = uePriority;
					chosenSize = tmpSize;
					chosenMap = tmpMap;
				}
			}
		}
		gRrc.mUEListLock.unlock();
	}

	if (chosenUE == 0) return false;	// Nothing to send anywhere.
	if (chosenUE->ueGetState() != stCELL_FACH) {return false;} // in case user switched states during above loop
	MaccTbs tbs(chosenUE,chosenMap);
	sendDownstreamTbs(tbs);
	tbs.clear();
	return true;
}

bool MaccWithTfc::flushQ()
{
	MaccTbDlCcch *tb;
#if USE_CCCH_Q
	tb = mTxCcchQ.readNoBlock();
	if (tb) {
#else
	URlcBasePdu *pdu = mCcchRlc->rlcReadLowSide();
	if (pdu) {
		tb = new MaccTbDlCcch(macGetDlTrBkSz(),pdu);
		tb->mDescr = pdu->mDescr;
		RN_MEMLOG(MaccTbDlCcch,tb);
		delete pdu;
#endif
                LOG(NOTICE) << "Found CCCH RLC pdu";

		// Make a tbs out of a single tb.  Use the TFS for CCCH.
		// There better not be multiple TrCh, and the logical channel is SRB0.
		// So just look through the TFS for one with this size.
		RrcTfcs *tfcs = gRrcCcchConfig->getDlTfcs();
		RrcTfc *tfc = findTfcOfTbSize(tfcs,0,tb->size());
		// This is a CCCH TransportBlock, which doesnt matter, but there is only one of them.
		MacTbsSingle tbs(tfc,tb);
		sendDownstreamTbs(tbs);
		tbs.clear();
		return true;
	}
	return false;
}

// Send one TransportBlock downstream, and return true if we did.
bool MacdSimple::flushUE()
{
	// Look in each logical channel.
	// The ones that might have something in them are 1,2,3 and 5, but we'll just check all.
        if (mUep->ueGetState() != stCELL_DCH) return false; 
	RN_UE_FOR_ALL_RLC_DOWN(mUep,rbid,rlcp) {
		URlcBasePdu *pdu = rlcp->rlcReadLowSide();
		if (! pdu) {continue;}
		// Format up a TransportBlock and send it off.
		// For this case we send a reference instead of a pointer to allocated.
		LOG(INFO) << "MacdSimple found RLC pdu on rb: " << rbid;
		LOG(INFO) << "Block: " << *pdu;
		//LOG(INFO) << "byte 0 : " << pdu->getByte(0);
		//if (pdu->getByte(0) != 0x80) { delete pdu; return true;}
                //LOG(INFO) << "DlTrBkSz: " << macGetDlTrBkSz();
		MacdTbDl tb(macGetDlTrBkSz(),pdu,rbid,true);	// Always multiplexed
		//tb.mTfci = 1;
		tb.mDescr = pdu->mDescr;
		sendDownstreamTb(tb);
		delete pdu;
		return true;
	}
	return false;
}

// Send one tbs downstream, and return true if we did.
bool MacdWithTfc::flushUE()
{
	//LOG(INFO) << "flushUE: ueGetState at time " << gNodeB.clock().get();
	if (mUep->ueGetState() != stCELL_DCH) { return false; } 	// This is Harvinds idea.
	//LOG(INFO) << "flushUE: uePullLowSide at time " << gNodeB.clock().get();
	mUep->uePullLowSide(mUep->ueGetTrChConfig()->dl()->getMaxAnyTfSize()/8);
	TfcMap map;
	//LOG(INFO) << "flushUE: findTfcforUe at time " << gNodeB.clock().get();
	if (! findTfcForUe(mUep,&map)) {
		// No TFC matched the data waiting in UE.
		// This is bad, because there should have been an option even for no data.
		LOG(WARNING) << "mac-d: No tfc matched available data in UE";
		return false;
	}
        //LOG(INFO) << "flushUE: Macdtbs at time " << gNodeB.clock().get();
	MacdTbs tbs(mUep,map);	// This handles the logical channel multiplexing
        //LOG(INFO) << "flushUE: sendDownstreamTbs at time " << gNodeB.clock().get();
	sendDownstreamTbs(tbs);
	tbs.clear();
	return true;
}

//// The service loop for FACH.
//void *MaccBase::macServiceLoop(void*arg)
//{
//	MaccBase *self = (MaccBase*)arg;	// gag
//	while (1) {
//		// The entire L1 is driven from here.  L1 blocks at the bottom.
//		// We should waitToSend and then send one block;
//		// note that the wait time depends on the TTI, but that is handled in L1.
//		// flushQ sends any CCCH messages which are in a common queue;
//		// flushUE sends any DCCH messages pending in any UE.
//		// I am wondering if it permissible for CCCH messages to intermingle
//		// with DCCH messages.
//		self->mDownstream->waitToSend();
//		self->flushQ() || self->flushUE();
//	}
//	return 0;
//}
//
//void MaccBase::macStart()
//{
//	macThread.start(macServiceLoop,this);
//}
//
//// TODO: Use a single serviceloop for all the macd.
//void *MacdBase::macServiceLoop(void*arg)
//{
//	MacdBase *self = (MacdBase*)arg;	// gag
//	while (1) {
//		self->mDownstream->waitToSend();
//		self->flushUE();
//	}
//	return 0;
//}
//
//void MacdBase::macStart()
//{
//	macThread.start(macServiceLoop,this);
//}

void MacdBase::macService(int fn)
{
	// Eg, for TTI=20ms only send on even numbered frames.
	if (fn % macGetDlNumRadioFrames()) { return; }
	flushUE();
}

// TODO: Wait for fn % log2(TTI) or something like that
void MaccBase::macService(int fn)
{
	if (fn % macGetDlNumRadioFrames()) { return; }
	// The entire L1 is driven from here.
	// flushQ sends any CCCH messages which are in a common queue;
	// flushUE sends any DCCH messages pending in any UE.
	// I am wondering if it is permissible for CCCH messages to intermingle
	// with DCCH messages.
	flushQ() || flushUE();
}

// Single service loop for all MAC entities.
// I am not using the prevWriteTime/nextWriteTime paradigm that was used in
// the GSM code because:  1.  We no longer have a complicated table to lookup
// the next write time slot, we just send data every TTI period.
// 2. Especially for packet data we dont care if we miss a slot, we just want
// to send on the next future opportunity.
// 3. The encoder class is not quite the right place to put that code anyway,
// because there can be multiple TrCh encoders per physical channel.
// The fly in the ointment is that different TrCh are allowed to run at
// different TTIs, a case I hope we dont support.  But in that case,
// the MAC is still responsible for picking the TFCI for each TTI,
// so it seems like the wait functionality still has to be in the MAC.
void *MacSwitch::macServiceLoop(void *arg)
{
	UMTS::Time mNextWriteTime;
	while (1) {
		// Roll forward to the next write opportunity.
		//mNextWriteTime = gNodeB.clock().get()+1;
		//gNodeB.clock().wait(mNextWriteTime);
		int nowFN = gNodeB.clock().get().FN();

		while (gNodeB.clock().get() <= UMTS::Time(nowFN,0)) {
        		struct timespec howlong, rem;
        		howlong.tv_sec = 0;
			// Its not * 1024, its * 1000
        		//howlong.tv_nsec = UMTS::gSlotMicroseconds << 10;
        		howlong.tv_nsec = UMTS::gSlotMicroseconds * 1000/2;
        		while (0 != nanosleep(&howlong, &rem)) { howlong = rem; }
		}
		// Lock the list of mac entities and service each.
		gMacSwitch.mMacListLock.lock();
		MacEngine *mac;
		RN_FOR_ALL(MacList_t,gMacSwitch.mMacList,mac) {
			LOG(DEBUG) << "Service MAC " << mac << " at time " << gNodeB.clock().get();
			mac->macService(nowFN);
			LOG(DEBUG) << "Service MAC " << mac << " done at time " << gNodeB.clock().get();
		}
		gMacSwitch.mMacListLock.unlock();
	}
	return 0;
}


};	// namespace UMTS
