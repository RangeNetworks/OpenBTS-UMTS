/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2010 Free Software Foundation, Inc.
 * Copyright 2012, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include <Logger.h>

#include "TRXManager.h"

#include "UMTSCommon.h"
#include "UMTSTransfer.h"
#include "UMTSLogicalChannel.h"
#include "UMTSConfig.h"
#include "UMTSL1FEC.h"


#include <string>
#include <string.h>
#include <stdlib.h>

#undef WARNING


using namespace UMTS;
using namespace std;

int ::ARFCNManager::sendCommandPacket(const char* command, char* response)
{
	int msgLen = 0;
	char cmdNameTest[15];
	int status;
	int timeout = 6000;
	response[0] = '\0';

	LOG(INFO) << "command " << command;
	mControlLock.lock();

	mControlSocket.write(command);

	/* Check for RSP response */
	msgLen = mControlSocket.read(response, timeout);

	if (msgLen == 0) {
		LOG(NOTICE) << "No RSP : lost control link to transceiver";
		mControlLock.unlock();
		return 0;
	}
	else {
		/* Check that it was a RSP message */
		response[msgLen] = '\0';	/* Terminate message */
		if ((msgLen>4) && (strncmp(response,"RSP ",4) == 0)) {
			mControlLock.unlock();
			return msgLen;
		} else {
			LOG(NOTICE) << "Bad RSP : lost control link to transceiver";
	                LOG(ALERT) << "RSP response " << response;
			mControlLock.unlock();
			return 0;
		}
	}

	/* Default */
	mControlLock.unlock();
	return 0;
}

bool ::ARFCNManager::powerOff()
{
        int status = sendCommand("POWEROFF");
        if (status!=0) {
                LOG(ALERT) << "POWEROFF failed with status " << status;
                return false;
        }
        return true;
}

bool ::ARFCNManager::setPower(int dB)
{
        int status = sendCommand("SETPOWER",dB);
        if (status!=0) {
                LOG(ALERT) << "SETPOWER failed with status " << status;
                return false;
        }
        return true;
}

bool ::ARFCNManager::setMaxDelay(unsigned km)
{
        int status = sendCommand("SETMAXDLY",km);
        if (status!=0) {
                LOG(ALERT) << "SETMAXDLY failed with status " << status;
                return false;
        }
        return true;
}

signed ::ARFCNManager::setRxGain(signed rxGain)
{
        signed newRxGain;
        int status = sendCommand("SETRXGAIN",rxGain,&newRxGain);
        if (status!=0) {
                LOG(ALERT) << "SETRXGAIN failed with status " << status;
                return false;
        }
        return newRxGain;
}

signed ::ARFCNManager::readTxPwr(void)
{
        signed txPwr;
        int status = sendCommand("READTXPWR", 0, &txPwr);
        if (status!=0) {
                LOG(ALERT) << "READTXPWR failed with status " << status;
                return false;
        }
        return txPwr;
}

signed ::ARFCNManager::readRxPwrCoarse(void)
{
        signed rxPwr;
        int status = sendCommand("READRXPWRCOARSE", 0, &rxPwr);
        if (status!=0) {
                LOG(ALERT) << "READRXPWRCOARSE failed with status " << status;
                return false;
        }
        return rxPwr;
}

long long ::ARFCNManager::readRxPwrFine()
{
        long long rxPwr;
        int status = sendCommand("READRXPWRFINE", &rxPwr);
        if (status!=0) {
                LOG(ALERT) << "READRXPWRFINE failed with status " << status;
                return false;
        }
        return rxPwr;
}

signed ::ARFCNManager::setFreqOffset(signed offset)
{
        signed newFreqOffset;
        int status = sendCommand("SETFREQOFFSET",offset,&newFreqOffset);
        if (status!=0) {
                LOG(ALERT) << "SETFREQOFFSET failed with status " << status;
                return false;
        }
        return newFreqOffset;
}

signed ::ARFCNManager::getTemperature(void)
{
        signed temperature;
        int status = sendCommand("TEMPERATURE", 0, &temperature);
        if (status!=0) {
                LOG(ALERT) << "TEMPERATURE failed with status " << status;
                return false;
        }
        return temperature;
}

signed ::ARFCNManager::getNoiseLevel(void)
{
        signed noiselevel;
        int status = sendCommand("NOISELEV",0,&noiselevel);
        if (status!=0) {
                LOG(ALERT) << "NOISELEV failed with status " << status;
                return false;
        }
        return noiselevel;
}

bool ::ARFCNManager::radioPowerOff()
{
	int status = sendCommand("RADIOPOWEROFF");
	if (status!=0) {
		LOG(ALERT) << "RADIOPOWEROFF failed with status " << status;
		return false;
	}
	return true;
}

bool ::ARFCNManager::radioPowerOn(bool warn)
{
	int status = sendCommand("RADIOPOWERON");
	if (status!=0) {
		if (warn) {
			LOG(ALERT) << "RADIOPOWERON failed with status " << status;
		} else {
			LOG(INFO) << "RADIOPOWERON failed with status " << status;
		}
		
		return false;
	}
	return true;
}

// (pat) There is a constructor race that culminates in UMTSRadioModem() failing
// because the BitVectors in gRACHSignatures are not initialized.
// The easiest fix is to do the TransceiverManager initialization in a function here
// instead of in its constructor.
void TransceiverManager::TransceiverManagerInit(int numARFCNs,
                const char* wTRXAddress, int wBasePort)
{
        //mHaveClock = false;
        mClockSocket.open(wBasePort+100);
        // set up the ARFCN managers
        for (int i=0; i<numARFCNs; i++) {
                int thisBasePort = wBasePort + 1 + 2*i;
                mARFCNs.push_back(new ::ARFCNManager(wTRXAddress,thisBasePort,*this,i));
        }
}

void TransceiverManager::trxStart()
{
        mClockThread.start((void*(*)(void*))ClockLoopAdapter,this);
        for (unsigned i=0; i<mARFCNs.size(); i++) {
                mARFCNs[i]->arfcnManagerStart();
        }
}

void* ClockLoopAdapter(TransceiverManager *transceiver)
{
        Timeval nextContact;
        while (1) {
                transceiver->clockHandler();
                /*if (nextContact.passed()) {
                        // FIXME -- Phone Home Here
                        if (gConfig.defines("NTP.Server")) {
                                string ntp = string("ntpdate ") + gConfig.getStr("NTP.Server");
                                system(ntp.c_str());
                        }
                        nextContact.addMinutes(random() % (24*60));
                }*/
        }
        return NULL;
}


void TransceiverManager::clockHandler()
{
        char buffer[MAX_UDP_LENGTH];
        int msgLen = mClockSocket.read(buffer,3000);

        // Did the transceiver die??
        if (msgLen<0) {
                LOG(ALERT) << "TRX clock interface timed out, assuming TRX is dead.";
                // FIXME: Pat removed abort to debug this thing.
                //abort();
                return;
        }

        if (msgLen==0) {
                LOG(ALERT) << "read error on TRX clock interface, return " << msgLen;
                return;
        }

        if (strncmp(buffer,"IND CLOCK",9)==0) {
                uint32_t FN;
                sscanf(buffer,"IND CLOCK %u", &FN);
                LOG(INFO) << "CLOCK indication, clock="<<FN << ", was=" << gNodeB.clock().FN();
                gNodeB.clock().setFN(FN);
                mHaveClock = true;
                return;
        }

        buffer[msgLen]='\0';
        LOG(ALERT) << "bogus message " << buffer << " on clock interface";
}

::ARFCNManager::ARFCNManager(const char* wTRXAddress, int wBasePort, TransceiverManager &wTransceiver, unsigned wCId)
        :mTransceiver(wTransceiver),
        mDataSocket(wBasePort+100+1,wTRXAddress,wBasePort+1),
        mControlSocket(wBasePort+100,wTRXAddress,wBasePort),
        mRadioModem(mDataSocket),
        mCId(wCId)
{
        mRadioModem.radioModemStart();
        // The default demux table is full of NULL pointers.
//      for (int i=0; i<8; i++) {
//              for (unsigned j=0; j<maxModulus; j++) {
//                      mDemuxTable[i][j] = NULL;
//              }
//      }
}




void ::ARFCNManager::arfcnManagerStart()
{
        mRxThread.start((void*(*)(void*))ReceiveLoopAdapter,this);
        mTxThread.start((void*(*)(void*))TransmitLoopAdapter,this);
}

void* TransmitLoopAdapter(::ARFCNManager* manager)
{
        manager->transmitLoop();
        // do not reach
        return NULL;    // pacify the compiler
}


void* ReceiveLoopAdapter(::ARFCNManager* manager)
{
        manager->receiveLoop();
        // do not reach
        return NULL;    // pacify the compiler
}

int ::ARFCNManager::sendCommand(const char*command, int param, int *responseParam)
{
        // Send command and get response.
        char cmdBuf[MAX_UDP_LENGTH];
        char response[MAX_UDP_LENGTH];
        sprintf(cmdBuf,"CMD %s %d", command, param);
        int rspLen = sendCommandPacket(cmdBuf,response);
        if (rspLen<=0) return -1;
        // Parse and check status.
        char cmdNameTest[15];
        int status;
        cmdNameTest[0]='\0';
        if (!responseParam)
          sscanf(response,"RSP %15s %d", cmdNameTest, &status);
        else
          sscanf(response,"RSP %15s %d %d", cmdNameTest, &status, responseParam);
        if (strcmp(cmdNameTest,command)!=0) return -1;
        return status;
}


int ::ARFCNManager::sendCommand(const char*command, const char* param)
{
        // Send command and get response.
        char cmdBuf[MAX_UDP_LENGTH];
        char response[MAX_UDP_LENGTH];
        sprintf(cmdBuf,"CMD %s %s", command, param);
        int rspLen = sendCommandPacket(cmdBuf,response);
        if (rspLen<=0) return -1;
        // Parse and check status.
        char cmdNameTest[15];
        int status;
        cmdNameTest[0]='\0';
        sscanf(response,"RSP %15s %d", cmdNameTest, &status);
        if (strcmp(cmdNameTest,command)!=0) return -1;
        return status;
}

int ::ARFCNManager::sendCommand(const char*command, long long *responseParam)
{
        // Send command and get response.
        char cmdBuf[MAX_UDP_LENGTH];
        char response[MAX_UDP_LENGTH];
        sprintf(cmdBuf,"CMD %s", command);
        int rspLen = sendCommandPacket(cmdBuf,response);
        if (rspLen<=0) return -1;
        // Parse and check status.
        char cmdNameTest[15];
        int status;
        cmdNameTest[0]='\0';
        if (!responseParam)
          sscanf(response,"RSP %15s %d", cmdNameTest, &status);
        else
          sscanf(response,"RSP %15s %d %llx", cmdNameTest, &status, responseParam);

        if (strcmp(cmdNameTest,command)!=0)
        {
                printf("TRXManager:sendCommand Failed because RSP cmdNameTest:%s != command:%s.\n\r", cmdNameTest, command);
                return -1;
        }
        return status;
}

int ::ARFCNManager::sendCommand(const char*command)
{
        // Send command and get response.
        char cmdBuf[MAX_UDP_LENGTH];
        char response[MAX_UDP_LENGTH];
        sprintf(cmdBuf,"CMD %s", command);
        int rspLen = sendCommandPacket(cmdBuf,response);
        if (rspLen<=0) return -1;
        // Parse and check status.
        char cmdNameTest[15];
        int status;
        cmdNameTest[0]='\0';
        sscanf(response,"RSP %15s %d", cmdNameTest, &status);
        if (strcmp(cmdNameTest,command)!=0) return -1;
        return status;
}


bool ::ARFCNManager::tune(unsigned wUARFCN)
{
        // convert ARFCN number to a frequency
        unsigned txFreq = channelFreqKHz(gNodeB.band(),wUARFCN);
        unsigned rxFreq = txFreq-uplinkOffsetKHz(gNodeB.band());

        // tune tx
        int status = sendCommand("TXTUNE",txFreq);
        if (status!=0) {
                LOG(ALERT) << "TXTUNE failed with status " << status;
                return false;
        }

        // tune rx
        status = sendCommand("RXTUNE",rxFreq);
        if (status!=0) {
                LOG(ALERT) << "RXTUNE failed with status " << status;
                return false;
        }

        // done
        mUARFCN=wUARFCN;
        return true;
}

bool ::ARFCNManager::tuneLoopback(int wARFCN)
{
        // convert ARFCN number to a frequency
        unsigned txFreq = channelFreqKHz(gNodeB.band(),wARFCN);
        // tune rx
        int status = sendCommand("RXTUNE",txFreq);
        if (status!=0) {
                LOG(ALERT) << "RXTUNE failed with status " << status;
                return false;
        }
        // tune tx
        status = sendCommand("TXTUNE",txFreq);
        if (status!=0) {
                LOG(ALERT) << "TXTUNE failed with status " << status;
                return false;
        }
        // done
        mUARFCN=wARFCN;
        return true;
}


void ::ARFCNManager::writeHighSide(TxBitsBurst* txBurst)
{
//      mTransmitLock.lock();
        bool underrun;
        Time updateTime;
        mRadioModem.addBurst(txBurst,underrun,updateTime);
        if (underrun) {
                //LOG(NOTICE) << "tx underrun: " << *txBurst << " at time " << txBurst->time();
// XXX RPERRY                LOG(NOTICE) << "tx underrun: "<<getId() <<LOGVARP(txBurst) <<LOGVAR(updateTime);
                delete txBurst;
                //gNodeB.clock().set(updateTime.FN());
                // TODO -- update clock offset here
        }

//      mUnderrun = mUnderrun | underrun;
//      mTransmitLock.unlock();
}

void ::ARFCNManager::transmitLoop(void)
{
        int32_t currFN = gNodeB.clock().FN();
        while (1) {
          bool underrun;
          unsigned fnbefore = gNodeB.clock().FN();
          usleep(UMTS::gFrameMicroseconds/10);
          //LOG(INFO) << "transmitLoop"<<LOGVAR(currFN)<<LOGVAR(fnbefore)<<LOGVAR2("clock.FN",gNodeB.clock().FN()) <<LOGVAR2("mLastTransmitTime",mRadioModem.mLastTransmitTime);
          // (pat) I moved the test from after to before this loop, which prevents it from running ahead.
          // Documentation says usleep may return early, and this prevents advancing currFN in that case.
          while (Time(currFN) < Time(gNodeB.clock().FN())) {
            underrun = false;
            for (unsigned int i = 0; i < gFrameSlots;i++) {
              mRadioModem.transmitSlot(Time(currFN,i),underrun);
            }
            currFN++;
            currFN = currFN % gHyperframe;
                  // (pat) Converting to Time() is a way to use modulo arithmetic on FN.
          } //while (Time(currFN) < Time(gNodeB.clock().FN()));
        }
}

void ::ARFCNManager::receiveLoop(void)
{
        while (1) mRadioModem.receiveBurst();
}

bool ::ARFCNManager::powerOn()
{
        int status = sendCommand("POWERON");
        if (status!=0) {
                LOG(ALERT) << "POWERON failed with status " << status;
                return false;
        }
        return true;
}

bool ::ARFCNManager::resetFx3()
{
        int status = sendCommand("RESETFX3");
        if (status!=0) {
                LOG(ALERT) << "RESETFX3 failed with status " << status;
                return false;
        }
        return true;
}

bool ::ARFCNManager::sysPowerOn(bool warn)
{
	int status = sendCommand("SYSPOWERON");
	if (status!=0) {
		if (warn) {
			LOG(ALERT) << "SYSPOWERON failed with status " << status;
		} else {
			LOG(INFO) << "SYSPOWERON failed with status " << status;
		}
		
		return false;
	}
	return true;
}

bool ::ARFCNManager::txPowerOn(unsigned band)
{
	int status = sendCommand("TXPOWERON", band);
	if (status!=0) {
		LOG(ALERT) << "TXPOWERON failed with status " << status;
		return false;
	}
	return true;
}

bool ::ARFCNManager::setTxQmcGain(unsigned gain_A, unsigned gain_B, unsigned phase)
{
        char paramBuf[MAX_UDP_LENGTH];
        sprintf(paramBuf,"%d %d %d", gain_A, gain_B, phase);
        int status = sendCommand("SETTXQMCGAIN", paramBuf);
        if (status!=0) {
                LOG(ALERT) << "SETTXQMCGAIN failed with status " << status;
                return false;
        }
        return true;
}

bool ::ARFCNManager::setTxQmcOffset(unsigned offset_A, unsigned offset_B)
{
        char paramBuf[MAX_UDP_LENGTH];
        sprintf(paramBuf,"%d %d", offset_A, offset_B);
        int status = sendCommand("SETTXQMCOFFSET", paramBuf);
        if (status!=0) {
                LOG(ALERT) << "SETTXQMCOFFSET failed with status " << status;
                return false;
        }
        return true;
}

bool ::ARFCNManager::setRxQmcGain(unsigned gain_A, unsigned gain_B, unsigned phase)
{
        char paramBuf[MAX_UDP_LENGTH];
        sprintf(paramBuf,"%d %d %d", gain_A, gain_B, phase);
        int status = sendCommand("SETRXQMCGAIN", paramBuf);
        if (status!=0) {
                LOG(ALERT) << "SETRXQMCGAIN failed with status " << status;
                return false;
        }
        return true;
}

bool ::ARFCNManager::setRxQmcOffset(unsigned offset_A, unsigned offset_B)
{
        char paramBuf[MAX_UDP_LENGTH];
        sprintf(paramBuf,"%d %d", offset_A, offset_B);
        int status = sendCommand("SETRXQMCOFFSET", paramBuf);
        if (status!=0) {
                LOG(ALERT) << "SETRXQMCOFFSET failed with status " << status;
                return false;
        }
        return true;
}



// vim: ts=4 sw=4
