/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2011-2021 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * See the COPYING and NOTICE files in the current or main directory for
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include <iostream>
#include <fstream>

#include <Configuration.h>
// Load configuration from a file.
ConfigurationTable gConfig("/etc/OpenBTS/OpenBTS-UMTS.db","OpenBTS-UMTS", getConfigurationKeys());

#include <TRXManager.h>
#include <UMTSConfig.h>
#include <SIPInterface.h>
#include <TransactionTable.h>
#include <ControlCommon.h>

#include <Logger.h>
#include <CLI.h>
#include <NodeManager.h>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#ifdef HAVE_LIBREADLINE // [
//#  include <stdio.h>
#  include <readline/readline.h>
#  include <readline/history.h>
#endif // HAVE_LIBREADLINE ]

using namespace std;
using namespace UMTS;

namespace UMTS { void testCCProgramming(); }

const char* gDateTime = __DATE__ " " __TIME__;


// All of the other globals that rely on the global configuration file need to
// be declared here.


// Configure the BTS object based on the config file.
// So don't create this until AFTER loading the config file.
UMTSConfig gNodeB;

// Our interface to the software-defined radio.
TransceiverManager gTRX;	// init below with TransceiverManagerInit()

// The TMSI Table.
Control::TMSITable gTMSITable(gConfig.getStr("Control.Reporting.TMSITable").c_str());

// The transaction table.
Control::TransactionTable gTransactionTable(gConfig.getStr("Control.Reporting.TransactionTable").c_str());

// The global SIPInterface object.
SIP::SIPInterface gSIPInterface;

/** The remote node manager. */ 
NodeManager gNodeManager;

/** Define a function to call any time the configuration database changes. */
void purgeConfig(void*,int,char const*, char const*, sqlite3_int64)
{
	LOG(INFO) << "purging configuration cache";
	gConfig.purge();
	// (pat) This is where the initial regenerateBeacon is called from,
	// not from UMTSConfig::start() as you might naively expect.
	// But I'm leaving both calls to regenerateBeacon intact in case someone changes
	// the initialization order in here, because it doesnt hurt to do it twice.
	gNodeB.regenerateBeacon();
}



const char* transceiverPath = "./transceiver";

pid_t gTransceiverPid = 0;

void startTransceiver()
{
	// Start the transceiver binary, if the path is defined.
	// If the path is not defined, the transceiver must be started by some other process.
	char TRXnumARFCN[4];
	sprintf(TRXnumARFCN,"%1d",(int)gConfig.getNum("UMTS.Radio.ARFCNs"));
	LOG(NOTICE) << "starting transceiver " << transceiverPath << " " << TRXnumARFCN;
	gTransceiverPid = vfork();
	LOG_ASSERT(gTransceiverPid>=0);
	if (gTransceiverPid==0) {
		// Pid==0 means this is the process that starts the transceiver.
		execlp(transceiverPath,transceiverPath,TRXnumARFCN,NULL);
		LOG(EMERG) << "cannot find " << transceiverPath;
		_exit(1);
	}
}



int main(int argc, char *argv[])
{
	bool testmode = false;

	// TODO: Properly parse and handle any arguments
	if (argc > 1) {
		for (int argi = 1; argi < argc; argi++) {		// Skip argv[0] which is the program name.
			if (!strcmp(argv[argi], "--version") ||
			    !strcmp(argv[argi], "-v")) {
				cout << gVersionString << endl;
				return 0;
			}
			if (!strcmp(argv[argi], "--gensql")) {
				cout << gConfig.getDefaultSQL(string(argv[0]), gVersionString) << endl;
				return 0;
			}
			if (!strcmp(argv[argi], "--gentex")) {
				cout << gConfig.getTeX(string(argv[0]), gVersionString) << endl;
				return 0;
			}
			// run without transceiver for testing.
			if (!strcmp(argv[argi], "-t")) {
				testmode = true;
			}
		}
	}

	//createStats();

	//gConfig.setCrossCheckHook(&configurationCrossCheck);

	//gReports.incr("OpenBTS-UMTS.Starts");

	//gNeighborTable.NeighborTableInit(
	//gConfig.getStr("Peering.NeighborTable.Path").c_str());

	int sock = socket(AF_UNIX,SOCK_DGRAM,0);
	if (sock<0) {
		perror("creating CLI datagram socket");
		LOG(ALERT) << "cannot create socket for CLI";
		//gReports.incr("OpenBTS.Exit.CLI.Socket");
		exit(1);
	}

	try {
	COUT("\n\n" << gOpenWelcome << "\n");

	gTRX.TransceiverManagerInit(gConfig.getNum("UMTS.Radio.ARFCNs"), gConfig.getStr("TRX.IP").c_str(), gConfig.getNum("TRX.Port"));

	srandom(time(NULL));

	gConfig.setUpdateHook(purgeConfig);
	gLogInit("openbts-umts",gConfig.getStr("Log.Level").c_str(),LOG_LOCAL7);
	LOG(ALERT) << "OpenBTS-UMTS (re)starting, ver " << VERSION << " build date " << __DATE__;

	//verify(argv[0]);
	gParser.addCommands();

	COUT("\nStarting the system...");
        ARFCNManager* downstreamRadio = gTRX.ARFCN(0);
	gNodeB.init(downstreamRadio);	// (pat) Nicer to test the beacon config before starting the transceiver.

	// (pat) DEBUG: Run these tests on startup.
	if (testmode) { UMTS::testCCProgramming(); return 0; }

	COUT("Starting the transceiver..." << endl);	// (pat) 11-9-2012 Taking this out causes OpenBTS-UMTS to malfunction!  Intermittently.
	//LOG(INFO) << "Starting the transceiver...";
	startTransceiver();	// (pat) This is now a no-op because transceiver is built-in.
	sleep(5);
	// Start the SIP interface.
	LOG(INFO) << "Starting the SIP interface...";
	gSIPInterface.start();


	//
	// Configure the radio.
	//
        Thread DCCHControlThread;
        DCCHControlThread.start((void*(*)(void*))Control::DCCHDispatcher,NULL);

	// Set up the interface to the radio.
	// Get a handle to the C0 transceiver interface.
	ARFCNManager* C0radio = gTRX.ARFCN(0);

	// Tuning.
	// Make sure its off for tuning.
	C0radio->powerOff();
	// Get the ARFCN list.
	unsigned C0 = gConfig.getNum("UMTS.Radio.C0");
	LOG(INFO) << "tuning TRX to UARFCN " << C0;
    ARFCNManager* radio = gTRX.ARFCN(0);
	radio->tune(C0);

	// Sleep long enough for the USRP to bootload.
	LOG(INFO) << "Starting the TRX ...";
	gTRX.trxStart();

	// Set maximum expected delay spread.
	//C0radio->setMaxDelay(gConfig.getNum("UMTS.Radio.MaxExpectedDelaySpread"));

	// Set Receiver Gain
	C0radio->setRxGain(gConfig.getNum("UMTS.Radio.RxGain"));

	// Turn on and power up.
	// get the band and set the RF filter muxes 
	unsigned gsm_band;
	unsigned band = gConfig.getNum("UMTS.Radio.Band");
	switch (band) {
	case 900:
	  gsm_band = 10;
	  break;
	case 850:
	  gsm_band = 8;
	  break;
	case 1800:
	  gsm_band = 13;
	  break;
	case 1900:
	  gsm_band = 14;
	  break;
	default:
	  gsm_band = 10;
	}


        C0radio->powerOn();
	C0radio->setPower(gConfig.getNum("UMTS.Radio.PowerManager.MinAttenDB"));
	// Turn on and power up.

	//
	// Create the baseic channel set.
	//

	// Set up the pager.
	// Set up paging channels.
	// HACK -- For now, use a single paging channel, since paging groups are broken.
	// gNodeB.addPCH(&CCCH2);

	// Be sure we are not over-reserving.
	//LOG_ASSERT(gConfig.getNum("UMTS.CCCH.PCH.Reserve")<(int)gNodeB.numAGCHs());

	// XXX skip this test
	// C0radio->readRxPwrCoarse();

	// OK, now it is safe to start the BTS.
	LOG(INFO) << "Starting the NodeB ...";
	gNodeB.start(C0radio);

	struct sockaddr_un cmdSockName;
	cmdSockName.sun_family = AF_UNIX;
	const string sockpath = gConfig.getStr("CLI.SocketPath");
	int rc = unlink(sockpath.c_str());
	if (rc == -1) {
		// If it does not exist just move on, otherwise say something
		if (errno != ENOENT) {
			LOG(ALERT) << "Cannot delete CLI sock file, error: " << strerror(errno);
		}
	}	
	strcpy(cmdSockName.sun_path,sockpath.c_str());
	LOG(INFO) "binding CLI datagram socket at " << sockpath;
	if (bind(sock, (struct sockaddr *) &cmdSockName, sizeof(struct sockaddr_un))) {
		perror("binding name to cmd datagram socket");
		LOG(ALERT) << "cannot bind socket for CLI at " << sockpath;
		//gReports.incr("OpenBTS-UMTS.Exit.CLI.Socket");
		exit(1);
	}

	COUT("\nsystem ready\n");
	COUT("\nuse the OpenBTS-UMTSCLI utility to access CLI\n");
	LOG(INFO) << "system ready";

	gParser.startCommandLine();
	//gNodeManager.setAppLogicHandler(&nmHandler);
	gNodeManager.start(45070);

	while (1) {
		char cmdbuf[1000];
		struct sockaddr_un source;
		socklen_t sourceSize = sizeof(source);
		int nread = recvfrom(sock,cmdbuf,sizeof(cmdbuf)-1,0,(struct sockaddr*)&source,&sourceSize);
		//gReports.incr("OpenBTS-UMTS.CLI.Command");
		cmdbuf[nread]='\0';
		LOG(INFO) << "received command \"" << cmdbuf << "\" from " << source.sun_path;
		std::ostringstream sout;
		int res = gParser.process(cmdbuf,sout);
		const std::string rspString= sout.str();
		const char* rsp = rspString.c_str();
		LOG(INFO) << "sending " << strlen(rsp) << "-char result to " << source.sun_path;
		if (sendto(sock,rsp,strlen(rsp)+1,0,(struct sockaddr*)&source,sourceSize)<0) {
			LOG(ERR) << "can't send CLI response to " << source.sun_path;
			//gReports.incr("OpenBTS-UMTS.CLI.Command.ResponseFailure");
		}
		// res<0 means to exit the application
		if (res<0) break;
	}

	} // try

	catch (ConfigurationTableKeyNotFound e) {
		LOG(EMERG) << "required configuration parameter " << e.key() << " not defined, aborting";
		//gReports.incr("OpenBTS-UMTS.Exit.Error.ConfigurationParamterNotFound");
	}

	//if (gTransceiverPid) kill(gTransceiverPid, SIGKILL);
	close(sock);
}

// vim: ts=4 sw=4
