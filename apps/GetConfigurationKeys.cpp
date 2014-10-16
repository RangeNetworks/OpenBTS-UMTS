/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2011, 2012, 2013, 2014 Range Networks, Inc.
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
#include <vector>
#include <string>

#include <Configuration.h>


std::string getARFCNsString(unsigned band) {
	std::stringstream ss;
	int i;
	float downlink;
	float uplink;

	if (band == 850) {
		// 128:251 GSM850
		downlink = 869.2;
		uplink = 824.2;
		for (i = 128; i <= 251; i++) {
			ss << i << "|GSM850 #" << i << " : " << downlink << " MHz downlink / " << uplink << " MHz uplink,";
			downlink += 0.2;
			uplink += 0.2;
		}

	} else if (band == 900) {
		// 1:124 PGSM900
		downlink = 935.2;
		uplink = 890.2;
		for (i = 1; i <= 124; i++) {
			ss << i << "|PGSM900 #" << i << " : " << downlink << " MHz downlink / " << uplink << " MHz uplink,";
			downlink += 0.2;
			uplink += 0.2;
		}

		// 975:1023 EGSM900
		downlink = 1130;
		uplink = 1085;
		for (i = 975; i <= 1023; i++) {
			ss << i << "|EGSM900 #" << i << " : " << downlink << " MHz downlink / " << uplink << " MHz uplink,";
			downlink += 0.2;
			uplink += 0.2;
		}

	} else if (band == 1800) {
		// 512:885 DCS1800
		downlink = 1805.2;
		uplink = 1710.2;
		for (i = 512; i <= 885; i++) {
			ss << i << "|DCS1800 #" << i << " : " << downlink << " MHz downlink / " << uplink << " MHz uplink,";
			downlink += 0.2;
			uplink += 0.2;
		}

	} else if (band == 1900) {
		// 512:810 PCS1900
		downlink = 1930.2;
		uplink = 1850.2;
		for (i = 512; i <= 810; i++) {
			ss << i << "|PCS1900 #" << i << " : " << downlink << " MHz downlink / " << uplink << " MHz uplink,";
			downlink += 0.2;
			uplink += 0.2;
		}
	}

	std::string tmp = ss.str();
	return tmp.substr(0, tmp.size()-1);
}

ConfigurationKeyMap getConfigurationKeys()
{

	//VALIDVALUES NOTATION:
	// * A:B : range from A to B in steps of 1
	// * A:B(C) : range from A to B in steps of C
	// * A:B,D:E : range from A to B and from D to E
	// * A,B : multiple choices of value A and B
	// * X|A,Y|B : multiple choices of string "A" with value X and string "B" with value Y
	// * ^REGEX$ : string must match regular expression

	/*
	TODO : double check: sometimes symbol periods == 0.55km and other times 1.1km?
	TODO : .defines() vs sql audit
	TODO : Optional vs *_OPT audit
	TODO : configGetNumQ()
	TODO : description contains "if specified" == key is optional
	TODO : These things exist but aren't defined as keys.
			- GSM.SI3RO
			- GSM.SI3RO.CBQ
			- GSM.SI3RO.CRO
			- GSM.SI3RO.TEMPORARY_OFFSET
			- GSM.SI3RO.PENALTY_TIME
	TODO : BOOLEAN from isDefined() to isBool()
	*/

	ConfigurationKeyMap map;
	ConfigurationKey *tmp;

	tmp = new ConfigurationKey("CLI.SocketPath","/var/run/OpenBTS-UMTS-command",
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::FILEPATH,
		"",
		false,
		"Path for Unix domain datagram socket used for the OpenBTS-UMTS console interface."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.AttachDetach","1",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::BOOLEAN,// audited
		"",
		false,
		"Use attach/detach procedure.  "
			"This will make initial LUR more prompt.  "
			"It will also cause an un-regstration if the handset powers off and really heavy LUR loads in areas with spotty coverage."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.FailedRegistration.Message","Your handset is not provisioned for this network. ",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::STRING_OPT,
		"^[ -~]+$",
		false,
		"Send this text message, followed by the IMSI, to unprovisioned handsets that are denied registration."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.FailedRegistration.ShortCode","1000",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::STRING,
		"^[0-9]+$",
		false,
		"The return address for the failed registration message."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.NormalRegistration.Message","",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::STRING_OPT,
		"^[ -~]+$",
		false,
		"The text message (followed by the IMSI) to be sent to provisioned handsets when they attach on Um.  "
			"By default, no message is sent.  "
			"To have a message sent, specify one.  "
			"To stop sending messages again, execute \"unconfig Control.LUR.NormalRegistration.Message\"."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.NormalRegistration.ShortCode","0000",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::STRING,
		"^[0-9]+$",
		false,
		"The return address for the normal registration message."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.OpenRegistration","",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::REGEX_OPT,
		"",
		false,
		"This is value is a regular expression.  "
			"Any handset with an IMSI matching the regular expression is allowed to register, even if it is not provisioned.  "
			"By default, this feature is disabled.  "
			"To enable open registration, specify a regular expression e.g. ^460 (which matches any IMSI starting with 460, the MCC for China).  "
			"To disable open registration again, execute \"unconfig Control.LUR.OpenRegistration\"."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.OpenRegistration.Message","Welcome to the test network.  Your IMSI is ",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::STRING,
		"^[ -~]+$",
		false,
		"Send this text message, followed by the IMSI, to unprovisioned handsets when they attach on Um due to open registration."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.OpenRegistration.ShortCode","101",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::STRING,
		"^[0-9]+$",
		false,
		"The return address for the open registration message."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.QueryClassmark","0",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::BOOLEAN,// audited
		"",
		false,
		"Query every MS for classmark during LUR."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.QueryIMEI","0",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::BOOLEAN,// audited
		"",
		false,
		"Query every MS for IMEI during LUR."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.SendTMSIs","0",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::BOOLEAN,// audited
		"",
		false,
		"Send new TMSI assignments to handsets that are allowed to attach."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.LUR.UnprovisionedRejectCause","0x04",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::CHOICE,
		"0x02|IMSI unknown in HLR,"
			//"0x03|Illegal MS,"
			"0x04|IMSI unknown in VLR,"
			"0x05|IMEI not accepted,"
			//"0x06|Illegal ME,"
			"0x0B|PLMN not allowed,"
			"0x0C|Location Area not allowed,"
			"0x0D|Roaming not allowed in this location area,"
			"0x11|Network failure,"
			"0x16|Congestion,"
			"0x20|Service option not supported,"
			"0x21|Requested service option not subscribed,"
			"0x22|Service option temporarily out of order,"
			"0x26|Call cannot be identified,"
			"0x30|Retry upon entry into a new cell,"
			"0x5F|Semantically incorrect message,"
			"0x60|Invalid mandatory information,"
			"0x61|Message type non-existent or not implemented,"
			"0x62|Message type not compatible with the protocol state,"
			"0x63|Information element non-existent or not implemented,"
			"0x64|Conditional IE error,"
			"0x65|Message not compatible with the protocol state,"
			"0x6F|Unspecified protocol error",
		false,
		"Reject cause for location updating failures for unprovisioned phones.  "
			"Reject causes come from GSM 04.08 10.5.3.6.  "
			"Reject cause 0x04, IMSI not in VLR, is usually the right one."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.Reporting.TMSITable","/var/run/OpenBTS-UMTS-TMSITable.db",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::FILEPATH,
		"",
		true,
		"File path for TMSITable database."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("Control.Reporting.TransactionTable","/var/run/OpenBTS-UMTS-TransactionTable.db",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::FILEPATH,
		"",
		true,
		"File path for transaction table database."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// TODO : this setting doesn't exist in C3.1, SMSCB incomplete: INSERT OR IGNORE INTO "CONFIG" VALUES('Control.SMSCB','1',0,1,'If not NULL, enable SMSCB.  If defined, ControlSMSCB.Table must also be defined.');
	// TODO : no reference to this table yet, SMSCB incomplete: INSERT OR IGNORE INTO "CONFIG" VALUES('Control.SMSCB.Table','/var/run/OpenBTS-UMTS-SMSCB.db',1,1,'File path for SMSCB scheduling database.  Static.');

	tmp = new ConfigurationKey("Control.VEA","0",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::BOOLEAN,// audited
		"",
		false,
		"Use very early assignment for speech call establishment.  "
			"See GSM 04.08 Section 7.3.2 for a detailed explanation of assignment types.  "
			"If VEA is selected, GSM.CellSelection.NECI should be set to 1.  "
			"See GSM 04.08 Sections 9.1.8 and 10.5.2.4 for an explanation of the NECI bit.  "
			"Note that some handset models exhibit bugs when VEA is used and these bugs may affect performance."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.DNS","",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::MIPADDRESS_OPT,
		"",
		true,
		"The list of DNS servers to be used by downstream clients.  "
			"By default, DNS servers are taken from the host system.  "
			"To override, specify a space-separated list of the DNS servers, in IP dotted notation, eg: 1.2.3.4 5.6.7.8.  "
			"To use the host system DNS servers again, execute \"unconfig GGSN.DNS\"."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.Firewall.Enable","1",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::CHOICE,
		"0|Disable Firewall,"
			"1|Block MS Access to OpenBTS-UMTS and Other MS,"
			"2|Block All Private IP Addresses",
		true,
		"0=no firewall; 1=block MS attempted access to OpenBTS-UMTS or other MS; 2=block all private IP addresses."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.IP.MaxPacketSize","1520",
		"bytes",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"1492:9000",// educated guess
		true,
		"Maximum size of an IP packet.  "
			"Should normally be 1520."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.IP.ReuseTimeout","180",
		"seconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"120:240",// educated guess,
		true,
		"How long IP addresses are reserved after a session ends."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.IP.TossDuplicatePackets","0",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::BOOLEAN,// audited
		"",
		true,
		"Toss duplicate TCP/IP packets to prevent unnecessary traffic on the radio."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.Logfile.Name","",
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::FILEPATH_OPT,
		"",
		true,
		"If specified, internet traffic is logged to this file e.g. ggsn.log"
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.MS.IP.Base","192.168.99.1",
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::IPADDRESS,
		"",
		true,
		"Base IP address assigned to MS."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.MS.IP.MaxCount","254",
		"addresses",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"1:254",// educated guess
		true,
		"Number of IP addresses to use for MS."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.MS.IP.Route","",
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::CIDR_OPT,
		"",
		true,
		"A route address to be used for downstream clients.  "
			"By default, OpenBTS-UMTS manufactures this value from the GGSN.MS.IP.Base assuming a 24 bit mask.  "
			"To override, specify a route address in the form xxx.xxx.xxx.xxx/yy.  "
			"The address must encompass all MS IP addresses.  "
			"To use the auto-generated value again, execute \"unconfig GGSN.MS.IP.Route\"."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.ShellScript","",
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::FILEPATH_OPT,
		"",
		false,
		"A shell script to be invoked when MS devices attach or create IP connections.  "
			"By default, this feature is disabled.  "
			"To enable, specify an absolute path to the script you wish to execute e.g. /usr/bin/ms-attach.sh.  "
			"To disable again, execute \"unconfig GGSN.ShellScript\"."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GGSN.TunName","sgsntun",
		"",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::STRING,
		"^[a-z0-9]+$",
		true,
		"Tunnel device name for GGSN."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GPRS.Multislot.Max.Downlink","1",
		"channels",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"0:10",// educated guess
		false,
		"Maximum number of channels used for a single MS in downlink."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GPRS.Multislot.Max.Uplink","1",
		"channels",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"0:10",// educated guess
		false,
		"Maximum number of channels used for a single MS in uplink."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	//DEFAULT of 1 in UMTSConfig.cpp:930
	//DEFAULT of 2 in Sgsn.cpp:455
	tmp = new ConfigurationKey("GPRS.NMO","1",// USED SQL DEFAULT
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::CHOICE,
		"1|Mode I,"
			"2|Mode II,"
			"3|Mode III",
		false,
		"Network Mode of Operation.  "
			"See GSM 03.60 Section 6.3.3.1 and 24.008 4.7.1.6.  "
			"Allowed values are 1, 2, 3 for modes I, II, III.  "
			"Mode II (2) is recommended.  "
			"Mode I implies combined routing updating procedures."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GPRS.RAC","100",// DEFAULT INLINE WAS 0
		"",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"0:255",
		true,
		"GPRS Routing Area Code, advertised in the C0T0 beacon."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.CCCH.CCCH-CONF","1",
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::CHOICE,
		"1|C-V Beacon,"
			"2|C-IV Beacon",
		true,
		"CCCH configuration type.  "
			"See GSM 10.5.2.11 for encoding.  "
			"Value of 1 means we are using a C-V beacon.  "
			"Any other value selects a C-IV beacon.  "
			"In C2.9 and earlier, the only allowed value is 1."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.CellSelection.CELL-RESELECT-HYSTERESIS","3",
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::CHOICE,
		"0|0dB,"
			"1|2dB,"
			"2|4dB,"
			"3|6dB,"
			"4|8dB,"
			"5|10dB,"
			"6|12dB,"
			"7|14dB",
		false,
		"Cell Reselection Hysteresis.  "
			"See GSM 04.08 10.5.2.4, Table 10.5.23 for encoding.  "
			"Encoding is $2N$ dB, values of $N$ are 0...7 for 0...14 dB."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.CellSelection.MS-TXPWR-MAX-CCH","0",
		"dB",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"0:31",
		false,
		"Cell selection parameters.  "
			"See GSM 04.08 10.5.2.4."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.CellSelection.NCCsPermitted","1",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::VALRANGE,
		"0:255",
		false,
		"NCCs Permitted.  "
			"An 8-bit mask of allowed NCCs.  "
			"Unless you are coordinating with another carrier, this should probably just select your own NCC."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.CellSelection.NECI","1",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::CHOICE,
		"0|New establishment causes are not supported,"
			"1|New establishment causes are supported",
		false,
		"NECI, New Establishment Causes.  "
			"This must be set to 1 if you want to support very early assignment (VEA).  "
			"It can be set to 1 even if you do not use VEA, so you might as well leave it as 1.  "
			"See GSM 04.08 10.5.2.4, Table 10.5.23 and 04.08 9.1.8, Table 9.9 and the Control.VEA parameter."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.CellSelection.RXLEV-ACCESS-MIN","0",
		"dB",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"0:63",
		false,
		"Cell selection parameters.  "
			"See GSM 04.08 10.5.2.4."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.Identity.BSIC.BCC","2",
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::VALRANGE,
		"0:7",
		false,
		"GSM basestation color code; lower 3 bits of the BSIC.  "
			"BCC values in a multi-BTS network should be assigned so that BTS units with overlapping coverage do not share a BCC.  "
			"This value will also select the training sequence used for all slots on this unit.",
		ConfigurationKey::NEIGHBORSUNIQUE
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.Identity.LAC","1000",//DEFAULT INLINE WAS 0
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::VALRANGE,
		"0:65280",
		false,
		"Location area code, 16 bits, values 0xFFxx are reserved.  "
			"For multi-BTS networks, assign a unique LAC to each BTS unit.  "
			"(That is not the normal procedure in conventional GSM networks, but is the correct procedure in OpenBTS-UMTS networks.)",
		ConfigurationKey::GLOBALLYUNIQUE
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.Identity.MCC","001",
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::STRING,
		"^[0-9]{3}$",
		false,
		"Mobile country code; must be three digits.  "
			"Defined in ITU-T E.212. 001 for test networks.",
		ConfigurationKey::GLOBALLYSAME
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.Identity.MNC","01",
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::STRING,
		"^[0-9]{2,3}$",
		false,
		"Mobile network code, two or three digits.  "
			"Assigned by your national regulator. "
			"01 for test networks.",
		ConfigurationKey::GLOBALLYSAME
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.MaxSpeechLatency","2",
		"20 millisecond frames",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"1:5",// educated guess
		false,
		"Maximum allowed speech buffering latency, in 20 millisecond frames.  "
			"If the jitter is larger than this delay, frames will be lost."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.RACH.AC","0x0400",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::CHOICE,
		"0|Full Access,"
			"0x0400|Emergency Calls Not Supported",
		false,
		"Access class flags.  "
			"This is the raw parameter sent on the BCCH.  "
			"See GSM 04.08 10.5.2.29 for encoding.  "
			"Set to 0 to allow full access.  "
			"Set to 0x0400 to indicate no support for emergency calls."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.RACH.MaxRetrans","1",
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::CHOICE,
		"0|1 retransmission,"
			"1|2 retransmissions,"
			"2|4 retransmissions,"
			"3|7 retransmissions",
		false,
		"Maximum RACH retransmission attempts.  "
			"This is the raw parameter sent on the BCCH.  "
			"See GSM 04.08 10.5.2.29 for encoding."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.RACH.TxInteger","14",
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::CHOICE,
		"0|3 slots,"
			"1|4 slots,"
			"2|5 slots,"
			"3|6 slots,"
			"4|7 slots,"
			"5|8 slots,"
			"6|9 slots,"
			"7|10 slots,"
			"8|11 slots,"
			"9|12 slots,"
			"10|14 slots,"
			"11|16 slots,"
			"12|20 slots,"
			"13|25 slots,"
			"14|32 slots,"
			"15|50 slots",
		false,
		"Parameter to spread RACH busts over time.  "
			"This is the raw parameter sent on the BCCH.  "
			"See GSM 04.08 10.5.2.29 for encoding."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.RRLP.SEED.LATITUDE","37.777423",
		"degrees",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::VALRANGE,
		"-90.000000:90.000000",
		false,
		"Seed latitude in degrees.  "
			"-90 (south pole) .. +90 (north pole)"
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.RRLP.SEED.LONGITUDE","-122.39807",
		"degrees",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::VALRANGE,
		"-180.000000:180.000000",
		false,
		"Seed longitude in degrees.  "
			"-180 (west of greenwich) .. 180 (east)"
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.ShowCountry","0",
		"",
		ConfigurationKey::CUSTOMER,
		ConfigurationKey::BOOLEAN,// audited
		"",
		false,
		"Tell the phone to show the country name based on the MCC."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.Timer.T3113","10000",
		"milliseconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"5000:15000(500)",// educated guess
		false,
		"Paging timer T3113 in milliseconds.  "
			"This is the timeout for a handset to respond to a paging request.  "
			"This should usually be the same as SIP.Timer.B in your VoIP network."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("GSM.Timer.T3212","0",
		"minutes",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"0:1530(6)",
		false,
		"Registration timer T3212 period in minutes.  "
			"Should be a factor of 6.  "
			"Set to 0 to disable periodic registration.  "
			"Should be smaller than SIP registration period."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("RTP.Range","98",
		"ports",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"25:200",// educated guess
		true,
		"Range of RTP port pool.  "
			"Pool is RTP.Start to RTP.Range-1."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("RTP.Start","16484",
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::PORT,
		"",
		true,
		"Base of RTP port pool.  "
			"Pool is RTP.Start to RTP.Range-1."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SGSN.Debug","0",
		"",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::BOOLEAN,// audited
		"",
		false,
		"Add layer-3 messages to the GGSN.Logfile, if any."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SGSN.Timer.ImplicitDetach","3480",
		"seconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"2000:4000(10)",// educated guess
		false,
		"3GPP 24.008 11.2.2.  "
			"GPRS attached MS is implicitly detached in seconds.  "
			"Should be at least 240 seconds greater than SGSN.Timer.RAUpdate.");
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SGSN.Timer.MS.Idle","600",
		"?seconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"300:900(10)",// educated guess
		false,
		"How long an MS is idle before the SGSN forgets TLLI specific information."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SGSN.Timer.RAUpdate","3240",
		"seconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"0:11160(2)",	// (pat) 0 deactivates, up to 31 deci-hours which is 11160 seconds.  Minimum increment is 2 seconds.
		false,
		"Also known as T3312, 3GPP 24.008 4.7.2.2.  "
			"How often MS reports into the SGSN when it is idle, in seconds.  "
			"Setting to 0 or >12000 deactivates entirely, i.e., sets the timer to effective infinity.  "
			"Note: to prevent GPRS Routing Area Updates you must set both this and GSM.Timer.T3212 to 0.  "
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SGSN.Timer.Ready","44",
		"seconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"30:90",// educated guess
		false,
		"Also known as T3314, 3GPP 24.008 4.7.2.1.  "
			"Inactivity period required before MS may perform another routing area or cell update, in seconds."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// BUG : this is still using .defines() in C3.1!
	tmp = new ConfigurationKey("SIP.DTMF.RFC2833","1",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::BOOLEAN,
		"",
		false,
		"Use RFC-2833 (RTP event signalling) for in-call DTMF."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.DTMF.RFC2833.PayloadType","101",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::VALRANGE,
		"96:127",
		false,
		"Payload type to use for RFC-2833 telephone event packets."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// BUG : this is still using .defines() in C3.1!
	tmp = new ConfigurationKey("SIP.DTMF.RFC2967","0",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::BOOLEAN,
		"",
		false,
		"Use RFC-2967 (SIP INFO method) for in-call DTMF."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Local.IP","127.0.0.1",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::IPADDRESS,
		"",
		true,
		"IP address of the OpenBTS-UMTS machine as seen by its proxies.  "
			"If these are all local, this can be localhost.",
		ConfigurationKey::NODESPECIFIC
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Local.Port","5062",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::PORT,
		"",
		true,
		"IP port that OpenBTS-UMTS uses for its SIP interface."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.MaxForwards","70",
		"referrals",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"1:100",
		false,
		"Maximum allowed number of referrals."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Proxy.Registration","127.0.0.1:5064",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::HOSTANDPORT,
		"",
		false,
		"The hostname or IP address and port of the proxy to be used for registration and authentication.  "
			"This should normally be the subscriber registry SIP interface, not Asterisk.",
		ConfigurationKey::GLOBALLYSAME
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Proxy.SMS","127.0.0.1:5063",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::HOSTANDPORT,
		"",
		false,
		"The hostname or IP address and port of the proxy to be used for text messaging.  "
			"This is smqueue, for example.",
		ConfigurationKey::GLOBALLYSAME
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Proxy.Speech","127.0.0.1:5060",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::HOSTANDPORT,
		"",
		false,
		"The hostname or IP address and port of the proxy to be used for normal speech calls.  "
			"This is Asterisk, for example.",
		ConfigurationKey::GLOBALLYSAME
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.RegistrationPeriod","90",
		"minutes",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"6:2298",// educated guess
		false,
		"Registration period in minutes for MS SIP users.  "
			"Should be longer than GSM T3212."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.SMSC","smsc",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::CHOICE_OPT,
		"smsc",
		false,
		"The SMSC handler in smqueue.  "
			"This is the entity that handles full 3GPP MIME-encapsulted TPDUs.  "
			"If not defined, use direct numeric addressing.  "
			"The value should be disabled with \"unconfig SIP.SMSC\" if SMS.MIMEType is \"text/plain\" or set to \"smsc\" if SMS.MIMEType is \"application/vnd.3gpp\".",
		ConfigurationKey::GLOBALLYSAME
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Timer.A","2000",
		"milliseconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"1500:2500(100)",// educated guess
		false,
		"SIP timer A, the INVITE retry period, RFC-3261 Section 17.1.1.2, in milliseconds."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Timer.B","10000",
		"milliseconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"5000:15000(100)",// educated guess
		false,
		"INVITE transaction timeout in milliseconds.  "
			"This value should usually match GSM.Timer.T3113."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Timer.E","500",
		"milliseconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"250:750(10)",// educated guess
		false,
		"Non-INVITE initial request retransmit period in milliseconds."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Timer.F","5000",
		"milliseconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"2500:7500(100)",// educated guess
		false,
		"Non-INVITE initial request timeout in milliseconds."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SIP.Timer.H","5000",
		"milliseconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"2500:7500(100)",// educated guess
		false,
		"ACK timeout period in milliseconds."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SMS.FakeSrcSMSC","0000",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::STRING,
		"^[0-9]+$",
		false,
		"Use this to fill in L4 SMSC address in SMS delivery."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SMS.MIMEType","application/vnd.3gpp.sms",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::CHOICE,
		"application/vnd.3gpp.sms,"
			"text/plain",
		false,
		"This is the MIME Type that OpenBTS-UMTS will use for RFC-3428 SIP MESSAGE payloads.  "
			"Valid values are \"application/vnd.3gpp.sms\" and \"text/plain\".",
		ConfigurationKey::GLOBALLYSAME
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SubscriberRegistry.A3A8","/OpenBTS/comp128",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::FILEPATH,
		"",
		false,
		"Path to the program that implements the A3/A8 algorithm."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SubscriberRegistry.db","/var/lib/asterisk/sqlite3dir/sqlite3.db",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::FILEPATH,
		"",
		false,
		"The location of the sqlite3 database holding the subscriber registry."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SubscriberRegistry.Port","5064",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::PORT,
		"",
		false,
		"Port used by the SIP Authentication Server. NOTE: In some older releases (pre-2.8.1) this is called SIP.myPort."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("SubscriberRegistry.UpstreamServer","",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::STRING_OPT,
		"",
		false,
		"URL of the subscriber registry HTTP interface on the upstream server.  "
			"By default, this feature is disabled.  "
			"To enable, specify a server URL eg: http://localhost/cgi/subreg.cgi.  "
			"To disable again, execute \"unconfig SubscriberRegistry.UpstreamServer\"."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("TRX.IP","127.0.0.1",
		"",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::IPADDRESS,
		"",
		true,
		"IP address of the transceiver application."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("TRX.Port","5700",
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::PORT,
		"",
		true,
		"IP port of the transceiver application."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("TRX.RadioFrequencyOffset","128",
		"~170Hz steps",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"64:192",
		true,
		"Fine-tuning adjustment for the transceiver master clock.  "
			"Roughly 170 Hz/step.  "
			"Set at the factory.  "
			"Do not adjust without proper calibration.",
		ConfigurationKey::NODESPECIFIC
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("TRX.TxAttenOffset","0",
		"dB of attenuation",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:100",// educated guess
		true,
		"Hardware-specific gain adjustment for transmitter, matched to the power amplifier, expessed as an attenuationi in dB.  "
			"Set at the factory.  "
			"Do not adjust without proper calibration.",
		ConfigurationKey::NODESPECIFIC
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.AICH.AICH-PowerOffset","-10",// DEFAULT INLINE WAS 0
		"dB",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"-100:100",//??????
		false,
		"AICH power offset in dB."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// FROM SOURCE : // For best-effort, default to 60K which is SF=16, or 480 kbit/s.
	tmp = new ConfigurationKey("UMTS.Best.Effort.BytesPerSec","60000",
		"dBm",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:600000",//??????
		false,
		"Target Physical Bit Rate in bytes/s for DCH channels, actual data rate is roughly one-third of this."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.CellSelect.MaxAlloweddUL-TX-Power","33",
		"dBm",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:100",//??????
		false,
		"Maximum Allowed UE transmit power in dBm."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	/*
	// DRX_CycleLengthCoeff: "A coefficient in the formula to count the paging occasions to be used by a specific UE (specified
	//	in  3GPP TS 25.304: "UE Procedures in Idle Mode and Procedures for Cell Reselection in Connected Mode".)
    	dsi2->cn_DRX_CycleLengthCoeff = gConfig.getNum("UMTS.CN-DSI.CycleLengthCoeff");        // FIXME -- What does this mean?!
	*/
	tmp = new ConfigurationKey("UMTS.CN-DSI.CycleLengthCoeff","6",//DEFAULT INLINE WAS 8
		"????",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:100",//??????
		false,
		"Have no idea."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Debug.ASN","0",
		"",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::BOOLEAN,
		"",
		false,
		"Enable debug printouts of ASN messages."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Debug.ASN.Beacon","1",// DEFAULT INLINE WAS 0
		"",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::BOOLEAN,
		"",
		false,
		"Flag to print debugging ASN info about BCH messages."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Debug.ASN.Free","0",
		"",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::BOOLEAN,
		"",
		false,
		"Have no idea."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Debug.Messages","1",
		"",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::BOOLEAN,
		"",
		false,
		"Enable debug printout of RRC messages."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// (pat) This print has been superceded by asn2String dumping all the SIBs to the log at end of function below.
	tmp = new ConfigurationKey("UMTS.Debug.SIB","1",// DEFAULT INLINE WAS 0
		"",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::BOOLEAN,
		"",
		false,
		"Enable debug printouts of SIB RRC message."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	/*
	DEFAULT was 0 in UMTSPhCh.cpp:610
	DEFAULT was 0 in URRCMessages.cpp:759
	DEFAULT was 1 in UMTSConfig.cpp:233 and :1431
	DEFAULT was 1 in UMTSRadioModem.cpp:91
	*/
	tmp = new ConfigurationKey("UMTS.Downlink.ScramblingCode","469",
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:511",
		false,
		"UMTS downlink scrambling code.  It is the same for all downlink channels."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	/*
	// "Offset in number of chips between the beginning of the P-CCPCH frame
	// and the beginning of the DPCH frame" aka Tau(DPCH,n)
	// DPCH_FrameOffset_t   dpch_FrameOffset;
	int dpchFrameOffset = gConfig.getNum("UMTS.DPCHFrameOffset");
	assert(dpchFrameOffset >= 0 && dpchFrameOffset <= 38144);	// harsh
	assert(dpchFrameOffset % 256 == 0);
	*/
	tmp = new ConfigurationKey("UMTS.DPCHFrameOffset","0",
		"chips",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:38144",
		false,
		"DPCH frame offset in chips.  Not used."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Identity.CI","10",
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::VALRANGE,
		"0:65535",
		false,
		"Cell ID, 16 bits.  Should be unique.",
		ConfigurationKey::GLOBALLYUNIQUE
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Identity.LAC","132",// DEFAULT INLINE WAS 0
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::VALRANGE,
		"0:65280",
		false,
		"Location area code, 16 bits, values 0xFFxx are reserved.  "
			"For multi-NodeB networks, assign a unique LAC to each NodeB unit.  "
			"(That is not the normal procedure in conventional UMTS networks, but is the correct procedure in OpenBTS-UMTS networks.)"
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Identity.MCC","001",
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::STRING,
		"^[0-9]{3}$",
		false,
		"Mobile country code; must be three.  "
			"Defined in ITU-T E.212. 001 for test networks."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Identity.MNC","01",
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::STRING,
		"^[0-9]{2,3}$",
		false,
		"Mobile network code, two or three digits.  "
			"Assigned by your national regulator. "
			"01 for test networks."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Identity.URAI","100",
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::VALRANGE,
		"0:65535",
		false,
		"UTRAN Registration Area Identity, 16 bits."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.PCPICHUsageForChannelEst","1",// BOOLEAN VALUE
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::BOOLEAN,
		"",
		false,
		"Flag to indicate that UE should use PCPICH for channel estimation."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.PICH.PICH-PowerOffset","-10",// DEFAULT INLINE WAS 0
		"dB",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"-100:100",//??????
		false,
		"PICH power offset in dB."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// FROM SOURCE: TODO UMTS -- what does this mean?
	tmp = new ConfigurationKey("UMTS.PRACH.DynamicPersistenceLevel","1",
		"??",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"-100:100",//??????
		false,
		"Dynamic Persistence Level for PRACH channel.  Valid values unknown."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.PRACH.ScramblingCode","0",// DEFAULT INLINE WAS 1
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:1000",//??????
		false,
		"PRACH scrambling code for PRACH bursts."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.PRACH.SF","32",// DEFAULT INLINE WAS 256
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::CHOICE,
		"32,"
			"64,"
			"128,"
			"256",
		false,
		"Spreading Factor of PRACH.  Valid values are 32, 64, 128 and 256."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.PRACH.Signature","13",// DEFAULT INLINE WAS 0
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:15",
		false,
		"Sequence used for PRACH accesses."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.PRACH.Subchannel","1",// DEFAULT INLINE WAS 0
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:11",
		false,
		"PRACH subchannel."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.ARFCNs","1",
		"ARFCNs",
		ConfigurationKey::CUSTOMERWARN,
		ConfigurationKey::VALRANGE,
		"1:10",// educated guess
		true,
		"The number of ARFCNs to use.  "
			"The ARFCN set will be C0, C0+2, C0+4, etc."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.Band","900",
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::CHOICE,
		"850,"
			"900,"
			"1700,"
			"1800,"
			"1900,"
			"2100",
		true,
		"The UMTS operating band.  "
			"Valid values are 850, 900, 1700, 1800, 1900 and 2100.  "
			"For most Range models, this value is dictated by the hardware and should not be changed."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.C0","3050",
		"",
		ConfigurationKey::CUSTOMERSITE,
		ConfigurationKey::CHOICE,
		getARFCNsString(900),
		true,
		"The UARFCN.  Range of valid values depend upon the selected operating band.",
		ConfigurationKey::NEIGHBORSUNIQUE
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.MaxExpectedDelaySpread","50",//"4",// mRACHSearchSize in UMTSRadioModem.cpp is a hard-coded override
		"symbol periods",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"1:200",//"1:4",??
		false,
		"Expected worst-case delay spread in symbol periods, roughly 3.7 us or 1.1 km per unit."
		//	"This parameter is dependent on the terrain type in the installation area.  "
		//	"Typical values are 1 for open terrain and small coverage areas.  "
		//	"For large coverage areas, a value of 4 is strongly recommended.  "
		//	"This parameter has a large effect on computational requirements of the software radio; values greater than 4 should be avoided."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.PowerManager.MaxAttenDB","10",
		"dB",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"0:80",// educated guess
		false,
		"Maximum transmitter attenuation level, in dB wrt full scale on the D/A output.  "
			"This sets the minimum power output level in the output power control loop."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.PowerManager.MinAttenDB","0",
		"dB",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"0:80",// educated guess
		false,
		"Minimum transmitter attenuation level, in dB wrt full scale on the D/A output.  "
			"This sets the maximum power output level in the output power control loop."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.PowerManager.NumSamples","10",
		"sample count",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"5:20",// educated guess
		false,
		"Number of samples averaged by the output power control loop."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.PowerManager.Period","6000",
		"milliseconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"4500:7500(100)",// educated guess
		false,
		"Power manager control loop master period, in milliseconds."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.PowerManager.SamplePeriod","2000",
		"milliseconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"1500:2500(100)",// educated guess
		false,
		"Sample period for the output power control loopm in milliseconds."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.PowerManager.TargetT3122","5000",
		"milliseconds",
		ConfigurationKey::DEVELOPER,
		ConfigurationKey::VALRANGE,
		"3750:6250(100)",// educated guess
		false,
		"Target value for T3122, the random access hold-off timer, for the power control loop."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Radio.RxGain","57",
		"dB",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:75",// educated guess
		true,
		"Receiver gain setting in dB.  "
			"Ideal value is dictacted by the hardware.  "
			"This database parameter is static but the receiver gain can be modified in real time with the CLI rxgain command."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.RLC.TransmissionBufferSize","1000000", // used sql default, hardcoded fallback was 10000
		"bytes",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"5000:5000000",// ??
		false,
		"Buffer size in Bytes for RLC Transmission layer."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.SCCPCH.SF","64",
		"",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::CHOICE,
		"4,"
			"8,"
			"16,"
			"32,"
			"64,"
			"128,"
			"256",
		false,
		"Spreading Factor of SCCPCH.  Valid values are 4, 8, 16, 32, 64, 128 and 256."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.SCCPCH.SpreadingCode","2",
		"??",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:100",// ??
		false,
		"Spreading code for SCCPCH bursts."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.SRNC_ID","0",
		"??",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:100",// ??
		false,
		"RNC ID."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("UMTS.Timer.T3212","30",// DEFAULT INLINE WAS 20
		"minutes",
		ConfigurationKey::CUSTOMERTUNE,
		ConfigurationKey::VALRANGE,
		"0:1530(6)",
		false,
		"Registration timer T3212 period in minutes.  "
			"Should be a factor of 6.  "
			"Set to 0 to disable periodic registration.  "
			"Should be smaller than SIP registration period."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// TODO : this doesn't seem to actually be used as "seconds"
	// int tInactivity = 1000*gConfig.getNum("UMTS.Timers.Inactivity.Release");
	tmp = new ConfigurationKey("UMTS.Timers.Inactivity.Release","180",
		"seconds",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"30:1800",// ??
		false,
		"In seconds, period of inactivity before UE in CELL_PCH mode is released.  Not used."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// TODO : this doesn't seem to actually be used as "seconds"
	// int tDelete = 1000*gConfig.getNum("UMTS.Timers.Inactivity.Delete");
	tmp = new ConfigurationKey("UMTS.Timers.Inactivity.Delete","300",
		"seconds",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"30:3000",// ??
		false,
		"In seconds, period of inactivity before UE in Idle mode is purged from active list."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// FROM SOURCE: Uplink puncturing limit expressed as percent in the range 40 to 100.
	// 		From sql "UMTS.Uplink.Puncturing.Limit"; default 100 (no puncturing); bounded if out of range.
	tmp = new ConfigurationKey("UMTS.Uplink.Puncturing.Limit","100",
		"percent",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"40:100",
		false,
		"Puncturing Limit of L1 rate-matcher for uplink.  Do not use."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	// The uplink scrambling codes should probably be chosen so as to maximize
	// the distance between adjacent cells using the same scrambing codes.
	// Currently we use a consecutive block of about 512 scrambling codes.
	// TODO: We could use just 256 scrambling codes by taking the scrambling code
	// for higher tiers in the tree from the lowest tier.
	tmp = new ConfigurationKey("UMTS.Uplink.ScramblingCode","543",// DEFAULT INLINE WAS 99
		"??",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:2147483648",
		false,
		"Base index for DCH scrambling codes assigned to UEs.  Valid values are 0 to 2^31."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;


	tmp = new ConfigurationKey("UMTS.UseTurboCodes","1",
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::BOOLEAN,
		"",
		false,
		"Are turbocodes enabled."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	return map;
}

