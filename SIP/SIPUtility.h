/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef SIP_UTILITY_H
#define SIP_UTILITY_H


namespace SIP {

/**@name SIP-specific exceptions. */
//@{
class SIPException {
	protected:
	unsigned mTransactionID;

	public:
	SIPException(unsigned wTransactionID=0)
		:mTransactionID(wTransactionID)
	{ }

	unsigned transactionID() const { return mTransactionID; }
};

class SIPError : public SIPException {};
class SIPTimeout : public SIPException {};
//@}


/** Codec codes, from RFC-3551, Table 4. */
enum RTPCodec {
	RTPuLaw=0,
	RTPGSM610=3
};


/** Get owner IP address; return NULL if none found. */
bool get_owner_ip( osip_message_t * msg, char * o_addr );

/** Get RTP parameters; return NULL if none found. */
bool get_rtp_params(const osip_message_t * msg, char * port, char * ip_addr );

void make_tag( char * tag );

void make_branch(char * branch);


};
#endif
// vim: ts=4 sw=4
