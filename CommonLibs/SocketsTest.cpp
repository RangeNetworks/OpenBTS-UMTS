/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008 Free Software Foundation, Inc.
 * Copyright 2011-2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#include "Sockets.h"
#include "Threads.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


static const int gNumToSend = 10;


void *testReaderIP(void *)
{
	UDPSocket readSocket(5934, "localhost", 5061);
	readSocket.nonblocking();
	int rc = 0;
	while (rc<gNumToSend) {
		char buf[MAX_UDP_LENGTH];
		int count = readSocket.read(buf);
		if (count>0) {
			COUT("read: " << buf);
			rc++;
		} else {
			sleep(2);
		}
	}
	return NULL;
}



void *testReaderUnix(void *)
{
	UDDSocket readSocket("testDestination");
	readSocket.nonblocking();
	int rc = 0;
	while (rc<gNumToSend) {
		char buf[MAX_UDP_LENGTH];
		int count = readSocket.read(buf);
		if (count>0) {
			COUT("read: " << buf);
			rc++;
		} else {
			sleep(2);
		}
	}
	return NULL;
}


int main(int argc, char * argv[] )
{

  Thread readerThreadIP;
  readerThreadIP.start(testReaderIP,NULL);
  Thread readerThreadUnix;
  readerThreadUnix.start(testReaderUnix,NULL);

  UDPSocket socket1(5061, "127.0.0.1",5934);
  UDDSocket socket1U("testSource","testDestination");
  
  COUT("socket1: " << socket1.port());

  // give the readers time to open
  sleep(1);

  for (int i=0; i<gNumToSend; i++) {
    socket1.write("Hello IP land");	
	socket1U.write("Hello Unix domain");
	sleep(1);
  }

  readerThreadIP.join();
  readerThreadUnix.join();
}

// vim: ts=4 sw=4
