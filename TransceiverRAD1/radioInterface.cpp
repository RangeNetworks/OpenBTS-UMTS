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

//#define NDEBUG
#include "radioInterface.h"
#include "sigProcLib.h"
#include <Logger.h>


UMTS::Time VectorQueue::nextTime() const
{
  UMTS::Time retVal;
  ScopedLock lock(mLock);
  while (mQ.size()==0) mWriteSignal.wait(mLock);
  return mQ.top()->time();
}

radioVector* VectorQueue::getStaleBurst(const UMTS::Time& targTime)
{
  ScopedLock lock(mLock);
  if ((mQ.size()==0)) {
    return NULL;
  }
  if (mQ.top()->time() < targTime) {
    radioVector* retVal = mQ.top();
    mQ.pop();
    return retVal;
  }
  return NULL;
}


radioVector* VectorQueue::getCurrentBurst(const UMTS::Time& targTime)
{
  ScopedLock lock(mLock);
  if ((mQ.size()==0)) {
    return NULL;
  }
  if (mQ.top()->time() == targTime) {
    radioVector* retVal = mQ.top();
    mQ.pop();
    return retVal;
  }
  return NULL;
}



RadioInterface::RadioInterface(RadioDevice *wRadio,
                               int wReceiveOffset,
			       int wRadioOversampling,
			       bool wLoadTest,
			       UMTS::Time wStartTime)

{
  underrun = false;
 
  sendCursor = 0; 
  rcvCursor = 0;
  mOn = false;
  
  mRadio = wRadio;
  receiveOffset = wReceiveOffset;
  samplesPerSymbol = wRadioOversampling;
  mClock.set(wStartTime);
  powerScaling = 1.0;

  loadTest = wLoadTest;

#define FILTLEN 17

  inverseCICFilter = new signalVector(FILTLEN);
  RN_MEMLOG(signalVector,inverseCICFilter);
  float invFilt[] = {0.010688,-0.021274,0.040037,-0.06247,0.10352,-0.15486,0.23984,-0.42166,0.97118,-0.42166,0.23984,-0.15486,0.10352,-0.06247,0.040037,-0.021274,0.010688};
  float invFiltRcv[] = {-3.7048e-03,-1.8549e-04,8.5879e-03,-2.1284e-02,3.8077e-02,-5.8165e-02,8.5446e-02,-1.5441e-01,1.0650,-1.5441e-01,8.5446e-02,-5.8165e-02,3.8077e-02,-2.1284e-02,8.5879e-03,-1.8549e-04,-3.7048e-03};

  signalVector::iterator itr = inverseCICFilter->begin();
  inverseCICFilter->isRealOnly(true);
  inverseCICFilter->setSymmetry(ABSSYM);
  for (int i = 0; i < FILTLEN; i++)  
    *itr++ = complex(invFilt[i],0.0);
  rcvInverseCICFilter = new signalVector(*inverseCICFilter);
  RN_MEMLOG(signalVector,rcvInverseCICFilter);
  rcvInverseCICFilter->isRealOnly(true);
  rcvInverseCICFilter->setSymmetry(ABSSYM);
  itr = rcvInverseCICFilter->begin();
  for (int i = 0; i < FILTLEN; i++)
    *itr++ = complex(invFiltRcv[i],0.0);
  scaleVector(*rcvInverseCICFilter,0.5*127.0/mRadio->fullScaleOutputValue());

  txHistoryVector = new signalVector(FILTLEN-1);
  RN_MEMLOG(signalVector,txHistoryVector);
  rxHistoryVector = new signalVector(FILTLEN-1);
  RN_MEMLOG(signalVector,rxHistoryVector);

}

RadioInterface::~RadioInterface(void) {
  if (rcvBuffer!=NULL) delete rcvBuffer;
  //mReceiveFIFO.clear();
}

double RadioInterface::fullScaleInputValue(void) {
  return mRadio->fullScaleInputValue();
}

double RadioInterface::fullScaleOutputValue(void) {
  return mRadio->fullScaleOutputValue();
}


void RadioInterface::setPowerAttenuation(double dBAtten)
{
  float HWdBAtten = mRadio->setTxGain(-dBAtten);
  dBAtten -= (-HWdBAtten);
  float linearAtten = powf(10.0F,0.1F*dBAtten);
  if (linearAtten < 1.0)
    powerScaling = 1.0;
  else
    powerScaling = 1.0/sqrt(linearAtten);
  LOG(INFO) << "setting HW gain to " << HWdBAtten << " and power scaling to " << powerScaling;
}


short *RadioInterface::radioifyVector(signalVector &wVector, short *retVector, double scale, bool zeroOut) 
{


  signalVector::iterator itr = wVector.begin();
  short *shortItr = retVector;
  if (zeroOut) {
    while (itr < wVector.end()) {
      *shortItr++ = 0;
      *shortItr++ = 0;
      itr++;
    }
  }
  else if (scale != 1.0) { 
    while (itr < wVector.end()) {
      *shortItr++ = (short) (itr->real()*scale);
      *shortItr++ = (short) (itr->imag()*scale);
      itr++;
    }
  }
  else {
    while (itr < wVector.end()) {
      *shortItr++ = (short) (itr->real());
      *shortItr++ = (short) (itr->imag());
      itr++;
    }
  }

  return retVector;

}

void RadioInterface::unRadioifyVector(short *shortVector, signalVector& newVector)
{
  
  signalVector::iterator itr = newVector.begin();
  short *shortItr = shortVector;
  while (itr < newVector.end()) {
    *itr++ = Complex<float>(*shortItr,(*(shortItr+1)));
    //LOG(INFO) << (*(itr-1));
    shortItr += 2;
  }

}


bool started = false;

void RadioInterface::pushBuffer(void) {

  if (sendCursor < 2*INCHUNK*samplesPerSymbol) return;

  // send resampleVector
  int samplesWritten = mRadio->writeSamples(sendBuffer,
					  INCHUNK*samplesPerSymbol,
					  &underrun,
					  writeTimestamp); 
  //LOG(DEBUG) << "writeTimestamp: " << writeTimestamp << ", samplesWritten: " << samplesWritten;
   
  writeTimestamp += (TIMESTAMP) samplesWritten;

  if (sendCursor > 2*samplesWritten) 
    memcpy(sendBuffer,sendBuffer+samplesWritten*2,sizeof(short)*(sendCursor-2*samplesWritten));
  sendCursor = sendCursor - 2*samplesWritten;
}


void RadioInterface::pullBuffer(void)
{
   
  bool localUnderrun;

   // receive receiveVector
  short* shortVector = rcvBuffer+rcvCursor;  
  //LOG(DEBUG) << "Reading USRP samples at timestamp " << readTimestamp;
  int samplesRead = mRadio->readSamples(shortVector,OUTCHUNK*samplesPerSymbol,&overrun,readTimestamp,&localUnderrun);
  underrun |= localUnderrun;
  readTimestamp += (TIMESTAMP) samplesRead;
  while (samplesRead < OUTCHUNK*samplesPerSymbol) {
    int oldSamplesRead = samplesRead;
    samplesRead += mRadio->readSamples(shortVector+2*samplesRead,
				     OUTCHUNK*samplesPerSymbol-samplesRead,
				     &overrun,
				     readTimestamp,
				     &localUnderrun);
    underrun |= localUnderrun;
    readTimestamp += (TIMESTAMP) (samplesRead - oldSamplesRead);
  }
  //LOG(DEBUG) << "samplesRead " << samplesRead;

  rcvCursor += samplesRead*2;

}

bool RadioInterface::setVCTCXO(unsigned int tuneVoltage)
{
  return mRadio->setVCTCXO(tuneVoltage);

}

bool RadioInterface::tuneTx(double freq, double adjFreq)
{
  return mRadio->setTxFreq(freq, adjFreq);
}

bool RadioInterface::tuneRx(double freq, double adjFreq)
{
  return mRadio->setRxFreq(freq, adjFreq);
}


void RadioInterface::start()
{
  LOG(INFO) << "starting radio interface...";
  mAlignRadioServiceLoopThread.start((void * (*)(void*))AlignRadioServiceLoopAdapter,
                                     (void*)this);
  writeTimestamp = mRadio->initialWriteTimestamp();
  readTimestamp = mRadio->initialReadTimestamp()+(TIMESTAMP) receiveOffset;
  mRadio->start(); 
  LOG(DEBUG) << "Radio started";
  mRadio->updateAlignment(writeTimestamp-10000); 
  mRadio->updateAlignment(writeTimestamp-10000);

  sendBuffer = new short[2*2*INCHUNK*samplesPerSymbol];
  rcvBuffer = new short[2*2*(2*OUTCHUNK)*samplesPerSymbol];
 
  mOn = true;
}

void *AlignRadioServiceLoopAdapter(RadioInterface *radioInterface)
{
  while (1) {
    radioInterface->alignRadio();
    pthread_testcancel();
  }
  return NULL;
}

void RadioInterface::alignRadio() {
  sleep(60);
  mRadio->updateAlignment(writeTimestamp+ (TIMESTAMP) 100000);
}

void RadioInterface::driveTransmitRadio(signalVector &radioBurst, bool zeroBurst) {

  if (!mOn) return;

  if (zeroBurst) {
        writeTimestamp += (TIMESTAMP) radioBurst.size(); //samplesWritten;
        return;
  }


  signalVector txFiltVector(*txHistoryVector,radioBurst);
  signalVector txVector(radioBurst.size());
  convolve(&txFiltVector,inverseCICFilter,&txVector,CUSTOM,FILTLEN/2-1,txVector.size());
  //signalVector txVector(radioBurst);
  //LOG(INFO) << "txVector: " << txVector.segment(0,100);
  radioifyVector(txVector, sendBuffer+sendCursor, 50.0*powerScaling, zeroBurst);

  radioBurst.segmentCopyTo(*txHistoryVector,radioBurst.size()-(FILTLEN-1),FILTLEN-1);

  sendCursor += (radioBurst.size()*2);

  pushBuffer();
}

void RadioInterface::driveReceiveRadio(int guardPeriod) {

  if (!mOn) return;

  if (mReceiveFIFO.size() > 150) return;

  pullBuffer();

  UMTS::Time rcvClock = mClock.get();
  int rcvSz = rcvCursor/2;
  int readSz = 0;
  const int symbolsPerSlot = UMTS::gSlotLen;

  // while there's enough data in receive buffer, form received 
  //    UMTS bursts and pass up to Transceiver
  int vecSz = symbolsPerSlot*samplesPerSymbol + guardPeriod;
  while (rcvSz > vecSz) {
    UMTS::Time tmpTime = rcvClock;
    mClock.incTN();
    rcvClock.incTN();

    signalVector rxDataVector(vecSz);
    unRadioifyVector(rcvBuffer+readSz*2,rxDataVector);
    signalVector rxFiltVector(*rxHistoryVector,rxDataVector);
    signalVector rxVector(vecSz);
    convolve(&rxFiltVector,rcvInverseCICFilter,&rxVector,CUSTOM,FILTLEN/2-1,vecSz);
    if (rcvClock.FN() >= 0) {
      //LOG(DEBUG) << "FN: " << rcvClock.FN();
      radioVector *rxBurst = NULL;
      //if (!loadTest)
        rxBurst = new radioVector(rxVector,tmpTime);
      //else {
      //  rxBurst = new radioVector(*finalVec,tmpTime); 
      //}
      mReceiveFIFO.put(rxBurst); 
    }
    //if (mReceiveFIFO.size() >= 16) mReceiveFIFO.wait(8);
    //LOG(DEBUG) << "receiveFIFO: wrote radio vector at time: " << mClock.get() << ", new size: " << mReceiveFIFO.size() ;
    readSz += (symbolsPerSlot)*samplesPerSymbol;
    rcvSz -= (symbolsPerSlot)*samplesPerSymbol;
    rxDataVector.segmentCopyTo(*rxHistoryVector,symbolsPerSlot*samplesPerSymbol-(FILTLEN-1),FILTLEN-1); 
  }

  if (readSz > 0) { 
    memcpy(rcvBuffer,rcvBuffer+2*readSz,sizeof(short)*(rcvCursor-2*readSz));
    rcvCursor = rcvCursor-2*readSz;
  }
} 
  
