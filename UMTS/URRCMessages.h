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

#ifndef URRCMESSAGES_H
#define URRCMESSAGES_H 1
//#include "URRC.h"
#include "ByteVector.h"

#include "asn_system.h"	// Dont let other includes land in namespace ASN.
namespace ASN {
//#include "RRCConnectionRequest.h"	// including causes duplicate definition error.
#include "InitialUE-Identity.h"
};

namespace UMTS {
class PhCh;
class RrcMasterChConfig;
class UEInfo;

// The CCCH messages arrive on RACH for connection establishment.
// The ASC argument is passed in the initial message,
// but we dont implement priorities so we dont care what it is.
// Another way of saying this is it comes in on RACH using SRB0.
// The only thing we need out of this message is the UE Id, which has many
// possiblities, but we dont care what they are - we are just going to retransmit
// the same information in the RRC Connection Setup message.
void rrcRecvCcchMessage(BitVector &tb,unsigned asc);
// The DCCH messages come through the UE RLC machinery.
void rrcRecvDcchMessage(ByteVector &bv,UEInfo *uep, RbId rbNum);
ByteVector* sendDirectTransfer(UEInfo* uep, ByteVector &dlpud, const char *descr);
bool sendRadioBearerSetup(UEInfo *uep, RrcMasterChConfig *masterConfig, PhCh *phch, bool srbstoo);
void sendRadioBearerRelease(UEInfo *uep, unsigned rabMask, bool finished);
void sendRrcConnectionRelease(UEInfo *uep);
void sendCellUpdateConfirm(UEInfo *uep);
void sendSecurityModeCommand(UEInfo *uep);

// The UE initially sends its identity in the RRC Connection Request Message.
// We dont really care what it is, we just need to copy the exact
// same UE id info into the RRC Connection Setup Message, which also assigns a U-RNTI.
// From then on, we use the U-RNTI only.
class AsnUeId {
	ASN::InitialUE_Identity_PR idType;
	public:
	ByteVector mImsi, mImei, mTmsiDS41;
	UInt32_z mMcc, mMnc;
	UInt32_z mTmsi, mPtmsi, mEsn;
	UInt16_z mLac;
	UInt8_z mRac;

	public:
	AsnUeId() { idType = ASN::InitialUE_Identity_PR_NOTHING; }
	AsnUeId(ByteVector &wImsi) : idType(ASN::InitialUE_Identity_PR_imsi), mImsi(wImsi) {}
	AsnUeId(ASN::InitialUE_Identity &uid) { asnParse(uid); }
	bool RaiMatches();
	bool eql(AsnUeId &other);
	void asnParse(ASN::InitialUE_Identity &uid);
};


//class RrcConnectionRequestMessage { };

}; // namespace UMTS
#endif
