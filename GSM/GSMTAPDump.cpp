/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include "GSMTAPDump.h"
#include "GSMTransfer.h"
#include <Sockets.h>
#include <Globals.h>

UDPSocket GSMTAPSocket;

void gWriteGSMTAP(unsigned ARFCN, unsigned TS, unsigned FN, const GSM::L2Frame& frame)
{
	if (!gConfig.defines("GSMTAP.TargetIP")) return;

	unsigned port = GSMTAP_UDP_PORT;	// default port for GSM-TAP
	if (gConfig.defines("GSMTAP.TargetPort"))
		port = gConfig.getNum("GSMTAP.TargetPort");

	// Write a GSMTAP packet to the configured destination.
	GSMTAPSocket.destination(port,gConfig.getStr("GSMTAP.TargetIP").c_str());
	char buffer[MAX_UDP_LENGTH];
	gsmtap_hdr header(ARFCN,TS,FN);
	memcpy(buffer,&header,sizeof(header));
	frame.pack((unsigned char*)buffer+sizeof(header));
	GSMTAPSocket.write(buffer, sizeof(header) + frame.size()/8);
}



// vim: ts=4 sw=4
