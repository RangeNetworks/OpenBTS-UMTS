/**@file Logical Channel.  */

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

#include "UMTSLogicalChannel.h"
#include "URRC.h"
#include "URRCDefs.h"

#include "ControlCommon.h"
#include <GSML3MMMessages.h>

namespace UMTS {

std::ostream& operator<<(std::ostream& os, const UMTS::LogicalChannel& chan)
{
	// (pat) This class will not know details about the PHY level.
	// TODO: It will be identified by a RadioBearer id and possibly a TrCh id.
	//os << chan.type() << " SF=" << chan.SF() << " SpC=" << chan.SpCode() << " SrC=" << chan.SrCode();
	return os;
}

L1ControlInterface *LogicalChannel::getPhy() const { return mUep->mUeDch->getControlInterface(); }
float LogicalChannel::RSSI() const { return mUep->RSSI(); }
float LogicalChannel::timingError() const { return mUep->timingError(); }

void LogicalChannel::setPhy(float wRSSI, float wTimingError)
{
	mUep->setPhy(wRSSI,wTimingError);
}

void LogicalChannel::send(const GSM::L3Message& msg, GSM::Primitive prim, unsigned SAPI)
{
	GSM::L3Frame frame(msg,prim);
	send(frame,SAPI);
}

void LogicalChannel::send(const GSM::Primitive& prim, unsigned SAPI)
{
	send(GSM::L3Frame(prim),SAPI);
}

//extern void Control::DCCHDispatchMessage(const GSM::L3Message*, UMTS::DCCHLogicalChannel*);

void LogicalChannel::l3writeHighSide(ByteVector &msg)
{
	GSM::L3Frame *frame3 = new GSM::L3Frame(msg.sizeBits(),GSM::DATA);
	RN_MEMLOG(L3Frame,frame3);
	frame3->unpack(msg.begin());
        //const GSM::L3Message* msg2 = (const GSM::L3Message*) GSM::parseL3(*frame3);
	//UMTS::DCCHLogicalChannel* thisChan = dynamic_cast<UMTS::DCCHLogicalChannel*>(this);
        //Control::DCCHDispatchMessage(msg2,thisChan);
        //delete msg2;
	mL3RxQ.write(frame3);
	gDCCHLogicalChannelFIFO.write(dynamic_cast<UMTS::DCCHLogicalChannel*>(this));
}

// TODO: What to do with SAPI?
GSM::L3Frame *LogicalChannel::recv(unsigned timeout_ms, unsigned SAPI)
{
	return mL3RxQ.read(timeout_ms);
	// TODO: Who deletes these? Make sure.
}

void LogicalChannel::send(const GSM::L3Frame& frame, unsigned SAPI)
{
	static const std::string description("GSM L3 Message");
	ByteVector sdu(frame);
	ByteVector *transferSdu = sendDirectTransfer(mUep, sdu, description.c_str(), false);
        /*GSM::L3MMMessage::MessageType MTI = (GSM::L3MMMessage::MessageType)(0xbf & frame.MTI());
	if (MTI == GSM::L3MMMessage::LocationUpdatingAccept) {
		LOG(INFO) << "Send Security Mode";
			sendSecurityModeCommand(mUep);
	}
	else {LOG(INFO) << "Security MTI =" << MTI;}*/
        mUep->ueWriteHighSide(SRB3, *transferSdu, description);
	delete transferSdu;
}

// TODO: Fix stubs to get through compilation:
int LogicalChannel::actualMSPower() const { return 0; }
/** Actual MS uplink timing advance. */
int LogicalChannel::actualMSTiming() const { return 0; }
void LogicalChannel::addTransaction(Control::TransactionEntry* transaction) {}
void LogicalChannel::open() {}
void LogicalChannel::connect() {}
};	// namespace
