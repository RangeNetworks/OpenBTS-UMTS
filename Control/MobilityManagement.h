/**@file GSM/SIP Mobility Management, GSM 04.08. */

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

#ifndef MOBILITYMANAGEMENT_H
#define MOBILITYMANAGEMENT_H


namespace GSM {
class LogicalChannel;
class L3CMServiceRequest;
class L3LocationUpdatingRequest;
class L3IMSIDetachIndication;
};

namespace Control {

void CMServiceResponder(const GSM::L3CMServiceRequest* cmsrq, UMTS::LogicalChannel* DCCH);

void IMSIDetachController(const GSM::L3IMSIDetachIndication* idi, UMTS::DCCHLogicalChannel* DCCH);

void LocationUpdatingController(const GSM::L3LocationUpdatingRequest* lur, UMTS::DCCHLogicalChannel* DCCH);

}


#endif
