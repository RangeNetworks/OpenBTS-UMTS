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

#define URRC_IMPLEMENTATION 1
#include "URRC.h"
#include "UMTSLogicalChannel.h"
#include "MACEngine.h"
#include "../SGSNGGSN/SgsnExport.h"
#include "../SGSNGGSN/GPRSL3Messages.h"
#include "../SGSNGGSN/SgsnBase.h"	// For SmCause values.
#define CASENAME(x) case x: return #x;

// Here we have documentation on the flows through RRC, excluding the internals of RLC,
// and the specific L3 message flows.
// 
// 
// ==== L2 Layer (MAC and UEInfo) Flow: ====
// Assuming the 'simple' versions of MAC-C and MAC-D.
// The complex MAC versions perform the extra step of selecting a Transport Format,
// but that is internal to MAC (with extra interface functions to RLC)
// and is not documented here.
// 
// RACHL1FEC ---- TransportBlock ---> MAC-C (either CCCH or DCCH)
// 	RACHL1FEC->upstream->writeLowSide(TransportBlock)
// 		MaccSimple::writeLowSide(TransportBlock)
// 			Note: message in RLC-TM mode so there is no RLC processing
// 			MacSwitch::writeLowSideRach(TransportBlock)
// 				if for ccch:
// 					rrcRecvCcchMessage(BitVector)
// 				if for dcch:
// 					uep->ueWriteLowSide(rbid,BitVector,CELL_FACH);
// 
// 
// FACHL1FEC <--- TransportBlock ---- MAC-C <--- CCCH Message
// 	MacSwitch::writeHighSideCcch(msg eg, RrcConnectionSetup)
// 		pick a FACH by picking a MAC-C
// 		MaccBase::writeHighSideCcch(sdu,descr);
// 			rlc for ccch->rlcWriteHighSide(sdu,0,0,descr);
// 			... sits in high side of rlc until pulled by below:
// 			MaccBase::macServiceLoop:
// 				MaccBase::flushQ()
// 					rlcReadLowSide();
// 					create TransportBlock
// 					sendDownstreamTb(TransportBlock);
// 						FACHL1FEC->writeHighSide(TransportBlock);	// blocks until sent.
// 
// MAC-C or MAC-D <--- DCCH Message (SRB3) or Data (RB5 - RB15)
// 	ueWriteHighSide(rbid,sdu,descr)
// 		rlc = pick an RLC based on ue state:
// 		rlc[rbid,CELL_FACH or CELL_DCH]->rlcWriteHighSide(sdu,0,0,descr);
// 		... sits in high side of rlc until pulled from below.
// 		if RLC is for CELL_FACH:
// 			MaccBase::macServiceLoop:
// 				MaccBase::flushUE()
// 					pdu = URlcTrans::rlcReadLowSide()
// 					MaccSimple::sendDownstreamTb(TransportBlock);
// 						FACHL1FEC->writeHighSide(TransportBlock);	// blocks until sent.
// 		if RLC is for CELL_DCH:
// 			MacdBase::macServiceLoop:
// 				MacdBase::flushUE()
// 					pdu = URlcTrans::rlcReadLowSide()
// 					MacdSimple::sendDownstreamTb(TransportBlock);
// 						DCHL1FEC->writeHighSide(TransportBlock);	// blocks until sent.
// 
// 
// MAC-C or MAC-D ---> DCCH Message (SRB3) or Data (RB5 - RB15)
// 	UEInfo::ueWriteLowSide(rbid,ByteVector,CELL_DCH or CELL_FACH)
// 		rlc[rbid,CELL_DCH or CELL_FASH]->rlcWriteLowSide
// 			goes through RLC, and finally:
// 			URlcRecv::rlcSendHighSide(sdu,rbid implicit in rlc)
// 			if rbid == SRB1, SRB2, SRB3:
// 				UEInfo::ueRecvDcchMessage(sdu,uep,SRB id), see below
// 			else:
// 				uep->ueRecvData(sdu,mrbid);
// 					Sgsn::sgsnWriteLowSide(sdu,SgsnInfo,rbid)
// 						SgsnInfo::sgsnSend2PdpLowSide(rbid, payload);
// 							pdp = getPdp(rbid)
// 							pdp->pdpWriteLowSide(payload);
// 								enqueue packet payload for mini-ggsn layer.
// 
// 
// ==== Message Flows for L2 and L3, above RLC layer: ====
// 
// MS ---- RrcConnectionRequest on CCCH ---> RRC
// 	rrcRecvCcchMessage(msg)
// 		handleRrcConnectionRequest
// 			find (findUeByAsnId) or create the UEInfo.
// 			sendRrcConnectionSetup
// 				uep->ueConnectRlc // prepare for CELL_FACH mode
// 				MS <--- RrcConnectionSetup on CCCH ---- Network
// 
// MS ---- RrcConnectionSetupComplete on DCCH ---> RRC
// 	UEInfo::ueRecvDcchMessage(msg,uep,SRB3)
// 		UEInfo::ueRecvRrcConnectionSetupResponse(transactionId)
// 			UEInfo::ueSetState(CELL_FACH)
// 
// MS ---- Generically, any L3 Message ---> Network
// 	UEInfo::ueRecvDcchMessage(msg,uep,SRB3)
// 		rrcRecvL3Msg
// 			Depending on message protocol descriptor, either:
// 			uep->(LogicalChannel)mGsmL3->l3writeHighSide(msgframe);
// 				queues message for the UMTS::LogicalChannel class
// 			Sgsn::handleL3Msg(uep->sgsnGetSgsnInfo(),msgframe);
// 				Depending on message protocol descriptor, either:
// 				Sgsn::handleL3GmmMsg(si,frame);
// 					creates SgsnInfo for UEInfo if necessary, calls handler for:
// 					AttachRequest, AttachComplete,
// 					DetachRequest, DetachAccept (todo),
// 					RoutingAreaUpdateRequest, RoutingAreaUpdateComplete
// 					IdentityResponse, GMMStatus
// 				Ggsn::handleL3SmMsg(si,frame);
// 					calls handler for:
// 					ActivatePdpContextRequest, DeactivatePdpContextRequest,
// 					SMStatus
// 
// MS <--- Generically, any L3 Message ---- SGSN/GGSN
// 	SgsnInfo->sgsnWriteHighSideMsg(msg)
// 		SgsnInfo::sgsnSend2MsHighSide(pdu,descr,rbid)
// 			UEAdapter->msWriteHighSide(pdu,rbid,descr);
// 				UEInfo::ueWriteHighSide(rbid,sdu,descr)
// 					uep->rlc[SRB3,uestate]->rlcWriteHighSide(sdu,0,0,descr);
// 
// ==== Specific L3 Messages: ====
// 
// See 23.060 9.2.2 Pdp Context Activation
// MS ---- L3 ActivatePdpContextRequest ---> GGSN
// 	Ggsn::handleL3SmMsg(si,frame);
// 		handleActivatePdpContextRequest
// 			on error, sends sendPdpContextReject, otherwise:
// 			mg_con_find_free	// Allocate IP address from miniggsn layer
// 			new PdpContext(SgsnInfo (the L3 id of the UE), rbid,
// 				L3 message transactionId, qos request, pco request, mgp (for IP address))
// 			SgsnInfo->connectPdp(pdp,mgp)	// PdpContext is now configured and ready.
// 			SgsnAdapter::allocateRabForPdp
// 				rrcAllocateRabForPdp(URNTI,rbid,qos)
// 					uep = findUeByUrnti(URNTI)
// 					check uep->mConfiguredRabs[]
// 						if RAB previously allocated, return it, which will cause
// 						GGSN to jump directly to sendPdpContextAccept below, otherwise:
// 					uep->mConfiguredRabs[rbid].status = RabPending.
// 					dch = chChooseByBW(bps from QoS)
// 					uep->ueConnectRlc(config,CELL_DCH)	// Prepare for incoming 
// 					macHookupDch(dch,uep)		// Allocate Mac-D and connect
// 					sendRadioBearerSetup
// 						UeTransaction(rbid, next state CELL_DCH)
// 						UEInfo::ueWriteHighSide(SRB2, RadioBearerSetup msg)
// 							MS <---- RRC RadioBearerSetup ---- RRC
// 					return RabStatus with actual QoS, status=RabPending
// 			Save RabStatus from RRC in PdpContext
// 			We wait for the RadioBearerSetup response before proceeding.
// 
// 
// MS ---- RRC RadioBearerSetupComplete ---> RRC
// 	UEInfo::ueRecvDcchMessage(msg,uep,SRB3)
// 		uep->ueRecvRadioBearerSetupResponse(transactionId)
// 			uep->mConfiguredRabs[rbid].status = RabComplete
// 			UeInfo::ueSetState(CELL_FACH)
// 			find UeTransaction from transactionId
// 			MSUEAdapter->sgsnHandleRabSetupResponse(rbid,success);
// 				sendPdpContextAccept(SgsnInfo, pdp corresponding to rbid)
// 				create message with Pco, IP address, QoS from PdpContext,
// 				and QoS from pdp->RabStatus from RRC in transaction above
// 				SgsnInfo->sgsnWriteHighSide(msg)
// 					MS <--- L3 ActivatePdpContextAccept --- GGSN
// 
// MS ---- RRC RadioBearerSetupFailure ---> RRC
// 	UEInfo::ueRecvDcchMessage(msg,uep,SRB3)
// 		uep->ueRecvRadioBearerSetupResponse(transactionId)
// 			find UeTransaction from transactionId and close it.
// 			uep->mConfiguredRabs[rbid from transaction].status = RabFailure
// 			MSUEAdapter->sgsnHandleRabSetupResponse(rbid,failure);
// 				SgsnInfo->freePdp(rbid [=NSAPI])
// 					mg_con_close	// release the IP connection.
// 
// 
// See 23.060 9.2.4 Pdp Context Deactivation
// MS ---- L3 DeactivatePdpContextRequest ---> GGSN
// 	Ggsn::handleL3SmMsg(si,frame);
// 		handleDeactivatePdpContextRequest(SgsnInfo,msg)
// 			freePdp(nsapi) or freePdpAll
// 			SgsnInfo->sgsnWriteHighSideMsg(DeactivatePdpContextAccept)
// 				MS <--- L3 DeactivatePdpContextAccept ---- GGSN
// 			MSUEAdapter->msDeactivateRabs(mask of RABs)
// 				mConfiguredRabs[for each rbid].status = RabDeactPending
// 				sendRadioBearerRelease(uep,rabMask,finished [release connection]);
// 				if (finished) uep->ueConnectRlc(next state: CELL_FACH)
// 				UeTransaction(rbid, next state = finished ? CELL_FACH : CELL_DCH)
// 				ueWriteHighSide(SRB2, RadioBearerRelease msg)
// 					MS <---- RRC RadioBearerRelease ---- RRC
// 				We wait for the RadioBearerRelease response before proceeding.
// 
// MS ---- RRC RadioBearerReleaseComplete ---> RRC
// or MS ---- RRC RadioBearerReleaseFailure ---> RRC
// 	UEInfo::ueRecvDcchMessage(msg,uep,rbid)
// 		uep->ueRecvRadioBearerReleaseResponse(transactionId)
// 			find UeTransaction from transactionId
// 			mConfiguredRabs[for each rbid in transation].mStatus = RabIdle
// 			if (last rb) UeInfo::ueSetState(CELL_FACH)
// 
			


namespace UMTS {

Rrc gRrc;
// These are the configs for CCCH and DCCH.
// The message may be on the same FACH, distinguished by MAC header.
RrcMasterChConfig gRrcCcchConfig_s;
RrcMasterChConfig gRrcDcchConfig_s;
//RrcMasterChConfig gRrcDchPSConfig_s;
// These point to the above.  We use a pointer so they can be referred to
// elsewhere without including URRC.h first.
RrcMasterChConfig *gRrcCcchConfig = 0; 	// RB[0] is SRB0
RrcMasterChConfig *gRrcDcchConfig = 0;	// adds RB[1] through RB[3].  Still on RACH/FACH
//RrcMasterChConfig *gRrcDchPSConfig = 0;
int rrcDebugLevel;


const char *UEDefs::TransType2Name(TransType ttype)
{
	switch (ttype) {
	CASENAME(ttComplete)
	CASENAME(ttRrcConnectionSetup)
	CASENAME(ttRrcConnectionRelease)
	CASENAME(ttRadioBearerSetup)
	CASENAME(ttRadioBearerRelease)
	CASENAME(ttCellUpdateConfirm)
	CASENAME(ttSecurityModeCommand)
	// Do not add a default case here; we want the error message if this
	// function gets out of phase with the enumeration.
	}
	return "unrecognized";
}

const char *UEState2Name(UEState state)
{
	switch (state) {
	CASENAME(stUnused)
	CASENAME(stIdleMode)
	CASENAME(stCELL_FACH)
	CASENAME(stCELL_DCH)
	CASENAME(stCELL_PCH)
	CASENAME(stURA_PCH)
	// Do not add a default case here; we want the error message if this
	// function gets out of phase with the enumeration.
	}
	return "unrecognized";
};

// 25.331 13.6: RB information parameters for SRB0
void RBInfo::defaultConfigSRB0()
{
	rb_Identity(0);	// Not in the spec, pat added.
	ul_RLC_Mode(URlcModeTm);
	ul_segmentationIndication(false);
	dl_RLC_Mode(URlcModeUm);	// Thats right: TM up but UM down.
	dl_LengthIndicatorSize(7);
#if USE_RBMAPPINGINFO	// Unneeded now.
	// RB Mapping Info
	UL_LogicalChannelMappings(1);
	MappingOption(1);
	ul_TransportChannelType(TrChRACHType);
	// Note: RLC size uses IE "Additional Transport Format Information for CCCH"
	mac_LogicalChannelPriority(1);
	// This is not in the 13.6 spec, but we have to set it to the one and only TrCh:
	ul_transportChannelIdentity(1);
	dl_transportChannelIdentity(1);
#endif
}

// Default config for SRBs using RLC-AM.
// Came from 3GPP 25.331 13.8 for SRB1, SRB2, SRB3
// Once we program the SRBs we want to leave their RLC setup the same, except for
// RLC-size, regardless of transitions between CELL_FACH and CELL_DCH,
// because when we setup the new RLCs for a UE state change in ueConnectRlc(),
// we do it by simply re-using the same RLC, which implies we cannot change
// any of its other programming.  As a corollary, that means we can affect
// the UE state change RLC programming by sending only the RBMappingInfo.
void RBInfo::defaultConfigSrbRlcAm()
{
	// This is designed for SRBs, so it forces each SDU completely through
	// using a poll before proceeding; note that PollSDU == 1.
	// The TimerPoll is required so that if the PDU that has the poll
	// bit is lost, it will be retransmitted after the timeout.
	ul_RLC_Mode(URlcModeAm);
	transmissionRLC_DiscardMode(TransmissionRlcDiscard::NoDiscard);
	maxDat(40);
	transmissionWindowSize(64);
	timerRST(200);
	max_RST(1);			// Interesting that only one reset attempt allowed.
	// max_RST must be 1 for SRB2, or UE behavior is unspecified
	TimerPoll(200);	// 1 sec seems pretty long.
	PollSDU(1);			// Poll after every SDU.
	lastTransmissionPDU_Poll(false);
	lastRetransmissionPDU_Poll(true);
	PollWindow(99);
	dl_RLC_Mode(URlcModeAm);
	// (pat) They specify the PDU size per-RB in the spec, but we are going to let the
	// RLC figure it out, which is much safer.
	//dl_RLC_PDU_size(128);	// actual size 144
	inSequenceDelivery(true);
	receivingWindowSize(64);
	missingPDU_Indicator(true);
	// rlc_OneSidedReEst(false);
}

// Default config for a packet-switched data channel.
// Pat just made this up from scratch.
void RBInfo::defaultConfigRlcAmPs()
{
	defaultConfigSrbRlcAm();	// Start with this config.
	transmissionWindowSize(2047);	// Lots more data, so increase the window size.
	max_RST(8);				// Allow more failures
	// The following should vary based on chosen TTI.
	lastTransmissionPDU_Poll(false);	// Poll on the last PDU.
	lastRetransmissionPDU_Poll(true);	// or last retransmitted PDU.
	TimerPoll(1000);				// Reduce poll timeout - 100ms = every 10 PDUs.
	PollSDU(0);				// Dont do this.
	PollPDU(0);			// Do this instead.
	inSequenceDelivery(false);
	receivingWindowSize(2047);
	PollWindow(90);
	timerStatusPeriodic(100);
	timerPollPeriodic(100);
	// Not sure the following is worth the effort.
	// It is probably better to wait and gang up any failures together.
	missingPDU_Indicator(false);
	//missingPDU_Indicator(true);	// Trigger a status report immediately on missing pdu.
}

// 25.331 13.8 for RB1, RB2, RB3
void RBInfo::defaultConfig0CFRb(unsigned rbn)
{
	rb_Identity(rbn);
	if (rbn == 1) {
		ul_RLC_Mode(URlcModeUm);
		dl_RLC_Mode(URlcModeUm);
		// According to the RRC spec, this mode is illegal for UM mode! Gotta love it.
		// See comments at TransmissionRlcDiscard.
		//transmissionRLC_DiscardMode(TransmissionRlcDiscard::NoDiscard);
		transmissionRLC_DiscardMode(TransmissionRlcDiscard::TimerBasedWithoutExplicitSignaling);
		// Our version of ASN does not transmit the UM RLC LI size, so dont do this.
		//dl_UM_RLC_LI_size(7);
	} else {
		defaultConfigSrbRlcAm();
	}

#if USE_RBMAPPINGINFO	// Unneeded now.
	ul_TransportChannelType(TrChRACHType);
	ul_logicalChannelIdentity(rbn);

	// rb-MappingInfo
	// Two options, depending on if uplink is DCH or RACH.
	rlc_SizeList(eExplicitList);
	rlc_SizeIndex(1);
	mac_LogicalChannelPriority(rbn);
	dl_TransportChannelType(TrChFACHType);
	dl_logicalChannelIdentity(rbn);
#endif
};

// 25.331 13.7 Default configuration number 3, for voice.
// SRBS: RB1 thru RB3 and data: RB5, RB6, RB7.
void RBInfo::defaultConfig3Rb(unsigned rbn)
{
	rb_Identity(rbn);
	switch (rbn) {
	case 1:				// SRB1, used to communicate with RRC itself.
		ul_RLC_Mode(URlcModeUm);
		dl_RLC_Mode(URlcModeUm);
		// Transmission RLC Discard Mode is 'NotConfigured'.
		break;
	case 2: case 3:		// SRB2 and SRB3.
		// This is the config from the manual, but I think we could just
		// use defaultConfigSrbRlcAm();
		ul_RLC_Mode(URlcModeAm);
		transmissionRLC_DiscardMode(TransmissionRlcDiscard::NoDiscard);
		transmissionWindowSize(128);
		maxDat(15);
		timerRST(300);
		max_RST(1);
		// >>pollingInfo
		lastTransmissionPDU_Poll(false);	// redundant, this is the default.
		lastRetransmissionPDU_Poll(false);	// redundant, this is the default.
		timerPollPeriodic(300);
		dl_RLC_Mode(URlcModeAm);
		receivingWindowSize(128);
		//dl_RLC_PDU_size(128);	not specified?
		inSequenceDelivery(true);
		// >>dl_RLC_StatusInfo
		timerStatusProhibit(100);
		missingPDU_Indicator(false);
		timerStatusPeriodic(300);
		//rlc_OneSidedReEst(false);	// Not in the old spec?
		break;
	case 4: assert(0);
	case 5: case 6: case 7:	// These are the AMR voice RBs.
		ul_RLC_Mode(URlcModeTm);
		ul_segmentationIndication(false);
		dl_RLC_Mode(URlcModeTm);
		dl_segmentationIndication(false);
		break;
	default: assert(0);
	}

	// As of 9-2012, this is the only setup that uses non-multiplexed channels
	// so it is the only one one that uses RBMappingInfo.
	// This is the new RBMappingInfo.
	// It must match what is specified in the defineTrCh for these TrCh in
	// TrChConfig setup, see defineTrCh().
	// Originally I started to implement UMTS RBMappingInfo, below, but then I simplified:
	// Took all the separate RBMappingInfo out and put it in these variables:
	// o mTcIsMultiplexed and mTcRbId in the TrChInfo.
	// o mTrChAssigned in the RBInfo.
	// All the other RBMappingInfo defaults, eg, logical channel mapping to rbid is 1-to-1
	// and the rlc size info is we always use all sizes.
	switch (rbn) {
	case 1: case 2: case 3:		// SRB1,2,3 multiplexed on TrCh 4.
		setTransportChannelIdentity(4); break;
	case 5:		// AMR voice channels on RB5,6,7 mapped to trch1,2,3
		setTransportChannelIdentity(1); break;
	case 6:
		setTransportChannelIdentity(2); break;
	case 7:
		setTransportChannelIdentity(3); break;
	default: assert(0);
	}

#if USE_RBMAPPINGINFO	// Unneeded now.
	// rb-MappingInfo
	// pats note: The ul and dl are identical, and probably always will be.
	UL_LogicalChannelMappings(1);
	ul_TransportChannelType(TrChUlDCHType);
	dl_TransportChannelType(TrChDlDCHType);
	switch (rbn) {
	case 1: case 2: case 3: case 4:	// All SRBs on TrCh 4, which is multiplexed.
		ul_transportChannelIdentity(4);
		dl_transportChannelIdentity(4);
		break;
	case 5:		// AMR codec class "A" bits on TrCh 1.
		ul_transportChannelIdentity(1);
		dl_transportChannelIdentity(1);
		break;
	case 6:		// AMR codec class "B" bits on TrCh 2. 
		ul_transportChannelIdentity(2);
		dl_transportChannelIdentity(2);
		break;
	case 7:		// AMR codec class "C" bits on TrCh 3.
		ul_transportChannelIdentity(3);
		dl_transportChannelIdentity(3);
		break;
	default:	break;	// N/A
	}
	ul_logicalChannelIdentity(rbn);	// Only needed for RB1-3
	dl_logicalChannelIdentity(rbn);
	// >DL-logicalChannelMappingList
	MappingOption(1);
	rlc_SizeList(eConfigured);
	mac_LogicalChannelPriority(rbn <= 3 ? rbn : 5);
#endif
};


void UEInfo::uePeriodicService()
{
	// TODO: what?
}

void UEInfo::ueRegisterActivity()
{
	mActivityTime.now();
}

// Throw away UEs that died.
// TODO: This needs work.  The UE will go to CELL_FACH mode just to send
// a cell_update message, but we dont want to count that as activity.
void Rrc::purgeUEs()
{
	time_t now; time(&now);
	ScopedLock lock(mUEListLock);
	// NOTE: The timers are described in 13.1 and default values are in 10.3.3.43 and 10.3.3.44
	// T300 is the timer for the RRC connection setup.
	// It also interacts with T302, the cell update retry timer, but we dont use those yet.
	//int t300 = gConfig.getNum("UMTS.Timers.T300",1000);
	int tInactivity = 1000*gConfig.getNum("UMTS.Timers.Inactivity.Release");
	int tDelete = 1000*gConfig.getNum("UMTS.Timers.Inactivity.Delete");
	RN_FOR_ITR(UEList_t,mUEList,itr) {
		UEInfo *uep = *itr;


		// If the UE does not respond to an RRC message, do something, but what?.
		// They are in RLC-AM mode, so no point in resending.
		UeTransaction *last = uep->getLastTransaction();
		/****
		if (last && last->mTransactionType != ttComplete &&
			last->mTransTime.elapsed() > t300) {
			switch (last->mTransactionType) {
			case ttRrcConnectionSetup:
				// The UE will retry in 1 second, so just ignore it.
				break;
			case ttRrcConnectionRelease:
				sendRrcConnectionRelease(UEInfo *uep);
				last->transClose();
			case ttRadioBearerSetup:
			case ttRadioBearerRelease:
			default: assert(0);
			}
			LOG(DEBUG) <<"Deleting "<<uep<<" timeout after event:"<<last->name();
			mUEList.erase(itr);
			delete uep;
		}
		***/

		// No pending transactions...
		// If the UE is inactive, attempt to drop it back to idle mode or delete it

		long elapsed = uep->mActivityTime.elapsed();
		switch (uep->ueGetState()) {
		case stIdleMode:
			if (elapsed > tDelete) {
				// Temporarily add an alert for this:
				LOG(ALERT) << "Deleting " << uep;
				mUEList.erase(itr);
				delete uep;
			}
			break;
		case stCELL_FACH:
		case stCELL_DCH:
		case stCELL_PCH:
			if (elapsed > tInactivity &&
				last->mTransactionType != ttRrcConnectionRelease) {
				sendRrcConnectionRelease(uep);
			}
			break;
		default: assert(0);
		}
	}
}

void Rrc::addUE(UEInfo *ue)
{
	purgeUEs(); // Now is a fine time to purge the UE list of any dead UEs.

	ScopedLock lock(mUEListLock);
	mUEList.push_back(ue);
}

// If ueidtype is 0, look for URNTI, else CRNTI
UEInfo *Rrc::findUe(bool ueidtypeCRNTI,unsigned uehandle)
{
	//ScopedLock lock(mUEListLock);
	UEInfo *uep;
	RN_FOR_ALL(UEList_t,mUEList,uep) {
		if (ueidtypeCRNTI) {
			if (uehandle == uep->mCRNTI) {return uep;}
		} else {
			if (uehandle == uep->mURNTI) {return uep;}
		}
	}
	return NULL;
}

// Interpreting 25.331 10.3.3.15 InitialUEIdentity.
// This is used in the RRC Intial Connection Request, used when UE is in idle mode.
// If we get an identical AsnId, we want to use the identical URNTI by returning the existing UEInfo,
// in case the UE sends second request before we finish the first request we dont confuse it.
// Other than that, I think it is ok to issue a new URNTI every time we get the RRC Initial Connection Request.
// The UE may also be in idle mode because we goofed up and lost it, but in that case I dont
// think it matters if we issue a new URNTI or not.
UEInfo *Rrc::findUeByAsnId(AsnUeId *asnId)
{
	{ ScopedLock lock(mUEListLock);
	  UEInfo *uep;
	  RN_FOR_ALL(UEList_t,mUEList,uep) {
		// If the whole thing matches just use it.
		// The UE may identify itself one way (eg IMSI) on the first rrc connection request,
		// then later use TMSI or P-TMSI.
		// The UE may identify itself by P-TMSI using a P-TMSI that it obtained from us days ago.
		// None of that matters; we are only trying to identify identical RRC Intial Connection Requests
		// from the same UE.
		if (asnId->eql(uep->mUid)) {return uep;}
	  }
	}

	// In a UMTS system, the NodeB layer identifies the UE by URNTI,
	// the SGSN layer identifies the UE by PTMSI, and the RNC layer maintains the mapping
	// between PTMSI and NodeB+URNTI.
	// To implement that, instead of updating the URNTI handle in the SGSN,
	// we are going to re-use the same URNTI for the same PTMSI forever.

	// One other case where this might be necessary is if a page comes in between the time we do this
	// and get around to registration with the SGSN - the SGSN will look
	// for the necessary UE in RRC using the old URNTI, so we want to keep the same one.
	uint32_t urnti = 0;
	UEInfo *result;
	if (asnId->mImsi.size() && (urnti = SGSN::Sgsn::findHandleByImsi(asnId->mImsi))) {
		result = findUeByUrnti(urnti);
		return result ? result : new UEInfo(urnti);
	}
	if (asnId->mPtmsi && asnId->RaiMatches() && (urnti = SGSN::Sgsn::findHandleByPTmsi(asnId->mPtmsi))) {
		result = findUeByUrnti(urnti);
		return result ? result : new UEInfo(urnti);
	}
	LOG(ALERT) << "no match"<<LOGHEX2("ptmsi",(uint32_t) asnId->mPtmsi)<<LOGVAR2("raimatch",asnId->RaiMatches())
		<<LOGHEX2("findHandlebyPTmsi-urnti",urnti);
	return NULL;
}

UeTransaction::UeTransaction(UEInfo *uep,TransType ttype, RbId wRabMask, unsigned wTransactionId, UEState newState)
{
	//mTransactionId = uep->newTransactionId();
	mTransactionId = wTransactionId;
	mNewState = newState;
	mRabMask = wRabMask;
	mTransactionType = ttype;
	mTransTime.now();
	// Squirrel it away in the UE.
	uep->mTransactions[mTransactionId] = *this;
}

std::string UeTransaction::str()
{
	return format("UeTransaction(type=%d=%s newState=%s rabMask=%d transId=%d)",
		mTransactionType,TransType2Name(mTransactionType),
		UEState2Name(mNewState),mRabMask,mTransactionId);
}

long UeTransaction::elapsed()	// How long since the transaction, in ms?
{
	return mTransTime.elapsed();
}

UeTransaction *UEInfo::getLastTransaction()
{
	if (mNextTransactionId == 0) return NULL;	// No transactions have occurred yet.
	return &mTransactions[(mNextTransactionId-1) % sMaxTransaction];
}


UeTransaction *UEInfo::getTransactionRaw(unsigned transId, const char *help)
{
	if (transId >= 4) {
		PATLOG(1,format("%s: invalid transaction id, out of range, transId:%d",
		help,transId));
		return NULL;
	}
	return &mTransactions[transId];
}

UeTransaction *UEInfo::getTransaction(unsigned transId, TransType ttype, const char *help)
{
	if (transId >= 4) {
		PATLOG(1,format("%s: invalid transaction id, out of range, type:%d transId:%d",
		help,ttype,transId));
		return NULL;
	}
	UeTransaction *tr = &mTransactions[transId];
	if (tr->mTransactionType != ttype) {
		PATLOG(1,format("%s: invalid transaction, expected type:%d got type:%d id:%d",
			help,ttype,tr->mTransactionType,transId));
		return NULL;
	}
	return tr;
}

// This is here to hide it from the brain-dead compiler.
DCCHLogicalChannel*UEInfo::allocateLogicalChannel()
{
	return new DCCHLogicalChannel(this);
}

void UEInfo::ueRecvRrcConnectionReleaseResponse(unsigned transId, bool success, const char *msgname)
{
	UeTransaction *tr = getTransaction(transId,ttRrcConnectionSetup,msgname);
	// For our purposes, we are going to forget about this phone, success or otherwise.
        /*if (success) {
                // If we were in DCH state, then unhook DCH.  
                if (this->ueGetState() == stCELL_DCH) {
                        // Must unhook from MAC before deleting the RLC entities.
                        macUnHookupDch(this);
                }
        }*/
	this->ueSetState(stIdleMode);
	if (tr) tr->transClose();	// Done with this one.
}

void UEInfo::ueRecvRrcConnectionSetupResponse(unsigned transId, bool success, const char *msgname)
{
	UeTransaction *tr = getTransaction(transId,ttRrcConnectionSetup,msgname);
	// Even if we dont find the correct UeTransaction, the UE is in
	// CELL_FACH state or we would not have gotten this message, so switch:
	if (success) this->ueSetState(stCELL_FACH);
	if (tr) tr->transClose();	// Done with this one.
}

void UEInfo::ueRecvRadioBearerReleaseResponse(unsigned transId, bool success, const char *msgname)
{
	UeTransaction *tr = getTransaction(transId,ttRadioBearerRelease,msgname);
	if (!tr) return;
	// TODO: If the transaction fails, what state should we use for the UE?
	if (success) {
		// If there are still RBs in use, for example, if the UE had two IPs and released one,
		// then we leave the UE in DCH state.
		if (tr->mNewState == stCELL_FACH) {
			// Must unhook from MAC before deleting the RLC entities.
			//macUnHookupDch(this);
			this->ueSetState(tr->mNewState);
		}
	}
	for (RbId rbid = 5; rbid < gsMaxRB; rbid++) {
		if (tr->mRabMask & (1<<rbid)) {
			mConfiguredRabs[rbid].mStatus = success ?
				SGSN::RabStatus::RabIdle : SGSN::RabStatus::RabFailure;
			// The Ggsn long since destroyed these RABs, so we do not need to notify it.
		}
	}
	tr->transClose();	// Done with this one.
}

void UEInfo::ueRecvRadioBearerSetupResponse(unsigned transId, bool success, const char *msgname)
{
	UeTransaction *tr = getTransaction(transId,ttRadioBearerSetup,msgname);
	if (!tr) return;
	// We want to send the L3 message back on the new DCCH in DCH
	if (success) {
		this->ueSetState(tr->mNewState);
	} else {
		macUnHookupDch(this);
	}
	for (RbId rbid = 5; rbid < gsMaxRB; rbid++) {
		if (tr->mRabMask & (1<<rbid)) {
			mConfiguredRabs[rbid].mStatus = success ?
				SGSN::RabStatus::RabAllocated : SGSN::RabStatus::RabFailure;
			// Notify the Ggsn to either send the Pdp Accept message or teardown this PdpContext:
			this->sgsnHandleRabSetupResponse(rbid,success);
		}
	}
	tr->transClose();	// Done with this one.
}

void UEInfo::ueSetState(UEState newState)
{
    printf("newState: %d %d\n",newState,mUeState);
	if (newState == mUeState) return;
	// TODO: When moving to CELL_FACH we have to set up the RBs and RLCs.
	// Assumes only one active DCH per UE
	if (/*mUeState == stCELL_DCH &&*/ newState != stCELL_DCH) {
                usleep(2000); // sleep to make sure other messages get processed through the RLC, MAC, L1, etc.
		macUnHookupDch(this);
	}
	switch (newState) {
	case stIdleMode:
		ueDisconnectRlc(stCELL_FACH);
		ueDisconnectRlc(stCELL_DCH);
		this->integrity.integrityStop();
		break;
	default: break;
	}
	mUeState = newState;
}


RrcMasterChConfig *UEInfo::ueGetConfig()
{
	switch (mUeState) {
	case stCELL_FACH:
		return gRrcDcchConfig;
	case stCELL_DCH:
		return &mUeDchConfig;
	default:
		return gRrcCcchConfig;
		assert(0);
	}
}

TrChConfig *UEInfo::ueGetTrChConfig()
{
	return &ueGetConfig()->mTrCh;
}

URlcPair *UEInfo::getRlc(RbId rbid)
{
	switch (mUeState) {
	case stCELL_FACH:
	case stIdleMode: // FIXME:: This happens from time to time.  patching over for now.
					// pat 12-30-2012:  We were putting the UE in idle mode accidentally,
					// so maybe this is fixed now, but I am leaving this case here.
		//LOG(INFO)<<format("getRlc this=%p URNTI=%x rbid=%d, 1=%p 2=%p 3=%p 2up=%p\n",this,
		//	mURNTI,rbid,mRlcsCF[1],mRlcsCF[2],mRlcsCF[3],mRlcsCF[2]?mRlcsCF[2]->mUp:0);
		return mRlcsCF[rbid];
	case stCELL_DCH:
		return mRlcsCDch[rbid];
	default:
		assert(0);
	}
}


URlcTrans *UEInfo::getRlcDown(RbId rbid)
{
	URlcPair *pair = getRlc(rbid);
	return pair ? pair->mDown : 0;
}

URlcRecv *UEInfo::getRlcUp(RbId rbid,UEState state)
{
	// pat 12-29-2012: Setting uestate here is very wrong, because we can get garbage coming
	// in the from the radio at any time and we dont want to misconfigure the RLC here.
	// mUeState = state;
	URlcPair *pair = getRlc(rbid);
	return pair ? pair->mUp : 0;
}

unsigned UEInfo::getDlDataBytesAvail(unsigned *uePriority)
{
	// We are assuming that priority increases with RbId.
	RN_UE_FOR_ALL_RLC_DOWN(this,rbid,rlcp) {
		unsigned bytes = rlcp->rlcGetBytesAvail();
		if (bytes) {
			*uePriority = rbid;
			return bytes;
		}
	}
	return 0;
}

void UEInfo::uePullLowSide(unsigned amt)
{
	RN_UE_FOR_ALL_RLC_DOWN(this,rbid,rlcp) {
		rlcp->rlcPullLowSide(amt);
	}
}

ByteVector *UEInfo::ueReadLowSide(RbId rbid)
{
	URlcTrans *rlc = getRlcDown(rbid);
	if (!rlc) return 0;
	return rlc->rlcReadLowSide();
}

void UEInfo::msWriteHighSide(ByteVector &dlpdu, uint32_t rbid, const char *descr) 
{  // Called by SGSN
	assert(rbid >= (int)SRB3);
	if (rbid == (int)SRB3) {
		// Wrap an L3 message inside Downlink Direct Transfer DCCH message
		ByteVector *result = sendDirectTransfer(this,dlpdu,descr,true);
		if (result) {
			ueWriteHighSide((RbId) rbid, *result, descr);
			delete result;
		}
	} else {
		ueWriteHighSide((RbId) rbid, dlpdu, descr);
	}
}		


void UEInfo::ueWriteHighSide(RbId rbid, ByteVector &sdu, string descr)
{
	ueRegisterActivity();
	PATLOG(1,format("ueWriteHighSide(%d,sizebytes=%d,%s)",rbid,sdu.size(),descr.c_str()));
	LOG(INFO) << "From SGSN: " << format("ueWriteHighSide(%d,sizebytes=%d,%s)",rbid,sdu.size(),descr.c_str());
        LOG(INFO) << "SGSN data: " << sdu;
	URlcTrans *rlc = getRlcDown(rbid);
	if (!rlc) {
		LOG(ERR) << "logic error in ueWriteHighSide: null rlc";
		//delete sdu;
		return;
	}
	rlc->rlcWriteHighSide(sdu,0,0,descr);
}

// This is usually called from the SGSN or GGSN for SM PdpContextDeactivation
// or GMM DetachRequest, and we should also call it for non-responsive UE somehow.
// At the time this is called there may be L3 messages in SRB3 for this UE,
// eg, PdpContextDeactivateAccept or DetachAccept.
// If we are releasing the last RAB, we also want to move the UE back to CELL_FACH mode.
// Note that the SRB3 RLC-AM is configured for in-order delivery, so we dont
// have to worry overly about a message race.
// Free all the RABs in rabMask.
// We use the RB id for the RAB id, so no mapping needed.
void UEInfo::msDeactivateRabs(unsigned rabMask)
{
	unsigned numActive = 0;
	// FIXME: The messages DeactivatePDPContextAccept and RadioBearerRelease are being put into
	// the RLC queues simultaneously, which results in them being reversed.  Try sleeping a little.
	//sleep(2);
	for (RbId rbid = 5; rbid < gsMaxRB; rbid++) {
		switch (mConfiguredRabs[rbid].mStatus) {
		case SGSN::RabStatus::RabPending:	// Hmmm...
		case SGSN::RabStatus::RabAllocated:
			if (rabMask & (1<<rbid)) {
				mConfiguredRabs[rbid].mStatus = SGSN::RabStatus::RabDeactPending;
			} else {
				numActive++;	// This RAB is still in use.
			}
			break;
		default: break;
		}
	}
	sendRadioBearerRelease(this,rabMask,numActive==0);
}

//unsigned UEInfo::getActiveRabMask()
//{
	//for (RbId rbid = 5; rbid < gsMaxRB; rbid++) {
	//}
//}

// This receives messages on DCCH on either RACH or DCH.
// It does not receive message on CCCH.
// The UEState here specifies whether the message arrived on RACH (state==stCell_FACH)
// or DCH (state==stCELL_DCH), and it is an error it does not match the state we think the UE is in.
void UEInfo::ueWriteLowSide(RbId rbid, const BitVector &pdu,UEState state)
{
	ueRegisterActivity();
	//UeTransaction(uep,UeTransaction::ttRrcConnectionSetup, 0, transactionId,stCELL_FACH);
	// The first transaction needs to be handled specially.
	// The first message, RrcConnectionSetupComplete, must be handled specially.
	// The fact of its arrival is confirmation of the transaction.
	if (mUeState == stIdleMode) {
		// It is NOT necessarily transaction number 0.
		// FIXME: This may have missed the uplink ConnectionSetupComplete message, in which case what should we do?
		UeTransaction *trans = getLastTransaction();
		if (trans && trans->mTransactionType == UeTransaction::ttRrcConnectionSetup) {
			ueRecvRrcConnectionSetupResponse(0, true, "RrcConnectionSetup");
		} else {
			LOG(ERR) << "Received DCCH message for UE in idle mode for trans. type: " << trans->mTransactionType << " "<<this;
		}
	}
	URlcRecv *rlc = NULL;
	if (rbid <= mUeMaxRlc) {
		rlc = getRlcUp(rbid,state);
	}
	if (!rlc) {
		LOG(ERR) << "invalid"<<LOGVAR(rbid)<<((state == stCELL_FACH)?" on RACH":" on DCH") <<this;
		return;
	}
	LOG(INFO) << "rbid: " << rbid << " rrc: rlcWriteLowSide: " <<this<<" "<<pdu;
	rlc->rlcWriteLowSide(pdu);
	// TODO: This rlc needs a connection on the top side.
}

// Destroy the RLC entities
// Take care because mRlcsCF[i] and mRlcsCDch[i] may point to the same RlcPair.
void UEInfo::ueDisconnectRlc(UEState state)
{
	int cnt=0, deleted=0;
	for (unsigned i=0; i < gsMaxRB; i++) {
		switch (state) {
		case stCELL_FACH:
			if (mRlcsCF[i]) {
				if (mRlcsCF[i] != mRlcsCDch[i]) {delete mRlcsCF[i]; deleted++;}
				mRlcsCF[i] = 0;
				cnt++;
			}
			break;
		case stCELL_DCH:
			if (mRlcsCDch[i]) {
				if (mRlcsCF[i] != mRlcsCDch[i]) {delete mRlcsCDch[i]; deleted++;}
				mRlcsCDch[i] = 0;
				cnt++;
			}
			break;
		default: assert(0);
		}
	}
	if (cnt) LOG(INFO)<<format("ueDisconnectRlc: new state=%s disconnected %d deleted %d %s",
		UEState2Name(state),cnt,deleted,this->ueid().c_str());
}

void UEInfo::reestablishRlcs() {
        for (unsigned i = 0; i < gsMaxRB; i++) {
		URlcPair *curr = getRlc(i);
		if (curr && curr->mDown->mRlcMode == URlcModeAm) {
			LOG(ALERT)<<format("Resetting RLC rb %d",i);
			dynamic_cast<URlcTransAm*>(curr->mDown)->transAmReset();
			dynamic_cast<URlcRecvAm*>(curr->mUp)->recvAmReset();
		}
	}
}	
// Connect this UE to some RLCs for the RBs defined in the config for the specified new state.
// The configuration is pending until we receive an answering message with
// the specified transactionId, at which time the specified RBs will become usable.
// NOTE!!  If the rlc-mode is the same (ie, TM,UM,AM) and the rlc-size does
// not change, the spec requires us to 'copy' the internal state when
// we change state, which we affect by simply re-using the same RLC,
// which implies we are not allowed to change the RLC programming during
// the state change.
void UEInfo::ueConnectRlc(
	RrcMasterChConfig *config, 	// This is the config we will use in nextState.
	UEState nextState)			// The state we are trying to configure.
{
	// Even if we were in DCH state before, we must look for up any new RBs that need RLCs.
	//if (config == mUeConfig) {return;}	// WRONG!
	bool isNewState = (nextState != ueGetState());
	// Note that when we move to CELL_FACH state we may be leaving RLCs from DCH state on rb 5-15.
	for (unsigned i = 1; i < config->mNumRB; i++) {	// Skip SRB0
		RBInfo *rb = config->getRB(i);
		if (rb->valid()) {
			unsigned rbid = rb->mRbId;
			// You may ask: we are allocating both up and down RLCs,
			// but we are only sending the downlink TFS, and the uplink may be different.
			// It is because we need the downlink TFS to compute some parameter
			// for the downlink RLC that is specified directly as a parameter for uplink RLC.
			// Update: now all we use dltfs for is to compute the mac header size,
			// so we should probably just send that instead.
			TrChId tcid = rb->mTrChAssigned;
			RrcTfs *dltfs = config->getDlTfs(tcid);
			if (rbid > mUeMaxRlc) { mUeMaxRlc = rbid; }

			// TODO: This may want to hook a PDCP on top of the RLC.
			URlcPair *other, *prev;
			unsigned newpdusize = computeDlRlcSize(rb,dltfs);
			const char *action = "";
			switch (nextState) {
			case stCELL_FACH:
				// First delete existing if necessary.
				prev = mRlcsCF[rbid];
				if (!isNewState && prev) { continue; }	// Leave previously configured RBs alone.
				other = mRlcsCDch[rbid];
				if (prev && prev != other) { delete prev; }
				if (other && other->mDown->mRlcMode == rb->getDlRlcMode() && other->mDown->rlcGetDlPduSizeBytes() == newpdusize) {
					mRlcsCF[rbid] = other;
					action = "copied";
				} else {
					mRlcsCF[rbid] = new URlcPair(rb,dltfs,this,tcid);
					action = "allocated";
				}
				if (isNewState) this->mStateChange = true;
				break;
			case stCELL_DCH:
				prev = mRlcsCDch[rbid];
				if (!isNewState && prev) { continue; }	// Leave previously configured RBs alone.
				other = mRlcsCF[rbid];
				if (prev && prev != other) { delete prev; }
				if (other) printf("PDU sizes: %d %d\n", other->mDown->rlcGetDlPduSizeBytes(),newpdusize);
				if (other && other->mDown->mRlcMode == rb->getDlRlcMode() && other->mDown->rlcGetDlPduSizeBytes() == newpdusize) {
					mRlcsCDch[rbid] = other;
					action = "copied";
				} else {
					mRlcsCDch[rbid] = new URlcPair(rb,dltfs,this,tcid);
					action = "allocated";
				}
				if (isNewState) this->mStateChange = true;
				//mRlcsCDch[rbid]->mDown->triggerReset();
				break;
			default: assert(0);
			}

			LOG(INFO)<<format("ueConnectRlc:curstate=%s next=%s %s i=%d rbid=%d up=%s down=%s %s",
				UEState2Name(ueGetState()),UEState2Name(nextState),action,
				i,rbid, URlcMode2Name(rb->getUlRlcMode()),URlcMode2Name(rb->getDlRlcMode()),this->ueid().c_str());
		}
	}
	//LOG(INFO)<<format("connectRlc this=%p URNTI=%x 1=%p 2=%p 3=%p 2up=%p\n",this,
		//	mURNTI,mRlcsCF[1],mRlcsCF[2],mRlcsCF[3],mRlcsCF[2]?mRlcsCF[2]->mUp:0);
	//mUeConfig = config;
}


//void UEInfo::flushSRB0()
//{
//	PATLOG(1,"flushSRB0 "<<this);
//	// TODO: flush the down rlc.
//	URlcTrans *rlc = getRlcDown(0);
//	if (!rlc) {
//		LOG(ERR) << "logic error in flushSRB0: null rlc";
//		return;
//	}
//	ByteVector *pdu;
//	while ((pdu = rlc->rlcReadLowSide())) {
//		PATLOG(1,"rlcReadLowSide returned size:"<<pdu->size());
//		MacSwitch::writeHighSideCcch(pdu,this);
//	}
//}

string UEInfo::ueid() const
{
    char buf[100];
    sprintf(buf," UE#%d URNTI=%x %s",mUeDebugId,(uint32_t)mURNTI,UEState2Name(ueGetState()));
    return string(buf); // not efficient, but only for debugging.
}


std::ostream& operator<<(std::ostream& os, const UEInfo*uep)
{
	if (uep) {
		os << uep->ueid();
	} else {
		os << " UE#(null)";
	}
	return os;
}


// Do we need a control channel for RRC messages?
// Pick 64 SF.
// Phone required FACH separate from PCH in SIB5?
// 25.211 Table 11, pick slot formats with TFCI and larger pilot bits,
// 		probably 3, 11, 14, 15, 16.
// Table 11 for CCPCH: ditto channels.
// TODO: Init the DCH as well.
void rrcInitCommonCh(unsigned rachSF, unsigned fachSF)	// (RACHFEC *rach, FACHFEC *fach)
{
	static bool inited = 0;
	gRrc.rrcDoInit();
	if (inited) return;
	inited = 1;

	// Pat set default FACH SF to 64:
	//unsigned fachSF = gConfig.getNum("UMTS.SCCPCH.SF");
	//unsigned rachSF = gConfig.getNum("UMTS.PRACH.SF");

	// Currently we use a single FACH and RACH broadcast in the beacon SIB5.
	// Therefore it is used for both CCCH (SRB0) and DCCH (SRB1,2,3)
	// (The alternative is to specify a second set of RACH/FACH for DCCH in SIB6.)
	// However, the CCCH channel is handled directly by the MAC,
	// while the DCCH logical channels go through the UE struct,
	// so we use two different master configs
	// NOTE: THe CCCH config is broadcast in SIB5.  Dont screw it up.
	gRrcCcchConfig = &gRrcCcchConfig_s;
	gRrcDcchConfig = &gRrcDcchConfig_s;
	//gRrcDchPSConfig = &gRrcDchPSConfig_s;

	// Configure CCCH and SRB0 for rach/fach
	gRrcCcchConfig->mTrCh.configRachTrCh(rachSF,TTI10ms,16,260); //-56+8);
	// Pat modified FACH parameters to avoid rate-matching.
	//gRrcCcchConfig->mTrCh.configFachTrCh(fachSF,TTI10ms,16);
	gRrcCcchConfig->mTrCh.configFachTrCh(fachSF,TTI10ms,12,360); //-56+8);

	// There are no RBs for CCCH.  The SRB0 RLC is in the Macc, not the UE.
	//gRrcCcchConfig->setSRB(0)->defaultConfigSRB0();

	// Configure DCCH and SRB1, SRB2, SRB3 for rach/fach
	// Currently use same rach fach channel as CCCH.
	gRrcDcchConfig->mTrCh.configRachTrCh(rachSF,TTI10ms,16,260); //-56+8);
	gRrcDcchConfig->mTrCh.configFachTrCh(fachSF,TTI10ms,12,360); //-56+8);
	gRrcDcchConfig->setSRB(1)->defaultConfig0CFRb(1);
	gRrcDcchConfig->setSRB(2)->defaultConfig0CFRb(2);
	gRrcDcchConfig->setSRB(3)->defaultConfig0CFRb(3);

	{
		std::ostringstream RachInform, FachInform;
		RachInform << "RACH TFS:";
		gRrcCcchConfig->getUlTfs()->text(RachInform);
		FachInform << "FACH TFS:";
		gRrcCcchConfig->getDlTfs()->text(FachInform);
		LOG(INFO) << RachInform.str();
		std::cout << RachInform.str() << std::endl;
		LOG(INFO) << FachInform.str();
		std::cout << FachInform.str() << std::endl;

		//std::cout << "RACH TFS:";
		//gRrcCcchConfig->getUlTfs()->text(std::cout);
		//std::cout << "\nFACH TFS:";
		//gRrcCcchConfig->getDlTfs()->text(std::cout);
		//std::cout <<"\n";
	}
}

RBInfo *RrcMasterChConfig::setSRB(unsigned rbid)
{
	assert(rbid<=4);
	if (rbid >= mNumRB) { mNumRB = rbid+1; }
	mRB[rbid].mRbId = rbid;	// Kinda redundant, but also marks this RB as valid.
	return &mRB[rbid];
}

RBInfo *RrcMasterChConfig::setRB(unsigned rbid,CNDomainId domain)
{
	assert(rbid > 4);
	assert(rbid<gsMaxRB);
	if (rbid >= mNumRB) { mNumRB = rbid+1; }
	mRB[rbid].mRbId = rbid;	// Kinda redundant, but also marks this RB as valid.
	mRB[rbid].mPsCsDomain = domain;
	return &mRB[rbid];
}

// For PS services the UE may request several IP connections at different times,
// which might generate several RAB messages.
// Not sure if we should/can send the SRBs on subsequent messages.
// For now, just throw the SRBs in every message and hope the UE ignores them if it needs to.
// Note that the config is only for the new RAB being allocated -
// pre-existing RABs with different ids inside the UE are not supposed to be affected.
void RrcMasterChConfig::rrcConfigDchPS(DCHFEC *dch, int RABid, bool useTurbo)
{
	// Define a simple multiplexed TrCh of width for dch.
	if (this->mTrCh.dl()->getNumTrCh() == 0) {
		this->mTrCh.configDchPS(dch, TTI10ms, 16, useTurbo, 340+40, 340);
	} else {
		// TrCh setup already configured.
		// We may be defining a second RAB for a second PDPContext.
		// FIXME: If the QOS or dch has changed or we are adding PS to CS services,
		// we need to add the TrCh to the existing configuration.
	}
	//this->mTrCh.tcdump();

	// The RLC for the SRB will take the rlc size from the TrCh,
	// and fill it up with filler after the miniscule message.
	this->setSRB(1)->defaultConfig0CFRb(1);
	this->setSRB(2)->defaultConfig0CFRb(2);
	this->setSRB(3)->defaultConfig0CFRb(3);

	// Now what about the RAB?
	assert(RABid >= 5 && RABid <= 15);
	//this->addRAB(rbid,CNDomainId)
	// TODO: We may want to use RLC-UM for a PFT for TCP/UDP.  Clear?
	this->setRB(RABid,PSDomain)->defaultConfigRlcAmPs();
	//this->mTrCh.tcdump();
}

// The DCH must be SF=128 or higher.
void RrcMasterChConfig::rrcConfigDchCS(DCHFEC *dch)
{
	this->mTrCh.defaultConfig3TrCh();
	this->setSRB(1)->defaultConfig3Rb(1);
	this->setSRB(2)->defaultConfig3Rb(2);
	this->setSRB(3)->defaultConfig3Rb(3);
	// AMR voice on RB5,6,7
	this->setRB(5,CSDomain)->defaultConfig3Rb(5);
	this->setRB(6,CSDomain)->defaultConfig3Rb(6);
	this->setRB(7,CSDomain)->defaultConfig3Rb(7);
}

// This is the entry point to send the big RB setup message for a PS (internet) data connection,
// called from L3 in the SGSN, or more precisely, the GGSN upon receiving a request
// for a new PdpContext to be allocated.
// The fact that we got here indicates we already have a DCCH connection to this UE.
// 25.331 8.6.4.2: If you send a radio bearer setup/reconfig with a RAB that was set up previously,
// the UE removes the old RLC,PDCP first.
// Result is 3 state: failure in which the SmCauseType is also returned;
// RAB already allocated, or pending - the message to allocate RAB successfully sent.
// 
// After return, the caller is going to send a message on SRB3, so are we supposed
// to copy the RLC state before or after that message?
static SGSN::RabStatus rrcAllocateRabForPdp(uint32_t urnti,int rbid,ByteVector &qosb)
{
	assert(rbid >= 5 && rbid < (int)gsMaxRB);
	UEInfo *uep = gRrc.findUeByUrnti(urnti);
	if (!uep) {
		// Not sure how this could happen, but dont crash.  Not sure what error to report.
		return SGSN::RabStatus(SGSN::SmCause::User_authentication_failed);
	}

	ScopedLock(uep->mUeLock);
	// Is this RAB already configured?
	// TODO: If so, it may be a request to change the QoS, but we are not smart enough to do that yet.
	// There are two opportunities for the PDP Context setup messages to be lost,
	// one of them here during RAB setup and one at the GGSN level.
	// So it is possible to have a successful RAB setup, but the PDP Context setup message did not get through.
	// So return OK immediately to inform the GGSN to send another PDP Context setup response.
	SGSN::RabStatus *rabstatus = &uep->mConfiguredRabs[rbid];
	if (rabstatus->mStatus == SGSN::RabStatus::RabAllocated) {
		// Only return if in CELL_DCH state, otherwise the PDP is still active and the RAB is still allocated
		if (uep->ueGetState()==stCELL_DCH)
		  return *rabstatus; 	// The RAB was allocated previously by this UE
	}

	// TODO: allow assymetric up/downlink.  For now, just use the downlink and assume it is higher.
	// TODO: If this is a second request, it may be for a higher BW, in which case
	// we need to re-allocate to get a higher speed connection.

	SGSN::SmQoS qos(qosb);
	int kbps = qos.getMaxBitRate(0);		// this comes in kbits/sec
	int ops;							// octets/sec
	if (kbps != -1) {
		ops = kbps * 1000/8;	// Convert to bytes/sec
	} else {
		ops = qos.getPeakThroughput();	// this comes in bytes/sec, severely quantized into just 9 levels
	}
	printf("kbps: %d, ops: %d\n",kbps,ops);
	if (ops == 0) {
		// It is either a mistake or 'best-effort'.
		// For best-effort, default to 60K which is SF=16, or 480 kbit/s.
		ops = gConfig.getNum("UMTS.Best.Effort.BytesPerSec");
	}

	DCHFEC *dch = uep->mUeDch;
	if (dch) {
		// This is ok in the case where we are allocating a second PDP.
		// FIXME: If the channel is too slow get a new one, which will require
		// reassigning all the existing RABs as well.
		if (uep->ueGetState() != stCELL_DCH) {
			// But how would this happen?
			LOG(ERR) <<"UE has DCH but is not in CELL_DCH state" <<" ue:"<<uep;
		}
	} else {
		// Try to allocate a physical channel.
		dch = gChannelTree.chChooseByBW(ops);
		while (dch == 0 && ops > 1000) {
			ops = ops/2;
	                dch = gChannelTree.chChooseByBW(ops);
		}
		if (dch == 0) {
			return SGSN::RabStatus(SGSN::SmCause::Insufficient_resources);	// All channels busy.
		}
	}
	PATLOG(1,format("allocated RAB sf=%d,%d",dch->getUlSF(),dch->getDlSF()));

	// Generate a config.
	// TODO: Support unacknowledged mode configs.
	RrcMasterChConfig *newConfig = &uep->mUeDchConfig;
	bool useTurbo = gConfig.getNum("UMTS.UseTurboCodes") != 0; 
	newConfig->rrcConfigDchPS(dch, rbid, useTurbo);

	// Configure dch.  Use turbo coding.
#if USE_OLD_DCH
	dch->fecConfig(newConfig->mTrCh, useTurbo); //true);
#else
	// The turbo flag is in the TrCh config.
	// But I didnt want to change the old fecConfig and risk breaking it.
	// The new fecConfig function is not working yet.  In the meantime use fecConfigForOneTrCh().
	dch->fecConfig(newConfig->mTrCh); //true);
#endif

	// Allocate the RLCs for CELL_DCH mode.
	// Note that 25.331 8.6.4.9 RLC-Info specifies the UE behavior after receiving
	// the RadioBearerSetup message, which is generally to re-establish the RLC only
	// if the "DL RLC PDU" size is changed.  NOTE!  This the UE side.
	// For the Network side, we must copy the RLC state after sending the RadioBearerSetup message.
	// We have to flush the message through RLC first to update the sequence numbers,
	// and we dont know a-priori into how many PDUs the message will be fractured.
	// Note that unless we wait for the the RLC-AM acknowledgement, the RLC-AM entity
	// will also contain state indicating it is waiting for that acknowledgement.
	// There is a danger that our thread will get suspended and the return
	// message will arrive before we have a chance to create the new RLCs,
	// so we have to temporarily stop the RLC while we do all this.
	// Even if we 'STOP' the rlc entities, if an additional uplink message arrives on RACH
	// after we copy the RLC entity state, and then we switch to CELL_DCH,
	// the new CELL_DCH RLC-AM would not know about it, request a resend, and then cleverly
	// resend the same message to higher layers yet again.  This all sucks.
	// I think it is better to just point to the same RLC for both old and new in this case,
	// even though the spec says you are supposed to be able to reconfigure other stuff
	// in the RLC at the same time, which implies an implementation where the rlc state is separate
	// from the configuration, and can have multiple pointers to the same state?
	// I REITERATE: This is supposed to be done after sending and flushing the RadioBearerSetup,
	// but we are working around that by duplicating the RLCs.
	uep->ueConnectRlc(newConfig,stCELL_DCH);

	// This sets uep->mUeDch, but we cant use it yet until the Ue state changes.
	LOG(NOTICE)<<"machookup start";
	macHookupDch(dch,uep);	// Allocates the corresponding Mac-D.
	LOG(NOTICE)<<"machookup stop";

	// The first time through, set up the SRBs also.
	bool srbsToo = uep->ueGetState() != stCELL_DCH;
	if (sendRadioBearerSetup(uep, newConfig, dch, srbsToo)) {
		// Dont know why it failed, but return some kind of error.
		return SGSN::RabStatus(SGSN::SmCause::Insufficient_resources);
	}
	LOG(NOTICE)<<"send RabStatus";
	rabstatus->mStatus = SGSN::RabStatus::RabPending; // Success, of a sort.
	// Return the bps that the UE supplied, not the one we allocated,
	// which may be a few % lower to better match the quantized QoS peak throughput.
	// See comments at bw2tier()
	rabstatus->mRateDownlink = rabstatus->mRateUplink = ops;
	return *rabstatus;
}

// Allocate a RAB for CS (voice) service.
// This needs to be called from the channel setup in the Control directory.
// TODO: Currently we use rbid 5,6,7 which will interfere with simultaneous PS connections.
SGSN::RabStatus rrcAllocateRabForCS(UEInfo *uep)
{
	ScopedLock(uep->mUeLock);
	RbId rbid = 5;		// the lowest rbid We use 

	// Is this RAB already configured?
	// TODO: If so, it may be a request to change the QoS, but we are not smart enough to do that yet.
	// There are two opportunities for the PDP Context setup messages to be lost,
	// one of them here during RAB setup and one at the GGSN level.
	// So it is possible to have a successful RAB setup, but the PDP Context setup message did not get through.
	// So return OK immediately to inform the GGSN to send another PDP Context setup response.
	SGSN::RabStatus *rabstatus = &uep->mConfiguredRabs[rbid];
	if (rabstatus->mStatus == SGSN::RabStatus::RabAllocated) {
		return *rabstatus; 	// The RAB was allocated previously by this UE
	}

	DCHFEC *dch = uep->mUeDch;
	if (dch) {
		assert(uep->ueGetState() == stCELL_DCH);
	} else {
		// Try to allocate a physical channel.
		dch = gChannelTree.chChooseBySF(128);	// The required downlink SF for 12.2K voice channel.
		if (dch == 0) {
			return SGSN::RabStatus(SGSN::SmCause::Insufficient_resources);	// All channels busy.
		}
	}
	PATLOG(1,format("allocated RAB sf=%d,%d",dch->getUlSF(),dch->getDlSF()));

	// Generate a config.
	// TODO: Support unacknowledged mode configs.
	RrcMasterChConfig *newConfig = &uep->mUeDchConfig;
	newConfig->rrcConfigDchCS(dch);
#if USE_OLD_DCH
	dch->fecConfig(newConfig->mTrCh,true);
#else
	dch->fecConfig(newConfig->mTrCh);	// We get turbo mode from the config.
#endif

	// Allocate the RLCs for CELL_DCH mode.
	// Note that 25.331 8.6.4.9 RLC-Info specifies the UE behavior after receiving
	// the RadioBearerSetup message, which is generally to re-establish the RLC only
	// if the "DL RLC PDU" size is changed.  NOTE!  This the UE side.
	// For the Network side, we must copy the RLC state after sending the RadioBearerSetup message.
	// We have to flush the message through RLC first to update the sequence numbers,
	// and we dont know a-priori into how many PDUs the message will be fractured.
	// Note that unless we wait for the the RLC-AM acknowledgement, the RLC-AM entity
	// will also contain state indicating it is waiting for that acknowledgement.
	// There is a danger that our thread will get suspended and the return
	// message will arrive before we have a chance to create the new RLCs,
	// so we have to temporarily stop the RLC while we do all this.
	// Even if we 'STOP' the rlc entities, if an additional uplink message arrives on RACH
	// after we copy the RLC entity state, and then we switch to CELL_DCH,
	// the new CELL_DCH RLC-AM would not know about it, request a resend, and then cleverly
	// resend the same message to higher layers yet again.  This all sucks.
	// I think it is better to just point to the same RLC for both old and new in this case,
	// even though the spec says you are supposed to be able to reconfigure other stuff
	// in the RLC at the same time, which implies an implementation where the rlc state is separate
	// from the configuration, and can have multiple pointers to the same state?
	// I REITERATE: This is supposed to be done after sending and flushing the RadioBearerSetup,
	// but we are working around that by duplicating the RLCs.
	uep->ueConnectRlc(newConfig,stCELL_DCH);

	// This sets uep->mUeDch, but we cant use it yet until the Ue state changes.
	macHookupDch(dch,uep);	// Allocates the corresponding Mac-D.

	// The first time through, set up the SRBs also.
	bool srbsToo = uep->ueGetState() != stCELL_DCH;
	if (sendRadioBearerSetup(uep, newConfig, dch, srbsToo)) {
		// Dont know why it failed, but return some kind of error.
		return SGSN::RabStatus(SGSN::SmCause::Insufficient_resources);
	}
	rabstatus->mStatus = SGSN::RabStatus::RabPending; // Success, of a sort.
	rabstatus->mRateDownlink = rabstatus->mRateUplink = 0;	// Not used for CS connections.
	return *rabstatus;
}

// TODO: Fix these for multiple code blocks like the R2 versions below.
// But since we are not using these other encode/decoders, no hurry.
unsigned R3EncodedSize(unsigned Ki) { assert(0); return 3*Ki+24; }       // rate 1/3
unsigned R3DecodedSize(unsigned Yi) { assert(0); return (Yi-24)/3; }     // rate 1/3

void R2Test()
{
	int Xi, Yi, Xi2, Yi2;
	static int didit = 0;
	if (didit++) return;
	for (Xi = 10; Xi < 2000; Xi++) {
		Yi = RrcDefs::R2EncodedSize(Xi);
		Xi2 = RrcDefs::R2DecodedSize(Yi);
		if (Xi2 != Xi) { printf("enc Xi=%d Yi=%d Xi2=%d\n",Xi,Yi,Xi2); }
	}
	for (Yi = 100; Yi < 5000; Yi++) {
		Xi = RrcDefs::R2DecodedSize(Yi);
		Yi2 = RrcDefs::R2EncodedSize(Xi);
		if (Yi2 != Yi) { printf("dec Yi=%d Xi=%d Yi2=%d\n",Yi,Xi,Yi2); }
	}
	// Find max Xi such that Ki = ceil(Xi/Ci) which is Xi = Ki * Ci.
	//int Xi = Ki / Ci;
	//int Ki2 = Xi/Ci;
	//// If Ci = 3,  Ki1=1,Xi=1,2,3 Ki=2,Xi=4,5,6
	//printf("Yi=%d Ki=%d Xi=%d r2=%d\n",Yi,Ki,Xi,R2EncodedSize(Xi) );
	//assert(R2EncodedSize(Xi) == Yi);
	//return Xi;
	// This is the result assuming there are no filler bits necessary.
}

// Encoding/decoding may require more than one code block if size > Z;
// It may not be possible to get an exact fit, which means rate matching will be required.
// Just round down to the next block size that will fit in Yi after encoding.
// This is surprisingly tricky.
unsigned RrcDefs::R2DecodedSize(unsigned Yi) // rate 1/2: base formula is: Ki = (Yi-16)/2;
{
	//R2Test();
	unsigned Zin = ZConvolutional;
	unsigned Zout = 2*Zin+16;			// encoded size of Z; this applies to each block.
	//unsigned Ci = (Yi + Zout-1)/Zout;	// number of coded blocks.
	unsigned Ci = 1 + (Yi-1)/Zout;		// number of coded blocks.
	unsigned Ki = (Yi - Ci*16)/2;		// This may be slightly too large.
	unsigned Yi2;
	while ((Yi2=R2EncodedSize(Ki)) > Yi) { Ki--; }	// Search for best.
	unsigned Ytest = R2EncodedSize(Ki);
	//printf("Yi=%d Ci=%d Ki=%d Yi2=%d Ytest=%d\n",Yi,Ci,Ki,Yi2,Ytest);
	//assert(Ytest <= Yi && Ytest >= Yi-16*Ci);
	assert(Ytest <= Yi);
	return Ki;
}


//unsigned TurboDecodedSize(unsigned Yi) { return (Yi-12)/3; }  // Turbo
unsigned RrcDefs::TurboDecodedSize(unsigned Yi) // rate 1/2: base formula is: Ki = (Yi-16)/2;
{
        //R2Test();
        unsigned Zin = ZTurbo;
        unsigned Zout = 3*Zin+12;                       // encoded size of Z; this applies to each block.
        //unsigned Ci = (Yi + Zout-1)/Zout;     // number of coded blocks.
        unsigned Ci = 1 + (Yi-1)/Zout;          // number of coded blocks.
        unsigned Ki = (Yi - Ci*12)/3;           // This may be slightly too large.
        unsigned Yi2;
        while ((Yi2=TurboEncodedSize(Ki)) > Yi) { Ki--; }  // Search for best.
        unsigned Ytest = TurboEncodedSize(Ki);
        //printf("Yi=%d Ci=%d Ki=%d Yi2=%d Ytest=%d\n",Yi,Ci,Ki,Yi2,Ytest);
        //assert(Ytest <= Yi && Ytest >= Yi-16*Ci);
        assert(Ytest <= Yi);
        return Ki;
}

// Xi is the total number of bits to encode, which will require Ci code blocks.
// rate 1/2: base formula is: YiR = 2*Ki+16
unsigned RrcDefs::R2EncodedSize(unsigned Xi, unsigned *codeBkSz, unsigned *fillBits)
{
	// From 24.212 4.2.2.2: code block segmentation.
	// The Xi may need to be encoded in several separate blocks, which may
	// require filler bits if Xi is not evenly divisible.
	int Z = ZConvolutional;
	int Ci = (Xi+Z-1)/Z;	// number of coded blocks. Ci = ceil(Xi/Z);
	int Ki = (Ci==0) ? 0 : (Xi+Ci-1)/Ci;	// number of bits per block, including filler bits. Ki = ceil(Xi/Ci)
	// If Ci=3, Xi=3,Ki=1 Xi=4,Ki=2
	if (fillBits) *fillBits = Ci * Ki - Xi;		// number of filler bits, unused here.
	if (codeBkSz) *codeBkSz = Ki;
	return Ci*(2*Ki+16);	// total number of bits, including filler bits.
}

// unsigned TurboEncodedSize(unsigned Ki) { return 3*Ki+12; }    // Turbo
unsigned RrcDefs::TurboEncodedSize(unsigned Xi, unsigned *codeBkSz, unsigned *fillBits) 
{
	// From 24.212 4.2.2.2: code block segmentation.
	// The Xi may need to be encoded in several separate blocks, which may
	// require filler bits if Xi is not evenly divisible. 
	int Z = ZTurbo;
	int Ci = (Xi+Z-1)/Z;    // number of coded blocks. Ci = ceil(Xi/Z);
	int Ki = (Ci==0) ? 0 : (Xi+Ci-1)/Ci;  // number of bits per block, including filler bits. Ki = ceil(Xi/Ci)
	// If Ci=3, Xi=3,Ki=1 Xi=4,Ki=2
	if (fillBits) *fillBits = Ci * Ki - Xi;             // number of filler bits, unused here.
	if (codeBkSz) *codeBkSz = Ki;		// Size of each coded block.
	return Ci*(3*Ki+12);    // total number of bits, including filler bits.
}


};	// namespace UMTS

// These routines are the interface between the SGSN and RRC:
namespace SGSN {
	MSUEAdapter *SgsnAdapter::findMs(uint32_t urnti) {
		return UMTS::gRrc.findUeByUrnti(urnti);
	}

	bool SgsnAdapter::isUmts() {
		return true;
	}


	// This allocates the RB and sends the message to the UE then returns.
	// We wont know if the UE gets the message until it replies.
	RabStatus SgsnAdapter::allocateRabForPdp(uint32_t msHandle,int rbid, ByteVector &qos) {
		return UMTS::rrcAllocateRabForPdp(msHandle,rbid,qos);
	}

	void SgsnAdapter::startIntegrityProtection(uint32_t urnti, string Kcs) {
		UMTS::UEInfo *uep = UMTS::gRrc.findUeByUrnti(urnti);
		uep->integrity.setKcs(Kcs);
		UMTS::sendSecurityModeCommand(uep);
	}
};
