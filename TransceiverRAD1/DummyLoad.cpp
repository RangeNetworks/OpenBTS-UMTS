/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 * 
 * This software is distributed under the terms of the GNU General Public 
 * License version 3. See the COPYING and NOTICE files in the current
 * directory for licensing information.
 * 
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

/*
	Compilation Flags
	SWLOOPBACK	compile for software loopback testing
*/ 

#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "Threads.h"
#include "DummyLoad.h"

#include <Logger.h>


using namespace std;



int DummyLoad::loadBurst(short *wDummyBurst, int len) {
  dummyBurst = wDummyBurst;
  dummyBurstSz = len;
}


DummyLoad::DummyLoad (double _desiredSampleRate) 
{
  LOG(INFO) << "creating USRP device...";
  sampleRate = _desiredSampleRate;
}

void DummyLoad::updateTime(void) {
    gettimeofday(&currTime,NULL);
    double timeElapsed = (currTime.tv_sec - startTime.tv_sec)*1.0e6 + 
      (currTime.tv_usec - startTime.tv_usec);
    currstamp = (TIMESTAMP) floor(timeElapsed/(1.0e6/sampleRate));
}

bool DummyLoad::make(bool wSkipRx) 
{

  samplesRead = 0;
  samplesWritten = 0;
  return true;
}

bool DummyLoad::start() 
{
  LOG(INFO) << "starting USRP...";
  underrun = false;
  gettimeofday(&startTime,NULL);
  dummyBurstCursor = 0;
  return true;
}

bool DummyLoad::stop() 
{
  return true;
}


// NOTE: Assumes sequential reads
int DummyLoad::readSamples(short *buf, int len, bool *overrun, 
			    TIMESTAMP timestamp,
			    bool *wUnderrun,
			    unsigned *RSSI) 
{
  updateTime();
  underrunLock.lock();
  *wUnderrun = underrun;
  underrunLock.unlock();
  if (currstamp+len < timestamp) {
	usleep(100); 
	return NULL;
  } 
  else if (currstamp < timestamp) {
	usleep(100);
	return NULL;
  }
  else if (timestamp+len < currstamp) {
	memcpy(buf,dummyBurst+dummyBurstCursor*2,sizeof(short)*2*(dummyBurstSz-dummyBurstCursor));
	int retVal = dummyBurstSz-dummyBurstCursor;
	dummyBurstCursor = 0;
	return retVal;
  }
  else if (timestamp + len > currstamp) {
	int amount = timestamp + len - currstamp;
	if (amount < dummyBurstSz-dummyBurstCursor) {
	        memcpy(buf,dummyBurst+dummyBurstCursor*2,sizeof(short)*2*amount);
        	dummyBurstCursor += amount;
        	return amount;
	}
	else {
        	memcpy(buf,dummyBurst+dummyBurstCursor*2,sizeof(short)*2*(dummyBurstSz-dummyBurstCursor));
        	int retVal = dummyBurstSz-dummyBurstCursor;
        	dummyBurstCursor = 0;
        	return retVal;
        }
  }
  return 0;
}

int DummyLoad::writeSamples(short *buf, int len, bool *wUnderrun, 
			     unsigned long long timestamp,
			     bool isControl) 
{
  updateTime();
  underrunLock.lock();
  underrun |= (currstamp+len < timestamp); 
  underrunLock.unlock();
  return len;
}

bool DummyLoad::updateAlignment(TIMESTAMP timestamp) 
{
  return true;
}

bool DummyLoad::setTxFreq(double wFreq) { return true;};
bool DummyLoad::setRxFreq(double wFreq) { return true;};
