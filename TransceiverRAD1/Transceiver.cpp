/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
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
	Compilation switches
	TRANSMIT_LOGGING	write every burst on the given slot to a log
*/

#include <stdio.h>
#include "Transceiver.h"
#include <Logger.h>

#include <Configuration.h>
#include <FactoryCalibration.h>

extern ConfigurationTable gConfig;
extern FactoryCalibration gFactoryCalibration;

Transceiver::Transceiver(int wBasePort,
			 const char *TRXAddress,
			 int wSamplesPerSymbol,
			 UMTS::Time wTransmitLatency,
			 RadioInterface *wRadioInterface)
	:mDataSocket(wBasePort+2,TRXAddress,wBasePort+102),
	 mControlSocket(wBasePort+1,TRXAddress,wBasePort+101),
	 mClockSocket(wBasePort,TRXAddress,wBasePort+100)
{
  //UMTS::Time startTime(0,0);
  //UMTS::Time startTime(gHyperframe/2 - 4*216*60,0);
  UMTS::Time startTime(random() % 1024,0);

  mFIFOServiceLoopThread = new Thread(8*32768);  ///< thread to push bursts into transmit FIFO
  mRFIFOServiceLoopThread = new Thread(8*32768);
  mControlServiceLoopThread = new Thread(32768);       ///< thread to process control messages from UMTS core
  mTransmitPriorityQueueServiceLoopThread = new Thread(8*32768);///< thread to process transmit bursts from UMTS core


  mSamplesPerSymbol = wSamplesPerSymbol;
  mRadioInterface = wRadioInterface;
  mTransmitLatency = wTransmitLatency;
  mTransmitDeadlineClock = startTime;
  mLastClockUpdateTime = startTime;
  mLatencyUpdateTime = startTime;
  mRadioInterface->getClock()->set(startTime);

  mDelaySpread = gConfig.getNum("UMTS.Radio.MaxExpectedDelaySpread");

  txFullScale = mRadioInterface->fullScaleInputValue();
  rxFullScale = mRadioInterface->fullScaleOutputValue();

  signalVector emptyVector(UMTS::gSlotLen);
  UMTS::Time emptyTime(0,0);
  mEmptyTransmitBurst = new radioVector((const signalVector&) emptyVector,
					(UMTS::Time&) emptyTime);

  mOn = false;
  mTxFreq = 0.0;
  mRxFreq = 0.0;
  mPower = -10;
}

Transceiver::~Transceiver()
{
  mTransmitPriorityQueue.clear();
}
  

void Transceiver::addRadioVector(signalVector &burst,
				 UMTS::Time &wTime)
{
  // modulate and stick into queue 
  radioVector *newVec = new radioVector(burst,wTime);
  RN_MEMLOG(radioVector,newVec);
  mTransmitPriorityQueue.write(newVec);
}

void Transceiver::pushRadioVector(UMTS::Time &nowTime)
{

  // dump stale bursts, if any
  while (radioVector* staleBurst = mTransmitPriorityQueue.getStaleBurst(nowTime)) {
    LOG(NOTICE) << "dumping STALE burst in TRX->USRP interface burst:" << staleBurst->time() << " now:" << nowTime;
    writeClockInterface();
    delete staleBurst;
  }
  
  // if queue contains data at the desired timestamp, stick it into FIFO
  if (radioVector *next = (radioVector*) mTransmitPriorityQueue.getCurrentBurst(nowTime)) {
    //LOG(DEBUG) << "transmitFIFO: wrote burst " << next << " at time: " << nowTime;
    mRadioInterface->driveTransmitRadio(*(next),false); //fillerTable[modFN][TN]));
    delete next;
    return;
  }

  // Extremely rare that we get here.
  // we need to send a blank burst to the radio interface to update the timestamp
  LOG(INFO) << "Sending empty burst at " << nowTime;
  mRadioInterface->driveTransmitRadio(*(mEmptyTransmitBurst),true);

}

void Transceiver::start()
{
  mControlServiceLoopThread->start((void * (*)(void*))ControlServiceLoopAdapter,(void*) this);
}

void Transceiver::reset()
{
  mTransmitPriorityQueue.clear();
  //mTransmitFIFO->clear();
  //mReceiveFIFO->clear();
}

  
void Transceiver::driveControl()
{

  int MAX_PACKET_LENGTH = 100;

  // check control socket
  char buffer[MAX_PACKET_LENGTH];
  int msgLen = -1;
  buffer[0] = '\0';
 
  msgLen = mControlSocket.read(buffer);

  if (msgLen < 1) {
    return;
  }

  char cmdcheck[4];
  char command[MAX_PACKET_LENGTH];
  char response[MAX_PACKET_LENGTH];

  sscanf(buffer,"%3s %s",cmdcheck,command);
 
  writeClockInterface();

  if (strcmp(cmdcheck,"CMD")!=0) {
    LOG(WARNING) << "bogus message on control interface";
    return;
  }
  LOG(INFO) << "command is " << buffer;

  if (strcmp(command,"POWEROFF")==0) {
    // turn off transmitter/demod
    sprintf(response,"RSP POWEROFF 0"); 
  }
  else if (strcmp(command,"POWERON")==0) {
    // turn on transmitter/demod
    if (!mTxFreq || !mRxFreq) 
      sprintf(response,"RSP POWERON 1");
    else {
      sprintf(response,"RSP POWERON 0");
      if (!mOn) {
        // Prepare for thread start
        mPower = -20;
        mRadioInterface->start();

        // Start radio interface threads.
        mRFIFOServiceLoopThread->start((void * (*)(void*))RFIFOServiceLoopAdapter,(void*) this);
        mFIFOServiceLoopThread->start((void * (*)(void*))FIFOServiceLoopAdapter,(void*) this);
        mTransmitPriorityQueueServiceLoopThread->start((void * (*)(void*))TransmitPriorityQueueServiceLoopAdapter,(void*) this);
        writeClockInterface();

        mOn = true;
      }
    }
  }
  else if (strcmp(command,"SETRXGAIN")==0) {
    // FIXME -- Use the configuration table instead.
    int newGain;
    sscanf(buffer,"%3s %s %d",cmdcheck,command,&newGain);
    newGain = mRadioInterface->setRxGain(newGain);
    sprintf(response,"RSP SETRXGAIN 0 %d",newGain);
  }
  else if (strcmp(command,"SETTXATTEN")==0) {
    // set output power in dB
    int dbPwr;
    sscanf(buffer,"%3s %s %d",cmdcheck,command,&dbPwr);
    if (!mOn)
      sprintf(response,"RSP SETTXATTEN 1 %d",dbPwr);
    else {
        mRadioInterface->setPowerAttenuation(mPower + dbPwr);
      sprintf(response,"RSP SETTXATTEN 0 %d",dbPwr);
    }
  }
  else if (strcmp(command,"SETFREQOFFSET")==0) {
    // set output power in dB
    int tuneVoltage;
    sscanf(buffer,"%3s %s %d",cmdcheck,command,&tuneVoltage);
    if (!mOn)
      sprintf(response,"RSP SETFREQOFFSET 1 %d",tuneVoltage);
    else {
        mRadioInterface->setVCTCXO(tuneVoltage);
      sprintf(response,"RSP SETFREQOFFSET 0 %d",tuneVoltage);
    }
  }
  else if (strcmp(command,"SETPOWER")==0) {
    // set output power in dB
    int dbPwr;
    sscanf(buffer,"%3s %s %d",cmdcheck,command,&dbPwr);
    printf("buffer: %s\n", buffer);
    if (!mOn) 
      sprintf(response,"RSP SETPOWER 1 %d",dbPwr);
    else {
      mPower = dbPwr;
      printf("AAA\n");
      mRadioInterface->setPowerAttenuation(dbPwr + gConfig.getNum("TRX.TxAttenOffset"));   
      printf("BBB\n");
      sprintf(response,"RSP SETPOWER 0 %d",dbPwr);
    }
  }
  else if (strcmp(command,"ADJPOWER")==0) {
    // adjust power in dB steps
    int dbStep;
    sscanf(buffer,"%3s %s %d",cmdcheck,command,&dbStep);
    if (!mOn) 
      sprintf(response,"RSP ADJPOWER 1 %d",mPower);
    else {
      mPower += dbStep;
      sprintf(response,"RSP ADJPOWER 0 %d",mPower);
    }
  }
#define FREQOFFSET 0//11.2e3
  else if (strcmp(command,"RXTUNE")==0) {
    // tune receiver
    int freqKhz;
    sscanf(buffer,"%3s %s %d",cmdcheck,command,&freqKhz);
    mRxFreq = freqKhz*1.0e3+FREQOFFSET;
    if (!mRadioInterface->tuneRx(mRxFreq,gConfig.getNum("TRX.RadioFrequencyOffset"))) {
       LOG(ALERT) << "RX failed to tune";
       sprintf(response,"RSP RXTUNE 1 %d",freqKhz);
    }
    else
       sprintf(response,"RSP RXTUNE 0 %d",freqKhz);
  }
  else if (strcmp(command,"TXTUNE")==0) {
    // tune txmtr
    int freqKhz;
    sscanf(buffer,"%3s %s %d",cmdcheck,command,&freqKhz);
    //freqKhz = 890e3;
    mTxFreq = freqKhz*1.0e3+FREQOFFSET;
    if (!mRadioInterface->tuneTx(mTxFreq,gConfig.getNum("TRX.RadioFrequencyOffset"))) {
       LOG(ALERT) << "TX failed to tune";
       sprintf(response,"RSP TXTUNE 1 %d",freqKhz);
    }
    else
       sprintf(response,"RSP TXTUNE 0 %d",freqKhz);
  }
  else if (strcmp(command,"SETFREQOFFSET")==0) {
    // set output power in dB
    int tuneVoltage;
    sscanf(buffer,"%3s %s %d",cmdcheck,command,&tuneVoltage);
    if (!mOn)
      sprintf(response,"RSP SETFREQOFFSET 1 %d",tuneVoltage);
    else {
      mRadioInterface->setVCTCXO(tuneVoltage);
      sprintf(response,"RSP SETFREQOFFSET 0 %d",tuneVoltage);
    }
  }
  else {
    LOG(WARNING) << "bogus command " << command << " on control interface.";
  }

  LOG(DEBUG) << "response: " << response;

  mControlSocket.write(response,strlen(response)+1);

}

bool Transceiver::driveTransmitPriorityQueue() 
{

#if 1 
  char buffer[MAX_UDP_LENGTH];

  // check data socket
  size_t msgLen = mDataSocket.read(buffer);

  if (msgLen!=2*UMTS::gSlotLen+4) {
    LOG(ERR) << "badly formatted packet on UMTS->TRX interface";
    return false;
  }
#else
  radioVector *newBurst = (radioVector *) mT.read();
  if (!newBurst) return false;
  //char *buffer = (char *) mT.read();
  //if (!buffer) return false;
  int timeSlot = (int) newBurst->time().TN();
  uint16_t frameNum = (uint16_t) newBurst->time().FN();
#endif
  int timeSlot = (int) buffer[0];
  uint16_t frameNum = 0;
  for (int i = 0; i < 2; i++)
    frameNum = (frameNum << 8) | (0x0ff & buffer[i+1]);
  

  // periodically update UMTS core clock
  //LOG(DEBUG) << "mTransmitDeadlineClock " << mTransmitDeadlineClock
  //		<< " mLastClockUpdateTime " << mLastClockUpdateTime;
  if (mTransmitDeadlineClock > mLastClockUpdateTime + UMTS::Time(100,0))
    writeClockInterface();


  //LOG(DEBUG) << "rcvd. burst at: " << UMTS::Time(frameNum,timeSlot);
  
  static signalVector newBurst(UMTS::gSlotLen);
  signalVector::iterator itr = newBurst.begin();
  signed char *bufferItr = (signed char *) (buffer+3);

  while (itr < newBurst.end()) {
    *itr++ = complex((int) *bufferItr, 
		     (int) *(bufferItr+1));
    bufferItr++; bufferItr++;
    //if ((frameNum % 100) == 0) LOG(INFO) << complex((int) *bufferItr, (int) *(bufferItr+1));
    //bufferItr+=2;
  }

#if 1
  //free(buffer);
#endif 

  UMTS::Time currTime = UMTS::Time(frameNum,timeSlot);
  
#if 0
  addRadioVector(*(newBurst),currTime);
  delete newBurst; 
#else
  addRadioVector(newBurst,currTime);
#endif
 
  //LOG(INFO) "added burst - time: " << currTime << ". radio: " << mRadioInterface->getClock()->get() << ", dline: " << mTransmitDeadlineClock; 

  return true;


}
 
void Transceiver::driveReceiveFIFO() 
{

  radioVector *rxBurst = NULL;
  int RSSI;
  int TOA;  // in 1/256 of a symbol
  UMTS::Time burstTime;

  mRadioInterface->driveReceiveRadio(1024+mDelaySpread);

  rxBurst = (radioVector *) mReceiveFIFO->get();

  if (!rxBurst) return;

  //mR.wait(100);

  if (rxBurst) { 

    //mR.write(rxBurst);

    burstTime = rxBurst->time();
    /*LOG(DEBUG) << "burst parameters: "
	  << " time: " << burstTime
	  << " RSSI: " << RSSI
	  << " TOA: "  << TOA 
	  << " bits: " << *rxBurst;
   */
    int burstSz = 2*rxBurst->size()+3+1+1; 
//    char *burstString = new char[burstSz];
    char *burstString = (char *) malloc(sizeof(char)*burstSz);

    burstString[0] = burstTime.TN();
    for (int i = 0; i < 2; i++)
      burstString[1+i] = (burstTime.FN() >> ((1-i)*8)) & 0x0ff;
    burstString[3] = RSSI;
    radioVector::iterator burstItr = rxBurst->begin();
    char *burstPtr = burstString+4;
    for (unsigned int i = 0; i < UMTS::gSlotLen + 1024 + mDelaySpread; i++) {
      *burstPtr++ = (char) (int8_t) burstItr->real(); //round((burstItr->real())*255.0);
      *burstPtr++ = -(char) (int8_t) burstItr->imag(); //round((burstItr->imag())*255.0); 
      // if (i == 100) LOG(INFO) << "burstStr: " << burstItr->real();
      burstItr++;
    }
    
    burstString[burstSz-1] = '\0';
    delete rxBurst;
   

#if 1
    mDataSocket.write(burstString,burstSz);
    delete[] burstString;
#else
    //mR.write(burstString);
#endif
  }

}

void Transceiver::driveTransmitFIFO() 
{

  /**
      Features a carefully controlled latency mechanism, to 
      assure that transmit packets arrive at the radio/USRP
      before they need to be transmitted.

      Deadline clock indicates the burst that needs to be
      pushed into the FIFO right NOW.  If transmit queue does
      not have a burst, stick in filler data.
  */


  RadioClock *radioClock = (mRadioInterface->getClock());
  
  if (mOn) {
    radioClock->wait(); // wait until clock updates
    //LOG(DEBUG) << "radio clock " << radioClock->get();
    while (radioClock->get() + mTransmitLatency > mTransmitDeadlineClock) {
      // if underrun, then we're not providing bursts to radio/USRP fast
      //   enough.  Need to increase latency by one UMTS frame.
      if (mRadioInterface->isUnderrun()) {
        // only do latency update every 10 frames, so we don't over update
	if (radioClock->get() > mLatencyUpdateTime + UMTS::Time(10,0)) {
	  mTransmitLatency = mTransmitLatency + UMTS::Time(1,0);
	  if (mTransmitLatency > UMTS::Time(15,0)) mTransmitLatency = UMTS::Time(15,0);
	  LOG(NOTICE) << "new latency: " << mTransmitLatency;
	  mLatencyUpdateTime = radioClock->get();
	}
      }
      else {
        // if underrun hasn't occurred in the last sec (100 frames) drop
        //    transmit latency by a timeslot
	if (mTransmitLatency > UMTS::Time(1,0)) { // if latency is one frame or less, don't bother decreasing, it's low enough
            if (radioClock->get() > mLatencyUpdateTime + UMTS::Time(100,0)) {
	    mTransmitLatency.decTN();
	    LOG(NOTICE) << "reduced latency: " << mTransmitLatency;
	    mLatencyUpdateTime = radioClock->get();
	  }
	}
      }
      // time to push burst to transmit FIFO
      pushRadioVector(mTransmitDeadlineClock);
      mTransmitDeadlineClock.incTN();
    }
    
  }
  // FIXME -- This should not be a hard spin.
  // But any delay here causes us to throw omni_thread_fatal.
  //else radioClock->wait();
}



void Transceiver::writeClockInterface()
{
  char command[50];
  // FIXME -- This should be adaptive.
  sprintf(command,"IND CLOCK %llu",(unsigned long long) (mTransmitDeadlineClock.FN()+8));

  LOG(INFO) << "ClockInterface: sending " << command;

  mClockSocket.write(command,strlen(command)+1);

  mLastClockUpdateTime = mTransmitDeadlineClock;

}   
  



void *FIFOServiceLoopAdapter(Transceiver *transceiver)
{
  while (1) {
    //transceiver->driveReceiveFIFO();
    transceiver->driveTransmitFIFO();
    pthread_testcancel();
  }
  return NULL;
}

void *RFIFOServiceLoopAdapter(Transceiver *transceiver)
{
  while (1) {
    transceiver->driveReceiveFIFO();
    //transceiver->driveTransmitFIFO();
    pthread_testcancel();
  }
  return NULL;
}

void *ControlServiceLoopAdapter(Transceiver *transceiver)
{
  while (1) {
    transceiver->driveControl();
    pthread_testcancel();
  }
  return NULL;
}

void *TransmitPriorityQueueServiceLoopAdapter(Transceiver *transceiver)
{
  while (1) {
    bool stale = false;
    // Flush the UDP packets until a successful transfer.
    while (!transceiver->driveTransmitPriorityQueue()) {
      stale = true; 
    }
    if (stale) {
      // If a packet was stale, remind the UMTS stack of the clock.
      transceiver->writeClockInterface();
    }
    pthread_testcancel();
  }
  return NULL;
}
