/**@file GSM/SIP Call Control -- GSM 04.08, ISDN ITU-T Q.931, SIP IETF RFC-3261, RTP IETF RFC-3550. */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef CALLCONTROL_H
#define CALLCONTROL_H

#include <UMTSCommon.h>


namespace GSM {
class L3CMServiceRequest;
};

namespace UMTS {
class LogicalChannel;
class DTCHLogicalChannel;
};

namespace Control {

class TransactionEntry;



/**@name MOC */
//@{
/** Run the MOC to the point of alerting, doing early assignment if needed. */
void MOCStarter(const GSM::L3CMServiceRequest*, UMTS::DTCHLogicalChannel*);
/** Complete the MOC connection. */
void MOCController(Control::TransactionEntry*, UMTS::DTCHLogicalChannel*);
//@}


/**@name MTC */
//@{
/** Run the MTC to the point of alerting, doing early assignment if needed. */
void MTCStarter(Control::TransactionEntry*, UMTS::DTCHLogicalChannel*);
/** Complete the MTC connection. */
void MTCController(Control::TransactionEntry*, UMTS::DTCHLogicalChannel*);
//@}


/**@name Test Call */
//@{
/** Run the test call. */
void TestCall(Control::TransactionEntry*, UMTS::DTCHLogicalChannel*);
//@}

/** Create a new transaction entry and start paging. */
void initiateMTTransaction(Control::TransactionEntry* transaction, UMTS::ChannelTypeL3 chanType, unsigned pageTime);


}


#endif
