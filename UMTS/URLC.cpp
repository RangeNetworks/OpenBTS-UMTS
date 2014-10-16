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

#define URLC_IMPLEMENTATION 1
#include "URRC.h"			// For UEInfo guts
#include "URRCMessages.h"
#include "Configuration.h"
#include "MACEngine.h"		// For macHeaderSize
#include "Logger.h"
#include "URLC.h"

//#define RLCLOG(stuff...) PATLOG(4, "RLC " << rlcid() <<":" << format(stuff) << mUep);
#define RLCLOG(stuff...) LOG(DEBUG) << "RLC " << rlcid() <<":" << format(stuff) << mUep;
#define RLCERR(stuff...) LOG(ERR) << "RLC "<< rlcid() <<":" << format(stuff) << mUep;

namespace UMTS {

const char*URlcMode2Name(URlcMode mode)
{
	switch (mode) {
	case URlcModeTm: return "TM";
	case URlcModeUm: return "UM";
	case URlcModeAm: return "AM";
	default: assert(0);
	}
}

// This is a temporary stub to send discard messages.
// This is what is defined in the spec, because normally these message would
// go to an MSC controller far away.  But we will probably do not do it this way,
// we will probably have a UEInfo struct and call an appropriate function
// inside the UE if messages are discared on SRBs, which indicates that the UE
// has become disconnected.
void informL3SduLoss(URlcDownSdu *sdu)
{
	PATLOG(1,"discarding sdu sizebytes="<<sdu->size());
	LOG(WARNING) << "discarding sdu sizebytes="<<sdu->size();
	// There, you're informed.
}

int URlcBase::deltaSN(URlcSN sn1, URlcSN sn2)
{
	int ws = mSNS/2;
	int delta = (int)sn1 - (int)sn2;
	//assert(!(delta >= ws));
	if (delta < - ws) delta += mSNS;      // modulo the sequence space
	if (delta > ws) delta -= mSNS;
	return delta;
}

// Warning: the numbers can be negative.
URlcSN URlcBase::addSN(URlcSN sn1, URlcSN sn2)
{
	return ((unsigned)((int)sn1 + (int)sn2)) % (unsigned) mSNS;
}

URlcSN URlcBase::minSN(URlcSN sn1, URlcSN sn2)
{
	return (deltaSN(sn1,sn2) <= 0) ? sn1 : sn2;
}

URlcSN URlcBase::maxSN(URlcSN sn1, URlcSN sn2)
{
	return (deltaSN(sn1,sn2) >= 0) ? sn1 : sn2;
}

void URlcBase::incSN(URlcSN &psn)
{
	psn = addSN(psn,1);
}

void URlcPdu::appendLIandE(unsigned licnt, unsigned E, unsigned lisize)
{
	if (lisize == 1) {
		appendByte((licnt<<1) | E);
	} else {
		appendUInt16((licnt<<1) | E);
	}
}

void URlcPdu::text(std::ostream &os) const
{
	os <<" URlcPdu(";
	switch (rlcMode()) {
	case URlcModeTm:
		os <<"TM" <<LOGVAR2("size",size());
		break;
	case URlcModeUm:
		os <<"UM" <<LOGVAR2("sn",getSN());
		os <<LOGVAR2("payloadSize",getPayloadSize());
		os <<LOGVAR2("E",getEIndicator());
		break;
	case URlcModeAm:
		if (getAmDC()) {
			os <<"AM" <<LOGVAR2("sn",getSN());
			os <<LOGVAR2("payloadSize",getPayloadSize());
			os <<LOGVAR2("E",getEIndicator());
		} else {
			switch ((int)getField(1,3)) {
			case 0: os <<"AM control type=status sufi="<<(int)getField(4,4); break;
			case 1: os <<"AM control type=reset"; break;
			case 2: os <<"AM control type=reset ack"; break;
			default: os <<"AM control type=unrecognized"; break;
			}
		}
	}
	os <<"\"" <<mDescr <<"\"";
	os <<")";
}

// This pops out of the high side of the RLC.
struct RrcUplinkMessage {
	ByteVector *msgSdu;	// memory is managed by the RrcUplinkMessage
	UEInfo *msgUep;
	RbId msgRbid;
	RrcUplinkMessage(ByteVector *wSdu, UEInfo *wUep, RbId wRbid) : msgSdu(wSdu), msgUep(wUep), msgRbid(wRbid) {}
	virtual ~RrcUplinkMessage() { if (msgSdu) delete msgSdu; }
};

// The high side of the RLCs stuffs messages into these loops, then the threads below pass them on.
// All started by l2RlcStart()
static InterthreadQueue<RrcUplinkMessage> gRrcUplinkQueue;
static InterthreadQueue<RrcUplinkMessage> gSgsnUplinkQueue;
static Thread mSgsnUplinkThread;
static Thread mRrcUplinkThread;

static void *rrcUplinkServiceLoop(void *arg)
{
	while (1) {
		RrcUplinkMessage *msg = gRrcUplinkQueue.read();
		msg->msgUep->ueRecvDcchMessage(*msg->msgSdu,msg->msgRbid);
		delete msg;
	}
	return NULL;
}


static void *sgsnUplinkServiceLoop(void *arg)
{
	while (1) {
		RrcUplinkMessage *msg = gSgsnUplinkQueue.read();
		msg->msgUep->ueRecvData(*msg->msgSdu,msg->msgRbid);
		delete msg;
	}
	return NULL;
}

void l2RlcStart()
{
	mSgsnUplinkThread.start(sgsnUplinkServiceLoop,0);
	mRrcUplinkThread.start(rrcUplinkServiceLoop,0);
}

// Send an sdu out the high side of the rlc, routed based on rbid.
void URlcRecv::rlcSendHighSide(URlcUpSdu *sdu)
{
	if (this->mHighSideFunc) {
		// This is used for testing.
		this->mHighSideFunc(*sdu,mrbid);
		delete sdu;
		return;
	}
	switch (this->mrbid) {
	case 0:
		// Probably RBInfo misconfiguration.
		// 12-29-2012 pat: This assertion is occurring; I think maybe we are deleting an AM-RLC
		// while it is still in use by the uplink.
		// The SRB0 messages were supposed to go to the RLC in the MAC-C.
		RLCERR("invalid RLC high side message to SRB0, probably RB mis-configuration");
		assert(0);
	case 1: case 2: case 3:
		// case 1 and 2 are messages to RRC.
		// case 3 is a Layer3 message, but handled the same to crack it out of its RRC message wrapper.
		gRrcUplinkQueue.write(new RrcUplinkMessage(sdu,mUep,mrbid));
		break;
	default:
		if (mrbid >= 16) {
			// RLC mis-configuration.
			assert(0);
		}
		// User data.
		gSgsnUplinkQueue.write(new RrcUplinkMessage(sdu,mUep,mrbid));
		break;
	}
}

#if 0	// previous code.  Can delete.
void URlcRecv::rlcSendHighSide(URlcUpSdu *sdu)
{
	if (this->mHighSideFunc) {
		// This is used for testing.
		this->mHighSideFunc(*sdu,mrbid);
		delete sdu;
		return;
	}

	switch (this->mrbid) {
	case 0:
		// Probably RBInfo misconfiguration.
		// The SRB0 messages were supposed to go to the RLC in the MAC-C.
		RLCERR("invalid RLC high side message to SRB0, probably RB mis-configuration");
		assert(0);
	case 1: case 2:
		// It is a message to RRC.
		mUep->ueRecvDcchMessage(*sdu,mrbid);
		delete sdu;
		break;
	case 3:
		// It is a Layer3 message, but handled the same to crack it out of its RRC message wrapper.
		mUep->ueRecvDcchMessage(*sdu,mrbid);
		delete sdu;
		break;
	default:
		if (mrbid >= 16) {
			// RLC mis-configuration.
			assert(0);
		}
		// User data.
		mUep->ueRecvData(*sdu,mrbid);
		delete sdu;
		break;
	}
}
#endif


// This is already associated with a specific RAB by being
// called within this particular URlcUMT
// Clause 8.1: The parameters are: Data, CNF Confirmation Request,
// which we wont implement, DiscardReq, which we will,
// MUI Message Unit Id for this sdu in confirm/discard messages back to layer3,
// and UE-Id type indicator (C-RNTI or U-RNTI), about which I have no clue yet,
// and doesnt make sense for what I know - the C-RNTI is only used on phy, not up here.
// Update: maybe the ue-id type indicator is used when the controlling RNC != serving RNC, which we never do.
// nope: We're going to use CNF for both Confirmation and Discard requests.
void URlcTrans::rlcWriteHighSide(ByteVector &data, bool DiscardReq, unsigned MUI,string descr)
{
	RLCLOG("rlcWriteHighSide sizebytes=%d rbid=%d descr=%s",
		data.size(),mrbid,descr.c_str());

	// pat 12-17: Changed the GGSN to pre-allocate this so we dont have to do it here.
	//ByteVector cloneData;
	//cloneData.clone(data);
	//URlcDownSdu *sdu = new URlcDownSdu(cloneData,DiscardReq,MUI,descr);
	URlcDownSdu *sdu = new URlcDownSdu(data,DiscardReq,MUI,descr);
	RN_MEMLOG(URlcDownSdu,sdu);
	ScopedLock lock(mQLock);
	//printf("pushing SDU of size: %u, addr: %0x, descr=%s\n",data.size(),sdu,descr.c_str());
	mSduTxQ.push_back(sdu);

                LOG(INFO) << "Bytes avail: " << rlcGetSduQBytesAvail();
	// Check for buffer overflow.
	if (!mSplitSdu && rlcGetSduQBytesAvail() > mTransmissionBufferSizeBytes)
	{
		// Throw away sdus.  We toss the oldest sdu that is not in progress,
		// which means just toss them off the front of the queue.
		URlcDownSdu *sdu = NULL;
		
		// Delete all but one of the sdus.
		// We leave one sdu in the queue to mark the spot where deletions ocurred.
		//LOG(INFO) << "Bytes avail: " << rlcGetSduQBytesAvail();
		while (mSduTxQ.size() && rlcGetSduQBytesAvail() > mTransmissionBufferSizeBytes) {
			//if (sdu) { sdu->free(); mVTSDU++; }
			if (sdu) { sdu->free(); mVTSDU++; }
			sdu = mSduTxQ.pop_front();
                        LOG(INFO) << "Discarding sdu %0x ," << sdu << " TxQ size: " << rlcGetSduQBytesAvail();
			if (!sdu->mDiscarded) {informL3SduLoss(sdu);}
		}

		// Shove the last deleted sdu back in the queue to mark the spot.
		// We null out mData to indicate that one or more sdus were deleted at this spot.
		//if (sdu->mData) {
			//delete sdu->mData;
			//sdu->mData = NULL;
		//}
		//sdu->mDiscarded = true;
		//mSduTxQ.push_front(sdu);
	}
}

// About the LI Length Indicator field.
// If the SDU exactly fills the PDU and there is no room for an LI field,
// you set the LI in the subsequent PDU as follows:
// - If the next PDU is exactly filled by a complete SDU (remembering that the length
// of the following PDU is less 1 to make room for this extra LI field)
// then use LI=0x7d, otherwise, LI=0.
// Note that the PDU must be sent even it if has not SDU data.
// If the PDU has padding, add LI=0x7f after the final length indicator.
// The following is mandatory for uplink, and 11.2.3.1 implies that it is optional in downlink.
// Every PDU that starts a new SDU and doesnt have any of the other special LI values,
// must start with LI=0x7c, so the nodeB can tell it is the start of a packet even if it did not
// receive the previous PDU.



// Common routine for Am and Um modes to fill the data part of the downlink pdu.
// The Am and Um pdus have different header sizes, which is passed in pduHeaderSize.
// The result is a discard notification which is true if UM sdus were discarded.
// The 'alternate interpretation of HE' is a wonderful thing that indicates
// the end of the SDU and gets rid of many of the special cases below,
// but unfortunately it only exists in AM, and we still need the special
// cases for UM, so I did not implement it.
bool URlcTransAmUm::fillPduData(URlcPdu *result,
	unsigned pduHeaderSize,
	bool *pNewSdu)	// Set if this pdu is the start of a new sdu.
{
	ScopedLock lock(mQLock);

	// Step one: how many sdus can we fit in this pdu?
	// remaining = How many bytes left in output PDU.
	unsigned remaining = result->size() - pduHeaderSize;

	bool mSduDiscarded = false;	// Set if discarded SDU is detected.

	unsigned sducnt = 0;		// Number of whole or partial sdus sent.
	unsigned sdufinalbytes = 0;	// If non-zero, final sdu is split and this number of bytes sent.
	unsigned vli[100];	// We will gather up the LI indicators as we go.
	unsigned libytes = mConfig->mDlLiSizeBytes;
	int licnt = 0;

	URlcDownSdu *sdu = mSplitSdu ? mSplitSdu : mSduTxQ.front();
	bool newSdu = sdu && !mSplitSdu;	// We are starting a new sdu.
	if (pNewSdu) {*pNewSdu = newSdu;}

	if (sdu == NULL) {
		// This special case occurs if the previous sdu exactly filled the
		// the pdu, but we need an indication in the next pdu (which is the
		// one we are currently creating) to mark the end of that sdu,
		// but the incoming sdu queue is empty.
		// We need to output one pdu with only li flags, no data, to notify
		// the receiver on the other end of the end of the sdu.
		if (mLILeftOver) {
			remaining -= libytes;	// For the LI field we are about to add.
			vli[licnt++] = (mLILeftOver == 1) ? 0 : 0x7ffb;
			mLILeftOver = 0;
		} else {
			assert(0);	// Caller prevents this by checking pdusFinished first.
		}
		result->mDescr = string("li_only");
	} else {
		result->mDescr += sdu->mDescr;
	}

	URlcDownSdu *sdunext = NULL;
	for ( ; sdu; sdu = sdunext) {
		if (! mConfig->mIsSharedRlc) { // SharedRlc only sends one SDU at a time.
			sdunext = (sdu == mSplitSdu) ? mSduTxQ.front() : sdu->next();
		}
		if (sdu->mDiscarded) {
			// SDU was discarded, possibly because transmission buffer full.
			if (mRlcMode == URlcModeUm) {
				// Perform function of 11.2.4.3: SDU discard without explicit signaling.
				// Also applies to the case of SDU discard not configured when transmission buffer
				// is full and a partially sent SDU is discarded as per 9.7.3.5, however,
				// that depends on whether we decide to discard in-progress sdus or not.
				// I am going to not do that, since the in-progress sdu probably represents
				// usable data on the other end.
				if (mSduDiscarded == false) {
					mSduDiscarded = true;
					if (mConfig->mRlcDiscard.mSduDiscardMode != TransmissionRlcDiscard::NotConfigured) {
						if (sducnt) {
							// We need to inform the peer where the missing SDU occurred
							// so we cant put back-to-back SDUs into the PDU with
							// the missing SDU (not) between them, so stop now.
							// And leave the discarded SDU here so we will see it next time.
							break;
						} else {
							// Force fillPduData to add an extra first Length Indicator
							// to indicate start of SDU.
							mLILeftOver = 1;
						}
					}
				}
			}
			// Skip over the discarded sdu.
			mVTSDU++;
			sdu->free();
			continue;
		}

		sducnt++;	// We will output all or part of this sdu.
		unsigned sdusize = sdu->size();	// Size of the whole remaining SDU.

		// Note: a non-zero mLILeftOver flag here implies this is the start of a new SDU,
		// but not the reverse, ie, mLILeftOver == 0 says nothing about start of SDU.
		if (mConfig->UM.mAltEBitInterpretation) {
			// We dont use "alternative-E bit interpretation".
			// The following special LI values are used only with "alternative E-bit interpretation"
			// 0x7ffa (1010), 0x7ffd (1101), 0x7ffe (1110) as first LI field
			assert(0);
		} else {
			// Handle the special case of extra LI field to mark previous PDU.
			// The previous PDU ended on, or 1 byte short of, the end of the PDU.
			if (mLILeftOver) {
				assert(licnt == 0);
				remaining -= libytes;	// For the LI field we are about to add.
				vli[licnt++] = (mLILeftOver == 1) ? 0 : 0x7ffb;
				mLILeftOver = 0;
			}
		}

		// And now, back to our originally scheduled programming:
		// Notes: the 0x7ffc (1100) special LI value is only used in uplink, not downlink.
		// 5-15-2012: However, we are going to try putting it in anyway; the spec says
		// the UE 'should be prepared to receive it' on downlink.
		// We might want to do this only if mConfig->mIsSharedRlc
		if ((result->rlcMode() == URlcModeUm) && (licnt == 0 && newSdu) ) {
			remaining -= libytes;	// For the LI field we are about to add.
			vli[licnt++] = 0x7ffc;
		}

		if (sdusize > remaining) {
			sdufinalbytes = remaining;
			remaining = 0;
			// No LI indicator - this sdu spills over the end of the pdu into the next pdu.
			//mSduIsSplit = true;
		} else if (sdusize == remaining) {
			mLILeftOver = 1;	// Special case: LI indicator goes in next pdu.
			remaining = 0;
		} else if (libytes == 2 && sdusize == remaining-1) {
			mLILeftOver = 2;	// Special case: LI indicator goes in next pdu.
			remaining = 1;
		} else if (libytes == 2 && sdusize == remaining-3) {
			// In above, 3 == 2-byte LI + 1 more byte, which is not enough for another LI field.
			vli[licnt++] = sdusize;
			remaining = 1;
		} else if (libytes == 2 && sdusize == remaining+1) {
			// Split pdu and put the final byte in the next pdu.
			sdufinalbytes = remaining;
			vli[licnt++] = remaining;
			//mSduIsSplit = true;
			remaining = 0;
		} else {
			remaining -= libytes;
			assert(sdusize <= remaining);	// We handled all the other special cases above.
			vli[licnt++] = sdusize;
			remaining -= sdusize;
		}

		if (remaining <= libytes) {
			// Not enough room left to do anything useful.
			break;
		}

		if (mConfig->UM.mSN_Delivery) {	// This config option means do not concatenate SDUs in a PDU
			break;
		}
		if (licnt == 100) break;
		RLCLOG("fillpdu resultsize:%d sdusize=%d sdufinalbytes=%d remaining=%d",result->size(),sdusize,sdufinalbytes,remaining);
	} // for sdu

	if (remaining >= libytes) {
		result->mPaddingLILocation = pduHeaderSize + licnt * libytes;
		result->mPaddingStart = result->size() - remaining;
		vli[licnt++] = 0x7fff;	// Mark padding
		remaining -= libytes;
	} else {
		result->mPaddingLILocation = 0;	// This is an impossible value.
		result->mPaddingStart = 0;
	}

	// Create the result ByteVector, add the LI+E fields.
	// E==1 implies a following LI+E field.
	// We dont implement the "Alternative E-bit interpretation".
	result->setEIndicator(licnt ? 1 : 0);	// Set the E or HE bit in the header.
	result->setAppendP(pduHeaderSize);
	if (licnt) {
		for (int i = 0; i < licnt; i++) {
			result->appendLIandE(vli[i],i != licnt-1,libytes);
		}
		RLCLOG("fillpdu after adding li, headersize:%d",result->size()); 
	} else {
		RLCLOG("fillpdu no li, headersize:%d",result->size()); 
	}

	// Step two: add the data from sducnt SDUs to the result PDU.
	for (unsigned n = 0; n < sducnt; n++) {
		if (mSplitSdu) {
			sdu = mSplitSdu; mSplitSdu = NULL;
		} else {
			sdu = mSduTxQ.pop_front();
		}

		// Need these checks to assure the mSplitSdu didn't just get discarded b/c mSduTxQ is too big.
		if (!sdu) {LOG(NOTICE) << "NULL Pointer in mSduTxQ"; break;}	// This is a bug.
		if (!sdu->sduData()->size()) { LOG(INFO) << "Empty SDU in mSduTxQ"; break;} // This is a bug.
		// If this is the final sdu and it is a partial one:
		
		if (n+1 == sducnt && sdufinalbytes) {
			// Copy part of this sdu.
			//LOG(INFO) << "sduData: " << *(sdu->sduData());
			result->append(sdu->sduData()->begin(),sdufinalbytes);
			//printf("sdu->sduData(): %0x\n",sdu->sduData());
			sdu->sduData()->trimLeft(sdufinalbytes);
			mSplitSdu = sdu;
			RLCLOG("fillpdu appending (partial) %d sdu bytes, result=%d bytes",
				sdufinalbytes, result->size());
		} else {
			// Copy the entire SDU.
			result->append(sdu->sduData());
			RLCLOG("fillpdu appending %d sdu bytes, result=%d bytes",
				sdu->sduData()->size(), result->size());
			mVTSDU++;
			sdu->free();
			//printf("Freeing SDU: %0x\n",sdu);
			/***  We now pad the entire remaining with 0, below.
			if (n+1 == sducnt && result->size() == dataSize-1) {
				// Special case of 1 extra byte:  It is filled with 0.
				// "In the case where a PDU contains a 15-bit "Length Indicator" indicating
				// that an RLC SDU ends with one octet left in the PDU,
				// the last octet of this PDU shall:
				// - be padded by the sender and ignored by the Receiver though there is no
				// "Length Indicator" indicating the existence of padding.
				result->appendByte(0);
			}
			***/
		}
	}

	if (remaining) {
		result->appendFill(0,remaining);
	}
	mVTPDU++;
	return mSduDiscarded;
}

// MAC reads the low side with this.
// Caller is responsible for deleting this.
URlcBasePdu *URlcTransAmUm::rlcReadLowSide()
{
	URlcPdu *pdu;
	if (mRlcState == RLC_STOP) {return NULL;}
	bool wasqueued = true;
	if (! (pdu = mPduOutQ.readNoBlock())) {
		pdu = readLowSidePdu();
		wasqueued = false;
	}
	if (pdu) RLCLOG("readlLowSide(q=%d,sizebytes=%d,payloadsize=%d,descr=%s,rb=%d header=%s)",
		wasqueued,pdu->size(),pdu->getPayloadSize(),
		pdu->mDescr.c_str(),mrbid,pdu->segment(0,2).hexstr().c_str());

	return pdu;
}

// Just turn the SDU into a PDU and send it along.
// Caller is responsible for deleting this.
URlcBasePdu *URlcTransTm::rlcReadLowSide()
{
	ScopedLock lock(mQLock);
	if (mRlcState == RLC_STOP) {return NULL;}
	if (!mSplitSdu) {
	  while (URlcDownSdu *sdu = mSduTxQ.pop_front()) {
		if (sdu->mDiscarded) {
			sdu->free();
		} else {
			RLCLOG("readlLowSide(sizebits=%d,descr=%s,rb=%d)",
				sdu->sizeBits(), sdu->mDescr.c_str(),mrbid);
			return sdu;
		}
	  }
	}
	return NULL;	// Shouldnt happen - MAC should check q size first.
}

// Pull data through the RLC to fill the output queue, up to the specified amt,
// which is the maximum amount needed for any Transport Format.
void URlcTransAmUm::rlcPullLowSide(unsigned amt)
{
	URlcPdu *vec;
	int cnt = 0;
	while (mPduOutQ.totalSize() < amt && ((vec = readLowSidePdu())) ) {
		mPduOutQ.write(vec);
		cnt++;
		//LOG(INFO) << "amt: " << amt << " sz: " << mPduOutQ.totalSize();
	}

	if (cnt) LOG(INFO) << format("rlcPullLowSide rb%d amt=%d sent %d pdus, pduq=%d(%dB), sduq=%d(%dB)",
				mrbid,amt,cnt,mPduOutQ.size(),mPduOutQ.totalSize(),mSduTxQ.size(),mSduTxQ.totalSize());
}

URlcPdu *URlcTransUm::readLowSidePdu()
{
	if (pdusFinished()) { return NULL; }
	URlcPdu *result = new URlcPdu(mConfig.mDlPduSizeBytes,this,"dl um");
	RN_MEMLOG(URlcPdu,result);

	bool newSdu = false;
	bool mSduDiscarded = fillPduData(result,1,&newSdu);
	if (newSdu && mConfig.mIsSharedRlc) { mVTUS = 0; }
	result->setUmSN(mVTUS);
	incSN(mVTUS);
	if (mSduDiscarded && mConfig.mRlcDiscard.mSduDiscardMode != TransmissionRlcDiscard::NotConfigured) {
		incSN(mVTUS);	// Informs peer that a discard occurred.
	}
	RLCLOG("readLowSidePdu sizebytes=%d",result->size());
	return result;
}

URlcPdu *URlcTransAm::getDataPdu()
{
	if (pdusFinished()) { return NULL;	} // No data waiting in the queue.

	URlcPdu *result = new URlcPdu(mConfig->mDlPduSizeBytes,parent(),"dl am");
	RN_MEMLOG(URlcPdu,result);
	result->fill(0,0,2);	// Be safe and clear out the header.
	result->setAmDC(1);		// Data pdu.
	// The spec says nothing about AM buffer overflow behavior except when
	// in "explicit signaling" modes.  If these are TCP packets, it is ok
	// to just drop them, so that's what we'll do.  Pretty easy:
	// just ignore the fillPduData returned result.
	//LOG(INFO) << "fPD time " << gNodeB.clock().get();
	fillPduData(result,2,0);
        //LOG(INFO) << "fPD done " << gNodeB.clock().get();
	result->setAmSN(mVTS);
	result->setAmP(0);	// Until we know better.
	RLCLOG("getDataPdu VTS=%d,VTA=%d bytes=%d header=%s dc=%d sn=%u",
		(int)mVTS,(int)mVTA,result->sizeBytes(),result->segment(0,2).hexstr().c_str(),
		result->getBit(0),(int)result->getField(1,12));
	if (mPduTxQ[mVTS]) {
		RLCERR("RLC-AM internal error: PduTxQ at %d not empty",(int)mVTS);
		delete mPduTxQ[mVTS];
	}
	mPduTxQ[mVTS] = result;
	incSN(mVTS);
	return result;
}

// Return a reset or reset ack pdu depending on type.
URlcPdu *URlcTransAm::getResetPdu(PduType type)
{
	RLCLOG(type == PDUTYPE_RESET ? "Sending reset pdu" : "Sending reset_ack pdu");
	URlcPdu *pdu = new URlcPdu(mConfig->mDlPduSizeBytes,parent(),"dl reset");
	RN_MEMLOG(URlcPdu,pdu);
	pdu->fill(0);
	pdu->setAppendP(0);
	pdu->appendField(0,1);		// DC == 0 for control pdu;
	pdu->appendField(type,3);
	pdu->appendField(type == PDUTYPE_RESET ? mResetTransRSN : mResetAckRSN,1);
	pdu->appendField(0,3);	// R1, reserved field;
	pdu->appendField(parent()->mDLHFN,20);
	pdu->appendZero();
	return pdu;
}

bool URlcRecvAm::isReceiverOk()
{
	// If mVRH < mVRR something horrible is wrong.
	if (deltaSN(mVRH,mVRR) < 0) {
		RLCERR("RLC out of synchronization: mVRR=%d mVRH=%d, doing reset",
				(int)mVRR,(int)mVRH);
		return false;
	}
	return true;
}

// Return true if there are no more status pdus to report after this one.
bool URlcRecvAm::addAckNack(URlcPdu *pdu)
{
	bool lastStatusReport = true;
	bool firstStatusReport;

	// mStatusSN is the mVRR at the time the status was triggered.
	// Bring the mStatusSN up to date in case additional pdus were received
	// between the pdu that triggered the status and now, which
	// is when a pdu is being transmitted with that status.
	firstStatusReport = (deltaSN(mVRR,mStatusSN) >= 0);
	if (firstStatusReport) {
		mStatusSN = mVRR;
	}

	if (mVRR != mVRH) {
		assert(deltaSN(mVRH,mVRR) > 0);  // by definition VRH >= VRR
		assert(mPduRxQ[mVRR] == NULL); // mVRR is last in-sequence pdu received+1
		URlcSN end = mVRH;	// SN+1 of highest pdu known.
		URlcSN sn = mStatusSN;
		// If this happens the RLC is hopelessly out of synchronization aka a bug.
		// We catch this case out readLowSidePdu2() and reset the connection.

		// Gather up ranges of blocks that have not been received.
		// 9.2.2.11.4 List SUFI.  Each one can acknowledge up to 15 missing PDU ranges.
		// The outer while loop stuffs as many of those into the PDU as will fit.
		// Each range low[n] to low[n]+cnt[n] is a series of PDUs that have not been received.
		int n, low[15], cnt[15];
		bool found = false;
		while (sn != end) {
			// The final ACK needs 2 bytes and each LIST SUFI takes 1 + n*2 bytes.
			int maxN = ((int)pdu->sizeRemaining() - 3)/2;
			if (maxN > 15) { maxN = 15; } 	// Max number per LIST SUFI.
			if (maxN == 0) {break;}

			//printf("START maxN=%d sizeBits=%d sizeRemaining=%d\n",maxN,pdu->sizeBits(),pdu->sizeRemaining());
			for (n = 0; n < maxN && sn != end; n++) {
				// Scan forward until we find an unreceived PDU, or are finished.
				// We will already be sitting on an unreceived PDU the first time
				// through this loop (because sn == mVRR) or if the max cnt was reached below.
				for (; sn != end && mPduRxQ[sn]; incSN(sn)) { continue; }
				if (sn == end) { break; }
				low[n] = sn;
				cnt[n] = 1;
				// Scan forward looking for a received PDU.
				for (incSN(sn); sn != end && !mPduRxQ[sn] && cnt[n] < 16; incSN(sn)) {
					cnt[n]++;	// sn is another adjacent unreceived PDU.
				}
			}
			if (n) {
				// Output the List SUFI.
				// example output: SUFI_LIST=3, n=1, low=0,0,1, cnt=0
				pdu->appendField(SUFI_LIST,4);
				pdu->appendField(n,4);
				char debugmsg[400], *cp = debugmsg;
				//printf("BEFORE sizeBits=%d sizeRemaining=%d\n",pdu->sizeBits(),pdu->sizeRemaining());
				cp += sprintf(cp,"Ack Sufi mVRR=%d mVRH=%d missing:",(int)mVRR,(int)mVRH);
				for (int i = 0; i < n; i++) {
					// The length field in the sufi is cnt-1, ie, 0 indicates
					// that only one pdu was missing.
					pdu->appendField(low[i],12);
					pdu->appendField(cnt[i]-1,4);
					cp += sprintf(cp," %d",low[i]);
					if (cnt[i]>1) { cp += sprintf(cp,"-%d(%d pdus)",low[i]+cnt[i]-1,cnt[i]);}

					//RLCLOG("AFTER i=%d sizeBits=%d sizeRemaining=%d\n",i,pdu->sizeBits(),pdu->sizeRemaining());
				}
				RLCLOG("%s",debugmsg);
				found = true;
			}
		}

		if (sn != mVRH) {
			// There are more status reports to transmit.
			lastStatusReport = false;
		}
		mStatusSN = sn;

		if (!found && firstStatusReport) {
			// This can not happen on the first acknack, but may happen
			// on subsequent ones because the pdu that was left to report
			// has been received in the intervening period.
			RLCLOG("Ack phase error mVRR=%d mVRH=%d but no missing pdus found\n",(int)mVRR,(int)mVRH);
		}
	} else {
		// Everything is up to date.
		RLCLOG("Ack Sufi mVRR=%d mVRH=%d all ok\n",(int)mVRR,(int)mVRH);
		assert(firstStatusReport && lastStatusReport);
	}
	// 9.2.2.11.2 Ack SUFI is the last SUFI in the Status PDU.
	// The LSN field here specifies that we have received all the PDUS up to
	// LSN except the ones in the Lists we added above.
	// It sets VTA (SN+1 of last positively acked PDU) in the UE transmitter.
	// If this status pdu does not include all the unacked blocks, we
	// are required to set it to VRR.
	// The LSN is confusing because in our case, a binary bit would have sufficed.
	// However, note that if the acks will not fit in a PDU, you are allowed to
	// send them in multiple PDUs, in which case the LSN would be useful.
	// But we are not doing that.
	pdu->appendField(SUFI_ACK,4);
	//pdu->appendField(mStatusSN == mVRH ? mVRH : mVRR,12);
	pdu->appendField((firstStatusReport && lastStatusReport) ? mVRH : mVRR,12);
	return lastStatusReport;
}

URlcPdu *URlcTransAm::getStatusPdu()
{
	URlcPdu *pdu = new URlcPdu(mConfig->mDlPduSizeBytes,parent(),"dl status");
	RN_MEMLOG(URlcPdu,pdu);
	pdu->fill(0);
	pdu->setAppendP(0);
	pdu->appendField(0,1);		// DC == 0 for control pdu;
	pdu->appendField(PDUTYPE_STATUS,3);
	// Now the sufis.
	if (receiver()->addAckNack(pdu)) {
		mStatusTriggered = false;
	}

	// Zero fill to a byte boundary:
	pdu->appendZero();
	return pdu;
}

// The SUFI received by the receiver advances VTA in the transmitter.
// PDUs up to sn (or sn+1?) have been acknowledged by the peer entity.
void URlcTransAm::advanceVTA(URlcSN newvta)
{
	for ( ; deltaSN(mVTA,newvta) < 0; incSN(mVTA)) {
		if (mPduTxQ[mVTA]) { delete mPduTxQ[mVTA]; mPduTxQ[mVTA] = NULL; }
	}
}

// The status pdu with the SUFIs is received by the receiving entity but the information
// applies to and is processed by the transmitting entity.
// Side effect: if we receive an acknowledgement set mNackedBlocksWaiting and
// mVSNack to the oldest negatively acknowledged block, if any.
// SUFI 25.322 9.2.2.11
void URlcTransAm::processSUFIs2(
	ByteVector *vec,	// May come from a status pdu or a piggy-backed status pdu.
	size_t rp)		// Bit position where the sufis start in vec; they are not byte-aligned.
{
	unsigned i, j;
	URlcSN sn;
	URlcSN newva = mVTA;	// SN of oldest block nacked by this pdu.
	bool newvaValid = false;
	RLCLOG("Sufis before: VTS=%d VTA=%d VSNack=%d NackedBlocksWaiting=%d",
		(int)mVTS,(int)mVTA,(int)mVSNack,mNackedBlocksWaiting);
	while (1) {
		SufiType sufitype = (SufiType) vec->readField(rp,4);
		switch (sufitype) {
		case SUFI_NO_MORE:
			return;
		case SUFI_WINDOW:
			// The minimum and maximum values are set by higher layers, which is us,
			// so this should not happen if we set them the same.
			sn = vec->readField(rp,12);
			mVTWS = sn;
			continue;
		case SUFI_ACK: {
			URlcSN lsn = vec->readField(rp,12);
			if (!newvaValid || deltaSN(lsn,newva) <= 0) {
				advanceVTA(lsn);
			} else {
				advanceVTA(newva);
			}
			if (mNackedBlocksWaiting) advanceVS(true);

			RLCLOG("Sufis after: lsn=%d newvaValid=%d newva=%d VTS=%d VTA=%d VSNack=%d NackedBlocksWaiting=%d",
				(int)lsn,newvaValid,(int)newva,(int)mVTS,(int)mVTA,(int)mVSNack,mNackedBlocksWaiting);
			return;	// SUFI_ACK is always the last field.
		}
		case SUFI_LIST: {
			unsigned numpairs = vec->readField(rp,4);
			if (numpairs == 0) {
				RLCERR("Received SUFI LIST with length==0");
				return;	// Give up.
			}
			for (i = 0; i < numpairs; i++) {
				sn = vec->readField(rp,12);
				if (i == 0) { newva = minSN(newva,sn); newvaValid=true; }
				unsigned nackcount = vec->readField(rp,4) + 1;
				for (j = 0; j < nackcount; j++, incSN(sn)) {
					setNAck(sn);
				}
			}
			RLCLOG("received SUFI_LIST n=%d",numpairs);
			continue;
		}
		case SUFI_BITMAP: {
			// There is a note in 9.2.2.11.5:
			// "NOTE: The transmission window is not advanced based on BITMAP SUFIs"
			// So why is this note in the BITMAP sufi and not in the LIST and RLIST sufis, you ask?
			// It is because all acks are negative, and instead of using positive acks,
			// UMTS sends an "ACK" sufi that advances VTA, which implicitly positively
			// acks all passed over blocks.  Therefore you are not to construe
			// the bitmap entries with the bit set as a positive ack, you are only
			// to pay attention to the negative acks in the bitmap.
			unsigned maplen = 8*(vec->readField(rp,4) + 1);	// Size of bitmap in bits
			sn = vec->readField(rp,12);
			for (i = 0; i < maplen; i++, incSN(sn)) {
				int bit = vec->readField(rp,1);
				if (bit == 0) {
					newva = minSN(newva,sn);
					newvaValid = true;
					setNAck(sn);
				}
			}
			continue;
		}
		case SUFI_RLIST: {	// The inventor of this was on drugs.
			unsigned numcw = vec->readField(rp,4);
			sn = vec->readField(rp,12);
			setNAck(sn);
			newva = minSN(newva,sn);
			newvaValid = true;
			incSN(sn);
			unsigned accumulator = 0;
			bool superSpecialErrorBurstIndicatorFlag = false;
			for (i = 0; i < numcw; i++) {
				unsigned cw = vec->readField(rp,4);
				if (cw == 1) {
					if (accumulator) {
						RLCERR("Received invalid SUFI RLIST with incorrectly placed error burst indicator");
						return;	// Give up.
					}
					superSpecialErrorBurstIndicatorFlag = true;
					continue;
				} else {
					accumulator = (accumulator << 3) | (cw>>1);
					if (cw & 1) {
						if (superSpecialErrorBurstIndicatorFlag) {
							while (accumulator--) {
								setNAck(sn);
								incSN(sn);
							}
						} else {
							// Gag me, the spec is not clear what the distance really is:
							// "the number ... represents a distance between the previous indicated
							// erroneous AMD PDU up to and including the next erroneous AMD PDU."
							sn = addSN(sn,accumulator-1);
							setNAck(sn);
							incSN(sn);
						}
						superSpecialErrorBurstIndicatorFlag = false;
						accumulator = 0;
					}
				}
			}
			if (superSpecialErrorBurstIndicatorFlag) {
				// It is an error. We are supposed to go back and undo everything
				// we just did, but heck with that.
				RLCERR("Received invalid SUFI RLIST with trailing error burst indicator");
				continue;
			}
			continue;
		}
		case SUFI_MRW: {		// We dont implement this, but parse over it anyway.
			unsigned numMRW = vec->readField(rp,4);
			if (numMRW == 0) numMRW = 1;	// special case
			for (i = 0; i < numMRW; i++) {
				vec->readField(rp,12);	// MRW
			}
			vec->readField(rp,4);	// N_length
			continue;
		}
		case SUFI_MRW_ACK: {	// We dont implement, but parse over it anyway.
			/*unsigned numMRWAck =*/ vec->readField(rp,4);
			vec->readField(rp,4);	// N
			vec->readField(rp,12);	// SN_ACK
			continue;
		}
		case SUFI_POLL: {
			// This can only be used if "flexible RLC PDU size" is configured,
			// and I dont think it will be.
			sn = vec->readField(rp,12);
			// TODO...
			continue;
		}
		default:
			RLCERR("Received invalid SUFI type=%d",sufitype);
			return;
		} // switch
	} // while
}

void URlcTransAm::processSUFIs(ByteVector *vec)
{
	size_t rp = 4;
	processSUFIs2(vec,rp);

	if (mConfig->mPoll.mTimerPoll && mTimer_Poll.active()) {
		// Reset Timer_Poll exactly as per 25.322 9.5 paragraph a.
		// The purpose of the poll timer is to positively insure that
		// the poll that occurred at mTimer_Poll_VTS gets through.
		// This complicated check is to prevent an in-flight status report from
		// turning of the Poll timer prematurely.  It is over-kill; I think
		// we could just test mNackedBlocksWaiting, which forces a re-poll.
		URlcPdu *pdu=NULL;
		assert(mTimer_Poll_VTS < AmSNS);
		if (deltaSN(mVTA,mTimer_Poll_VTS) >= 0 ||
			((pdu = mPduTxQ[mTimer_Poll_VTS]) && pdu->mNacked)) {
			RLCLOG("Timer_Poll.reset VTA=%d Timer_Poll_VTS=%d nacked=%d",
				(int)mVTA,(int)mTimer_Poll_VTS,pdu?pdu->mNacked:-1);
			mTimer_Poll.reset();
		}
	}
}

// Move mVSNack forward to the next negatively acknowledged block, if any.
// If none, reset mNackedBlocksWaiting.
// Apparently we dont resend blocks awaiting an acknack.
// If fromScratch, start over from the beginning.
void URlcTransAm::advanceVS(bool fromScratch)
{
	if (fromScratch) {
		mVSNack = mVTA;
	} else {
		incSN(mVSNack);	// Skip nacked block we just sent.
	}
	for ( ; deltaSN(mVSNack,mVTS) < 0; incSN(mVSNack)) {
		URlcPdu *pdu = mPduTxQ[mVSNack];
		if (! pdu) { continue; } // block was acked and deleted.
		if (pdu->mNacked) return;
	}
	// No more negatively acknowledged blocks at the moment.
	// But note there may be lots of blocks that are UnAcked.
	mNackedBlocksWaiting = false;
}

bool URlcTransAm::IsPollTriggered()
{
	if (mPollTriggered) return true;

	// 9.7.1 case 4.
	if (mConfig->mPoll.mPollPdu) {
		if (deltaSN(mVTPDU,mVTPDUPollTrigger) >= 0) {
			RLCLOG("PollPdu triggered, VTPDU=%d trig=%d",(int)mVTPDU,(int)mVTPDUPollTrigger);
			mPollTriggered = true;
			mVTPDUPollTrigger = mVTPDU + mConfig->mPoll.mPollPdu;
		}
	}

	// 9.7.1 case 5.
	// The poll is meant to be sent after the entire SDU has been sent
	// so we get acknowledgement for the whole thing, so if the
	// special case mLILeftOver is still outstanding, wait for that.
	if (mConfig->mPoll.mPollSdu) {
		int diff = deltaSN(mVTSDU,mVTSDUPollTrigger);
		if (diff > 0 || (diff == 0 && mLILeftOver == 0)) {
			RLCLOG("PollSdu triggered VTSDU=%d trig=%d",(int)mVTSDU,(int)mVTSDUPollTrigger);
			mPollTriggered = true;
			mVTSDUPollTrigger = mVTSDU + mConfig->mPoll.mPollSdu;
		}
	}

	// 9.7.1 case 7.
	if (mConfig->mPoll.mTimerPollPeriodic) {
		if (mTimer_Poll_Periodic.expired()) {
			RLCLOG("TimerPollPeriodic triggered");
			mPollTriggered = true;
			mTimer_Poll_Periodic.set();
		}
	}

	// 9.7.1 case 3.  Described more thoroughly in 9.5
	if (mConfig->mPoll.mTimerPoll) {
		if (mTimer_Poll.expired()) {
			RLCLOG("TimerPoll triggered");
			mPollTriggered = true;
			// This timer is reset when we actually send the poll.
		}
	}
	return mPollTriggered;
}

bool URlcTransAm::stalled()
{
	// FIXME: This does not work unless mVTWS is less than mSNS/2.
	return deltaSN(mVTS,mVTA) >= mVTWS;
}

URlcPdu *URlcTransAm::readLowSidePdu2()
{
	if (mRlcState != RLC_RUN) { return NULL; }

	if (mSendResetAck) {
		mSendResetAck = false;
		return getResetPdu(PDUTYPE_RESET_ACK);
	}

	// Did we initiate a reset procedure, and are awaiting the Reset_ack?
	if (resetInProgress()) {
		RLCLOG("Reset in progress remaining=%d",(int)mTimer_RST.remaining());
		// Is the timer expired?
		if (mTimer_RST.expired()) {
			// Resend the same reset.
			// Note that the default max_RST for SRBs is just 1,
			// so if a reset is lost, the channel is abandoned.
			mResetTriggered = true;	// handled below.
		} else {
			return NULL;	// Nothing to send; waiting on reset ack.
		}
	}

	// If VRH and VRR become inverted, something terrible is wrong.
	// Send a reset.  Also see addAckNack().
	if (!receiver()->isReceiverOk()) {
		mResetTriggered = true;
	}

	// 11.4.2 Reset Initiation
	if (mResetTriggered) {
		RLCLOG("reset triggered VTRST=%d max=%d",(int)mVTRST,mConfig->mMaxRST);
		mResetTriggered = 0;
		// We dont flush mPDUs and discard partial sdus until we get the reset_ack,
		// although I'm not sure why the timing would matter.
		mVTRST++;
		if (mVTRST > mConfig->mMaxRST) {
			RLCERR("too many resets, connection disabled");
			// Too many resets.  Give up.
			mTimer_RST.reset();	// Finished with the timer.
			mRlcState = RLC_STOP;
			// TODO: We may want to flush the SDU buffer.
			// TODO: clause 11.4.4a, which is send unrecoverable error to upper layer.
			return NULL;
		}
		mTimer_RST.set(mConfig->mTimerRSTValue);
		// We need to increment RSN between resets, but we cant really do it
		// after we send the RESET_ACK in recvResetAck,
		// because we might get multiple ones of those.
		// It is easier to increment it before starting a new reset procedure.
		return getResetPdu(PDUTYPE_RESET);
	}
	
	// Optional here: timer based status transfer;  If mTimer_status_periodic expired,
	// set mStatusTriggered.

	// mStatusTriggered may be triggered when the receiver notices a missing pdu,
	// or when requested by a poll (that indicates PDU was last in senders buffer,
	// or optionally by timer in sender), or optionally by timer in receiver.
	// mStatusTriggered will not be reset until we have transmitted
	// enough pdus for a complete status report.
	if (mStatusTriggered) {
		// We are allowed to piggy-back the status, but not implemented for downlink.
		return getStatusPdu();
	}

	// Find a pdu to send.  The pdu variable may only be a data pdu, because
	// at the end we will set/unset the poll bit, which is only valid in data pdus.
	URlcPdu *pdu = NULL;

	// Section 11.3.2 Transmission of AMD PDU: this section is confusing.
	// Essentially it is establishing the priority of PDUs to be sent, which is:
	//	1. negatively acknowledged PDUs.
	//	2. new PDUs.
	// 	3. I dont see where it says anything about resending pdus before
	//		receiving negative ack.
	// If a status report is triggered, it can be sent in a stand-alone status report,
	// or it can be piggy-backed onto a previously sent PDU.
	// If the Configured_Tx_Window_Size >= 2048 (half the sequence space) then
	// you may only resend the most recently sent PDU.

	if (mNackedBlocksWaiting) {
		// Send this negatively acknowledged pdu.
		// TODO: If we support piggy-backed status, that needs to be fixed here too.
		pdu = mPduTxQ[mVSNack];
		pdu->mNacked = false;
		// Unset the poll bit in case it had been set on the previous transmission.
		pdu->setAmP(false);
		advanceVS(false);
		// 9.7.1 case 2: If the AMD PDU is the last of the AMD PDUs scheduled for retransmission... poll.
		if (!mNackedBlocksWaiting && mConfig->mPoll.mLastRetransmissionPduPoll) {
			mPollTriggered = true;
		}
	} else if (stalled()) {
		RLCLOG("Stalled VTS=%d VTA=%d",(int)mVTS,(int)mVTA);
		// No new data to send, but go to maybe_poll because if the
		// previous poll was prohibited by the mTimerPollProhibit, it may
		// have expired now and we can finally send the poll.
		goto maybe_poll;
	} else if ((pdu = getDataPdu())) {
		// 9.7.1 case 1: If it is the last PDU available... poll.
		if (mConfig->mPoll.mLastTransmissionPduPoll && pdusFinished()) {
			mPollTriggered = true;
		}
	} else {
		// No new pdus to send, but according to 9.7.1, we may need to send
		// a poll anyway so dont return yet.
	}

	if (pdu && ++pdu->mVTDAT >= mConfig->mMaxDAT) {
		// 25.322 11.3.3a: Reached maximum number of attempts.
		// Note that the documentation of the option names does not exactly match
		// the names in 25.331 RRC 10.3.4.25 Transmission RLC Discard IE.
		RLCLOG("pdu %d exceeded VTDAT=%d, discarded",pdu->getAmSN(),pdu->mVTDAT);
		switch (mConfig->mRlcDiscard.mSduDiscardMode) {
		default:
			RLCERR("Unsupported RLC discard mode configured:%d",
					(int)mConfig->mRlcDiscard.mSduDiscardMode);
			// fall through.
		case TransmissionRlcDiscard::NoDiscard:
			// "No discard" means no explicit discard of just this SDU using MRW sufis;
			// instead we just reset the whole connection.
			mResetTriggered = true;
			return readLowSidePdu2();	// recur to send reset pdu
		}
	}

	// Set the poll bit if any poll triggers as per 9.7.1, starting at:
	// "When the Polling function is triggered, the Sender shall..."
	maybe_poll:
	if (mConfig->mPoll.mTimerPoll) {
		if (mTimer_Poll.active()) {
			RLCLOG("Timer_poll active=%d remaining=%ld",mTimer_Poll.active(),mTimer_Poll.remaining());
		}
	}
	if (IsPollTriggered()) {
		if (mConfig->mPoll.mTimerPollProhibit && !mTimer_Poll_Prohibit.expired()) {
			// Delay polling.
		} else {
			if (pdu == NULL) {
				// 9.7.1, near top of page 54:
				// "If there is one or more AMD PDUs to be transmitted or
				// there are AMD PDUs not acknowledged by the Receiver:"
				// Note carefully: It does not say "negatively acknowledged" blocks,
				// which would be a test of mNackedBlocksWaiting; rather we resend
				// the poll request if any blocks have not been acknowledged,
				// which occurs if mVTS > mVTA.
				if (mVTS != mVTA) {
					// We need to resend a PDU just to set the poll bit.
					// This is particularly important for the Timer_Poll.
					// Consider what happens if the PDU carrying the poll bit is lost;
					// then the RLC on the other end does not respond, the
					// Timer_Poll expires, and we get to here now.
					// 11.3.2 says if we need to set a poll and have nothing
					// to send, resent pdu[mVTS-1]
					URlcSN psn = addSN(mVTS,-1);
					pdu = mPduTxQ[psn];
					if (pdu == NULL) {
						RLCERR("internal error: pdu[mVTS-1] is missing");
						return NULL;
					}
				} else {
					return NULL;
				}
			}

			// Send the poll.
			mPollTriggered = false;
			pdu->setAmP(true);
			RLCLOG("Poll Requested at sn=%d",pdu->getSN());
			if (mConfig->mPoll.mTimerPollProhibit) { mTimer_Poll_Prohibit.set(); }
			if (mConfig->mPoll.mTimerPoll) {
				mTimer_Poll.set();
				mTimer_Poll_VTS = mVTS;
			}
		}
	}

	// If sending a data pdu, it is saved in mPduTxQ, so we have to send a copy
	// for the caller to delete.
	if (pdu) {
		pdu = new URlcPdu(pdu);
		RN_MEMLOG(URlcPdu,pdu);
	}

	return pdu;
}

URlcPdu *URlcTransAm::readLowSidePdu()
{
	ScopedLock lock(parent()->mAmLock);
	URlcPdu *pdu = readLowSidePdu2();
	if (pdu) {
		bool dc = pdu->getBit(0);
		if (dc) {
			// Data pdu:
			RLCLOG("readLowSidePdu dc=data sn=%d poll=%d header=%s",
				(int)pdu->getField(1,12),
				(int)pdu->getField(URlcPdu::sPollBit,1),
				pdu->segment(0,2).hexstr().c_str());
		} else {
			// Control pdu: the sn is not applicable.
			RLCLOG("readLowSidePdu dc=control header=%s", pdu->segment(0,2).hexstr().c_str())
		}
	}
	return pdu;
}

void URlcRecvAmUm::addUpSdu(ByteVector &payload)
{
	if (mUpSdu == NULL) {
		mUpSdu = new URlcUpSdu(mConfig->mMaxSduSize);
		RN_MEMLOG(URlcUpSdu,mUpSdu);
		mUpSdu->setAppendP(0);	// Allow appending
	}
	mUpSdu->append(payload);
}

// A gag me special case for LI == 0x7ffc buried in sec 9.2.2.8
void URlcRecvAmUm::ChopOneByteOffSdu(ByteVector &payload)
{
	if (mUpSdu == NULL || mUpSdu->size() < 1) {
		RLCERR("Logic error in the horrible LI=0x7ffc special case");
		return;	// and we are done with that, I guess
	}
	mUpSdu->trimRight(1);	// Chop off the last byte.
}

void URlcRecvAmUm::sendSdu()
{
	rlcSendHighSide(mUpSdu);
	mUpSdu = NULL;
}

void URlcRecvAmUm::discardPartialSdu()
{
	if (mUpSdu) {
		RLCLOG("discardPartialSdu");
		delete mUpSdu;
		mUpSdu = 0;
		// todo: alert other layers.
	}
}

// It cant be const.
void URlcTrans::textTrans(std::ostream &os)
{
	ScopedLock lock(mQLock);	// We are touching mSplitSdu
	os <<LOGVAR(mVTSDU);
	os <<LOGVAR2("mSplitSdu.size",(mSplitSdu ? mSplitSdu->size() : 0));
	os <<LOGVAR2("getSduCnt", getSduCnt());
	os <<LOGVAR2("rlcGetSduQBytesAvail",rlcGetSduQBytesAvail());
	os <<LOGVAR2("rlcGetPduCnt",rlcGetPduCnt());
	os <<LOGVAR2("rlcGetFirstPduSizeBits",rlcGetFirstPduSizeBits());
	os <<LOGVAR2("rlcGetDlPduSizeBytes",rlcGetDlPduSizeBytes());
}

void URlcTransAmUm::textAmUm(std::ostream &os)
{
	os <<LOGVAR2("mDlPduSizeBytes",mConfig->mDlPduSizeBytes)
		<<LOGVAR2("PduOutQ.size",mPduOutQ.size())
		<<LOGVAR(mVTPDU)<<LOGVAR(mLILeftOver)
		<<LOGVAR2("rlcGetBytesAvail",rlcGetBytesAvail())
		<<LOGVAR2("rlcGetPduCnt",rlcGetPduCnt());
}

void URlcTransAm::transAmReset()
{
	RLCLOG("transAmReset");
	URlcTransAmUm::transDoReset();
	mVTS = 0;
	mVTA = 0;
	mVTWS = mConfig->mConfigured_Tx_Window_Size;
	mVTPDUPollTrigger = mConfig->mPoll.mPollPdu;
	mVTSDUPollTrigger = mConfig->mPoll.mPollSdu;
	mPollTriggered = mStatusTriggered = mResetTriggered = false;
	mNackedBlocksWaiting = false;
	mVSNack = 0;
	mSendResetAck = false;
	for (int i = 0; i < AmSNS; i++) {
		if (mPduTxQ[i]) { delete mPduTxQ[i]; mPduTxQ[i] = 0; }
	}
	// mResetTransRSN is explicitly not reset.
	// mVTRST is explicitly not reset.
	// mVTMRW = 0;	currently unused

	//if (mConfig->mTimerRSTValue) mTimer_RST.reset(); // <- done by caller:
	// 25.322 11.4.1: Reset does not reset mTimerPollPeriodic
	//if (mConfig->mPoll.mTimerPollPeriodic) mTimer_Poll_Periodic.reset();
	if (mConfig->mPoll.mTimerPollProhibit) mTimer_Poll_Prohibit.reset();
	if (mConfig->mPoll.mTimerPoll) mTimer_Poll.reset();
}

void URlcTransAm::text(std::ostream&os)
{
	URlcTrans::textTrans(os);
	URlcTransAmUm::textAmUm(os);

	os <<LOGVAR(mVTS) <<LOGVAR(mVTA) <<LOGVAR(mVTWS)
		<<LOGVAR(mPollTriggered) <<LOGVAR(mStatusTriggered) <<LOGVAR(mResetTriggered)
		<<LOGVAR(mNackedBlocksWaiting) <<LOGVAR(mSendResetAck)
		<<LOGVAR(mResetTransRSN) <<LOGVAR(mResetAckRSN) <<LOGVAR(mResetRecvCount);
	int cnt = 0;
	for (int sns = 0; sns < AmSNS; sns++) {
		if (mPduTxQ[sns]) {
			if (0==cnt++) os <<"\nPduTxQ=";
			os <<"\t"<<LOGVAR(sns); mPduTxQ[sns]->text(os); os<<"\n";
		}
	}
}

void URlcTransUm::text(std::ostream&os)
{
	URlcTrans::textTrans(os);
	URlcTransAmUm::textAmUm(os);
	os <<LOGVAR(mVTUS);
}

void URlcTransAmUm::transDoReset()
{
	mLILeftOver = 0;
	mVTPDU = 0;
	mVTSDU = 0;
	ScopedLock lock(mQLock);	// We are touching mSplitSdu
	if (mSplitSdu) {
		// First sdu was partially sent; throw it away.
		mSplitSdu->free();
		mSplitSdu = NULL;
	}
}

void URlcTransAm::transAmInit()
{
	mResetTransRSN = 0;
	mResetAckRSN = 0;
	mResetRecvCount = 0;
	memset(mPduTxQ,0,sizeof(mPduTxQ));
	mTimer_RST.configure(mConfig->mTimerRSTValue);
	mTimer_Poll_Periodic.configure(mConfig->mPoll.mTimerPollPeriodic);
	mTimer_Poll_Prohibit.configure(mConfig->mPoll.mTimerPollProhibit);
	mTimer_Poll.configure(mConfig->mPoll.mTimerPoll);
	// etc...
	transAmReset();
}

// Set the nack indicator for queued block with this sequence number.
void URlcTransAm::setNAck(URlcSN sn)
{
	assert(sn >= 0 && sn < AmSNS);
	if (URlcPdu *pdu = mPduTxQ[sn]) {
		pdu->mNacked = true;
		mNackedBlocksWaiting = true;
		RLCLOG("setNack %d pdu->sn=%d",(int)sn,pdu->getAmSN());
	} else {
		RLCLOG("setNack %d MISSING PDU!",(int)sn);
	}
}


// reset procedure goes both ways:
// 1. we send reset, finish reset upon receipt of reset_ack.
//		In this case, continue to send reset until we get reset_ack.
//		If we send MaxRST attempts, unrecoverable error.
//		11.4.5.3: We may receive a reset after we sent one, with a different RSN
//		than the one we sent, in which case send an ack using the received RSN.
//		This implies that there are two RSN variables: one of the Reset we sent
//		so we can resend the same value on timer expirey, and one of the incoming Reset.
// 2. we get reset, do the reset and send reset_ack.
//		In this case, continue to respond to reset by sending reset_ack as
//		long as incoming reset matches most recent.
// The reset procedure is really queer.  The receiver of the RESET only
// does the full reset the first time it is received.  It then returns
// a RESET_ACK and can immediately begin blasting away with more pdus
// oblivious to whether the RESET_ACK was received or not.
// If the sender does not receive the first RESET_ACK, it times out and sends
// another RESET which does NOT reset the receiver; the receiver merely sends
// another RESET_ACK.  When the RESET_ACK finally gets through to the sender,
// it discards everything that has been sent in the interim, so the first order
// of business is to send status reports back to the receiver so it can
// resent everything again.
// 11.4.3 or 11.4.5.3, which are handled identically except that case 11.4.3 ignores
// duplicate resets.
void URlcAm::recvResetPdu(URlcPdu *pdu)
{
	unsigned rsn = pdu->getBit(4);
	unsigned hfn = pdu->getField(8,20);
	RLCLOG("receive reset pdu cnt=%d rsn=%d ResetAckRSN=%d",mResetRecvCount,rsn,mResetAckRSN);
	// If resetInProgress, we already sent a reset, and now we have received one back,
	// which corresponds to clause 11.4.5.3. Do the reset now.
	if (resetInProgress() || mResetRecvCount == 0 || rsn != mResetAckRSN) {
		// This is a new reset.
		RLCLOG("full reset");
		transAmReset();
		recvAmReset();
	} else {
		// Redundant reset message received.
		// Just resend another reset ack and we are done.
	}
	mULHFN = hfn;
	mResetAckRSN = rsn;	// RSN in outgoing reset ack must match most recent incoming reset.
	mResetRecvCount++;	// Counts all resets received ever, unlike mVTRST counts resents of this reset
	mSendResetAck = true;
}

// 11.4.4
void URlcAm::recvResetAck(URlcPdu *pdu)
{
	unsigned rsn = pdu->getBit(4);
	RLCLOG("receive reset_ack pdu rsn=%d ResetTransRSN=%d",rsn,mResetTransRSN);
	// unused unsigned hfn = pdu->getField(8,20);
	if (! resetInProgress()) {
		// This is nothing to worry about because they can pass each other in flight.
		LOG(INFO) <<"Discarding Reset_Ack pdu that does not correspond to a Reset";
		return;
	}
	if (rsn != mResetTransRSN) {
		// This is slightly disturbing, but we'll press on.
		LOG(INFO) <<"Discarding Reset_Ack pdu with invalid rsn";
		return;
	}
	transAmReset();
	recvAmReset();
	mVTRST = 0;
	mTimer_RST.reset();
	mResetTransRSN++;
	mULHFN++;
	mDLHFN++;
}

void URlcRecvAm::recvAmReset()
{
	URlcRecvAmUm::recvDoReset();
	mVRR = 0;
	mVRH = 0;
	mStatusSN = 0;
	for (int i = 0; i < AmSNS; i++) {
		if (mPduRxQ[i]) { delete mPduRxQ[i]; mPduRxQ[i] = 0; }
	}
}

void URlcRecvUm::text(std::ostream &os)
{
	os <<LOGVAR(mVRUS);
#if RLC_OUT_OF_SEQ_OPTIONS
	os <<LOGVAR(VRUDR) <<LOGVAR(VRUDH) <<LOGVAR(VRUDT) <<LOGVAR(VRUOH)
	<<LOGVAR2("VRUM",VRUM());
#endif
}

void URlcRecvAmUm::textAmUm(std::ostream &os)
{
	os <<LOGVAR2("mUpSdu.size",(mUpSdu ? mUpSdu->size() : 0));
	os <<LOGVAR(mLostPdu);	// This is UM only, but easier to put in this class.
}

void URlcRecvAm::text(std::ostream &os)
{
	// There is nothing interesting in URlcRecv to warrant a textRecv() function.
	URlcRecvAmUm::textAmUm(os);

	os <<LOGVAR(mVRR) <<LOGVAR(mVRH) <<LOGVAR2("VRMR",VRMR());
	int cnt=0;
	for (int sns = 0; sns < AmSNS; sns++) {
		if (mPduRxQ[sns]) {
			if (0==cnt++) os <<"\nPduRxQ=";
			os <<"\t"<<LOGVAR(sns); mPduRxQ[sns]->text(os); os <<"\n";
		}
	}
}

void URlcRecvAm::recvAmInit()
{	// Happens once.
	memset(mPduRxQ,0,sizeof(mPduRxQ));
	recvAmReset();
}

void URlcRecvAmUm::parsePduData(URlcPdu &pdu,
	int headersize,		// 1 for UM, 2 for AM
	bool Eindicator,	// Was the E-bit in the header?
	bool statusOnly)	// If true, process only the piggy-back status, do nothing else.
{
	ByteVector payload = pdu.tail(headersize);
	if (0 == Eindicator) {
		if (statusOnly) return;
		RLCLOG("parsePduData sn=%d Eindicator=0", pdu.getSN());
		// No LI indicators.  Whole payload is appended to current SDU.
		// mLostPdu is only set in UM mode.
		if (! mLostPdu) {
			addUpSdu(payload);
		}
		return;
	}

	// Crack out the length indicators.
	unsigned licnt = 0;
	unsigned vli[32+1];
	unsigned libytes = mConfig->mUlLiSizeBytes;
	unsigned libits = mConfig->mUlLiSizeBits;
	bool end = 0;
	bool overflow = 0;
	while (!end) {
		if (licnt == 32) {
			overflow = true;
			if (payload.size() < libytes) {
				// Block is complete trash.
				RLCERR("UMTS RLC: Invalid incoming PDU");
				if (! statusOnly) discardPartialSdu();
				return;
			}
		} else {
			vli[licnt++] = payload.getField(0,libits);
		}
		end = (0 == payload.getBit(libits));	// e bit.
		payload.trimLeft(libytes);
		RLCLOG("parsePduData sn=%d li=%d",
			pdu.getSN(),vli[licnt-1]);	// Note: li==127 means padding.
	}

	if (statusOnly) {
		// See if the last li indicates a piggy-backed status is present.
		unsigned lastli = vli[licnt-1];
		if (!(lastli == 0x7ffe || (libytes == 1 && lastli == 0x7e))) {
			return;	// There is no piggy-backed status present.
		}
		// keep going and we will handle the piggy back status at the bottom.
	}

	if (overflow) {
		RLCERR("More than 32 segments in an incoming PDU");
	}

	// Use the length indicators to slice up the payload into segments.
	bool start_sdu = false;		// first data byte in pdu starts a new sdu.
	unsigned n = 0;	// index into li fields.

	// Section 11.2.3.1, RLC-UM LI indicators in downlink:
	// first li= 0x7c or 0x7ffc - start of RLC SDU
	// first li= 0x7d or 0x7ffd - complete SDU
	// first li= 0x7ffa - complete SDU - 1 byte

	// Section 9.2.2.8 RLC-UM LI indicators in uplink:
	// 0x7c or 0x7ffc start of RLC SDU.
	// Section 9.2.2.8 RLC-UM LI indicators in downlink:
	// 0x00 or 0x0000 - start of RLC SDU, if no other indication
	// 0x7ffb - prevous pdu was end of RLC SDU - 1 byte
	
	// Alternative E-bit values:
	// first lie=0x7e or 0x7ffe, 0x7d or 0x7ffd, 0x7ffa

	// 9.2.2.8 Special cases for first length indicator:
	if (vli[0] == 0) {
		// "The previous RLC PDU was exactly filled..."
		start_sdu = true;
		n++;		// And we are finished with this LI field - it didnt actually tell us a length.
	} else if (vli[0] == 0x7ffa) {	// This is only used with the alternative-E-bit config,
				// so we should probably throw an error.
		start_sdu = true;
		// Dont increment n - instead modify vli with the correct length:
		vli[0] = payload.size() - 1;
		payload.trimRight(1);
	} else if (vli[0] == 0x7ffb) {
		// "The previous RLC PDU was one octet short of exactly filling the previous sdu"
		start_sdu = true;
		n++;
		// This horrible special case instructs to chop one byte off the PREVIOUS pdu.  Gag me.
		if (!statusOnly) ChopOneByteOffSdu(payload);
	} else if (vli[0] == 0x7ffc || (libytes == 1 && vli[0] == 0x7c)) {
		// "UMD PDU: The first data octet of this RLC PDU is the first octet
		// of an RLC SDU.  AMD PDU: Reserved."
		start_sdu = true;
		n++;		// And we are finished with this LI field - it didnt actually tell us a length.
	} else if (vli[0] == 0x7ffd || (libytes == 1 && vli[0] == 0x7d)) {
		// "UMD PDU: The first data octet in this RLC PDU is the first octet of an
		// RLC SDU and the last octet in this RLC PDU is the last octet of the same
		// RLC SDU.  AMD PDU: Reserved."
		start_sdu = true;
		vli[0] = payload.size();
		if (licnt != 1) {
			RLCERR("Incoming PDU invalid: LI==0x7ffd but more than one LI");
		}
	} else {
		// If the alternative-E bit, then 0x7ffe and 0x7e have a special meaning
		// in the first position, but we dont use it.
	}

	// Process the piggy-back status and return.
	if (statusOnly) {
		// Scan past all the other li fields to get to the final one.
		for ( ; n < licnt; n++) {
			unsigned lenbytes = vli[n];
		    if (lenbytes == 0x7ffe || (libytes == 1 && lenbytes == 0x7e)) {
				// Finally, here it is.
				if (n+1 != licnt) {
					RLCERR("Incoming piggy-back status indication before end of LI fields");
					return;
				}
				URlcRecvAm *recv = dynamic_cast<URlcRecvAm*>(this);
				if (recv) {
					recv->transmitter()->processSUFIs(&payload);
				} else {
					// The other possibility is that the this object is URlcRecvUm.
					RLCERR("invalid li=0x7fffe or 0x7e in UM mode");
				}
				return;
			}
			if (lenbytes > payload.size()) {
				RLCERR("Incoming piggy-back status LI size=%d less than PDU length=%d",
						lenbytes,payload.size());
				return;
			}
			payload.trimLeft(lenbytes);
		}
		return;
	}

	if (start_sdu) {
		if (mLostPdu) { assert(mUpSdu == NULL); }	// this case handled earlier.
		mLostPdu = false;
		// It is an error if mUpSdu is not set, because the sender gave us an LI
		// field that implied that there is an mUpSdu.  But lets not crash...
		if (mUpSdu) sendSdu();
	}

	for ( ; n < licnt; n++) {
		unsigned lenbytes = vli[n];
		//printf("HERE: n=%d libytes=%d lenbytes=%d\n",n,libytes,lenbytes);
		if (lenbytes == 0x7fff || (libytes == 1 && lenbytes == 0x7f) ||
		    lenbytes == 0x7ffe || (libytes == 1 && lenbytes == 0x7e)) {
			//printf("THERE: n=%d libytes=%d lenbytes=%d\n",n,libytes,lenbytes);
			// Rest of pdu is padding or a piggy-backed status, which was handled elsewhere.
			payload.trimLeft(payload.size());	// Discard rest of payload.
			if (n+1 != licnt) {
				RLCERR("Incoming PDU padding indication before end of LI fields");
			}
			break;
		}

		// sanity check.
		if (lenbytes > payload.size()) {
			RLCERR("Incoming PDU LI size=%d less than PDU length=%d", lenbytes,payload.size());
			n = licnt;	// End loop after this iteration.
			lenbytes = payload.size();	// Should probably discard this.
		}
		if (!mLostPdu) {
			ByteVector foo(payload.segment(0,lenbytes));	// C++ needs temp variable, sigh.
			addUpSdu(foo);
			sendSdu();
		}
		mLostPdu = false;
		payload.trimLeft(lenbytes);
	}
	if (payload.size()) {
		addUpSdu(payload);	// The left-over is the start of a new sdu to be continued.
	}
}


// TODO: Should we lock this? In case the MAC manages to send a second
// while we are still doing the first?  Probably so.
void URlcRecvAm::rlcWriteLowSide(const BitVector &pdubits)
{
	ScopedLock lock(parent()->mAmLock);
	int dc = pdubits.peekField(0,1);
	if (dc == 0) { // is it a control pdu?
		URlcPdu pdu1(pdubits,this,"ul am control");
		PduType pdutype = PduType(pdubits.peekField(1,3));

		std::ostringstream foo;
		pdubits.hex(foo);
		RLCLOG("rlcWriteLowSide(control,sizebits=%d,pdutype=%d,payload=%s) mVRR=%d",
				pdubits.size(),pdutype,foo.str().c_str(),(int)mVRR);

		switch (pdutype) {
			case PDUTYPE_STATUS: {
				transmitter()->processSUFIs(&pdu1);
				break;
			}
			case PDUTYPE_RESET: {
				parent()->recvResetPdu(&pdu1);
				break;
			}
			case PDUTYPE_RESET_ACK: {	// 11.4.4 Reception of RESET ACK PDU
				parent()->recvResetAck(&pdu1);
				break;
			}
			default:
				RLCERR("RLC received control block with unknown RLC type=%d",pdutype);
				break;
		}
		//delete pdu1;	// formerly I allocated the pdu1, but now not needed.
		return;
	} else {
		// It is a data pdu.
		// Check for poll bit:
		if (pdubits.peekField(URlcPdu::sPollBit,1)) {
			RLCLOG("Received poll bit at SN %d",(int)pdubits.peekField(1,12));
			transmitter()->mStatusTriggered = true;
			mStatusSN = mVRR;	// This is where we will start the status reports.
		}

		// If we already have a copy we can discard it before we go to the
		// effort of converting it to a ByteVector.
		URlcSN sn = pdubits.peekField(1,12);

		if (mUep->mStateChange) {
			int beforesn = sn, beforevrr = mVRR;
			if (sn==0 && (mVRR!=0)) {mUep->reestablishRlcs();}
			LOG(ALERT) << format("stateChange: before %d %d after %d %d",beforesn,beforevrr,(int)sn,(int)mVRR);
	 	}
		mUep->mStateChange = false;

		std::ostringstream foo;
		pdubits.hex(foo);
		RLCLOG("rlcWriteLowSide(data,sizebits=%d,sn=%d,payload=%s) mVRR=%d",
				pdubits.size(),(int)sn,foo.str().c_str(),(int)mVRR);

		if (deltaSN(sn,mVRR) < 0) {
			// The other transmitter sent us a block we have already processed
			// and is no longer in our reception window.
			// This is a common occurrence when there is alot of transmission loss
			// because the other transmitter did not receive our status report
			// informing them that we no longer want this block.
			RLCLOG("rlcWriteLowSide ignoring block sn=%d less than VRR=%d", (int)sn, (int)mVRR);
                        // retransmit status message, otherwise this will go on indefinitely, especially on RACH
			transmitter()->mStatusTriggered = true;
			return;
		}

		URlcPdu *pdu2 = new URlcPdu(pdubits,parent(),"ul am data");
		RN_MEMLOG(URlcPdu,pdu2);

		// Process piggy-backed status immediately.
		parsePduData(*pdu2,2,pdu2->getAmHE() & 1,true);

		if (mPduRxQ[sn]) {
			// Already received this block.
			// This is a common occurrence if there are many lost pdus
			// because if the status response is lost, the sender will
			// resend a block just to get a poll across.
			// If we were less lazy, we could check that the two blocks match.
			// If the designers had been more clever, they could have used the two blocks
			// to decode the data more securely.
			delete pdu2;
			RLCLOG("rlcWriteLowSide ignoring duplicate block sn=%d", (int)sn);
                        // retransmit status message, otherwise this will go on indefinitely, especially on RACH
                        transmitter()->mStatusTriggered = true;
			return;
		}

		mPduRxQ[sn] = pdu2;

		if (deltaSN(sn,mVRH) >= 0) {
			if (mConfig->mStatusDetectionOfMissingPDU) {
				if (sn != addSN(mVRH,1)) {
					// We skipped a pdu.  Trigger a status report to inform the other.
					transmitter()->mStatusTriggered = true;
				}
			}
			mVRH = sn; incSN(mVRH);
		}

		// Now parse any new blocks received.  This is advanceVRR();
		if (sn == mVRR) {
			// Woo hoo!  This is the block we have been waiting for!
			// Advance mVRR over all consecutive blocks that have been received.
			URlcPdu *pdu3;
			while ((pdu3 = mPduRxQ[mVRR])) {
				parsePduData(*pdu3,2,pdu3->getAmHE() & 1,false);
				delete pdu3;
				mPduRxQ[mVRR] = 0;
				incSN(mVRR);
			}
		} else {
			// It is not possible for block mVRR to exist yet.
			assert(mPduRxQ[mVRR] == NULL);
		}
	}
}

// Out of Sequence SDU Delivery not implemented.
// We would only need this if we used multiple physical channels, and we wont.
void URlcRecvUm::rlcWriteLowSide(const BitVector &pdubits)
{
	URlcPdu pdu(pdubits,this,"ul um");

	URlcSN sn = pdu.getSN();
#if RLC_OUT_OF_SEQ_OPTIONS	// not fully implemented
	if (mConfig->mmConfigOSR) {
		// 11.2.3.1 SDU discard and re-assembly
		if (deltaSN(sn,VRUM()) >= 0) { /*delete pdu;*/ return; }
	}
#endif
	//cout << "sn: " << sn << ", mVRUS: " << mVRUS << endl;
	if (sn != mVRUS) {
		// Lost one or more pdus.
		// Discard any partially assembled SDU.
		discardPartialSdu();
		// Set mLostPdu to continue to discard data until we find a certain start of a new sdu.
		mLostPdu = true;
	}
	mVRUS = addSN(sn,1);

	// Note: payload does not 'own' memory; must delete original pdu when finished.
	parsePduData(pdu, 1, pdu.getUmE(),false);
	// Automatic deletion of ByteVector in pdu.
}

// This is applicable only to RLC-UM and RLC-AM.  Return 0 for RLC-TM.
unsigned computeDlRlcSize(RBInfo *rb, RrcTfs *dltfs)
{
	if (rb->getDlRlcMode() == URlcModeTm) {return 0;}
	TrChInfo *dltc = dltfs->getTrCh();
	unsigned dlmacbits = macHeaderSize(dltc->mTransportChType,rb->mRbId,dltc->mTcIsMultiplexed);

	// For UM and AM there should only be one TB size, although there is
	// sometimes a 0 size as well, which is why we use the MaxTBSize.
	// For TM there could be multiple TB sizes, but we dont care because
	// they just pass through for the MAC to worry about.
	// UM and AM RLC are byte-oriented, so ??
	return (dltfs->getMaxTBSize() - dlmacbits)/8;
}


// Allocate the uplink and downlink RLC entities for this rb.
URlcPair::URlcPair(RBInfo *rb, RrcTfs *dltfs, UEInfo *uep, TrChId tcid)
	: mTcid(tcid)
{
	// We need the mac header size for this particular rb to subtract from the TrCh TBSize.
	// We pass that size to the AM/UM mode configs, and they subtract the RLC AM or UM
	// header size out of this pdusize.
	unsigned dlPduSizeBytes = computeDlRlcSize(rb,dltfs);

	{
	TrChInfo *dltc = dltfs->getTrCh();
	unsigned dlmacbits = macHeaderSize(dltc->mTransportChType,rb->mRbId,dltc->mTcIsMultiplexed);
	PATLOG(4,format("URlcPair(rb=%d,ul=%s,dl=%s,pdusizebits=%d+%d,%s)",
			rb->mRbId, URlcMode2Name(rb->getUlRlcMode()), URlcMode2Name(rb->getDlRlcMode()),
			dlmacbits,dlPduSizeBytes, uep->ueid().c_str()));
	}

	// We do not need to pass a pdu size to the uplink RLC entities because they just
	// assemble whatever size pdus come in.
	switch (rb->getUlRlcMode()) {
	case URlcModeAm: {		// If ul is AM, dl is AM too.
		assert(rb->getDlRlcMode() == URlcModeAm);
		URlcAm *amrlc = new URlcAm(rb,dltfs,uep,dlPduSizeBytes);	// Includes UrlcTransAm and UrlcRecvAm
		mDown = amrlc;
		mUp = amrlc;
		return;
		}
	case URlcModeUm:
		mUp = new URlcRecvUm(rb,dltfs,uep);
		break;
	case URlcModeTm:
		// TODO: Add config to TM?
		mUp = new URlcRecvTm(rb,uep);
		break;
	}
	switch (rb->getDlRlcMode()) {
	case URlcModeAm: assert(0);	// handled above.
		break;
	case URlcModeUm:
		mDown = new URlcTransUm(rb,dltfs,uep,dlPduSizeBytes);
		break;
	case URlcModeTm:
		mDown = new URlcTransTm(rb,uep);
		break;

	}
}

URlcPair::~URlcPair()
{
	if (mDown->mRlcMode == URlcModeAm) {
		// It is RLC-AM, and there is only one combined entity.
		assert(mUp->mRlcMode == URlcModeAm);
		URlcAm *am = dynamic_cast<URlcAm*>(mDown);
		assert(am);
		delete am;
	} else {
		delete mUp;
		delete mDown;
	}
}

};	// namespace UMTS
