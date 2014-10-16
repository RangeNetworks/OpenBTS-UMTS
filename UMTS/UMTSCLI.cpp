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

#include "UMTSConfig.h"
#include "UMTSTransfer.h"
#include "URRCTrCh.h"
#include "URRCRB.h"
#include "MACEngine.h"
#include "URRCMessages.h"
#include "URLC.h"
#include "URRC.h"
#include "GPRSL3Messages.h"	// For SmQoS
#include <stdlib.h>	// for rand

#include "asn_system.h"
namespace ASN {
//#include "BIT_STRING.h"
#include "UL-CCCH-Message.h"
#include "DL-CCCH-Message.h"
#include "UL-DCCH-Message.h"
#include "DL-DCCH-Message.h"
#include "InitialUE-Identity.h"

//#include "asn_SEQUENCE_OF.h"
//#include "RRCConnectionRequest.h"
//#include "RRCConnectionSetup.h"
};
extern FILE *gLogToFile;

namespace UMTS {
void handleRrcConnectionRequest(ASN::RRCConnectionRequest_t *msg);	// This kinda sucks.

// Return rand in the range 0..maxval inclusive.
static int rangerand(int minval, int maxval, unsigned *pseed)
{
	int range = maxval - minval;
	// Note: Should be /(RAND_MAX+1) but that overflows, but we will post-bound.
	double randfrac = ((double)rand_r(pseed) / RAND_MAX); // range 0..1
	int result = minval + (int) round(randfrac * range);
	return RN_BOUND(result,minval,maxval);	// Double check that it is in range.
}

extern int gFecTestMode;

class MacTester : public MacEngine
{
	void writeLowSide(const TransportBlock&tb) {
		printf("Received uplink TB size %d\n",tb.size());
		std::cout <<"tb="<<(ByteVector)tb;
	}

	void macService(int fn) {}
};

void testCreateFakeUE_Identity(ASN::InitialUE_Identity_t *ueIdentity)
{
	// I dont think the UE will send an IMSI right off, but it was the easiest encoding.
	memset(ueIdentity,0,sizeof(ASN::InitialUE_Identity_t));
	ueIdentity->present = ASN::InitialUE_Identity_PR_imsi;
	setASN1SeqOfDigits((void*)&ueIdentity->choice.imsi.list,"1234567890"); 
}

// This is an uplink message, so we only ever create this message for testing.
static void testCreateRRCConnectionRequest(ASN::RRCConnectionRequest_t *ccrmsg)
{
	memset(ccrmsg,0,sizeof(ASN::RRCConnectionRequest_t));
	// The only part of this message we need to fake is the UE id.
	testCreateFakeUE_Identity(&ccrmsg->initialUE_Identity);

	// Here is some other goo we dont care about but that has to be set to encode it:
	asn_long2INTEGER(&ccrmsg->establishmentCause,ASN::EstablishmentCause_originatingConversationalCall);
	asn_long2INTEGER(&ccrmsg->protocolErrorIndicator,ASN::ProtocolErrorIndicator_noError);
}

static UEInfo *testCreateFakeUe()
{
	// Test Radio Bearer Setup message stand-alone.
	//ASN::InitialUE_Identity_t imsi;
	//testCreateFakeUE_Identity(&imsi);
	ByteVector fakeimsi=ByteVector("1234567890");
	AsnUeId imsiId(fakeimsi);
	return new UEInfo(&imsiId);
}

static const unsigned sMaxTestVectors = 100;
static ByteVector *svectors[sMaxTestVectors];
static unsigned sTestRecvVectorNum;
static unsigned sTestFailCnt;
static unsigned sseed;
static unsigned sNumTestVectors;

// Receives vectors from recv2 during rlc test.
void testRlcRecv(URlcUpSdu &sdu, RbId rbid)
{
	assert(rbid == 1 || rbid == 2);
	if (rbid == 1) {
		// No data vectors are supposed to pop out of recv1.
		// It should only be getting control pdus.
		printf("ERROR: recv1 got:%s\n",sdu.hexstr().c_str());
	}
	if (sTestRecvVectorNum >= sNumTestVectors) {
		assert(0);
	}
	assert(svectors[sTestRecvVectorNum]);
	ByteVector *expect = svectors[sTestRecvVectorNum];
	if (sdu != *expect) {
		sTestFailCnt++;
		printf("RLC test vector %d did not match: expected=%s got=%s\n",
			sTestRecvVectorNum,sdu.hexstr().c_str(),expect->hexstr().c_str());
	} else {
		printf("testRlcRecv: test vector %d matched\n",sTestRecvVectorNum);
		//printf("testRlcRecv: test vector %d matched %s=%s\n",
			//sTestRecvVectorNum,sdu.hexstr().c_str(),expect->hexstr().c_str());
	}
	sTestRecvVectorNum++;
}


// For testing, copy data from the trans to the recv.
// Return how many.
static unsigned rlcTransfer(URlcTrans *trans, URlcRecv *recv, int percentloss,int dir)
{
	printf("rlcTransfer dir=%d\n",dir);
	ByteVector *pdu;
	unsigned pducnt = 0;
	while ((pdu = trans->rlcReadLowSide())) {
		pducnt++;
		if (percentloss && rangerand(1,100,&sseed) < percentloss) {
			//URlcPdu *updu = dynamic_cast<URlcPdu*>(pdu);
			//printf("Tossing pdu number %d %s\n",pducnt,updu->ByteVector::str().c_str());
			continue;
		}
		// Turn the bytevector into a bitvector:
		BitVector bits(pdu->sizeBits());
		bits.unpack(pdu->begin());
		delete pdu;
		recv->rlcWriteLowSide(bits);
	}
	return pducnt;
}

static unsigned rlcRun(URlcPair *pair1,URlcPair *pair2,int percentloss, bool statuslossless)
{
	int pducnt = 0;
	URlcTrans *trans1 = pair1->mDown, *trans2 = pair2->mDown;
	URlcRecv *recv1 = pair1->mUp, *recv2 = pair2->mUp;
	while (1) {
		int cnt = rlcTransfer(trans1,recv2,percentloss,1);
		if (statuslossless) percentloss = 0;
		cnt += rlcTransfer(trans2,recv1,percentloss,2);
		if (cnt == 0) break;
		pducnt += cnt;
	}
	return pducnt;
}

//static void testRlc(const char*subcmd, int argc, char **argv)
int rlcTest(int argc, char** argv, ostream& os)
{
	gLogToConsole = true;
	if (gLogToFile == NULL) { gLogToFile = fopen("debug.log","w"); }

	int argi = 1;   // The number of arguments consumed so far; argv[0] was name of cmd
	URlcMode mode = URlcModeAm; //URlcModeTm, URlcModeUm,

	int percentloss = 0;
	int reset1 = 0;
	int reset2 = 0;
	bool ps = 0;
	sseed = 1;
	sNumTestVectors = sMaxTestVectors;
	bool statuslossless = 0; // Let status pdus go through lossless.

	while (argi < argc) {
		if (0 == strcmp(argv[argi],"-loss") && argi+1<argc) {
			percentloss = atoi(argv[argi+1]);
			argi += 2;
		} else if (0 == strcmp(argv[argi],"-um")) {
			mode = URlcModeUm;
			argi++;
		} else if (0 == strcmp(argv[argi],"-tm")) {
			mode = URlcModeTm;
			argi++;
		} else if (0 == strcmp(argv[argi],"-am")) {
			mode = URlcModeAm;
			argi++;
		} else if (0 == strcmp(argv[argi],"-ps")) {
			ps = 1;
			argi++;
		} else if (0 == strcmp(argv[argi],"-s")) {
			statuslossless = 1;
			argi++;
		} else if (0 == strcmp(argv[argi],"-d")) {
			rrcDebugLevel = 0xffff;		// Debugging this now...
			argi++;
		} else if (0 == strcmp(argv[argi],"-n") && argi+1<argc) {
			sNumTestVectors = atoi(argv[argi+1]);
			argi += 2;
		} else if (0 == strcmp(argv[argi],"-seed") && argi+1<argc) {
			sseed = atoi(argv[argi+1]);
			argi += 2;
		} else if (0 == strcmp(argv[argi],"-reset1") && argi+1<argc) {
			// Start a reset at the indicated sdu number.
			reset1 = atoi(argv[argi+1]);
			argi += 2;
		} else if (0 == strcmp(argv[argi],"-reset2") && argi+1<argc) {
			// Start a reset at the indicated sdu number.
			reset2 = atoi(argv[argi+1]);
			argi += 2;
		} else {
			printf("unrecognized: %s\n",argv[argi]);
			help:
			printf("rlctest -am|-tm|-um -s -d -ps -n <numvectors> -loss <percentloss> -seed <randomseed> -reset[12] <pdunum>\n");
			printf("note: -ps = packet-switched-config -d = debug; -s = lossless transmission for status\n");
			return 0;
		}
	}

	if (sNumTestVectors > sMaxTestVectors) {
		printf("too many test vectors\n");
		goto help;
	}

	if (percentloss < 0 || percentloss > 90) {
		printf("rrctest invalid percentloss arg\n");
		goto help;
	}

	URlcPair *pair1, *pair2;
	{
		UEInfo *uep = testCreateFakeUe();
		RBInfo rb;
		if (mode == URlcModeAm) {	// strstr(subcmd,"am"))
			// I tried using an existing RLC but it doesnt work because
			// the MAC service loop is running on it, so manufacture our own:
			if (ps) {
				rb.defaultConfigRlcAmPs();
			} else {
				rb.defaultConfigSrbRlcAm();
			}
		} else if (mode == URlcModeUm) { 	//strstr(subcmd,"um"))
			rb.defaultConfig0CFRb(1);		// This is an RLC-UM configuration
		} else {	// mode TM
			rb.ul_RLC_Mode(URlcModeTm);
			rb.dl_RLC_Mode(URlcModeTm);
		}
		//else {
			//printf("Invalid rrctest rlc subcommand\n");
			//goto help;
		//}
		RrcTfs *dltfs = gRrcDcchConfig->getDlTfs(0);	// this will do
		rb.TimerPoll(20);	// Speed this way up for testing.
		rb.rb_Identity(1);
		pair1 = new URlcPair(&rb,dltfs,uep,0);
		rb.rb_Identity(2);
		pair2 = new URlcPair(&rb,dltfs,uep,0);
	}

	// Test RLCs are hooked up like this:
	// generated vectors -> trans1 (AMT1) -> code below -> recv2 (AMR2) -> testRlcRecv()
	// recv1 (AMR1) <- code below <- trans2 (AMT2),
	// Since we are sending data only one way, the latter will only
	// be control pdus generated inside trans2 and only for AM mode.

	URlcTrans *trans1 = pair1->mDown, *trans2 = pair2->mDown;
	URlcRecv *recv1 = pair1->mUp, *recv2 = pair2->mUp;

	// Data from the high side of the receiver goes here.
	// see URlcRecv::rlcSendHighSide()
	recv1->rlcSetHighSide(testRlcRecv);
	recv2->rlcSetHighSide(testRlcRecv);
	sTestRecvVectorNum = 0;
	sTestFailCnt = 0;

	// We want to randomize: vector content, size, and how many incoming vectors per TTI.
	int pducnt = 0;
	//int loss;

	for (unsigned n = 0; n < sNumTestVectors; n++) {
		// Generate a random sized test vector.
		int len = rangerand(2,100,&sseed);	// 2..100 bytes.
		svectors[n] = new ByteVector(len);
		svectors[n]->setField(0,n,16);
		for (int j = 2; j < len; j++) {
			svectors[n]->setByte(j,j); // Fill the rest of the test vec with numbers.
		}
		// Write the test vec to the RLC-AM transmitter.
		trans1->rlcWriteHighSide(*svectors[n],false,0,string("testvec"));

		// Pull data out of either transmitter low side and send back to recv.
		// Keep doing this until they stop talking to each other.
		pducnt += rlcRun(pair1,pair2,percentloss,statuslossless);

		if (reset1 && (int)n == reset1) { trans1->triggerReset(); }
		if (reset2 && (int)n == reset2) { trans2->triggerReset(); }
	}
		/***
		loss = percentloss;
		while (1) {
			int cnt = rlcTransfer(trans1,recv2,loss,1);
			if (statuslossless) loss = 0;
			cnt += rlcTransfer(trans2,recv1,loss,2);
			if (cnt == 0) break;
			pducnt += cnt;
		}
		***/
	// After the last test, if the PDU carrying the Poll bit was lost,
	// it should be resent when the Timer_Poll expires, which is 1 second.
	int activity;
	do {
		printf("Sleeping...\n");
		sleepf(0.03);
		activity = rlcRun(pair1,pair2,statuslossless?0:percentloss,statuslossless);
		pducnt += activity;
		// The resets have long timers and we have to wait for them...
		if (activity == 0 && (reset1 | reset2)) {
			printf("Sleeping 0.4...\n");
			sleepf(0.4);
			activity = rlcRun(pair1,pair2,statuslossless?0:percentloss,statuslossless);
		}
	} while (activity);

	/***
	loss = statuslossless ? 0 : percentloss;
	int activity;
	do {
		activity = 0;
		printf("Sleeping...\n");
		sleep(2);
		while (1) {
			int cnt = rlcTransfer(trans1,recv2,loss,1) + rlcTransfer(trans2,recv1,loss,2);
			if (cnt == 0) break;
			activity=1;
		}
	} while (activity);
	***/

	printf("testRlc: received %d sdus, %d pdus , expected=%d failed=%d\n",
		sTestRecvVectorNum, pducnt, sNumTestVectors, sTestFailCnt);

	os <<"\nTrans1:"; trans1->text(os);
	os <<"\nRecv1:"; recv1->text(os);
	os <<"\nTrans2:"; trans2->text(os);
	os <<"\nRecv2:"; recv2->text(os);
	os <<"\n";
	return 0;
}


// This can be called by the OpenBTS-UMTS command line interface via the CLI module.
int rrcTest(int argc, char** argv, ostream& os)
{
	if (argc <= 1) { return 1; }

	//gLogToConsole = true;
	//if (gLogToFile == NULL) { gLogToFile = fopen("debug.log","w"); }

	int argi = 1;   // The number of arguments consumed so far; argv[0] was name of cmd
	char *subcmd = argv[argi++];
	char *arg1 = (argc > argi) ? argv[argi] : NULL;
	if (0==strcmp(subcmd,"rbrelease")) {
		UEInfo *uep = testCreateFakeUe();
		DCHFEC *dch = gChannelTree.chChooseByBW(12000);
		RrcMasterChConfig *newConfig = &uep->mUeDchConfig;
		bool useTurbo = true;
		RbId rbid = 5;
		newConfig->rrcConfigDchPS(dch, rbid, useTurbo);
		sendRadioBearerRelease(uep, 1<<rbid, false);
		sendRadioBearerRelease(uep, 1<<rbid, true);
	} else if (0==strcmp(subcmd,"cc")) {
		extern void testCCProgramming();
		testCCProgramming();
	} else if (0==strcmp(subcmd,"level")) {
		if (!arg1) { printf("missing argument\n"); return 0; }
		rrcDebugLevel = strtol(arg1,NULL,0);	// strtol allows hex

	} else if (0==strncmp(subcmd,"fec",3)) {
#if USE_OLD_FEC
		// Test the L1FEC encoder/decoder.
		int testnum = 1;
		if (arg1) { testnum = atoi(arg1); }
		printf("rrctest fec %d\n",testnum);
		gFecTestMode = testnum;	// bursts sent on FACH will come back on RACH.
		MacTester macTester;
		RACHFEC *originalRach = gNodeB.mRachFec;
		if (0==strcmp(subcmd,"fec2")) {
			// The RACH and FACH are not the same size any more, so create a temp rach.
			// Make a dummy RACH to match the FACH: SF=64,PB=12,TB=512,TTI=10ms.
			gNodeB.mRachFec = new RACHFEC(64,1,12,512,TTI10ms);
		}
		macTester.macSetDownstream(gNodeB.mFachFec);
		MacEngine *saveme = gNodeB.mRachFec->l1SetUpstream(&macTester,true);
		//TransportBlock tb(gNodeB.mRachFec->decoder()->trBkSz());
		TransportBlock tb(gNodeB.mFachFec->encoder()->trBkSz());
		printf("Sending %d bits\n",tb.size());
		tb.zero();
		gNodeB.mFachFec->l1WriteHighSide(tb);
		tb.fillField(0,0x1234,16);
		gNodeB.mFachFec->l1WriteHighSide(tb);
		// Put it back the way we found it:
		gNodeB.mRachFec = originalRach;
		gNodeB.mRachFec->l1SetUpstream(saveme,true);
#endif
	} else if (0==strcmp(subcmd,"2")) {
		// Test the RRC Connection Request/Setup, which will create a UEInfo and send
		// an RRC Connection Setup message down it on SRB0, where it disappears because
		// no one is listening, but you can see it get fragmented by the CCCH RLC,
		// have the MAC header attached, and go through the encoder.
		gFecTestMode = 1;	// bursts sent on FACH will come back on RACH.
		ASN::RRCConnectionRequest_t ccrmsg;
		testCreateRRCConnectionRequest(&ccrmsg);
		handleRrcConnectionRequest(&ccrmsg);
		// Do it again, to see the sequence number stays the same.
		handleRrcConnectionRequest(&ccrmsg);
	} else if (0==strcmp(subcmd,"3")) {
		// Test the RRC Connection Request/Setup more thoroughly,
		// by encoding the RRC Connection Request as an uplink message,
		// and sending it all the way down through L1 and back, so that
		// it appears as though it had arrived on the air, and will
		// cause an RRC Connection Setup to be sent.
		gFecTestMode = 1;	// FEC encoder bursts are reflected back on decoder channel.

		ASN::UL_CCCH_Message ulmsg;
		memset(&ulmsg,0,sizeof(ulmsg));
		ulmsg.message.present = ASN::UL_CCCH_MessageType_PR_rrcConnectionRequest;
		testCreateRRCConnectionRequest(&ulmsg.message.choice.rrcConnectionRequest);

		char errbuf[200];
		size_t errlen = 200;
		errbuf[0] = 0;
		int stat = asn_check_constraints(&ASN::asn_DEF_UL_CCCH_Message,&ulmsg,errbuf,&errlen);
		printf("asn_check_constraints returned %d %s\n",stat,errbuf);

		// Try sending the ccrmsg through the fecs.  We cannot send it through the normal
		// downlink RLC because SRB0 is assymetric: UM down and TM up.
		// Send it to MAC directly.
		// This pdu is not nearly the correct size, but mac should zero pad it to TB size.
		ByteVector pdu(100);
		const char *help = "RRCConnectionRequest";
		if (!uperEncodeToBV(&ASN::asn_DEF_UL_CCCH_Message, &ulmsg,pdu,help)) {
			printf("failed to encode %s\n",help);
			return 0;
		}
		printf("==== RRCConnectionRequest sizebits=%d\n",pdu.sizeBits());

		// Try decoding immediately:
		ASN::UL_CCCH_Message *msg1 = (ASN::UL_CCCH_Message*)
			uperDecodeFromByteV(&ASN::asn_DEF_UL_CCCH_Message, pdu);
		printf("==== uperDecode returned: %p\n",msg1);

		if (1) {
			// Try sending ASN directly to rrcRecvCcchMessage.
			BitVector foo(pdu.sizeBits());	// This would not work if it were not divisible by 8.
			foo.unpack(pdu.begin());
			rrcRecvCcchMessage(foo,0);
			printf("FINISHED\n");
			return 0;
		}

		// We have to format this as an UPLINK block, so that it will be recognized by MAC
		// as such on the return trip.
		//MaccTbDlCcch tb(gNodeB.mFachFec->l1GetDlTrBkSz(),&pdu);		// Will go down on fach.
		MacTbDl tb(gNodeB.mFachFec->l1GetDlTrBkSz());
		tb.fillField(0,0,2);	// On RACH, this is the TCTF field for a CCCH uplink block.
		tb.segment(2,pdu.sizeBits()).unpack(pdu.begin());	// add the pdu data.

		// Find the mac and send it off.
#if USE_OLD_FEC
		MacEngine *mactmp = gNodeB.mRachFec->decoder()->mUpstream;	// Then come up on rach.
		MaccSimple *macc = dynamic_cast<MaccSimple*>(mactmp);
		macc->sendDownstreamTb(tb);
#else
		printf("This test uminplemented\n");
#endif
	} else if (0==strcmp(subcmd,"4")) {
		rrcDebugLevel = 0xffff;		// Debugging this now...
		UEInfo *uep = testCreateFakeUe();
		// We have to start with the UE in CELL_FACH state or
		// assertions will fail about moving from CELL_FACH to CELL_DCH state.
		// May need to hook up the FACH rlcs too?
		uep->ueConnectRlc(gRrcDcchConfig,stCELL_FACH);
		uep->ueSetState(stCELL_FACH);

		// Manufacture a Quality-of-Service.  Currently we only use two fields.
		SGSN::SmQoS qos(12);
		qos.setMaxBitRate(16*8,0);		// in kbits/sec
		qos.setPeakThroughput(16000);	// this comes in bytes/sec,

		SGSN::RabStatus result;
		//result = rrcAllocateRabForPdp(uep->mURNTI,5,qos);
		result = SGSN::SgsnAdapter::allocateRabForPdp(uep->mURNTI,5,qos);
		printf("result=%d failcode=%d\n",result.mStatus,result.mFailCode); 
		result.text(std::cout);

		/***
		// Allocate a physical channel.
		DCHFEC *dch = gChannelTree.chChooseBySF(256);
		if (dch == NULL) {
			printf("oops: NULL dch\n");
			return 0;
		}
		// TODO: We need a config...
		sendRadioBearerSetup(uep, masterConfig, dch,true);
		***/
	} else if (0==strcmp(subcmd,"5")) {
		rrcDebugLevel = 0xffff;		// Debugging this now...
		UEInfo *uep = testCreateFakeUe();
		uep->ueConnectRlc(gRrcDcchConfig,stCELL_FACH);
		uep->ueSetState(stCELL_FACH);
		sendRrcConnectionRelease(uep);
	} else if (0==strcmp(subcmd,"6")) {
		rrcDebugLevel = 0xffff;		// Debugging this now...
		UEInfo *uep = testCreateFakeUe();
		uep->ueConnectRlc(gRrcDcchConfig,stCELL_FACH);
		uep->ueSetState(stCELL_FACH);
		sendRadioBearerRelease(uep,1<<5,1);
		sendCellUpdateConfirm(uep);
	} else {
		os << "invalid sub-command\n";
		return 2;	// bad command
	}

	return 0;	// aka SUCCESS
}

};
