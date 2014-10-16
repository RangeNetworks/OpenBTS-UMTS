/**@file GSM/SIP Mobility Management, GSM 04.08. */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include <Timeval.h>
#include <Utils.h>

#include "ControlCommon.h"
#include "MobilityManagement.h"
#include "SMSControl.h"
#include "CallControl.h"

#include <GSML3RRMessages.h>
#include <GSML3MMMessages.h>
#include <GSML3CCMessages.h>
#include <UMTSConfig.h>
#include <UMTSLogicalChannel.h>

using namespace std;

#include <SIPInterface.h>
#include <SIPUtility.h>
#include <SIPMessage.h>
#include <SIPEngine.h>

using namespace SIP;

#include <Logger.h>
#undef WARNING


using namespace Control;


/** Controller for CM Service requests, dispatches out to multiple possible transaction controllers. */
void Control::CMServiceResponder(const GSM::L3CMServiceRequest* cmsrq, UMTS::LogicalChannel* DCCH)
{
	assert(cmsrq);
	assert(DCCH);
	LOG(INFO) << *cmsrq;
	switch (cmsrq->serviceType().type()) {
		case GSM::L3CMServiceType::MobileOriginatedCall:
			MOCStarter(cmsrq,dynamic_cast<UMTS::DTCHLogicalChannel*>(DCCH));
			break;
		case GSM::L3CMServiceType::ShortMessage:
			MOSMSController(cmsrq,dynamic_cast<UMTS::DCCHLogicalChannel*>(DCCH));
			break;
		default:
			LOG(NOTICE) << "service not supported for " << *cmsrq;
			// Cause 0x20 means "serivce not supported".
			DCCH->send(GSM::L3CMServiceReject(0x20));
			DCCH->send(GSM::L3ChannelRelease());
	}
	// The transaction may or may not be cleared,
	// depending on the assignment type.
}




/** Controller for the IMSI Detach transaction, GSM 04.08 4.3.4. */
void Control::IMSIDetachController(const GSM::L3IMSIDetachIndication* idi, UMTS::DCCHLogicalChannel* DCCH)
{
	assert(idi);
	assert(DCCH);
	LOG(INFO) << *idi;

	// The IMSI detach maps to a SIP unregister with the local Asterisk server.
	try { 
		// FIXME -- Resolve TMSIs to IMSIs.
		if (idi->mobileID().type()==GSM::IMSIType) {
			SIPEngine engine(gConfig.getStr("SIP.Proxy.Registration").c_str(), idi->mobileID().digits());
			engine.unregister();
		}
	}
	catch(SIPTimeout) {
		LOG(ALERT) "SIP registration timed out.  Is Asterisk running?";
	}
	// No reponse required, so just close the channel.
	DCCH->send(GSM::L3ChannelRelease());
	// Many handsets never complete the transaction.
	// So force a shutdown of the channel.
	DCCH->send(GSM::HARDRELEASE);
}




bool authenticateViaCaching(const char *IMSI, UMTS::DCCHLogicalChannel *DCCH)
{
	uint64_t uRAND=0, lRAND=0;
	uint32_t SRES=0;
	// bool foundIMSI = gTMSITable.getAuthTokens(IMSI,uRAND,lRAND,SRES);

	// First time?
	if (!uRAND) {
		LOG(NOTICE) << "First cache-based authentication for " << IMSI;
		// Generate 128 bits of RAND.
		uRAND = random(); uRAND = (uRAND<<32) + random();
		lRAND = random(); lRAND = (lRAND<<32) + random();
		// Do the transaction.
		DCCH->send(GSM::L3AuthenticationRequest(0,GSM::L3RAND(uRAND,lRAND)));
		GSM::L3Message* msg = getMessage(DCCH);
		GSM::L3AuthenticationResponse *resp = dynamic_cast<GSM::L3AuthenticationResponse*>(msg);
		if (!resp) {
			if (msg) {
				LOG(WARNING) << "Unexpected message " << *msg;
				delete msg;
			}
			throw UnexpectedMessage();
		}
		LOG(INFO) << *resp;
		gTMSITable.putAuthTokens(IMSI,uRAND,lRAND,resp->SRES().value());
		delete msg;
		return true;
	}

	// Next time.
	// Do the transaction using tokens from the table.
	DCCH->send(GSM::L3AuthenticationRequest(0,GSM::L3RAND(uRAND,lRAND)));
	GSM::L3Message* msg = getMessage(DCCH);
	GSM::L3AuthenticationResponse *resp = dynamic_cast<GSM::L3AuthenticationResponse*>(msg);
	if (!resp) {
		if (msg) {
			LOG(WARNING) << "Unexpected message " << *msg;
			delete msg;
		}
		throw UnexpectedMessage();
	}
	LOG(INFO) << *resp;
	// Compare and reject if comparison failed.
	bool OK = SRES == resp->SRES().value();
	delete msg;
	return OK;
}


/**
	Send a given welcome message from a given short code.
	@return true if it was sent
*/
bool sendWelcomeMessage(const char* messageName, const char* shortCodeName, const char *IMSI, UMTS::DCCHLogicalChannel* DCCH, const char *whiteListCode = NULL)
{
	if (!gConfig.defines(messageName)) return false;
	LOG(INFO) << "sending " << messageName << " message to handset";
	ostringstream message;
	message << gConfig.getStr(messageName) << " IMSI:" << IMSI;
	// This returns when delivery is acked in L3.
	deliverSMSToMS(
		gConfig.getStr(shortCodeName).c_str(),
		message.str().c_str(), "text/plain",
		random()%7,DCCH);
	return true;
}


/**
	Controller for the Location Updating transaction, GSM 04.08 4.4.4.
	@param lur The location updating request.
	@param DCCH The Dm channel to the MS, which will be released by the function.
*/
void Control::LocationUpdatingController(const GSM::L3LocationUpdatingRequest* lur, UMTS::DCCHLogicalChannel* DCCH)
{
	assert(DCCH);
	assert(lur);
	LOG(INFO) << *lur;

	// The location updating request gets mapped to a SIP
	// registration with the Asterisk server.

	// We also allocate a new TMSI for every handset we encounter.
	// If the handset is allow to register it may receive a TMSI reassignment.

	// Resolve an IMSI and see if there's a pre-existing IMSI-TMSI mapping.
	// This operation will throw an exception, caught in a higher scope,
	// if it fails in the GSM domain.
	GSM::L3MobileIdentity mobileID = lur->mobileID();
	bool sameLAI = (lur->LAI() == gNodeB.LAI());
	unsigned preexistingTMSI = resolveIMSI(sameLAI,mobileID,DCCH);
	const char *IMSI = mobileID.digits();
	// IMSIAttach set to true if this is a new registration.
	bool IMSIAttach = (preexistingTMSI==0);

	// We assign generate a TMSI for every new phone we see,
	// even if we don't actually assign it.
	unsigned newTMSI = 0;
	if (!preexistingTMSI) newTMSI = gTMSITable.assign(IMSI,lur);

	// Try to register the IMSI with Asterisk.
	// This will be set true if registration succeeded in the SIP world.
	bool success = false;
	string RAND;
	try {
		SIPEngine engine(gConfig.getStr("SIP.Proxy.Registration").c_str(),IMSI);
		LOG(DEBUG) << "waiting for registration";
		success = engine.Register(SIPEngine::SIPRegister, &RAND); 
	}
	catch(SIPTimeout) {
		LOG(ALERT) "SIP registration timed out.  Is the proxy running at " << gConfig.getStr("SIP.Proxy.Registration");
		// Reject with a "network failure" cause code, 0x11.
		DCCH->send(GSM::L3LocationUpdatingReject(0x11));
		// HACK -- wait long enough for a response
		// FIXME -- Why are we doing this?
		sleep(4);
		// Release the channel and return.
		DCCH->send(GSM::L3ChannelRelease());
		return;
	}

	// Did we get a RAND for challenge-response?
	if (RAND.length() != 0) {
		// Get the mobile's SRES.
		LOG(INFO) << "sending " << RAND << " to mobile";
		uint64_t uRAND;
		uint64_t lRAND;
		stringToUint(RAND, &uRAND, &lRAND);
		DCCH->send(GSM::L3AuthenticationRequest(0,GSM::L3RAND(uRAND,lRAND)));
		GSM::L3Message* msg = getMessage(DCCH);
		GSM::L3AuthenticationResponse *resp = dynamic_cast<GSM::L3AuthenticationResponse*>(msg);
		if (!resp) {
			if (msg) {
				LOG(WARNING) << "Unexpected message " << *msg;
				delete msg;
			}
			// FIXME -- We should differentiate between wrong message and no message at all.
			throw UnexpectedMessage();
		}
		LOG(INFO) << *resp;
		uint32_t mobileSRES = resp->SRES().value();
		delete msg;
		// verify SRES 
		try {
			SIPEngine engine(gConfig.getStr("SIP.Proxy.Registration").c_str(),IMSI);
			LOG(DEBUG) << "waiting for registration";
			ostringstream os;
			os << hex << mobileSRES;
			string SRESstr = os.str();
			success = engine.Register(SIPEngine::SIPRegister, &RAND, IMSI, SRESstr.c_str()); 
		}
		catch(SIPTimeout) {
			LOG(ALERT) "SIP authentication timed out.  Is the proxy running at " << gConfig.getStr("SIP.Proxy.Registration");
			// Reject with a "network failure" cause code, 0x11.
			DCCH->send(GSM::L3LocationUpdatingReject(0x11));
			// HACK -- wait long enough for a response
			// FIXME -- Why are we doing this?
			sleep(4);
			// Release the channel and return.
			DCCH->send(GSM::L3ChannelRelease());
			return;
		}
	}

	// This allows us to configure Open Registration
	bool openRegistration = gConfig.defines("Control.LUR.OpenRegistration");

	// Authentication.
	// If no method is assigned, assume authentication is not required.
	bool authenticateOK = gConfig.defines("Control.LUR.DefaultAuthenticationAccept");

	 // RAND-SRES Exchange compared to cache?
	if (gConfig.defines("Control.LUR.CachedAuthentication")) {
		authenticateOK = authenticateViaCaching(IMSI,DCCH);
		LOG(INFO) << "cache-based authentication for IMSI " << IMSI << " result " << authenticateOK;
	}

	if (!authenticateOK && !openRegistration) {
		LOG(CRIT) << "failed authentication for IMSI " << IMSI;
		DCCH->send(GSM::L3AuthenticationReject());
		DCCH->send(GSM::L3ChannelRelease());
		return;
	}


	// Query for IMEI?
	if (IMSIAttach && gConfig.getBool("Control.LUR.QueryIMEI")) {
		DCCH->send(GSM::L3IdentityRequest(GSM::IMEIType));
		GSM::L3Message* msg = getMessage(DCCH);
		GSM::L3IdentityResponse *resp = dynamic_cast<GSM::L3IdentityResponse*>(msg);
		if (!resp) {
			if (msg) {
				LOG(WARNING) << "Unexpected message " << *msg;
				delete msg;
			}
			throw UnexpectedMessage();
		}
		LOG(INFO) << *resp;
		if (!gTMSITable.IMEI(IMSI,resp->mobileID().digits()))
			LOG(WARNING) << "failed access to TMSITable";
		delete msg;
	}

	// Query for classmark?
	if (IMSIAttach && gConfig.getBool("Control.LUR.QueryClassmark")) {
		DCCH->send(GSM::L3ClassmarkEnquiry());
		GSM::L3Message* msg = getMessage(DCCH);
		GSM::L3ClassmarkChange *resp = dynamic_cast<GSM::L3ClassmarkChange*>(msg);
		if (!resp) {
			if (msg) {
				LOG(WARNING) << "Unexpected message " << *msg;
				delete msg;
			}
			throw UnexpectedMessage();
		}
		LOG(INFO) << *resp;
		const GSM::L3MobileStationClassmark2& classmark = resp->classmark();
		if (!gTMSITable.classmark(IMSI,classmark))
			LOG(WARNING) << "failed access to TMSITable";
		delete msg;
	}

	// We fail closed unless we're configured otherwise
	if (!success && !openRegistration) {
		LOG(INFO) << "registration FAILED: " << mobileID;
		DCCH->send(GSM::L3LocationUpdatingReject(gConfig.getNum("Control.LUR.UnprovisionedRejectCause")));
		if (!preexistingTMSI) {
			sendWelcomeMessage( "Control.LUR.FailedRegistration.Message",
				"Control.LUR.FailedRegistration.ShortCode", IMSI,DCCH);
		}
		// Release the channel and return.
		DCCH->send(GSM::L3ChannelRelease());
		return;
	}

	// If success is true, we had a normal registration.
	// Otherwise, we are here because of open registration.
	// Either way, we're going to register a phone if we arrive here.

	if (success) {
		LOG(INFO) << "registration SUCCESS: " << mobileID;
	} else {
		LOG(INFO) << "registration ALLOWED: " << mobileID;
	}


	// Send the "short name" and time-of-day.
	if (IMSIAttach && gConfig.defines("UMTS.Identity.ShortName")) {
		DCCH->send(GSM::L3MMInformation(gConfig.getStr("UMTS.Identity.ShortName").c_str()));
	}
	// Accept. Make a TMSI assignment, too, if needed.
	if (preexistingTMSI || !gConfig.getBool("Control.LUR.SendTMSIs")) {
		DCCH->send(GSM::L3LocationUpdatingAccept(gNodeB.LAI()));
	} else {
		assert(newTMSI);
		DCCH->send(GSM::L3LocationUpdatingAccept(gNodeB.LAI(),newTMSI));
		// Wait for MM TMSI REALLOCATION COMPLETE (0x055b).
		GSM::L3Frame* resp = DCCH->recv(1000);
		// FIXME -- Actually check the response type.
		if (!resp) {
			LOG(NOTICE) << "no response to TMSI assignment";
		} else {
			LOG(INFO) << *resp;
		}
		delete resp;
	}

	// If this is an IMSI attach, send a welcome message.
	if (IMSIAttach) {
		if (success) {
			sendWelcomeMessage( "Control.LUR.NormalRegistration.Message",
				"Control.LUR.NormalRegistration.ShortCode", IMSI, DCCH);
		} else {
			sendWelcomeMessage( "Control.LUR.OpenRegistration.Message",
				"Control.LUR.OpenRegistration.ShortCode", IMSI, DCCH);
		}
	}


	// Release the channel and return.
	DCCH->send(GSM::L3ChannelRelease());
	return;
}




// vim: ts=4 sw=4
