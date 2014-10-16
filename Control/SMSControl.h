/**@file Declarations for common-use control-layer functions. */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef SMSCONTROL_H
#define SMSCONTROL_H

#include <SMSMessages.h>

namespace GSM {
class L3Message;
class L3CMServiceRequest;
}

namespace UMTS {
class DCCHLogicalChannel;
}



namespace Control {

class TransactionEntry;

/** MOSMS state machine.  */
void MOSMSController(const GSM::L3CMServiceRequest *req, UMTS::DCCHLogicalChannel *LCH);

/** MOSMS-with-parallel-call state machine.  */
void InCallMOSMSStarter(Control::TransactionEntry *parallelCall);

/** MOSMS-with-parallel-call state machine.  */
void InCallMOSMSController(const SMS::CPData *msg, Control::TransactionEntry* transaction, UMTS::DCCHLogicalChannel *LCH);
/**
	Basic SMS delivery from an established CM.
	On exit, SAP3 will be in ABM and LCH will still be open.
	Throws exception for failures in connection layer or for parsing failure.
	@return true on success in relay layer.
*/
bool deliverSMSToMS(const char *callingPartyDigits, const char* message, const char* contentType, unsigned TI, UMTS::DCCHLogicalChannel *LCH);

/** MTSMS */
void MTSMSController(Control::TransactionEntry* transaction, UMTS::DCCHLogicalChannel *LCH);

}




#endif

// vim: ts=4 sw=4
