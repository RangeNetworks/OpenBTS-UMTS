/**@file Idle-mode dispatcher for dedicated control channels. */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009 Free Software Foundation, Inc.
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include "ControlCommon.h"
#include "TransactionTable.h"
#include "RadioResource.h"
#include "MobilityManagement.h"
#include <GSML3MMMessages.h>
#include <GSML3RRMessages.h>
#include <SIPUtility.h>
#include <SIPInterface.h>

#include <UMTSLogicalChannel.h>

#include <Logger.h>
#undef WARNING

using namespace std;
using namespace Control;




/**
	Dispatch the appropriate controller for a Mobility Management message.
	@param req A pointer to the initial message.
	@param DCCH A pointer to the logical channel for the transaction.
*/
void DCCHDispatchMM(const GSM::L3MMMessage* req, UMTS::DCCHLogicalChannel *DCCH)
{
	assert(req);
	GSM::L3MMMessage::MessageType MTI = (GSM::L3MMMessage::MessageType)req->MTI();
	switch (MTI) {
		case GSM::L3MMMessage::LocationUpdatingRequest:
			LocationUpdatingController(dynamic_cast<const GSM::L3LocationUpdatingRequest*>(req),DCCH);
			break;
		case GSM::L3MMMessage::IMSIDetachIndication:
			IMSIDetachController(dynamic_cast<const GSM::L3IMSIDetachIndication*>(req),DCCH);
			break;
		case GSM::L3MMMessage::CMServiceRequest:
			CMServiceResponder(dynamic_cast<const GSM::L3CMServiceRequest*>(req),DCCH);
			break;
		default:
			LOG(NOTICE) << "unhandled MM message " << MTI << " on " << *DCCH;
			throw UnsupportedMessage();
	}
}


/**
	Dispatch the appropriate controller for a Radio Resource message.
	@param req A pointer to the initial message.
	@param DCCH A pointer to the logical channel for the transaction.
*/
void DCCHDispatchRR(const GSM::L3RRMessage* req, UMTS::DCCHLogicalChannel *DCCH)
{
	LOG(DEBUG) << "checking MTI"<< (GSM::L3RRMessage::MessageType)req->MTI();

	// TODO SMS -- This needs to handle SACCH Measurement Reports.

	assert(req);
	GSM::L3RRMessage::MessageType MTI = (GSM::L3RRMessage::MessageType)req->MTI();
	switch (MTI) {
		case GSM::L3RRMessage::PagingResponse:
#if 0
			RRC::PagingResponseHandler(dynamic_cast<const GSM::L3PagingResponse*>(req),DCCH);
#else
			LOG(NOTICE) << "unhandled paging response " << MTI << " on " << *DCCH;
                        throw UnsupportedMessage();
#endif
			break;
		default:
			LOG(NOTICE) << "unhandled RR message " << MTI << " on " << *DCCH;
			throw UnsupportedMessage();
	}
}

void DCCHDispatchMessage(const GSM::L3Message* msg, UMTS::DCCHLogicalChannel* DCCH)
{
	// Each protocol has it's own sub-dispatcher.
	switch (msg->PD()) {
		case GSM::L3MobilityManagementPD:
			DCCHDispatchMM(dynamic_cast<const GSM::L3MMMessage*>(msg),DCCH);
			break;
		case GSM::L3RadioResourcePD:
			DCCHDispatchRR(dynamic_cast<const GSM::L3RRMessage*>(msg),DCCH);
			break;
		default:
			LOG(NOTICE) << "unhandled protocol " << msg->PD() << " on " << *DCCH;
			throw UnsupportedMessage();
	}
}


DCCHLogicalChannelFIFO gDCCHLogicalChannelFIFO;

/** Example of a closed-loop, persistent-thread control function for the DCCH. */
//void Control::DCCHDispatcher(UMTS::DCCHLogicalChannel *DCCH)
void Control::DCCHDispatcher()
{
	while (1) {
                UMTS::DCCHLogicalChannel *DCCH = gDCCHLogicalChannelFIFO.read(20000);
                if (DCCH==NULL) continue;
		try {
			// Wait for a transaction to start.
			LOG(DEBUG) << "waiting for " << *DCCH << " ESTABLISH";
			DCCH->waitForPrimitive(GSM::ESTABLISH);
			// Pull the first message and dispatch a new transaction.
			const GSM::L3Message *message = getMessage(DCCH);
			LOG(DEBUG) << *DCCH << " received " << *message;
			DCCHDispatchMessage(message,DCCH);
			delete message;
		}

		// Catch the various error cases.

		catch (ChannelReadTimeout except) {
			LOG(NOTICE) << "ChannelReadTimeout";
			// Cause 0x03 means "abnormal release, timer expired".
			DCCH->send(GSM::L3ChannelRelease(0x03));
			gTransactionTable.remove(except.transactionID());
		}
		catch (UnexpectedPrimitive except) {
			LOG(NOTICE) << "UnexpectedPrimitive";
			// Cause 0x62 means "message type not not compatible with protocol state".
			DCCH->send(GSM::L3ChannelRelease(0x62));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (UnexpectedMessage except) {
			LOG(NOTICE) << "UnexpectedMessage";
			// Cause 0x62 means "message type not not compatible with protocol state".
			DCCH->send(GSM::L3ChannelRelease(0x62));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (UnsupportedMessage except) {
			LOG(NOTICE) << "UnsupportedMessage";
			// Cause 0x61 means "message type not implemented".
			DCCH->send(GSM::L3ChannelRelease(0x61));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (Q931TimerExpired except) {
			LOG(NOTICE) << "Q.931 T3xx timer expired";
			// Cause 0x03 means "abnormal release, timer expired".
			// TODO -- Send diagnostics.
			DCCH->send(GSM::L3ChannelRelease(0x03));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (SIP::SIPTimeout except) {
			// FIXME -- The transaction ID should be an argument here.
			LOG(WARNING) << "Uncaught SIPTimeout, will leave a stray transcation";
			// Cause 0x03 means "abnormal release, timer expired".
			DCCH->send(GSM::L3ChannelRelease(0x03));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
		catch (SIP::SIPError except) {
			// FIXME -- The transaction ID should be an argument here.
			LOG(WARNING) << "Uncaught SIPError, will leave a stray transcation";
			// Cause 0x01 means "abnormal release, unspecified".
			DCCH->send(GSM::L3ChannelRelease(0x01));
			if (except.transactionID()) gTransactionTable.remove(except.transactionID());
		}
	}
}




// vim: ts=4 sw=4
