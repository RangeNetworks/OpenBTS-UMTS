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

#include "UMTSTransfer.h"
#include "URRCMessages.h"
#include "MACEngine.h"
#include "AsnHelper.h"
#include "Logger.h"
#include "SgsnExport.h"
#include "URRC.h"
#include "UMTSLogicalChannel.h"
//#include "asn_system.h"	included from AsnHelper.h
namespace ASN {
//#include "BIT_STRING.h"
#include "UL-CCCH-Message.h"
#include "DL-CCCH-Message.h"
#include "UL-DCCH-Message.h"
#include "DL-DCCH-Message.h"
#include "InitialUE-Identity.h"
#define PAT_SAMSUNG_TEST 1	// Try to get the samsung galaxy to accept this message.

#include "asn_SEQUENCE_OF.h"
//#include "RRCConnectionRequest.h"
//#include "RRCConnectionSetup.h"
};
#define CASENAME(x) case x: return #x;
#define CASEASNCOMMENT(foo) case ASN::foo: return #foo;

using namespace SGSN;

namespace UMTS {
const std::string descrRrcConnectionSetup("RRC_Connection_Setup_Message");
const std::string descrRrcConnectionRelease("RRC_Connection_Release_Message");
const std::string descrRadioBearerSetup("RRC Radio Bearer Setup Message");
const std::string descrRadioBearerRelease("RRC Radio Bearer Release Message");
const std::string descrCellUpdateConfirm("RRC Cell Update Confirm Message");
const std::string descrSecurityModeCommand("RRC Security Mode Command");

typedef unsigned char uchar;

static const char *asnUlDcchMsg2Name(ASN::UL_DCCH_MessageType_PR mtype)
{
	switch (mtype) {
	CASEASNCOMMENT(UL_DCCH_MessageType_PR_activeSetUpdateComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_activeSetUpdateFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_cellChangeOrderFromUTRANFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_counterCheckResponse)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_handoverToUTRANComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_initialDirectTransfer)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_handoverFromUTRANFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_measurementControlFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_measurementReport)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_physicalChannelReconfigurationComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_physicalChannelReconfigurationFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_radioBearerReconfigurationComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_radioBearerReconfigurationFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_radioBearerReleaseComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_radioBearerReleaseFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_radioBearerSetupComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_radioBearerSetupFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_rrcConnectionReleaseComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_rrcConnectionSetupComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_rrcStatus)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_securityModeComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_securityModeFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_signallingConnectionReleaseIndication)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_transportChannelReconfigurationComplete)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_transportChannelReconfigurationFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_transportFormatCombinationControlFailure)
	CASEASNCOMMENT(UL_DCCH_MessageType_PR_ueCapabilityInformation)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_uplinkDirectTransfer)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_utranMobilityInformationConfirm)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_utranMobilityInformationFailure)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_mbmsModificationRequest)
    CASEASNCOMMENT(UL_DCCH_MessageType_PR_spare1)
	default: return "UL_DCCH_unrecognized_message_type";
	}
}

static const char *asnUlCcchMsg2Name(ASN::UL_CCCH_MessageType_PR type)
{
	switch (type) {
	CASEASNCOMMENT(UL_CCCH_MessageType_PR_cellUpdate)
	CASEASNCOMMENT(UL_CCCH_MessageType_PR_rrcConnectionRequest)
	CASEASNCOMMENT(UL_CCCH_MessageType_PR_uraUpdate)
	default:
		return "UL_CCCH_unrecognized_message_type";
	}
}

static string getRBFailureCause(ASN::FailureCauseWithProtErr_t *failureCause)
{
	unsigned failcode = failureCause->present;
	string msg;
	switch (failcode) {
		case ASN::FailureCauseWithProtErr_PR_configurationUnsupported:
			msg = "configurationUnsupported"; break;
		case ASN::FailureCauseWithProtErr_PR_physicalChannelFailure:
			msg = "physicalChannelASN::Failure"; break;
		case ASN::FailureCauseWithProtErr_PR_incompatibleSimultaneousReconfiguration:
			msg = "incompatibleSimultaneousReconfiguration"; break;
		case ASN::FailureCauseWithProtErr_PR_compressedModeRuntimeError:
			msg = "compressedModeRuntimeError"; break;
		case ASN::FailureCauseWithProtErr_PR_protocolError:
			msg = "protocolError";
			// The protocolError choice has additional info, but since our spec has only one
			// possible value for that info, it is of little interest so ignore it.
			break;
		case ASN::FailureCauseWithProtErr_PR_cellUpdateOccurred:
			msg = "cellUpdateOccurred"; break;
		case ASN::FailureCauseWithProtErr_PR_invalidConfiguration:
			msg = "invalidConfiguration"; break;
		case ASN::FailureCauseWithProtErr_PR_configurationIncomplete:
			msg = "configurationIncomplete"; break;
		case ASN::FailureCauseWithProtErr_PR_unsupportedMeasurement:
			msg = "unsupportedMeasurement"; break;
		case ASN::FailureCauseWithProtErr_PR_mbmsSessionAlreadyReceivedCorrectly:
			msg ="mbmsSessionAlreadyReceivedCorrectly"; break;
		case ASN::FailureCauseWithProtErr_PR_lowerPriorityMBMSService:
			msg = "lowerPriorityMBMSService"; break;
		default: 
			msg = "unknown type"; break;
	}
	return msg;
}


ASN::RRC_StateIndicator UEState2Asn(UEState state)
{
	switch (state) {
	case stIdleMode:
		LOG(ERR) << "UE is unexpectedly in idle mode";
		// Assume CELL_FACH and fall through.
		return ASN::RRC_StateIndicator_cell_FACH;
	case stCELL_FACH: return ASN::RRC_StateIndicator_cell_FACH;
	case stCELL_DCH: return ASN::RRC_StateIndicator_cell_DCH;
	case stCELL_PCH: return ASN::RRC_StateIndicator_cell_PCH;
	case stURA_PCH: return ASN::RRC_StateIndicator_ura_PCH;
	default: assert(0);
	}
}

// Run the Integrity Protection Algorithm and ASN encode the message.
// Return result in &result.
static bool encodeDcchMsg(UEInfo *uep, RbId rbid, ASN::DL_DCCH_Message_t *msg, ByteVector &result, string descr)
{
	if (uep->integrity.isStarted()) {
		// Step 1: Set the integrity check info to the values for encoding specified in 10.3.3.16.
		ASN::IntegrityCheckInfo *ici = RN_CALLOC(ASN::IntegrityCheckInfo);
		msg->integrityCheckInfo = ici;
		ici->messageAuthenticationCode = allocAsnBIT_STRING(32);
		// The RBID goes in the 5 LSB of the 32-bit string that will
		// later hold the MAC-I [Message Authentication Code].
		AsnBitString2BVTemp(ici->messageAuthenticationCode).setField(32-5,rbid-0*1,5);
		ici->rrc_MessageSequenceNumber = 0; // 10.3.3.16 of 25.331;

		// Step 2: Encode the message
		if (!uperEncodeToBV(&ASN::asn_DEF_DL_DCCH_Message,msg,result,descr)) {return false;}
		// After encoding it is safe to advance the COUNT-I, ie, this message advances
		// the RRC SN [sequence number.]
		//uep->integrity.advanceDlRrcSn(rbid);


		// Step 3: Run the message through the Integrity Protection Algorithm.
		uint32_t maci = uep->integrity.runF9(rbid,1,result);
                // After encoding it is safe to advance the COUNT-I, ie, this message advances
                // the RRC SN [sequence number.]
		ici->rrc_MessageSequenceNumber = uep->integrity.getDlRrcSn(rbid); // 10.3.3.16 of 25.331
                uep->integrity.advanceDlRrcSn(rbid);


		// Step 4: Set the Message Authentication Code in the message.
		AsnBitString2BVTemp(ici->messageAuthenticationCode).setField(0,maci,32);
	}

	// Encode the message (again), completely oblivious to cpu cycles consumed.
	if (!uperEncodeToBV(&ASN::asn_DEF_DL_DCCH_Message,msg,result,descr)) {return false;}

	std::string comment = format("DL_DCCH %s message size=%d",descr.c_str(),result.size());
	asnLogMsg(rbid, &ASN::asn_DEF_DL_DCCH_Message, msg,comment.c_str(),uep);

	//if (gConfig.getNum("UMTS.Debug.Messages")) {
	//	asn_fprint(stdout,&ASN::asn_DEF_DL_DCCH_Message, msg);
	//	std::string readable = asn2string(&ASN::asn_DEF_DL_DCCH_Message, msg);
	//	fprintf(stdout,"Encoded message size=%d bytes\n%s",result.size());
	//	fflush(stdout);
	//}
	return true;
}

// Return TRUE on success.
static bool encodeCcchMsg(ASN::DL_CCCH_Message_t *msg, ByteVector &result,
	string descr,	// Description for the log
	UEInfo *uep,	// UE for the log, or NULL if none.
	uint32_t urnti	// URNTI for the log, unneeded if uep is non-null.
	)
{
	// We can just return: uperEncodeToBV printed a message on failure.
	bool stat = uperEncodeToBV(&ASN::asn_DEF_DL_CCCH_Message,msg,result,descr);
	if (stat) {
		string comment = format("DL_CCCH %s message size=%d",descr.c_str(),result.size());
		asnLogMsg(0, &ASN::asn_DEF_DL_CCCH_Message, msg,comment.c_str(),uep,urnti);
	}
	//if (gConfig.getNum("UMTS.Debug.Messages")) {
	//	asn_fprint(stdout,&ASN::asn_DEF_DL_CCCH_Message, msg);
	//	fprintf(stdout,"Encoded message size=%d bytes\n",result.size());
	//	fflush(stdout);
	//}
	return stat;
}


// Same as RB_InformationSetup but without PDCP info.
// The list we put these things in may be either SRB_InformationSetupList or SRB_InformationSetupList2,
// which are 100% identical, but nevertheless the result list type must be void**
// 25.331 10.3.4.24
static void toAsnSRB_InformationSetupList(RrcMasterChConfig *masterConfig, void*srblist)
{
	// Set up SRBs.
	for (unsigned rbid = SRB1; rbid <= SRB3; rbid++) {
		RBInfo *rb = masterConfig->getRB(rbid);
		assert(rb->valid());
		ASN::SRB_InformationSetup *srbie = RN_CALLOC(ASN::SRB_InformationSetup);
		srbie->rb_Identity = RN_CALLOC(long);
		*srbie->rb_Identity = rbid;
		srbie->rlc_InfoChoice.present = ASN::RLC_InfoChoice_PR_rlc_Info;
		rb->toAsnRLC_Info(&srbie->rlc_InfoChoice.choice.rlc_Info);
		// This assumes uplink and downlink are configured with identical TrCh.
		// They dont have to be.
		TrChId tcid = rb->mTrChAssigned;
		defaultRbMappingInfoToAsn(masterConfig->getUlTrChInfo(tcid),
			masterConfig->getDlTrChInfo(tcid),rbid,&srbie->rb_MappingInfo);
		ASN_SEQUENCE_ADD(srblist,srbie);
	}
}

// This is almost the same as above, but allows you to change
// only the RBMappinginfo, not other RLC programming.
// We can use it to switch the SRBs between CELL_DCH and CELL_FACH mode,
// because it allows us to specify a new TrCh, which implies a new TFS,
// which allows the new rlcs to resize to match the new TFS.
static ASN::RB_InformationAffectedList *toAsnRB_InformationAffectedList(
	RrcMasterChConfig *masterConfig)
{
	ASN::RB_InformationAffectedList *result = RN_CALLOC(ASN::RB_InformationAffectedList); 
	// Set up SRBs.
	for (unsigned rbid = SRB1; rbid <= SRB3; rbid++) {
		RBInfo *rb = masterConfig->getRB(rbid);
		assert(rb->valid());
		ASN::RB_InformationAffected *srbie = RN_CALLOC(ASN::RB_InformationAffected);
		srbie->rb_Identity = rbid;

		// This assumes uplink and downlink are configured with identical TrCh.
		// They dont have to be.
		TrChId tcid = rb->mTrChAssigned;
		defaultRbMappingInfoToAsn(masterConfig->getUlTrChInfo(tcid),
			masterConfig->getDlTrChInfo(tcid),rbid,&srbie->rb_MappingInfo);
		ASN_SEQUENCE_ADD(&result->list,srbie);
	}
	return result;
}

static void toAsnRAB_Identity(int rbid,ASN::RAB_Identity_t *rabp)
{
	// 10.3.1.14: RAB Identity "This information uniquely identifies a RAB within a CN domain."
	// TODO: There are different kinds for GSM-MAP or ANSI-41, but I dont think it matters
	// because we dont use it and the phone shouldnt care, although it may need to be unique,
	// which we can insure simply by using the rbid.
	rabp->present = ASN::RAB_Identity_PR_gsm_MAP_RAB_Identity;
	setAsnBIT_STRING(&rabp->choice.gsm_MAP_RAB_Identity,(uint8_t*)calloc(1,1),8);
	AsnBitString2BVTemp(&rabp->choice.gsm_MAP_RAB_Identity).setField(0,rbid,8);
}

// 3GPP 25.331 10.3.4.8
// ASN RAB_Info_t   rab_Info;
void toAsnRAB_Info(RBInfo *rb,ASN::RAB_Info_t *rabInfoIE)
{
	// RAB_Identity_t   rab_Identity;
	toAsnRAB_Identity(rb->mRbId,&rabInfoIE->rab_Identity);

	// CN_DomainIdentity_t  cn_DomainIdentity;	// cs or ps?
	bool csdomain = rb->isCsDomain();
	rabInfoIE->cn_DomainIdentity = toAsnEnumerated(
		 csdomain ? ASN::CN_DomainIdentity_cs_domain : ASN::CN_DomainIdentity_ps_domain);

	// NAS_Synchronisation_Indicator_t *nas_Synchronisation_Indicator  /* OPTIONAL */;

	// Re_EstablishmentTimer_t  re_EstablishmentTimer;
	// TODO: Which should we use here?  Choice is T314 or T315
	// 25.331 13.1 implies T314 is for CS and T315 is for PS, so why are they asking us?
	// Also says see 8.3.1.13 and 8.3.1.14
	rabInfoIE->re_EstablishmentTimer = toAsnEnumerated(
		csdomain ? ASN::Re_EstablishmentTimer_useT314 : ASN::Re_EstablishmentTimer_useT315);
	// Finished with RAB_Info_t
}

// 3GPP 10.3.4.20
// ASN RB_InformationSetup
static ASN::RB_InformationSetup *toAsnRB_InformationSetup(RrcMasterChConfig *masterConfig, RBInfo *rb)
{
	assert(rb->valid());
	int rbid = rb->mRbId;

	ASN::RB_InformationSetup *rbie = RN_CALLOC(ASN::RB_InformationSetup);
	// RB_Identity_t    rb_Identity;
	rbie->rb_Identity = rbid;

	// struct PDCP_Info    *pdcp_Info  /* OPTIONAL */;
    // We dont use PDCP.  So why is it here at all?  Maybe it is mandatory for non-signalling RBs?
	rbie->pdcp_Info = RN_CALLOC(ASN::PDCP_Info);
	rbie->pdcp_Info->losslessSRNS_RelocSupport = RN_CALLOC(ASN::LosslessSRNS_RelocSupport);
	rbie->pdcp_Info->losslessSRNS_RelocSupport->present = ASN::LosslessSRNS_RelocSupport_PR_notSupported;
	//rbie->pdcpInfo->mLosslessSRNS_RelocSupport->choice = ASN::NULL;
	rbie->pdcp_Info->pdcp_PDU_Header = toAsnEnumerated(ASN::PDCP_PDU_Header_absent);
	
	// RLC_InfoChoice_t     rlc_InfoChoice;
	rbie->rlc_InfoChoice.present = ASN::RLC_InfoChoice_PR_rlc_Info;
	rb->toAsnRLC_Info(&rbie->rlc_InfoChoice.choice.rlc_Info);

	// RB_MappingInfo_t     rb_MappingInfo;
	TrChId tcid = rb->mTrChAssigned;	// The TrCh carrying this rb.
	defaultRbMappingInfoToAsn(masterConfig->getUlTrChInfo(tcid),
		masterConfig->getDlTrChInfo(tcid),rbid,&rbie->rb_MappingInfo);
	return rbie;
}

// 3GPP 25.331 10.3.4.10
// ASN struct RAB_InformationSetup 
// For a CS connection we use three consecutive rbids.
static ASN::RAB_InformationSetup *toAsnRAB_InformationSetup(
	RrcMasterChConfig *masterConfig, int rbid,
	int *pNumRBperRAB)	// Return the number of RBs in this RAB.  1 for PS and 3 for CS.
{
	ASN::RAB_InformationSetup *result = RN_CALLOC(ASN::RAB_InformationSetup);

	// RAB_Info_t   rab_Info;
	RBInfo *rb = masterConfig->getRB(rbid);
	toAsnRAB_Info(rb,&result->rab_Info);
	*pNumRBperRAB = rb->isCsDomain() ? 3 : 1;

	// RB_InformationSetupList_t    rb_InformationSetupList;
	//  	A_SEQUENCE_OF(struct RB_InformationSetup) list
	for (int i = 0; i < *pNumRBperRAB; i++, rbid++) {
		rb = masterConfig->getRB(rbid);
		// 3GPP 10.3.4.20
		ASN::RB_InformationSetup *rbInfoIE = toAsnRB_InformationSetup(masterConfig, rb);
		ASN_SEQUENCE_ADD(&result->rb_InformationSetupList.list,rbInfoIE);
	}
	return result;
}

// 3GPP 25.331 10.3.4.10 RAB Information for Setup.
static void toAsnRAB_InformationSetupList(RrcMasterChConfig *masterConfig, ASN::RAB_InformationSetupList *rbSetupListIE)
{
	// For PS each RB is a RAB using one rb-id.  There can be multiple RABs,
	// although I think we will only setup one at a time because the UE can
	// only ask for one at a time.
	// For voice, the first 3 RBs constitute a RAB with 3 'sub-rab-flows' for the AMR codec.
	// We dont allow mixing cs and ps connections on the same UE.
	for (unsigned rbid = 5; rbid < masterConfig->mNumRB; ) {
		RBInfo *rb = masterConfig->getRB(rbid);
		if (!rb || ! rb->valid()) { continue; }

		int numRBperRAB;
		ASN::RAB_InformationSetup *rabSetupIE =
			toAsnRAB_InformationSetup(masterConfig, rbid, &numRBperRAB);
		ASN_SEQUENCE_ADD(&rbSetupListIE->list,rabSetupIE);
		rbid += numRBperRAB;	// Advance by the number of rbids used by this RAB.
	}
}

static ASN::UL_CommonTransChInfo *toAsnUL_CommonTransChInfo(RrcMasterChConfig *masterConfig)
{
	ASN::UL_CommonTransChInfo *result = RN_CALLOC(ASN::UL_CommonTransChInfo);
	// struct TFC_Subset   *tfc_Subset /* OPTIONAL */;
	// struct TFCS *prach_TFCS /* OPTIONAL */;
	// struct UL_CommonTransChInfo__modeSpecificInfo {...} modeSpecificInfo;
	// The whole modeSpecificInfo is optional, even though it is not marked as such in ASN.
	typedef ASN::UL_CommonTransChInfo::UL_CommonTransChInfo__modeSpecificInfo msi;
	result->modeSpecificInfo = RN_CALLOC(msi);
	result->modeSpecificInfo->present = ASN::UL_CommonTransChInfo__modeSpecificInfo_PR_fdd;
	masterConfig->getUlTfcs()->toAsnTfcs(&result->modeSpecificInfo->choice.fdd.ul_TFCS,TrChUlDCHType);
	return result;
}


static ASN::DL_CommonTransChInfo *toAsnDL_CommonTransChInfo(RrcMasterChConfig *masterConfig)
{
	// struct DL_CommonTransChInfo 
	ASN::DL_CommonTransChInfo *result = RN_CALLOC(ASN::DL_CommonTransChInfo);
	// struct TFCS *sccpch_TFCS    /* OPTIONAL */;
	// struct DL_CommonTransChInfo__modeSpecificInfo
	// DL_CommonTransChInfo__modeSpecificInfo_PR present;
	result->modeSpecificInfo.present = ASN::DL_CommonTransChInfo__modeSpecificInfo_PR_fdd;
	// union DL_CommonTransChInfo__modeSpecificInfo_u {...} choice;
	// struct DL_CommonTransChInfo__modeSpecificInfo__fdd {...} fdd;
	// struct DL_CommonTransChInfo__modeSpecificInfo__fdd__dl_Parameters {...} *dl_Parameters;
	typedef ASN::DL_CommonTransChInfo::
		DL_CommonTransChInfo__modeSpecificInfo::
		DL_CommonTransChInfo__modeSpecificInfo_u::
		DL_CommonTransChInfo__modeSpecificInfo__fdd::
		DL_CommonTransChInfo__modeSpecificInfo__fdd__dl_Parameters longthing;
	result->modeSpecificInfo.choice.fdd.dl_Parameters = RN_CALLOC(longthing);
	// This is where you can say same as uplink: (but we dont)
	// DL_CommonTransChInfo__modeSpecificInfo__fdd__dl_Parameters_PR present;
	result->modeSpecificInfo.choice.fdd.dl_Parameters->present =
		ASN::DL_CommonTransChInfo__modeSpecificInfo__fdd__dl_Parameters_PR_dl_DCH_TFCS;
	// union DL_CommonTransChInfo__modeSpecificInfo__fdd__dl_Parameters_u {...} choice;
	// TFCS_t   dl_DCH_TFCS;
	masterConfig->getDlTfcs()->toAsnTfcs(
		&result->modeSpecificInfo.choice.fdd.dl_Parameters->choice.dl_DCH_TFCS,TrChDlDCHType);
	return result;
}


ASN::UL_AddReconfTransChInfoList *toAsnUL_AddReconfTransChInfoList(RrcMasterChConfig *masterConfig)
{
	ASN::UL_AddReconfTransChInfoList *result = RN_CALLOC(ASN::UL_AddReconfTransChInfoList);
	unsigned numtc = masterConfig->getUlNumTrCh();
	// This assumes transport channels always start at 0.
	for (TrChId tcid = 0; tcid < numtc; tcid++) {
		//TrChInfo *tc = masterConfig->getUlTrChInfo(tcid);
		// typedef struct UL_AddReconfTransChInformation
		ASN::UL_AddReconfTransChInformation *addTcIE =
			RN_CALLOC(ASN::UL_AddReconfTransChInformation);
		// UL_TrCH_Type_t   ul_TransportChannelType;	// dch or usch.
		asn_long2INTEGER(&addTcIE->ul_TransportChannelType,ASN::UL_TrCH_Type_dch);
		// TransportChannelIdentity_t   transportChannelIdentity;
		addTcIE->transportChannelIdentity = tcid+1;	// ASN TrCh starts at 1
		// TransportFormatSet_t     transportFormatSet;
		masterConfig->getUlTfs()->toAsnTfs(&addTcIE->transportFormatSet);

		ASN_SEQUENCE_ADD(&result->list,addTcIE);
	}
	return result;
}

// Create a fake uplink TrCh, needed just to align an ASN struct.
static void toAsnFakeUL_AddReconfTransChInfoList(ASN::UL_AddReconfTransChInfoList *ulchlist)
{
	// The easiest way to do this is to manufacturer a TFS and use that:
	UlTrChInfo ulfoo;
	ulfoo.setTrCh(TrChRACHType,1,true);
	ulfoo.getTfs()
		->setSemiStatic(10,RrcDefs::Convolutional,ASN::CodingRate_half,256,16)
		// ->setCommonCh()  This did not work - failed to ASN encode.
		->setDedicatedCh()
		->addTF(0+4,0);

	ASN::UL_AddReconfTransChInformation *dummyUlDCH = RN_CALLOC(ASN::UL_AddReconfTransChInformation);
	asn_long2INTEGER(&dummyUlDCH->ul_TransportChannelType,ASN::UL_TrCH_Type_dch);
	dummyUlDCH->transportChannelIdentity = 31;

	// This does not work; asn_CHOICE does not except PR_NOTHING, anywhere!
	//dummyUlDCH->transportFormatSet.present = ASN::TransportFormatSet_PR_NOTHING;
	ulfoo.getTfs()->toAsnTfs(&dummyUlDCH->transportFormatSet);
	ASN_SEQUENCE_ADD(&ulchlist->list,dummyUlDCH);
}

static ASN::DL_AddReconfTransChInfoList *toAsnDL_AddReconfTransChInfoList(RrcMasterChConfig *masterConfig)
{
	ASN::DL_AddReconfTransChInfoList *result = RN_CALLOC(ASN::DL_AddReconfTransChInfoList);
	unsigned numtc = masterConfig->getDlNumTrCh();
	// This assumes transport channels always start at 0.
	for (TrChId tcid = 0; tcid < numtc; tcid++) {
		//TrChInfo *tc = getDlTrChInfo(tcid);
		// v typedef struct DL_AddReconfTransChInformation
		ASN::DL_AddReconfTransChInformation *addTcIE =
			RN_CALLOC(ASN::DL_AddReconfTransChInformation);

		// . DL_TrCH_Type_t   dl_TransportChannelType;
		asn_long2INTEGER(&addTcIE->dl_TransportChannelType,ASN::DL_TrCH_Type_dch);
		//  TransportChannelIdentity_t   dl_transportChannelIdentity;
		addTcIE->dl_transportChannelIdentity = tcid+1;

		// vv struct DL_AddReconfTransChInformation__tfs_SignallingMode {...} tfs_SignallingMode;
		// .. DL_AddReconfTransChInformation__tfs_SignallingMode_PR present;
		addTcIE->tfs_SignallingMode.present = ASN::DL_AddReconfTransChInformation__tfs_SignallingMode_PR_explicit_config;
		// vvv union DL_AddReconfTransChInformation__tfs_SignallingMode_u {...} choice;
		// ... TransportFormatSet_t     explicit_config;

		// vvvv typedef struct TransportFormatSet
		masterConfig->getDlTfs()->toAsnTfs(&addTcIE->tfs_SignallingMode.choice.explicit_config);
		// ^^^^ struct TransportFormatSet
		// ^^^ union DL_AddReconfTransChInformation__tfs_SignallingMode_u {...} choice;

		// .. struct QualityTarget    *dch_QualityTarget  /* OPTIONAL */;
		// .. struct TM_SignallingInfo    *dummy  /* OPTIONAL */;
		// ^^ struct DL_AddReconfTransChInformation__tfs_SignallingMode {...} tfs_SignallingMode;

		ASN_SEQUENCE_ADD(&result->list,addTcIE);
		// ^ struct DL_AddReconfTransChInfoList
	}
	return result;
}


void toAsnDL_AddReconfTransChInfoListSameAsUl(
	ASN::DL_AddReconfTransChInfoList *dlchlist,
	int dltcid,		// Downlink 1-based TrCh-id to configure
	int ultcid)		// Uplink 1-based TrCh-id to copy to downlink trch.
{
	ASN::DL_AddReconfTransChInformation *dummyDlDCH =
		RN_CALLOC(ASN::DL_AddReconfTransChInformation); 
	asn_long2INTEGER(&dummyDlDCH->dl_TransportChannelType,ASN::DL_TrCH_Type_dch);
	dummyDlDCH->dl_transportChannelIdentity = dltcid;

	// Since this is just a dummy structure, use the simplest type,which is sameAsUlTrCh.
	dummyDlDCH->tfs_SignallingMode.present = ASN::DL_AddReconfTransChInformation__tfs_SignallingMode_PR_sameAsULTrCH;
	asn_long2INTEGER(&dummyDlDCH->tfs_SignallingMode.choice.sameAsULTrCH.ul_TransportChannelType,
				ASN::UL_TrCH_Type_dch);
	dummyDlDCH->tfs_SignallingMode.choice.sameAsULTrCH.ul_TransportChannelIdentity = ultcid;
	ASN_SEQUENCE_ADD(&dlchlist->list,dummyDlDCH);
}

ByteVector* sendDirectTransfer(UEInfo* uep, ByteVector &dlpdu, const char *descr, bool psDomain)
{
	ASN::DL_DCCH_Message_t msg;
	memset(&msg,0,sizeof(msg));
	msg.message.present = ASN::DL_DCCH_MessageType_PR_downlinkDirectTransfer;
	ASN::DownlinkDirectTransfer_t *ddt = &msg.message.choice.downlinkDirectTransfer;
	ddt->present = ASN::DownlinkDirectTransfer_PR_r3;
	ASN::DownlinkDirectTransfer_r3_IEs *ies = &ddt->choice.r3.downlinkDirectTransfer_r3;

	ies->rrc_TransactionIdentifier = uep->newTransactionId();
	ies->cn_DomainIdentity = psDomain ? toAsnEnumerated(ASN::CN_DomainIdentity_ps_domain) : toAsnEnumerated(ASN::CN_DomainIdentity_cs_domain);
	ies->nas_Message.buf = dlpdu.begin();
	ies->nas_Message.size = dlpdu.size();

	ByteVector *result = new ByteVector(dlpdu.size()+100);
	RN_MEMLOG(ByteVector,result);
	if (!encodeDcchMsg(uep,SRB3,&msg,*result,descr)) {return NULL;}
	return result;
}

static void toAsnURNTI(ASN::U_RNTI_t *urnti,unsigned srncid,unsigned srnti)
{
	setAsnBIT_STRING(&urnti->srnc_Identity,(uint8_t*)calloc(1,2),12);
	AsnBitString2BVTemp(urnti->srnc_Identity).setField(0,srncid,12);
	setAsnBIT_STRING(&urnti->s_RNTI,(uint8_t*)calloc(1,3),20);
	AsnBitString2BVTemp(urnti->s_RNTI).setField(0,srnti,20);
}

static ASN::C_RNTI_t *toAsnCRNTI(unsigned crnti)
{
	// C_RNTI_t    *new_c_RNTI /* OPTIONAL */;
	ASN::C_RNTI_t *result  = RN_CALLOC(ASN::C_RNTI_t);
	// new_c_RNTI is a BIT_STRING_t
	setAsnBIT_STRING(result,(uint8_t*)calloc(1,2),16);
	AsnBitString2BVTemp(result).setField(0,crnti,16);
	return result;
}

// NOTE: The RRC Connection Setup messages are defined as using CCCH and SRB0 which is TM
// uplink and UM downlink.  That makes sense because uplink messages are small and the downlink
// message is huge and may need to be segmented.
// Note that for CCCH downlink messages the MAC does not include a UE-id, in fact,
// it doesn't have one yet because the RRC Connection Setup message is what is going
// to supply the URNTI and CRNTI; rather the UE-id is included in the message.
// Apparently the UE monitors the entire CCCH all the time in order to gather all the 
// segments of all the messages and must look at them all to see if they are for it.
// Note that this message is still small: the rlc-info for TM&UM is pretty small,
// and the TrCh info comes from SIB5/6.

// Release 3 version of this message.  The samsung and other phones did not seem to like this,
// so 11-16-2012 tried switching to release 4 version.
void sendRrcConnectionSetup(UEInfo *uep, ASN::InitialUE_Identity *ueInitialId)
{
	ASN::DL_CCCH_Message msg;
	memset(&msg,0,sizeof(msg));
	msg.message.present = ASN::DL_CCCH_MessageType_PR_rrcConnectionSetup;
	ASN::RRCConnectionSetup *csp = &msg.message.choice.rrcConnectionSetup;
	unsigned transactionId = uep->newTransactionId();
	ByteVector result(1000);

	bool version4 = false;	// Use version4 of this message.
	bool version5 = false;	// Use version5 of this message with a default config.
	if (version4 | version5) {
		csp->present = ASN::RRCConnectionSetup_PR_later_than_r3;	// Guessing we can use any of the variants.
		// WARNING: Now there are temporarily two pointers to the memory in UE_Identity.
		csp->choice.later_than_r3.initialUE_Identity =  *ueInitialId;
		csp->choice.later_than_r3.rrc_TransactionIdentifier = transactionId;
		
		//ASN::RRCConnectionSetup::RRCConnectionSetup_u::RRCConnectionSetup__later_than_r3 *iep =
			//&csp->choice.later_than_r3;
		//iep->initialUE_Identity = *ueInitialId;
		//iep->rrc_TransactionIdentifier = transactionId;
		//typedef ASN::RRCConnectionSetup::RRCConnectionSetup_u::RRCConnectionSetup__later_than_r3 later_than_r3_t;
		//ASN::RRCConnectionSetup::RRCConnectionSetup_u::RRCConnectionSetup__later_than_r3::RRCConnectionSetup__later_than_r3__criticalExtensions *extp = &iep->criticalExtensions;

		if (version5) {
			// version 5 of the message.  A little verbose here...
			csp->choice.later_than_r3.criticalExtensions.present = ASN::RRCConnectionSetup__later_than_r3__criticalExtensions_PR_criticalExtensions;
			csp->choice.later_than_r3.criticalExtensions.choice.criticalExtensions.present = ASN::RRCConnectionSetup__later_than_r3__criticalExtensions__criticalExtensions_PR_r5;
			ASN::RRCConnectionSetup_r5_IEs_t *ie5 = &csp->choice.later_than_r3.criticalExtensions.choice.criticalExtensions.choice.r5.rrcConnectionSetup_r5;

			// ActivationTime_t    *activationTime /* OPTIONAL */;
			// U_RNTI_t     new_U_RNTI;
			toAsnURNTI(&ie5->new_U_RNTI,uep->getSrncId(),uep->getSRNTI());

			// C_RNTI_t    *new_c_RNTI /* OPTIONAL */;
			ie5->new_c_RNTI = toAsnCRNTI(uep->mCRNTI);

			// RRC_StateIndicator_t     rrc_StateIndicator;
			asn_long2INTEGER(&ie5->rrc_StateIndicator,ASN::RRC_StateIndicator_cell_FACH);
			// UTRAN_DRX_CycleLengthCoefficient_t   utran_DRX_CycleLengthCoeff;
			ie5->utran_DRX_CycleLengthCoeff = 3;	// Must be in range 3..9
			ie5->specificationMode.present = ASN::RRCConnectionSetup_r5_IEs__specificationMode_PR_preconfiguration;
			ie5->specificationMode.choice.preconfiguration.preConfigMode.present =
				ASN::RRCConnectionSetup_r5_IEs__specificationMode__preconfiguration__preConfigMode_PR_defaultConfig;
			asn_long2INTEGER(&ie5->specificationMode.choice.preconfiguration.preConfigMode.choice.defaultConfig.defaultConfigMode,ASN::DefaultConfigMode_fdd);
			ie5->specificationMode.choice.preconfiguration.preConfigMode.choice.defaultConfig.defaultConfigIdentity = 0;

		} else {
			// version 4 of the message.
			csp->choice.later_than_r3.criticalExtensions.present = ASN::RRCConnectionSetup__later_than_r3__criticalExtensions_PR_r4;
			ASN::RRCConnectionSetup_r4_IEs_t *ie4 = &csp->choice.later_than_r3.criticalExtensions.choice.r4.rrcConnectionSetup_r4;
			// ActivationTime_t    *activationTime /* OPTIONAL */;
			// U_RNTI_t     new_U_RNTI;
			toAsnURNTI(&ie4->new_U_RNTI,uep->getSrncId(),uep->getSRNTI());

			// C_RNTI_t    *new_c_RNTI /* OPTIONAL */;
			ie4->new_c_RNTI = toAsnCRNTI(uep->mCRNTI);

			// RRC_StateIndicator_t     rrc_StateIndicator;
			// 11-17-2012: Tried cell_pch state instead, to see if Samsung likes that better.
			// 		Nope.  Still sends protocol error.
			asn_long2INTEGER(&ie4->rrc_StateIndicator,ASN::RRC_StateIndicator_cell_FACH);

			// UTRAN_DRX_CycleLengthCoefficient_t   utran_DRX_CycleLengthCoeff;
			ie4->utran_DRX_CycleLengthCoeff = 3;	// Must be in range 3..9

			// struct CapabilityUpdateRequirement_r4   *capabilityUpdateRequirement    /* OPTIONAL */;
			// SRB_InformationSetupList2_t  srb_InformationSetupList;
			toAsnSRB_InformationSetupList(gRrcDcchConfig, &ie4->srb_InformationSetupList.list);

			if (0) {
				// As a debug measure to try to get the Samsung to work, try putting in the TrCh items.
				// But this is just not right, the DL_CommonTransChInfo IEs dont even have an option of setting up a FACH TrCh config.
				// struct UL_CommonTransChInfo_r4  *ul_CommonTransChInfo   /* OPTIONAL */;
				ie4->ul_CommonTransChInfo = RN_CALLOC(ASN::UL_CommonTransChInfo_r4);
				typedef ASN::UL_CommonTransChInfo_r4::UL_CommonTransChInfo_r4__modeSpecificInfo ulmode;
				ie4->ul_CommonTransChInfo->modeSpecificInfo = RN_CALLOC(ulmode);
				ie4->ul_CommonTransChInfo->modeSpecificInfo->present = ASN::UL_CommonTransChInfo_r4__modeSpecificInfo_PR_fdd;
				gRrcDcchConfig->getUlTfcs()->toAsnTfcs(&ie4->ul_CommonTransChInfo->modeSpecificInfo->choice.fdd.ul_TFCS,TrChUlDCHType);

				// struct UL_AddReconfTransChInfoList  *ul_AddReconfTransChInfoList    /* OPTIONAL */;
				ie4->ul_AddReconfTransChInfoList = toAsnUL_AddReconfTransChInfoList(gRrcDcchConfig);

				// struct DL_CommonTransChInfo_r4  *dl_CommonTransChInfo   /* OPTIONAL */;
				ie4->dl_CommonTransChInfo = RN_CALLOC(ASN::DL_CommonTransChInfo_r4);
				typedef ASN::DL_CommonTransChInfo_r4::DL_CommonTransChInfo_r4__modeSpecificInfo dlmode;
				ie4->dl_CommonTransChInfo->modeSpecificInfo = RN_CALLOC(dlmode);
				ie4->dl_CommonTransChInfo->modeSpecificInfo->present = ASN::DL_CommonTransChInfo_r4__modeSpecificInfo_PR_fdd;
				typedef ASN::DL_CommonTransChInfo_r4::
					DL_CommonTransChInfo_r4__modeSpecificInfo::
					DL_CommonTransChInfo_r4__modeSpecificInfo_u::
					DL_CommonTransChInfo_r4__modeSpecificInfo__fdd::
					DL_CommonTransChInfo_r4__modeSpecificInfo__fdd__dl_Parameters dl_ridiculous;
				ie4->dl_CommonTransChInfo->modeSpecificInfo->choice.fdd.dl_Parameters = RN_CALLOC(dl_ridiculous);
				ie4->dl_CommonTransChInfo->modeSpecificInfo->choice.fdd.dl_Parameters->present =
					ASN::DL_CommonTransChInfo_r4__modeSpecificInfo__fdd__dl_Parameters_PR_sameAsUL;

				// struct DL_AddReconfTransChInfoList_r4   *dl_AddReconfTransChInfoList    /* OPTIONAL */;

				//gRrcCcchConfig->getDlTfs()->toAsnTfs(&fachPchInfo->transportFormatSet);
				//gRrcCcchConfig->getUlTfs()->toAsnTfs((prach_SI->rach_TransportFormatSet = RN_CALLOC(ASN::TransportFormatSet)));
			}
			// struct FrequencyInfo    *frequencyInfo  /* OPTIONAL */;
			// MaxAllowedUL_TX_Power_t *maxAllowedUL_TX_Power  /* OPTIONAL */;
			// struct UL_ChannelRequirement_r4 *ul_ChannelRequirement  /* OPTIONAL */;
			// struct DL_CommonInformation_r4  *dl_CommonInformation   /* OPTIONAL */;
			// struct DL_InformationPerRL_List_r4  *dl_InformationPerRL_List   /* OPTIONAL */;
		} // version 4

		if (!encodeCcchMsg(&msg,result,descrRrcConnectionSetup,uep,0)) {return;}

		// Zero out the initialUE_Identity that we copied above so that there
		// is only one copy of it and we dont try to free it twice.
		memset(&csp->choice.later_than_r3.initialUE_Identity,0,sizeof(ASN::InitialUE_Identity_t));

	} else {
		// Version 3 of this message.

		// struct RRCConnectionSetup_r3_IEs
		csp->present = ASN::RRCConnectionSetup_PR_r3;	// Guessing we can use any of the variants.
		ASN::RRCConnectionSetup_r3_IEs_t *iep = &csp->choice.r3.rrcConnectionSetup_r3;

		// InitialUE_Identity_t     initialUE_Identity;
		// WARNING: Now there are temporarily two pointers to the memory in UE_Identity.
		iep->initialUE_Identity = *ueInitialId;

		// RRC_TransactionIdentifier_t  rrc_TransactionIdentifier;
		iep->rrc_TransactionIdentifier = transactionId;

		// ActivationTime_t    *activationTime /* OPTIONAL */;

		// U_RNTI_t     new_U_RNTI;
		// U-RNTI is mandatory.
		// They took apart the U_RNTI into its constituent parts, which was kinda dumb:
		// the parts are 12 bit SRNC id and 20 bit S-RNTI.
		toAsnURNTI(&iep->new_U_RNTI,uep->getSrncId(),uep->getSRNTI());
		//setAsnBIT_STRING(&iep->new_U_RNTI.srnc_Identity,(uint8_t*)calloc(1,2),12);
		//AsnBitString2BVTemp(iep->new_U_RNTI.srnc_Identity).setField(0,uep->getSrncId(),12);
		//setAsnBIT_STRING(&iep->new_U_RNTI.s_RNTI,(uint8_t*)calloc(1,3),20);
		//AsnBitString2BVTemp(iep->new_U_RNTI.s_RNTI).setField(0,uep->getSRNTI(),20);

		// srnti = 0x12345;
		//ByteVector tst(3);
		//tst.setField(0,srnti,20);
		//printf("SRNTI=0x%x bv=%s\n",srnti,tst.hexstr().c_str());

		// C_RNTI_t    *new_c_RNTI /* OPTIONAL */;
		// C-RNTI
		iep->new_c_RNTI = toAsnCRNTI(uep->mCRNTI);
		//iep->new_c_RNTI = RN_CALLOC(ASN::C_RNTI_t);
		// new_c_RNTI is a BIT_STRING_t
		//setAsnBIT_STRING(iep->new_c_RNTI,(uint8_t*)calloc(1,2),16);
		//AsnBitString2BVTemp(iep->new_c_RNTI).setField(0,uep->mCRNTI,16);


		// RRC_StateIndicator_t     rrc_StateIndicator;
		asn_long2INTEGER(&iep->rrc_StateIndicator,ASN::RRC_StateIndicator_cell_FACH);

		// UTRAN_DRX_CycleLengthCoefficient_t   utran_DRX_CycleLengthCoeff;
		// 10.3.3.49: DRC mode.  "Refers to 'k' in the formula 25.304 Discontinous Reception".
		iep->utran_DRX_CycleLengthCoeff = 3;	// Must be in range 3..9
		// skip optional CapabilityUpdateRequirement

		// struct CapabilityUpdateRequirement  *capabilityUpdateRequirement    /* OPTIONAL */;
		iep->capabilityUpdateRequirement = RN_CALLOC(ASN::CapabilityUpdateRequirement);
		iep->capabilityUpdateRequirement->ue_RadioCapabilityFDDUpdateRequirement = true;
			iep->capabilityUpdateRequirement->ue_RadioCapabilityTDDUpdateRequirement = false;

		// SRB_InformationSetupList2_t  srb_InformationSetupList;
		toAsnSRB_InformationSetupList(gRrcDcchConfig, &iep->srb_InformationSetupList.list);

		// struct UL_CommonTransChInfo *ul_CommonTransChInfo   /* OPTIONAL */;
		// struct DL_CommonInformation *dl_CommonInformation   /* OPTIONAL */;
		// UL_AddReconfTransChInfoList_t    ul_AddReconfTransChInfoList;
		// DL_AddReconfTransChInfoList_t    dl_AddReconfTransChInfoList;

		// All the TrCh info is optional, and not used in our case because we are
		// defining RACH/FACH rather than DCH, BUT...
		// For ul_ and dl_AddReconfTransChInfoList we are required to put in something anyway,
		// and 8.1.3.4 recommends a single zero-sized TF.
		// You can not use the "NOTHING" option - the ASN compiler just uses
		// that to mark an uninitialized value and fails.
		toAsnFakeUL_AddReconfTransChInfoList(&iep->ul_AddReconfTransChInfoList);
		toAsnDL_AddReconfTransChInfoListSameAsUl(&iep->dl_AddReconfTransChInfoList,31,31);

		// These IEs are all skipped, needed only for DCH:
		// struct UL_ChannelRequirement    *ul_ChannelRequirement  /* OPTIONAL */;
		// struct DL_InformationPerRL_List *dl_InformationPerRL_List   /* OPTIONAL */;
		//PhCh *phch = new PhCh(DPDCHType,256,254,256,9999,NULL); 
		//iep->ul_ChannelRequirement = phch->toAsnUL_ChannelRequirement();
		//iep->dl_CommonInformation = phch->toAsnDL_CommonInformation();
		//iep->dl_InformationPerRL_List = phch->toAsnDL_InformationPerRL_List(); 
		/*ASN::DL_InformationPerRL_List *result2 = RN_CALLOC(ASN::DL_InformationPerRL_List);
			ASN::DL_InformationPerRL *one = RN_CALLOC(ASN::DL_InformationPerRL);
			one->modeSpecificInfo.present = ASN::DL_InformationPerRL__modeSpecificInfo_PR_fdd;
			int primarySC = gConfig.getNum("UMTS.Downlink.ScramblingCode");
			one->modeSpecificInfo.choice.fdd.primaryCPICH_Info.primaryScramblingCode = primarySC;
			//one->dl_DPCH_InfoPerRL = toAsnDL_DPCH_InfoPerRL();
			ASN_SEQUENCE_ADD(&result2->list,one);
			iep->dl_InformationPerRL_List = result2;*/

		// struct FrequencyInfo    *frequencyInfo  /* OPTIONAL */;

		// TODO: Do we need this?
		// MaxAllowedUL_TX_Power_t *maxAllowedUL_TX_Power  /* OPTIONAL */;

		if (!encodeCcchMsg(&msg,result,descrRrcConnectionSetup,uep,0)) {return;}

		// Zero out the initialUE_Identity that we copied above so that there
		// is only one copy of it and we dont try to free it twice.
		memset(&iep->initialUE_Identity,0,sizeof(ASN::InitialUE_Identity_t));
	}

	LOG(INFO) << "gNodeB: " << gNodeB.clock().get() << ", SCCPCH: " << result;

	// TODO: This crashes
	// ASN_STRUCT_FREE_CONTENTS_ONLY(ASN::asn_DEF_DL_CCCH_Message,&msg);

	// Configure the UE to be ready for incoming on the new SRBs...
	uep->ueConnectRlc(gRrcDcchConfig,stCELL_FACH);
	// Set a UeTransaction: see ueWriteLowSide for its use.
	// there is no chance that other unrelated messages can happen simultaneously.
	// But we will print an error if the received transaction does not match.
	// The Rab Mask parameter is not relevant for this transaction type.
	UeTransaction(uep,UeTransaction::ttRrcConnectionSetup, 0, transactionId,stCELL_FACH);
	gMacSwitch.writeHighSideCcch(result,descrRrcConnectionSetup);
}

// Sent when an unrecognized UE tries to talk to us.
// Tell it to release the connection and start over.
static void sendRrcConnectionReleaseCcch(int32_t urnti)
{
	ASN::DL_CCCH_Message_t msg;
	memset(&msg,0,sizeof(msg));
	msg.message.present = ASN::DL_CCCH_MessageType_PR_rrcConnectionRelease;
	ASN::RRCConnectionRelease_CCCH *m1 = &msg.message.choice.rrcConnectionRelease;
	m1->present = ASN::RRCConnectionRelease_CCCH_PR_r3;
	ASN::RRCConnectionRelease_CCCH_r3_IEs *m2 = &m1->choice.r3.rrcConnectionRelease_CCCH_r3;
	unsigned srncid = urnti >> 20 & 0xfff;
	unsigned srnti = urnti & 0xfffff;
	toAsnURNTI(&m2->u_RNTI, srncid,srnti);
	ASN::RRCConnectionRelease_r3_IEs_t *m3 = &m2->rrcConnectionRelease;
	m3->rrc_TransactionIdentifier = 0;	// bogus, because we are not expecting a reply.
	m3->releaseCause = toAsnEnumerated(ASN::ReleaseCause_unspecified);	// TODO: What cause should we use?

	ByteVector result(1000);
	if (!encodeCcchMsg(&msg,result,descrRrcConnectionRelease,NULL,urnti)) {return;}
	gMacSwitch.writeHighSideCcch(result,descrRrcConnectionRelease);
}

// This puts the phone in idle mode.
void sendRrcConnectionRelease(UEInfo *uep) //, ASN::InitialUE_Identity *ueInitialId
{
	// Create the RB Setup Message.
	ASN::DL_DCCH_Message_t msg;
	memset(&msg,0,sizeof(msg));
	msg.message.present = ASN::DL_DCCH_MessageType_PR_rrcConnectionRelease;
	msg.message.choice.rrcConnectionRelease.present = ASN::RRCConnectionRelease_PR_r3;
	ASN::RRCConnectionRelease_r3_IEs_t *ies =
		&msg.message.choice.rrcConnectionRelease.choice.r3.rrcConnectionRelease_r3;

	// Note: The RRC spec has a U-RNTI in the message, but it is not in the ASN.
	// I do not see why it would be needed.  If the message goes on DCCH the
	// U-RNTI will be in the MAC header.


    // RRC_TransactionIdentifier_t  rrc_TransactionIdentifier;
	unsigned transactionId = uep->newTransactionId();
	ies->rrc_TransactionIdentifier = transactionId;

	// N_308_t *n_308  /* OPTIONAL */;
	// N308 is the number of times UE sends response RrcConnectionReleaseComplete.
	// It is mandatory when this message is sent on DCCH.
	ies->n_308 = RN_CALLOC(ASN::N_308_t);	// It is a long.
	*ies->n_308 = 1;						// must be 1..8

	// ReleaseCause_t   releaseCause;
	// Causes we might use are: normalEvent, userInactivity, pre_emptiveRelease.
	ies->releaseCause = toAsnEnumerated(ASN::ReleaseCause_normalEvent);
	// struct Rplmn_Information    *rplmn_information  /* OPTIONAL */;

	ByteVector result(1000);
	if (!encodeDcchMsg(uep,SRB2,&msg,result,descrRrcConnectionRelease)) {return;}

	// Prepare to receive the reply to this message:
	UeTransaction(uep,UeTransaction::ttRrcConnectionRelease,0,transactionId,stIdleMode);

	uep->ueWriteHighSide(SRB2, result, descrRrcConnectionRelease);
}

// 3GPP 25.331 10.2.33
// This is the main message to create DCH channels.
// It is invoked by the SGSN to create a RAB for an internet connection.
// It will also be invoked by GMM L3 to create CS connections.
// In both cases a state machine in the caller must wait for the phones
// response before proceeding.
// 
// We are sending a setup for a DCH and moving the UE to CELL_DCH state.
// The masterConfig indicates the DCH L2 setup, for example, how many TrCh.
// Return non-zero on error.
bool sendRadioBearerSetup(UEInfo *uep, RrcMasterChConfig *masterConfig, PhCh *phch, bool srbstoo)
{
	// Create the RB Setup Message.
	ASN::DL_DCCH_Message_t msg;
	memset(&msg,0,sizeof(msg));
	msg.message.present = ASN::DL_DCCH_MessageType_PR_radioBearerSetup;
	ASN::RadioBearerSetup_t &rbstop = msg.message.choice.radioBearerSetup;
	// There are various versions of this message.  Lets use the oldest version:
	rbstop.present = ASN::RadioBearerSetup_PR_r3;
	ASN::RadioBearerSetup_r3_IEs_t &rbs = rbstop.choice.r3.radioBearerSetup_r3;

	// =============== UE Information Elements ==============

	// Comments are directly from the ASN::RadioBearerSetup_r3_IEs_t
	// RRC_TransactionIdentifier_t  rrc_TransactionIdentifier;
	unsigned transactionId = uep->newTransactionId();
	rbs.rrc_TransactionIdentifier = transactionId;

	// struct IntegrityProtectionModeInfo  *integrityProtectionModeInfo    /* OPTIONAL */;
	// struct CipheringModeInfo    *cipheringModeInfo  /* OPTIONAL */;
	// ActivationTime_t    *activationTime /* OPTIONAL */;
	// struct U_RNTI   *new_U_RNTI /* OPTIONAL */;
	// C_RNTI_t    *new_C_RNTI /* OPTIONAL */;

	// RRC_StateIndicator_t     rrc_StateIndicator;
	asn_long2INTEGER(&rbs.rrc_StateIndicator, ASN::RRC_StateIndicator_cell_DCH);

	// UTRAN_DRX_CycleLengthCoefficient_t  *utran_DRX_CycleLengthCoeff /* OPTIONAL */;

	// URA_Identity_t  *ura_Identity   /* OPTIONAL */;
	// struct CN_InformationInfo   *cn_InformationInfo /* OPTIONAL */;

	// =============== RB Information Elements ==============

	// Set up new SRBs on the new DCH.
	// The new SRBs will be multiplexed using the paradigm of the defaultRBMappingInfo
	// struct SRB_InformationSetupList *srb_InformationSetupList   /* OPTIONAL */;
	if (srbstoo) {
		// 3GPP 25.331 10.3.4.24
		rbs.srb_InformationSetupList = RN_CALLOC(ASN::SRB_InformationSetupList);
		toAsnSRB_InformationSetupList(masterConfig,&rbs.srb_InformationSetupList->list);
	}

	// The RAB is where the new RBID for the data channel needs to go.
	// struct RAB_InformationSetupList *rab_InformationSetupList   /* OPTIONAL */;


	// The UE is allowed to request a PS channel on any of RBs 5..15, so check them all.
	bool haveDataCh = false; // Does the config define any data channels?
	RBInfo *rb;  unsigned rbid;
	for (rbid = 5; rbid < masterConfig->mNumRB; rbid++) {
		if (!(rb = masterConfig->getRB(rbid)) || !rb->valid()) { continue; }
		haveDataCh = true;
		break;
	}
	if (haveDataCh) {
		// 3GPP 25.331 10.3.4.10 RAB Information for Setup.
		// TODO: AMR rate defaults to "t7" - what is that?
		rbs.rab_InformationSetupList = RN_CALLOC(ASN::RAB_InformationSetupList);
		// A_SEQUENCE_OF(struct RAB_InformationSetup) list;
		toAsnRAB_InformationSetupList(masterConfig, rbs.rab_InformationSetupList);
	}

	// struct RB_InformationAffectedList   *rb_InformationAffectedList /* OPTIONAL */;
	// struct DL_CounterSynchronisationInfo    *dl_CounterSynchronisationInfo  /* OPTIONAL */;

	// =============== Uplink Transport Channels ==============

	// struct UL_CommonTransChInfo *ul_CommonTransChInfo   /* OPTIONAL */;
	rbs.ul_CommonTransChInfo = toAsnUL_CommonTransChInfo(masterConfig);

	// struct UL_DeletedTransChInfoList    *ul_deletedTransChInfoList  /* OPTIONAL */;

	// struct UL_AddReconfTransChInfoList  *ul_AddReconfTransChInfoList    /* OPTIONAL */;
	rbs.ul_AddReconfTransChInfoList = toAsnUL_AddReconfTransChInfoList(masterConfig);

	// =============== Downlink Transport Channels ==============

	//  struct RadioBearerSetup_r3_IEs__dummy { } *dummy;
	// struct DL_CommonTransChInfo *dl_CommonTransChInfo   /* OPTIONAL */;
	rbs.dl_CommonTransChInfo = toAsnDL_CommonTransChInfo(masterConfig);

	// struct DL_DeletedTransChInfoList    *dl_DeletedTransChInfoList  /* OPTIONAL */;

	// struct DL_AddReconfTransChInfoList  *dl_AddReconfTransChInfoList    /* OPTIONAL */;
	rbs.dl_AddReconfTransChInfoList = toAsnDL_AddReconfTransChInfoList(masterConfig);

	// =============== PhyCH Information Elements ==============

	// struct FrequencyInfo    *frequencyInfo  /* OPTIONAL */;
	// TODO: Do we need to add frequency info?  For FDD is it is just the ARFCN,
	// and I assume we are single ARFCN for now.

	// =============== Uplink Radio Resources ==============

	// MaxAllowedUL_TX_Power_t *maxAllowedUL_TX_Power  /* OPTIONAL */;
	// TODO: Do we need to specify UL power?

	// This is a container for 10.3.6.88 "Uplink DPCH Info" in RRC spec.
	// struct UL_ChannelRequirement    *ul_ChannelRequirement  /* OPTIONAL */;
	// This is for DPCH only.
	rbs.ul_ChannelRequirement = phch->toAsnUL_ChannelRequirement();

	// struct RadioBearerSetup_r3_IEs__modeSpecificPhysChInfo { ...  } modeSpecificPhysChInfo;
	rbs.modeSpecificPhysChInfo.present = ASN::RadioBearerSetup_r3_IEs__modeSpecificPhysChInfo_PR_fdd;
	// But the contents are optional.

	// =============== Downlink Radio Resources ==============

	// struct DL_CommonInformation *dl_CommonInformation   /* OPTIONAL */;
	// And I quote 8.2.1:
	// If after state transition the UE enters CELL_DCH state from CELL_FACH or from CELL_PCH state:
	//		1> if the IE "Default DPCH Offset Value" is not included:
	//			2> the UE behaviour is not specified.
	// The DPCH offset is inside this element:
	// (with the totally unpredictable value of "0" which apparently the UE could never have guessed on its own.)
	rbs.dl_CommonInformation = phch->toAsnDL_CommonInformation();

	// struct DL_InformationPerRL_List *dl_InformationPerRL_List   /* OPTIONAL */;
	rbs.dl_InformationPerRL_List = phch->toAsnDL_InformationPerRL_List();

		// note: encodeDcchMsg dumps the message to the log file.
        //asn_fprint(stdout,&ASN::asn_DEF_DL_DCCH_Message, &msg);  // Dump it all.
		//fflush(stdout);


	ByteVector result(1000);
	if (!encodeDcchMsg(uep,SRB2,&msg,result,descrRadioBearerSetup)) {return 1;}

        LOG(INFO) << "gNodeB: " << gNodeB.clock().get() << ", RadioBearerSetup: " << result;

	// Prepare to receive the reply to this message:
	UeTransaction(uep,UeTransaction::ttRadioBearerSetup, 1<<rbid, transactionId,stCELL_DCH);

	// This message is being written into a queue, but the MAC could find
	// it there and send it immediately.  The caller is responsible
	// for making sure the UE is prepared to receive replies before calling us.
	uep->ueWriteHighSide(SRB2, result, descrRadioBearerSetup);
	return 0;	// ok
}

// 3GPPP 25.331 8.2.2
// pat 12-27-2012: This message is implemented incorrectly.  I was hoping if you told the UE
// to go to CELL_FACH it would use the info from SIB5 or SIB6, but nope, you have to download
// a completely new config that is basically going to be a copy of SIB5.
// I'm not sure we have to do that at all, because when the last radio bearer is released
// we can send an RrcConnectionRelease to drop the UE back into idle mode.
// The UE keeps its SGSN level attach status regardless of mode; idle mode is fine.
// Update: Some phones (Samsung) activate/deactivate the PDPContexts rapidly, and we dont want
// to drop them all the way back to idle mode, because the transition from idle->fach takes a long
// time because we have to go through the authorization and security steps, while fach->dch is just a single message.
// Long term, we dont want to leave the UE in CELL_FACH mode after the radio-bearer-release anyway,
// it should be in CELL_PCH so we can page it, or if we dont need to page it, in idle mode.

// Harvind (3-17-13)  I think this message is fine.  The UE seems to like it during the PDP Deactivation Process

void sendRadioBearerRelease(UEInfo *uep,
	unsigned rabMask,	// Mask of RABs/RBs to release.
	bool finished)		// If set, all RABs released so move UE back to CELL_FACH mode.
{

#if 1 
	if (finished) {
		sendRrcConnectionRelease(uep);
		return;
	}
	//return;	// Do nothing.  We will just leave the existing DCH setup alone; it does not matter
#endif
			// how many RBs are sharing the DCH.
	UEState nextState = finished ? stCELL_FACH : stCELL_DCH;
	ASN::DL_DCCH_Message_t msg;
	memset(&msg,0,sizeof(msg));
	msg.message.present = ASN::DL_DCCH_MessageType_PR_radioBearerRelease;
	ASN::RadioBearerRelease_t &rbrtop = msg.message.choice.radioBearerRelease;
	// There are various versions of this message.  Lets use the oldest version:
	rbrtop.present = ASN::RadioBearerRelease_PR_r3;
	ASN::RadioBearerRelease_r3_IEs_t &rbr = rbrtop.choice.r3.radioBearerRelease_r3;

	// =============== UE Information Elements ==============

	//RRC_TransactionIdentifier_t  rrc_TransactionIdentifier;
	unsigned transactionId = uep->newTransactionId();
	rbr.rrc_TransactionIdentifier = transactionId;

	//struct IntegrityProtectionModeInfo  *integrityProtectionModeInfo    /* OPTIONAL */;
	//struct CipheringModeInfo    *cipheringModeInfo  /* OPTIONAL */;
	//ActivationTime_t    *activationTime /* OPTIONAL */;
	//struct U_RNTI   *new_U_RNTI /* OPTIONAL */;
	//C_RNTI_t    *new_C_RNTI /* OPTIONAL */;

	//RRC_StateIndicator_t     rrc_StateIndicator;
	rbr.rrc_StateIndicator = toAsnEnumerated(UEState2Asn(nextState));

	//UTRAN_DRX_CycleLengthCoefficient_t  *utran_DRX_CycleLengthCoeff /* OPTIONAL */;

	// =============== CN Information Elements ==============
	//struct CN_InformationInfo   *cn_InformationInfo /* OPTIONAL */;
	//CN_DomainIdentity_t *signallingConnectionRelIndication  /* OPTIONAL */;

	// =============== UTRAN Mobility Information Elements ==============

	//URA_Identity_t  *ura_Identity   /* OPTIONAL */;

	// =============== RB Information Elements ==============
	// 8.6.4.5 says RB Information to Reconfigure can modify SRBs.

#if 0	// If you include this code you get an ASN encoder violation from encodeDcchMsg
		// Furthermore, the examples I have seen dont use this, they release the RBs,
		// and the UE must infer the RAB from the RB.
	//struct RAB_InformationReconfigList  *rab_InformationReconfigList    /* OPTIONAL */;
	for (RbId rbid = 5; rbid < gsMaxRB; rbid++) {
		if (rabMask & (1<<rbid)) {
			// 25.331 10.3.4.11 RAB Information to Reconfigure.
			RBInfo *rb;
			if (!(rb = uep->ueGetConfig()->getRB(rbid)) || !rb->valid()) { continue; }
			ASN::RAB_InformationReconfig *rabreconfig = RN_CALLOC(ASN::RAB_InformationReconfig);
			toAsnRAB_Identity(rbid,&rabreconfig->rab_Identity);
			bool csdomain = rb->isCsDomain();
			rabreconfig->cn_DomainIdentity = toAsnEnumerated(
				 csdomain ? ASN::CN_DomainIdentity_cs_domain : ASN::CN_DomainIdentity_ps_domain);

			// NAS_Synchronisation_Indicator_t  nas_Synchronisation_Indicator;
			// From the documentation:
			//	 A container for non-access stratum information to be transferred transparently through UTRAN.
			//   Note 1: The nas_Synchronisation_Indicator is only relevant for CS domains.

			// On the first valid RAB, allocate the list.
			if (rbr.rab_InformationReconfigList == NULL) {
				rbr.rab_InformationReconfigList = RN_CALLOC(ASN::RAB_InformationReconfigList);
			}
			ASN_SEQUENCE_ADD(&rbr.rab_InformationReconfigList->list,rabreconfig);
		}
	}
#endif

	//RB_InformationReleaseList_t  rb_InformationReleaseList;

	for (RbId rbid = 5; rbid < gsMaxRB; rbid++) {
		if (rabMask & (1<<rbid)) {
			ASN::RB_Identity_t *val = RN_CALLOC(ASN::RB_Identity_t);	// Its just a long.
			*val = rbid;
			ASN_SEQUENCE_ADD(&rbr.rb_InformationReleaseList.list,val);
		}
	}

	// The RB Information to Reconfigure IE is only in ASN version 6 and higher.
	// The difference between that and RB Information to be Affected is that
	// the former lets you change other RLC programming, while
	// RB Information to be Affected lets you change only the RB Mapping Info,
	// but that is all you need to switch the SRBs between CELL_DCH or CELL_FACH state.

	//struct RB_InformationAffectedList   *rb_InformationAffectedList /* OPTIONAL */;
	// 12-22-2012: This message is not working, so tried removing this, but did not help:
	if (finished) {
		// Reconfigure the SRBs to whatever they need to be in CELL_FACH state.
		rbr.rb_InformationAffectedList  = toAsnRB_InformationAffectedList(gRrcDcchConfig);
	}

	//struct DL_CounterSynchronisationInfo    *dl_CounterSynchronisationInfo  /* OPTIONAL */;

	// =============== TrCh Information Elements ==============
	//struct UL_CommonTransChInfo *ul_CommonTransChInfo   /* OPTIONAL */;
	//struct UL_DeletedTransChInfoList    *ul_deletedTransChInfoList  /* OPTIONAL */;
	//struct UL_AddReconfTransChInfoList  *ul_AddReconfTransChInfoList    /* OPTIONAL */;
	//struct RadioBearerRelease_r3_IEs__dummy { } *dummy;
	//struct DL_CommonTransChInfo *dl_CommonTransChInfo   /* OPTIONAL */;
	//struct DL_DeletedTransChInfoList    *dl_DeletedTransChInfoList  /* OPTIONAL */;
	//struct DL_AddReconfTransChInfo2List *dl_AddReconfTransChInfoList    /* OPTIONAL */;


	// pat 1-24-2013: Now making the assumption that we use different TrCh for CELL_FACH and CELL_DCH mode,
	// so when we are 'finished' with DCH we can just delete the TrCh used for CELL_DCH.
	// In this case we still need to send an updated TFCS.
	// If we are not 'finished' with the DCH, we dont touch it, because we use logical channel multiplexing
	// to distinguish the various RBs on a single TrCh.
	if (finished) {

		// pat 1-24-2013:
		// Apparently we are supposed to delete the TrCh being used by DCH.  I still dont know if UE stores
		// the two separate TFS for use in CELL_FACH and CELL_DCH modes; this critical piece of info is not in 25.331.
		// To be safe, I would like to use different TrChs numbers for RACH/FACH and DCH, but 
		// unfortunately there are several places in our code that assume that all TrChId in use
		// are numbered starting at 1 (TrCh ids are 1-based), so we cannot yet allocate a
		// different TrCh for DCH than RACH/FACH, and I was hesitant to change all those places
		// because I cannot test because no radio.
		// So instead, just delete TrCh 1 and hope the UE reloads TrCh 1 for FACH from SIB5.
		// EXCEPT: The SIB5 can (and currently does) have the wrong FACH TrCh numbers in it, and the UE apparently does not even care!
		// The only thing I can imagine is that the UE is pretty much ignoring the TrCh ids in FACH state, implying that 
		// the spec is totally screwed up here and nobody knows how it is really supposed to work.
		{
			rbr.ul_deletedTransChInfoList = RN_CALLOC(ASN::UL_DeletedTransChInfoList_t);
			ASN::UL_TransportChannelIdentity_t *ult = RN_CALLOC(ASN::UL_TransportChannelIdentity_t);
			ult->ul_TransportChannelType = toAsnEnumerated(ASN::UL_TrCH_Type_dch);
			// TODO: We are simply assuming that there is only one TrCh here.
			//ult->ul_TransportChannelIdentity = uep->mUeDchConfig.getUlTrChInfo(0)->mTransportChannelIdentity;
			ult->ul_TransportChannelIdentity = 1;
			ASN_SEQUENCE_ADD(&rbr.ul_deletedTransChInfoList->list,ult);
		}

		{
			rbr.dl_DeletedTransChInfoList = RN_CALLOC(ASN::DL_DeletedTransChInfoList_t);
			ASN::DL_TransportChannelIdentity_t *dlt = RN_CALLOC(ASN::DL_TransportChannelIdentity_t);
			dlt->dl_TransportChannelType = toAsnEnumerated(ASN::DL_TrCH_Type_dch);
			// TODO: We are simply assuming that there is only the TrCh here.
			//dlt->dl_TransportChannelIdentity = uep->mUeDchConfig.getDlTrChInfo(0)->mTransportChannelIdentity;
			dlt->dl_TransportChannelIdentity = 1;
			ASN_SEQUENCE_ADD(&rbr.dl_DeletedTransChInfoList->list,dlt);
		}

		// The only example RadioBearerRelease message example I have is Nokia's "Call Setup PS" but that
		// leaves the UE in CELL_DCH mode afterward, not CELL_FACH.  They download a new TFCS but not a TFS.
		// I dont think we should do this unless we are leaving the UE in CELL_DCH state too.
		 RrcMasterChConfig *newConfig = gRrcDcchConfig;
		 rbr.ul_CommonTransChInfo = toAsnUL_CommonTransChInfo(newConfig);
		// The Add/Reconfig TrChInfoList is only for DCHs, not FACH/RACHes
		// Therefore it must pick up the FACH TFS from SIB5, and It is totally nonsensical to me why
		// we must download a TFCS here instead of the UE using the one from SIB5.
		//rbr.ul_AddReconfTransChInfoList = toAsnUL_AddReconfTransChInfoList(newConfig);
		 rbr.dl_CommonTransChInfo = toAsnDL_CommonTransChInfo(newConfig);
		//rbr.dl_AddReconfTransChInfo2List = toAsnDL_AddReconfTransChInfo2List(newConfig);
	}


	// =============== Physical Information Elements ==============  We dont need these.
	//struct FrequencyInfo    *frequencyInfo  /* OPTIONAL */;
	//MaxAllowedUL_TX_Power_t *maxAllowedUL_TX_Power  /* OPTIONAL */;
	//struct UL_ChannelRequirement    *ul_ChannelRequirement  /* OPTIONAL */;
	//struct RadioBearerRelease_r3_IEs__modeSpecificPhysChInfo { } modeSpecificPhysChInfo;

	// Must set present, but the contents are optional.
	rbr.modeSpecificPhysChInfo.present = ASN::RadioBearerRelease_r3_IEs__modeSpecificPhysChInfo_PR_fdd;

	//struct DL_CommonInformation *dl_CommonInformation   /* OPTIONAL */;
	//struct DL_InformationPerRL_List *dl_InformationPerRL_List   /* OPTIONAL */;

	ByteVector result(1000);
	if (!encodeDcchMsg(uep,SRB2,&msg,result,descrRadioBearerRelease)) {return;}

	if (finished) {
		// Configure the UE to be ready for incoming on the new SRBs...
		uep->ueConnectRlc(gRrcDcchConfig,nextState);
	}
	// Prepare to receive the reply to this message:
	UeTransaction(uep,UeTransaction::ttRadioBearerRelease, rabMask, transactionId,
		nextState);

	uep->ueWriteHighSide(SRB2, result, descrRadioBearerRelease);
}

static unsigned commonCellUpdateConfirm(UEInfo *uep, ASN::CellUpdateConfirm_r3_IEs_t *ies)
{

	// Apparently even CCCH messages get a transaction id, which will be used if the UE
	// replies to indicate an error in this message.
	unsigned transactionId = uep->newTransactionId();
	ies->rrc_TransactionIdentifier = transactionId;

	// Huge message but almost everything is optional.  Here are the mandatory parts:
	ies->rrc_StateIndicator = toAsnEnumerated(UEState2Asn(uep->ueGetState()));
	ies->rlc_Re_establishIndicatorRb2_3or4 = 0;
	ies->rlc_Re_establishIndicatorRb5orAbove = 0;
	ies->modeSpecificTransChInfo.present = ASN::CellUpdateConfirm_r3_IEs__modeSpecificTransChInfo_PR_fdd;
	ies->modeSpecificPhysChInfo.present = ASN::CellUpdateConfirm_r3_IEs__modeSpecificPhysChInfo_PR_fdd;

	// We can define a new URNTI.  
	// TODO: Do we need to assign a new URNTI if this message is on CCCH?
	return transactionId;
}


// The CellUpdateConfirm message may be sent out on either DCCH or CCCH.
// This version is for DCCH.
// TODO: It would be wise to implement the RLC re-establish indicators.
static void sendCellUpdateConfirmDcch(UEInfo *uep)
{
	ASN::DL_DCCH_Message_t msg;
	memset(&msg,0,sizeof(msg));
	// struct IntegrityCheckInfo   *integrityCheckInfo /* OPTIONAL */;
	msg.message.present = ASN::DL_DCCH_MessageType_PR_cellUpdateConfirm;
	msg.message.choice.cellUpdateConfirm.present = ASN::CellUpdateConfirm_PR_r3;
	ASN::CellUpdateConfirm_r3_IEs_t *ies =
		&msg.message.choice.cellUpdateConfirm.choice.r3.cellUpdateConfirm_r3;
	unsigned transactionId = commonCellUpdateConfirm(uep,ies);

	ByteVector result(1000);
	if (!encodeDcchMsg(uep,SRB2,&msg,result,descrCellUpdateConfirm)) {return;}
	UeTransaction(uep,UeTransaction::ttCellUpdateConfirm, 0, transactionId);
	uep->ueWriteHighSide(SRB2, result, descrCellUpdateConfirm);
}

static void sendCellUpdateConfirmCcch(UEInfo *uep)
{
	ASN::DL_CCCH_Message_t msg;
	memset(&msg,0,sizeof(msg));
	msg.message.present = ASN::DL_CCCH_MessageType_PR_cellUpdateConfirm;
	msg.message.choice.cellUpdateConfirm.present = ASN::CellUpdateConfirm_CCCH_PR_r3;

	// U_RNTI_t     u_RNTI;
	toAsnURNTI(&msg.message.choice.cellUpdateConfirm.choice.r3.u_RNTI,uep->getSrncId(),uep->getSRNTI());

	// CellUpdateConfirm_r3_IEs_t   cellUpdateConfirm_r3;
	ASN::CellUpdateConfirm_r3_IEs_t *ies =
		&msg.message.choice.cellUpdateConfirm.choice.r3.cellUpdateConfirm_r3;
	unsigned transactionId = commonCellUpdateConfirm(uep,ies);


	ByteVector result(1000);
	if (!encodeCcchMsg(&msg,result,descrCellUpdateConfirm,uep,0)) {return;}
	UeTransaction(uep,UeTransaction::ttCellUpdateConfirm, 0, transactionId);

	gMacSwitch.writeHighSideCcch(result,descrCellUpdateConfirm);
}

void sendCellUpdateConfirm(UEInfo *uep)
{
	switch (uep->ueGetState()) {
	case stCELL_DCH:
		sendCellUpdateConfirmDcch(uep);
		return;
	default:
		sendCellUpdateConfirmCcch(uep);
		return;
	}
}

// 33.102 5.1.2 Specifies two types of Security procedure: section 6.3 describes the main
// authentication method using Ki; section 6.5 describes a local authentication mechanism
// using an integrity key.  You must do one of the two procedures at each connection setup,
// including the one for an L3 Service Request.
void sendSecurityModeCommand(UEInfo *uep)
{
	ASN::DL_DCCH_Message_t msg;
	memset(&msg,0,sizeof(msg));
	msg.message.present = ASN::DL_DCCH_MessageType_PR_securityModeCommand;
	msg.message.choice.securityModeCommand.present = ASN::SecurityModeCommand_PR_r3;
	//ASN::SecurityModeCommand_t::SecurityModeCommand_u::SecurityMode_command__r3::Security *ies =
	ASN::SecurityModeCommand_r3_IEs *ies = 
		&msg.message.choice.securityModeCommand.choice.r3.securityModeCommand_r3;

	unsigned transactionId = uep->newTransactionId();
	ies->rrc_TransactionIdentifier = transactionId;

	// SecurityCapability_t     securityCapability;
	if (uep->radioCapability) {
		// just copy this from UE Info to make UE happy.  
		// 8.1.12.3 implies that if these capabilities don't exactly match those of the UE's, the UE will release
		ies->securityCapability = uep->radioCapability->securityCapability;
	}
	else {
		ies->securityCapability.cipheringAlgorithmCap = allocAsnBIT_STRING(16);
		// We are not going to do ciphering, but I am going to set one of the
		// ciphering algorithm bits in case it is needed to make the UE happy.
		AsnBitString2BVTemp(ies->securityCapability.cipheringAlgorithmCap).setField(
			ASN::SecurityCapability__cipheringAlgorithmCap_uea0,1,1);
        	AsnBitString2BVTemp(ies->securityCapability.cipheringAlgorithmCap).setField(
                	ASN::SecurityCapability__cipheringAlgorithmCap_uea1,1,1);

		ies->securityCapability.integrityProtectionAlgorithmCap = allocAsnBIT_STRING(16);
		// Set the bit for algorithm UIA1, which is the kasumi-based one we support.
		AsnBitString2BVTemp(ies->securityCapability.integrityProtectionAlgorithmCap).setField(
		ASN::SecurityCapability__integrityProtectionAlgorithmCap_uia1,1,1); // FIXME: 10.3.3.7 of 25.331 doesn't follow this
	}	

	// Not doing ciphering
	// struct CipheringModeInfo    *cipheringModeInfo  /* OPTIONAL */;
        /*ASN::CipheringModeInfo *cmi = RN_CALLOC(ASN::CipheringModeInfo);
        ies->cipheringModeInfo = cmi;
	cmi->cipheringModeCommand.present = ASN::CipheringModeCommand_PR_startRestart;
	cmi->cipheringModeCommand.choice.startRestart = toAsnEnumerated(ASN::CipheringAlgorithm_uea0);
	*/

	// Since we are setting up Integrity Protection (and not just ciphering)
	// we need to include this optional IE.
	// struct IntegrityProtectionModeInfo  *integrityProtectionModeInfo    /* OPTIONAL */;
	ASN::IntegrityProtectionModeInfo *ipmi = RN_CALLOC(ASN::IntegrityProtectionModeInfo);
	ies->integrityProtectionModeInfo = ipmi;
	ipmi->integrityProtectionModeCommand.present = ASN::IntegrityProtectionModeCommand_PR_startIntegrityProtection;
	ipmi->integrityProtectionModeCommand.choice.startIntegrityProtection.integrityProtInitNumber = allocAsnBIT_STRING(32);
	AsnBitString2BVTemp(ipmi->integrityProtectionModeCommand.choice.startIntegrityProtection.integrityProtInitNumber) 
		.setField(0, uep->integrity.getFresh(),32);

	// This is optional - is it necessary?
	ipmi->integrityProtectionAlgorithm = RN_CALLOC(ASN::IntegrityProtectionAlgorithm_t);
	*ipmi->integrityProtectionAlgorithm = toAsnEnumerated(ASN::IntegrityProtectionAlgorithm_uia1);

	ies->cn_DomainIdentity = toAsnEnumerated(ASN::CN_DomainIdentity_ps_domain);

	// struct InterRAT_UE_SecurityCapList  *ue_SystemSpecificSecurityCap   /* OPTIONAL */;
	/*ies->ue_SystemSpecificSecurityCap = RN_CALLOC(ASN::InterRAT_UE_SecurityCapList);
	ASN::InterRAT_UE_SecurityCapability* secCap = RN_CALLOC(ASN::InterRAT_UE_SecurityCapability);
	secCap->present = ASN::InterRAT_UE_SecurityCapability_PR_gsm;
	secCap->choice.gsm.gsmSecurityCapability = allocAsnBIT_STRING(7);
	AsnBitString2BVTemp(secCap->choice.gsm.gsmSecurityCapability).setField(ASN::GsmSecurityCapability_a5_1,1,1);
	ASN_SEQUENCE_ADD(&ies->ue_SystemSpecificSecurityCap->list,secCap);
	*/

	// 25.331 8.5.10 and 8.6.3.5 
	// The security mode command itself is the first one that is protected.
	// The start of the protection is indicated to the UE by the inclusion of the IntegrityCheck IE in the message.
	// 13.4.10: Integrity Protection is turned off when entering/leaving idle mode.
	uep->integrity.integrityStart();
	ByteVector result(1000);
	if (!encodeDcchMsg(uep,SRB2,&msg,result,descrSecurityModeCommand)) {return;}
	UeTransaction(uep,UeTransaction::ttSecurityModeCommand, 0, transactionId);
	uep->ueWriteHighSide(SRB2, result, descrSecurityModeCommand);
}

static void handleSecurityModeComplete(UEInfo*uep, ASN::SecurityModeComplete_t *secmsg)
{
	// I'm not sure what to do with any of the optional contents of this message.
	// struct IntegrityProtActivationInfo  *ul_IntegProtActivationInfo /* OPTIONAL */;
	// struct RB_ActivationTimeInfoList    *rb_UL_CiphActivationTimeInfo   /* OPTIONAL */;

	// If the UE is not confused, the security procedure was initiated
	// by the SGSN during an AttachRequest, so notify the SGSN that we are ready to roll.
	uep->sgsnHandleSecurityModeComplete(true);

}

// Just print an error message based on the error code.
// We have lost communication with this UE and should do something.
// TODO: what?
static void handleSecurityModeFailure(UEInfo*uep, ASN::SecurityModeFailure_t *secfailmsg)
{
	const char *why = "";
	char whybuf[100];
	switch (secfailmsg->failureCause.present) {
	case ASN::FailureCauseWithProtErr_PR_configurationUnsupported: why = "configurationUnsupported"; break;
	case ASN::FailureCauseWithProtErr_PR_physicalChannelFailure: why = "physicalChannelFailure";break;
	case ASN::FailureCauseWithProtErr_PR_incompatibleSimultaneousReconfiguration: why="incompatibleSimultaneousReconfiguration"; break;
	case ASN::FailureCauseWithProtErr_PR_compressedModeRuntimeError: why="compressedModeRuntimeError"; break;
	case ASN::FailureCauseWithProtErr_PR_protocolError: {
		why="protocolError";
		// There is more interesting information provided with this cause:
		ASN::ProtocolErrorInformation *pei = &secfailmsg->failureCause.choice.protocolError;
		if (pei->diagnosticsType.present == ASN::ProtocolErrorInformation__diagnosticsType_PR_type1) {
			long cause = asnEnum2long(pei->diagnosticsType.choice.type1.protocolErrorCause);
			switch (cause) {
			case ASN::ProtocolErrorCause_asn1_ViolationOrEncodingError:
				why = "ProtocolErrorCause_asn1_ViolationOrEncodingError";
				break;
			case ASN::ProtocolErrorCause_messageTypeNonexistent:
				why = "ProtocolErrorCause_messageTypeNonexistent";
				break;
			case ASN::ProtocolErrorCause_messageNotCompatibleWithReceiverState:
				why = "ProtocolErrorCause_messageNotCompatibleWithReceiverState";
				break;
			case ASN::ProtocolErrorCause_ie_ValueNotComprehended:
				why = "ProtocolErrorCause_ie_ValueNotComprehended";
				break;
			case ASN::ProtocolErrorCause_informationElementMissing:
				why = "ProtocolErrorCause_informationElementMissing";
				break;
			case ASN::ProtocolErrorCause_messageExtensionNotComprehended:
				why = "ProtocolErrorCause_messageExtensionNotComprehended";
				break;
			default:
				sprintf(whybuf,"protocol error cause %ld",cause);
				why = whybuf;
				break;
			}
		}

		break;
	}
	case ASN::FailureCauseWithProtErr_PR_cellUpdateOccurred: why="cellUpdateOccurred";break;
	case ASN::FailureCauseWithProtErr_PR_invalidConfiguration: why="invalidConfiguration";break;
	case ASN::FailureCauseWithProtErr_PR_configurationIncomplete: why="configurationIncomplete";break;
	case ASN::FailureCauseWithProtErr_PR_unsupportedMeasurement: why="unsupportedMeasurement";break;
	case ASN::FailureCauseWithProtErr_PR_mbmsSessionAlreadyReceivedCorrectly: why="mbmsSessionAlreadyRecievedCorrectly";break;
	case ASN::FailureCauseWithProtErr_PR_lowerPriorityMBMSService: why="lowerPriorityMBMSService";break;
	default: break;
	}
	LOG(ERR)<<format("Security Mode Failure cause=%d %s",secfailmsg->failureCause.present,why)<<uep;

	uep->sgsnHandleSecurityModeComplete(false);
}

static void handleCellUpdate(ASN::CellUpdate_t *msg)
{
	// This message sends a whole bunch of possible errors.  Lets print them out.
	bool srbErr = msg->am_RLC_ErrorIndicationRb2_3or4;
	bool rbErr = msg->am_RLC_ErrorIndicationRb5orAbove;
	bool radioErr = 0;
	bool otherErr = 0;
	long cuCause = 0;
	long failCause = 0;
	bool protoErrValid = 0; long protoErr;
	// I think the transaction id in this message, which is hidden down in
	// the failureCause, is the transaction id of the transaction that failed.
	unsigned failedTransactionId = 0;

	asn_INTEGER2long(&msg->cellUpdateCause,&cuCause);
	switch (cuCause) {
	// These are the two failure causes; all the other causes are normal.
	case ASN::CellUpdateCause_radiolinkFailure: radioErr = true; break;
	case ASN::CellUpdateCause_rlc_unrecoverableError: otherErr = true; break;
	}

	if (msg->failureCause) {
		failedTransactionId = msg->failureCause->rrc_TransactionIdentifier;
		failCause = msg->failureCause->failureCause.present;
		if (failCause == ASN::FailureCauseWithProtErr_PR_protocolError) {
			protoErrValid = true;
			// Geesh.  Just assume type1 and get it.
			asn_INTEGER2long(&msg->failureCause->failureCause.choice.protocolError.
				diagnosticsType.choice.type1.protocolErrorCause,&protoErr);
		}
	}
	// They transmit the URNTI in its constituent SRNCid and S-RNTI parts.
	uint32_t srnc = AsnBitString2BVTemp(msg->u_RNTI.srnc_Identity).getField(0,12);
	uint32_t srnti = AsnBitString2BVTemp(msg->u_RNTI.s_RNTI).getField(0,20);
	uint32_t urnti = (srnc<<20) | srnti;

	const char *comment = "UL_CCCH_MessageType_PR_cellUpdate";
	UEInfo *uep = gRrc.findUeByUrnti(urnti);
	if (!uep) { comment = "UL_CCCH_MessageType_PR_cellUpdate (new UE)"; }
	asnLogMsg(0, &ASN::asn_DEF_CellUpdate, msg,comment,uep,urnti);

	bool anyErr = (srbErr || rbErr || otherErr || radioErr || failCause || protoErrValid);
	if (anyErr) {
		LOG(ERR) << "Received cell update message with failure info:"
			<<LOGVAR(cuCause) <<LOGVAR(srbErr) <<LOGVAR(rbErr)
			<<LOGVAR(otherErr) <<LOGVAR(radioErr) <<LOGVAR(failCause)
			<<LOGVAR(protoErrValid) <<LOGVAR(protoErr)
			<<LOGVAR(failedTransactionId)
			<<uep;
	} else if (uep == NULL) {
		LOG(ERR) << format("Received cell update message with unrecognized URNTI=0x%x",
			urnti);
	}


	if (uep) {
		//msg->startList.
                //if (rbscmsg->start_Value) {
                //        uint32_t startVal = AsnBitString2BV(rbscmsg->start_Value).getField(0,20);
                //        uep->integrity.updateStartValue(startVal);
                //}
		// Dont need this printf.  The START value is not supposed to change.  It was a bug that was changing it.
		// printf("actual START value is: %u\n", uep->integrity.getStart());
		sendCellUpdateConfirm(uep);
	}
	else {
		// This is the first we have heard from this UE.  This is illegal.
		sendRrcConnectionReleaseCcch(urnti);
	}
}

void handleRrcConnectionRequest(ASN::RRCConnectionRequest_t *msg)
{
	//RrcUeId ueid(&msg->initialUE_Identity);
	// establishmentCause; Do we care?
	// We dont care about  protocolError.
	// We dont yet care about MeasuredResultsOnRACH.

	// TODO: Do we need to worry about multiple requests from the same UE?
	// If we get an identical request keep the same URNTI.
	// (Not doing so was one of the bugs in GPRS.)
	// There is an argument that if it is a duplicate request, just issue another URNTI,
	// and whichever URNTI the UE decides to use will be fine with us, but I think
	// that confuses the UE sometimes.
	AsnUeId aid(msg->initialUE_Identity);

	const char *comment = "UL_CCCH_MessageType_PR_rrcConnectionRequest";
	UEInfo *uep = gRrc.findUeByAsnId(&aid);
	if (uep == NULL) {
		uep = new UEInfo(&aid);
		comment = "UL_CCCH_MessageType_PR_rrcConnectionRequest (new UE)";
	}
	asnLogMsg(0, &ASN::asn_DEF_RRCConnectionRequest, msg,comment,uep);

	// (pat 12-18-2012) If we get this it means the UE dropped into idle mode possibly
	// without telling us, so fix that:
	// FIXME: What about CELL_PCH or CELL_URA
	uep->ueSetState(stIdleMode);
	// We need to do stop integrity protection before sending the L3 Authentication message,
	// which gets wrapped in an RRC direct transfer, which must not be integrity protected.
	uep->integrity.integrityStop();	// Redudant with code in ueSetState, but make sure.

	// Send the RRC Connection Setup Message,
	// using the exact initialUE_Identity the UE provided, whatever it is.
	sendRrcConnectionSetup(uep,&msg->initialUE_Identity);
}

#if 0	// Probably works, but not used, so take out to shut up compiler.
static bool fromAsnCnDomainIdentity(const char *inform, ASN::CN_DomainIdentity_t &asnthing)
{
	bool psDomain;
	long cnDomainId;
	if (asn_INTEGER2long(&asnthing,&cnDomainId)) {
		LOG(ERR) <<inform <<":"<<"invalid CN Domain Identity";
	}
	switch (cnDomainId) {
		case ASN::CN_DomainIdentity_cs_domain: psDomain = false; break;
		case ASN::CN_DomainIdentity_ps_domain: psDomain = true; break;
		default: LOG(ERR) <<inform <<":"<<"invalid CN domain value";
	}
	return psDomain;
}
#endif

// This message is passed transparently from the MAC using RLC-TM.
// The MAC should pop off the headers and pass the rest.
// The only possible messages are:
//		RRC Connection Request
//		Cell Update
//		URA Update
void rrcRecvCcchMessage(BitVector &tb,unsigned asc)
{
	ASN::UL_CCCH_Message *msg1 = (ASN::UL_CCCH_Message*)uperDecodeFromBitV(&ASN::asn_DEF_UL_CCCH_Message, tb);
	if (!msg1) {
		LOG(ALERT) << "undecodable CCCH message received";
		return;
	}

	//if (gConfig.getNum("UMTS.Debug.Messages")) {
	//	asn_fprint(stdout,&ASN::asn_DEF_UL_CCCH_Message, msg1);
	//	fflush(stdout);
	//}

	switch (msg1->message.present) {
	case ASN::UL_CCCH_MessageType_PR_cellUpdate:
		LOG(INFO) << "CELL_UPDATE message received";
		//asn_fprint(stdout,&ASN::asn_DEF_CellUpdate, &msg1->message.choice.cellUpdate);
		handleCellUpdate(&msg1->message.choice.cellUpdate);
		return;
	case ASN::UL_CCCH_MessageType_PR_rrcConnectionRequest:
		LOG(INFO) << "RRC Connection Request received";
		handleRrcConnectionRequest(&msg1->message.choice.rrcConnectionRequest);
		return;
	case ASN::UL_CCCH_MessageType_PR_uraUpdate:
		// We didnt crack out the URNTI for the message so just leave it null.
		asnLogMsg(0, &ASN::asn_DEF_UL_CCCH_Message, msg1,asnUlCcchMsg2Name(msg1->message.present),NULL);
		LOG(INFO) << "URA_UPDATE message ignored";
		return;
	default:
		asnLogMsg(0, &ASN::asn_DEF_UL_CCCH_Message, msg1,asnUlCcchMsg2Name(msg1->message.present),NULL);
		LOG(ERR) << "CCCH message ignored, unknown type="<<msg1->message.present;
		return;
	}
	ASN_STRUCT_FREE(ASN::asn_DEF_UL_CCCH_Message, msg1);
}

void UEInfo::ueRecvL3Msg(ByteVector &msgframe, UEInfo *uep)
{
	unsigned pd = msgframe.getNibble(0,0);	// protocol descriminator
        LOG(INFO) << "Received L3 message of with protocol discriminator " << pd; 
	switch ((GSM::L3PD) pd) {
	case GSM::L3GPRSMobilityManagementPD:	// Couldnt we shorten this?
	case GSM::L3GPRSSessionManagementPD: 	// Couldnt we shorten this?
		//LOG(INFO) << "Sending L3 message of descr " << pd << "up to SGSN"; 
		sgsnHandleL3Msg(uep->mURNTI,msgframe);
		//LOG(INFO) << "Sent to SGSN";
		break;
	// TODO: Send GSM messages somewhere
	case GSM::L3CallControlPD:
	case GSM::L3MobilityManagementPD:
	case GSM::L3RadioResourcePD:
		// In GSM these go on the logical channel, which is polled by DCCHDispatcher,
		// then calls DCCHDispatchMessage, which then calls some sub-processor
		// which may generate message traffic on a LogicalChannel class.
		// The best way to interface to the existing code is probably to put
		// these on the LogicalChannel and let the DCCH service loop find them.
		// It wants to find an ESTABLISH primitive as the first thing,
		// and then it times out if nothing happens soon?
		// Or we could try calling direct: DCCHDispatchMessage(??,??);

		// FIXME: Ignore these until L3 GSM code is integrated
		LOG(ERR) << "L3 GSM control message ignored";
		//uep->mGsmL3->l3writeHighSide(msgframe);
		return;
	case GSM::L3SMSPD:
		// In GSM these apparently arrive on the DTCHLogicalChannel?
		// Not sure how SMS works.  Looks like an MM message CMServiceRequest
		// arrives to DCCHDispatchMM() which then calls CMServiceResponder()
		// which calls MOSMSController() which seems to do a bunch of message
		// traffic on a DCCHLogicalChannel, and SMS messages go there.
		LOG(ERR) <<"L3 SMS Message ignored";
		return;
	default:
		LOG(ERR)<< "unsupported L3 Message PD:"<<pd;
	}
}

void UEInfo::ueRecvStatusMsg(ASN::UL_DCCH_Message *msg1)
{
	ASN::RRCStatus_t *statusmsg = &msg1->message.choice.rrcStatus;
	if (statusmsg->protocolErrorInformation.diagnosticsType.present !=
		ASN::ProtocolErrorMoreInformation__diagnosticsType_PR_type1) {
		LOG(WARNING) <<"Received RRCStatus that is not type1 " <<this;
		return;	// Cant do anything more; type1 is all we handle.
	}
	// ASN gets a little verbose here...
	ASN::ProtocolErrorMoreInformation::
		ProtocolErrorMoreInformation__diagnosticsType::
		ProtocolErrorMoreInformation__diagnosticsType_u::
		ProtocolErrorMoreInformation__diagnosticsType__type1 *info =
		&statusmsg->protocolErrorInformation.diagnosticsType.choice.type1;
	char const *errtype;
#define STATUS_INFO_TYPE(what) \
	ASN::ProtocolErrorMoreInformation__diagnosticsType__type1_PR_##what: \
	errtype = #what;

	// This is a overkill since rmsg is in the same location in the union every time.
	ASN::IdentificationOfReceivedMessage_t *rmsg = 0;
	switch (info->present) {
	case STATUS_INFO_TYPE(asn1_ViolationOrEncodingError)
		break;
	case STATUS_INFO_TYPE(messageTypeNonexistent)
		break;
	case STATUS_INFO_TYPE(messageNotCompatibleWithReceiverState)
		rmsg = &info->choice.messageNotCompatibleWithReceiverState;
		break;
	case STATUS_INFO_TYPE(ie_ValueNotComprehended)
		rmsg = &info->choice.ie_ValueNotComprehended;
		break;
	case STATUS_INFO_TYPE(conditionalInformationElementError)
		rmsg = &info->choice.conditionalInformationElementError;
		break;
	case STATUS_INFO_TYPE(messageExtensionNotComprehended)
		rmsg = &info->choice.messageExtensionNotComprehended;
		break;
	default: errtype = "unrecognized";
	}

	// Finally, lets print something.
	// I'm not going to bother decoding all this info into strings,
	// so the message wont make sense unless you have access to ASN and our code.
	char rbuf[200]; rbuf[0] = 0;
	if (rmsg) {
		long rmsgtype;
		asn_INTEGER2long(&rmsg->receivedMessageType,&rmsgtype);
		unsigned transId = rmsg->rrc_TransactionIdentifier;
		UeTransaction *tr = this->getTransactionRaw(transId,"RrcStatus");
		sprintf(rbuf," ReceivedMessageType=%ld transId=%d %s",
			rmsgtype,transId,tr?tr->str().c_str():"");
	}
	LOG(WARNING) <<"Received RRCStatus errnum="<<(int)info->present
		<<" errtype="<<errtype <<this <<rbuf;
}


// This is called by the high side of the RLC engine for SRB1,2,3,4.
void UEInfo::ueRecvDcchMessage(ByteVector &bv,unsigned rbNum)
{
	ueRegisterActivity();	// Not sure if all these messages count as activity.
	UEInfo *uep = this;
	//bool verbose = gConfig.getNum("UMTS.Debug.Messages"); // redundant with asnLogMsg.
	//if (verbose) { fprintf(stdout,"Received DCCH message for %s rb=%d\n",uep->ueid().c_str(),rbNum); }
	LOG(INFO) << "DCCH ByteVector: " << bv;
	ASN::UL_DCCH_Message *msg1 = (ASN::UL_DCCH_Message*)uperDecodeFromByteV(&ASN::asn_DEF_UL_DCCH_Message, bv);
	if (!msg1) return;

	//if (verbose) { asn_fprint(stdout,&ASN::asn_DEF_UL_DCCH_Message, msg1); fflush(stdout); }
	asnLogMsg(rbNum, &ASN::asn_DEF_UL_DCCH_Message, msg1, asnUlDcchMsg2Name(msg1->message.present),uep);

	//const char *inform;	// informative string for error messages.
	unsigned transId;	// Transaction Id.
	// The DCCH messages should arrive on rbnum 3, but who cares.
	switch (msg1->message.present) {
	case ASN::UL_DCCH_MessageType_PR_rrcConnectionSetupComplete: {
		ASN::RRCConnectionSetupComplete *rcscmsg =
			&msg1->message.choice.rrcConnectionSetupComplete;
		if (rcscmsg->ue_RadioAccessCapability) {
			uep->radioCapability = RN_CALLOC(ASN::UE_RadioAccessCapability);
			memcpy(uep->radioCapability,rcscmsg->ue_RadioAccessCapability,sizeof(struct ASN::UE_RadioAccessCapability));
		}
		// This message should arrive on SRB1 to indicate initial RRC connection is set up.
		// If you get this far, you have succeeded in establishing an RRC connection.
		LOG(NOTICE) << "Received RRC Connection Setup Complete!  Congratulations!";

		// The fact of the message arriving is the major piece of information.
		// Message also contains START info and UE Radio Access Capability Info.
		// The UE has already switched to CELL_FACH state in order to send this
		// message on DCCH.
		// Now inform ourselves; we already connected DCCH appropriately
		// before sending the ConnectionSetup msg.
		// This transaction was handled specially in ueWriteLowSide because
		// we had to process it before it can go through the RLC.
		//transId = rcscmsg->rrc_TransactionIdentifier;
		//uep->ueRecvRrcConnectionSetupResponse(transId,true,"RrcConnectionSetupComplete");
		break;
		}
	case ASN::UL_DCCH_MessageType_PR_rrcConnectionReleaseComplete: {
		ASN::RRCConnectionReleaseComplete *rcrcmsg =
			&msg1->message.choice.rrcConnectionReleaseComplete;
		transId = rcrcmsg->rrc_TransactionIdentifier;
		uep->ueRecvRrcConnectionReleaseResponse(transId,true,"RrcConnectionReleaseComplete");
		break;
	}
	case ASN::UL_DCCH_MessageType_PR_radioBearerSetupComplete: {
		// Currently we only send RadioBearerSetup for PS connections, so it must be for an SGSN PDP context.
		ASN::RadioBearerSetupComplete *rbscmsg =
			&msg1->message.choice.radioBearerSetupComplete;
		// I am ignoring the transaction identifier.
		// I am ignoring the "START" values.
		///*if (rbscmsg->start_Value) {
		//	uint32_t startVal = AsnBitString2BV(rbscmsg->start_Value).getField(0,20);
		//	uep->integrity.updateStartValue(startVal);
		//}*/
		// We need the rbid of the PDP context, which in the RRCP spec is in
		// "RB with PDPCP Information", but this does not appear in our ASN description,
		// unless it is buried inside the extensions which are a bitstring,
		// and no decoder provided.
		// So I guess the presense of this message and the transactionId
		// is the only indication we get.
		transId = rbscmsg->rrc_TransactionIdentifier;
		uep->ueRecvRadioBearerSetupResponse(transId,true,"RadioBearerSetupComplete");
		break;
		}
	case ASN::UL_DCCH_MessageType_PR_radioBearerSetupFailure: {
		ASN::RadioBearerSetupFailure *rbsfmsg =
			&msg1->message.choice.radioBearerSetupFailure;
		transId = rbsfmsg->rrc_TransactionIdentifier;
		// Print out the failure cause; Just print the ASN enum value directly.
		// It is FailureCauseWithProtErr_t
		unsigned failcode = rbsfmsg->failureCause.present;
		string msg = getRBFailureCause(&rbsfmsg->failureCause);
		LOG(ERR) << "Radio Bearer Setup Failure code="<<failcode <<" ("<<msg<<")"
			<<" rb="<<rbNum
			<<" ue:"<<uep;
		uep->ueRecvRadioBearerSetupResponse(transId,false,"RadioBearerSetupFailure");
		break;
		}

	// TODO: Not sure we need these at all:
	case ASN::UL_DCCH_MessageType_PR_radioBearerReleaseComplete: {
		ASN::RadioBearerReleaseComplete *rbrcmsg =
			&msg1->message.choice.radioBearerReleaseComplete;
		transId = rbrcmsg->rrc_TransactionIdentifier;
		uep->ueRecvRadioBearerReleaseResponse(transId,true,"RadioBearerReleaseComplete");
		break;
		}
	case ASN::UL_DCCH_MessageType_PR_radioBearerReleaseFailure: {
		ASN::RadioBearerReleaseFailure *rbrfmsg =
			&msg1->message.choice.radioBearerReleaseFailure;
		transId = rbrfmsg->rrc_TransactionIdentifier;
		// Print out the failure cause; Just print the ASN enum value directly.
		// It is FailureCauseWIthProtErr_t
		unsigned failcode = rbrfmsg->failureCause.present;
		LOG(ERR) << "Radio Bearer Setup Failure code="<<failcode
			<<" rb="<<rbNum
			<<" ue:"<<uep;
		uep->ueRecvRadioBearerReleaseResponse(transId,false,"RadioBearerReleaseFailure");
		break;
		}
	case ASN::UL_DCCH_MessageType_PR_uplinkDirectTransfer: {
		// This message should arrive on SRB3
		// This is a subsequent uplink message to L3.
		//inform = "Uplink Direct Transfer Message";
		ASN::UplinkDirectTransfer_t *udtmsg = &msg1->message.choice.uplinkDirectTransfer;
		// Do we care about this ps-cs domain flag at this point?
		// We can direct the message based on the message protocol descriptor.
		// bool psDomain = fromAsnCnDomainIdentity(&udtmsg.cn_DomainIdentity);

		ByteVector l3UdtMsgBytes(udtmsg->nas_Message.buf,udtmsg->nas_Message.size);
		uep->ueRecvL3Msg(l3UdtMsgBytes,uep);
		break;
		}
	case ASN::UL_DCCH_MessageType_PR_initialDirectTransfer: {
		//inform = "Initial Direct Transfer Message";
		// This message should arrive on SRB3
		// This is the first uplink message to L3.
		// On the UE side, it causes SRB1-SRB3 (and optionally SRB4) to be set up
		// before it complete.  On the network side, it is like the uplink direct transfer
		// but includes setup information for routing on the CN [Core Network] side.
		// This msg contains IntraDomainNasNodeSelector which has another UE-id in it,
		// but it doesnt include anything really interesting, for example, if it is an IMSI,
		// they only send a few bits of it, not the whole thing,
		// we already know what UE we are talking to, so dont bother to even look at it.
		ASN::InitialDirectTransfer_t *idtmsg = &msg1->message.choice.initialDirectTransfer;

		// Get CS or PS?
		// bool psDomain = fromAsnCnDomainIdentity(&idtmsg.cn_DomainIdentity);
		// We can direct the message based on the message protocol descriptor.

		// What we want is the NAS message, which is an ASN::OCTET_STRING;
		// convert it to an allocated ByteVector; allocate it just for safety's sake,
		// but TODO: Use a ByteVectorTemp because we shouldnt need to allocate it,
		// we can just use it right out of the OCTET_STRING struct.
		ByteVector l3IdtMsgBytes(idtmsg->nas_Message.buf,idtmsg->nas_Message.size);
		uep->ueRecvL3Msg(l3IdtMsgBytes,uep);
		break;
		}
	case ASN::UL_DCCH_MessageType_PR_rrcStatus: {
		// This sends an error indication about some received message.
		uep->ueRecvStatusMsg(msg1);
		break;
	}
	case ASN::UL_DCCH_MessageType_PR_signallingConnectionReleaseIndication: {
		// The UE sends this to tell us it wants to drop the connection.
		// FIXME: Or does it send this to tell us it already dropped the connection?  In which case
		// we dont need to send the RrcConnectionRelease message.
		// Harvind (3-17-13).  This message is the industry standard for terminating a DCH connection to save battery life
 		//           SCRI->RRC Conn. Release
		//           When UE wants to reconnect... RRC Conn. Setup->Service Request procedure	
		UeTransaction *lastTrans = getLastTransaction();
		if (lastTrans->mTransactionType != ttComplete && lastTrans->elapsed()<1000) {
			// There is some current transaction happening.
			// Lets wait a while to see what happens.
			return;
		}
		switch (uep->ueGetState()) {
		case stIdleMode:
			// This is not possible.
			LOG(ERR) << "Received SignallingConnectionReleaseIndication message"
				<<" for ue inIdleMode"<<uep;
			break;
		case stCELL_DCH:
			// The SGSN will call back to rrc to send a RadioBearerRelease message,
			// which should cause the UE to send RadioBearerReleaseComplete/Failure,
			// which will kick the UE down to CELL_FACH state.
			// Harvind (3-17-13) See above.  UE barfs when we send RadioBearerRelease.
			uep->ueSetState(stCELL_DCH);
			if (lastTrans->mTransactionType == ttRadioBearerRelease &&
				lastTrans->elapsed()<1000) {
				// Hmm.  Its not working.  Not sure what to do.  Try harder.
				sendRrcConnectionRelease(uep);
			} else {
				// Harvind (3-17-13) See above.  Keep PDP context intact for smoother operation.
				//printf("Freeing all PDP contexts.");
				//uep->sgsnFreePdpAll(mURNTI);
                                sendRrcConnectionRelease(uep);
			}
			break;
		default:
			sendRrcConnectionRelease(uep);
			break;
		}
		break;
	}
	case ASN::UL_DCCH_MessageType_PR_securityModeComplete: {
		ASN::SecurityModeComplete_t *secmsg = &msg1->message.choice.securityModeComplete;
		transId = secmsg->rrc_TransactionIdentifier;
		handleSecurityModeComplete(uep,secmsg);
		UeTransaction *tr = getTransaction(transId,ttSecurityModeCommand,"SecurityModeComplete");
		// The security mode was started when we sent the command, not when we receive the response,
		// so there is nothing special to do here.  If we changed the Kc by re-running the Layer3
		// Authentication procedure while a connection was running, then we would have to apply the
		// new keys at this time, but we dont ever do that.
		if (tr) tr->transClose();	// Done with this one.
		break;
	}
	case ASN::UL_DCCH_MessageType_PR_securityModeFailure: {
		ASN::SecurityModeFailure_t *secfailmsg = &msg1->message.choice.securityModeFailure;
		transId = secfailmsg->rrc_TransactionIdentifier;
		handleSecurityModeFailure(uep,secfailmsg);
		UeTransaction *tr = getTransaction(transId,ttSecurityModeCommand,"SecurityModeFailure");
		if (tr) tr->transClose();	// Done with this one.
		break;
	}
	default:
		LOG(ERR) << "DCCH message ignored, unhandled type="<<msg1->message.present
			<<" "<<asnUlDcchMsg2Name(msg1->message.present);
	}
}

bool AsnUeId::eql(AsnUeId &other)
{
	// All fields are inited, so just compare everything.
	if (idType != other.idType) return false;
	if (mImsi != other.mImsi || mImei != other.mImei || mTmsiDS41 != other.mTmsiDS41) return false;
	if (mMcc != other.mMcc || mMnc != other.mMnc) return false;
	if (mTmsi != other.mTmsi || mPtmsi != other.mPtmsi || mEsn != other.mEsn) return false;
	if (mLac != other.mLac || mRac != other.mRac) return false;
	return true;
}

void AsnUeId::asnParse(ASN::InitialUE_Identity &uid)
{
	switch (uid.present) {
	case ASN::InitialUE_Identity_PR_imsi:
		// A_SEQUENCE_OF(Digit_t) list
		mImsi = AsnSeqOfDigit2BV(&uid.choice.imsi.list);
		break;
	case ASN::InitialUE_Identity_PR_tmsi_and_LAI:
		// TMSI_GSM_MAP_t (aka BIT_STRING_t) tmsi
		// LAI_t lai;
		//		PLMN_Identity_t plmn_Identity
		//			MCC_t (A_SEQUENCE_OF(Digit_t) mcc;
		//			MNC_t (A_SEQUENCE_OF(Digit_t) mnc;
		//		BIT_STRING_t lac;
		mTmsi = AsnBitString2BVTemp(uid.choice.tmsi_and_LAI.tmsi).getField(0,32);
		mLac = AsnBitString2BVTemp(uid.choice.tmsi_and_LAI.lai.lac).getField(0,16);
		{ ByteVector bmcc = AsnSeqOfDigit2BV(&uid.choice.tmsi_and_LAI.lai.plmn_Identity.mcc);
		  mMcc = bmcc.getField(0,bmcc.sizeBits());
		}
		{ ByteVector bmnc = AsnSeqOfDigit2BV(&uid.choice.tmsi_and_LAI.lai.plmn_Identity.mnc);
		  mMnc = bmnc.getField(0,bmnc.sizeBits());
		}
		//mMcc = AsnSeqOfDigit2BV(&uid.choice.tmsi_and_LAI.lai.plmn_Identity.mcc);
		//mMnc = AsnSeqOfDigit2BV(&uid.choice.tmsi_and_LAI.lai.plmn_Identity.mnc);
		break;
	case ASN::InitialUE_Identity_PR_p_TMSI_and_RAI:
		// P_TMSI_GSM_MAP_t (aka BIT_STRING_t)     p_TMSI;
		// RAI_t rai;
		//		LAI_t lai;
		//			PLMN_Identity_t (see above) plmn_Identity;
		//			BIT_STRING_t lac;
		//		RoutingAreaCode_t (aka BIT_STRING_t) rac;
		mPtmsi = AsnBitString2BVTemp(uid.choice.p_TMSI_and_RAI.p_TMSI).getField(0,32);
		mLac = AsnBitString2BVTemp(uid.choice.p_TMSI_and_RAI.rai.lai.lac).getField(0,16);
		mRac = AsnBitString2BVTemp(uid.choice.p_TMSI_and_RAI.rai.rac).getField(0,8);
		{ ByteVector bmcc = AsnSeqOfDigit2BV(&uid.choice.p_TMSI_and_RAI.rai.lai.plmn_Identity.mcc);
		  mMcc = bmcc.getField(0,bmcc.sizeBits());
		}
		{ ByteVector bmnc = AsnSeqOfDigit2BV(&uid.choice.p_TMSI_and_RAI.rai.lai.plmn_Identity.mnc);
		  mMnc = bmnc.getField(0,bmnc.sizeBits());
		}
		//mMcc = AsnSeqOfDigit2BV(&uid.choice.p_TMSI_and_RAI.rai.lai.plmn_Identity.mcc);
		//mMnc = AsnSeqOfDigit2BV(&uid.choice.p_TMSI_and_RAI.rai.lai.plmn_Identity.mnc);
		break;
	case ASN::InitialUE_Identity_PR_imei:
		// A_SEQUENCE_OF(Digit_t) list
		mImei =AsnSeqOfDigit2BV(&uid.choice.imei.list);
		break;
	case ASN::InitialUE_Identity_PR_esn_DS_41:
		// BIT_STRING_t	// 32 bits
		mEsn = AsnBitString2BVTemp(uid.choice.esn_DS_41).getField(0,32);
		break;
	case ASN::InitialUE_Identity_PR_imsi_DS_41:
		// OCTET_STRING_t
		mImsi = AsnOctetString2BV(uid.choice.imsi_DS_41);
		break;
	case ASN::InitialUE_Identity_PR_imsi_and_ESN_DS_41:
		// IMSI_DS_41_t (aka OCTET_STRING_t) imsi_DS_41;	// 5-7 bytes
		// ESN_DS_41_t  (aka BIT_STRING_t) esn_DS_41;	// 32 bits.
		mImsi = AsnOctetString2BV(uid.choice.imsi_and_ESN_DS_41.imsi_DS_41);
		mEsn = AsnBitString2BVTemp(uid.choice.imsi_and_ESN_DS_41.esn_DS_41).getField(0,32);
		break;
	case ASN::InitialUE_Identity_PR_tmsi_DS_41:
		// OCTET_STRING_t
		mTmsiDS41 = AsnOctetString2BV(uid.choice.tmsi_DS_41);
		break;
	default:
		LOG(ERR) << "Unexpected UE id type:"<<(int)uid.present;
		break;
	}
}

#if 0	// Works, but unused.
// Get the MCC as a ByteVector.  Assumes str is correct.
static ByteVector stringOfDigitsToByteVector(const string &str)
{
	ByteVector result(str.length());
	for (unsigned i=0; i < str.length(); i++) {
		result.setByte(i,(int8_t) str[i] - '0');
	}
	return result;
}
#endif


// Does the routing/location area in this asn id match our BTS?
bool AsnUeId::RaiMatches()
{
	// MCC and MNC from the PLMN_identity must exist and match.
	//if (mMcc.size() == 0 || mMnc.size() == 0) { return false; }
	const string mccstr = gConfig.getStr("UMTS.Identity.MCC");
	const string mncstr = gConfig.getStr("UMTS.Identity.MNC");
	unsigned mcc = atoi(mccstr.c_str());
	unsigned mnc = atoi(mncstr.c_str());
	//ByteVector mcc = stringOfDigitsToByteVector(mccstr);
	//ByteVector mnc = stringOfDigitsToByteVector(mncstr);
	if (mMcc != mcc && mMnc != mnc) {
		LOG(INFO) << "RAI of UE"<<LOGVAR(mcc)<<LOGVAR(mnc) <<" does not match"<<LOGVAR(mMcc)<<LOGVAR(mMnc);
		return false;
	}

	// LAC and RAC must match.
	unsigned lac = gConfig.getNum("UMTS.Identity.LAC");
	unsigned rac = gConfig.getNum("GPRS.RAC");
	if (lac != mLac || rac != mRac) {
		LOG(INFO) << "RAI of UE" <<LOGVAR(lac)<<LOGVAR(rac)<<" does not match"<<LOGVAR(mLac)<<LOGVAR(mRac);
		return false;
	}
	LOG(INFO) << "RAI of UE matches";
	return true;
}

}; // namespace UMTS
