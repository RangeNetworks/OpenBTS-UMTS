/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 * 
 * This software is distributed under the terms of the GNU General Public 
 * License version 3. See the COPYING and NOTICE files in the current
 * directory for licensing information.
 * 
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#ifndef __RADIOINTERFACE_H__
#define __RADIOINTERFACE_H__

#include "signalVector.h"  
#include "LinkedLists.h"
#include "radioDevice.h"

#include <UMTSCommon.h>
#include <Interthread.h>

/** samples per UMTS symbol */
#define SAMPSPERSYM 1 
#define INCHUNK    (4*2560)
#define OUTCHUNK   (4*2560)

/** class used to organize UMTS bursts by UMTS timestamps */
class radioVector : public signalVector {

private:

  UMTS::Time mTime;   ///< the burst's UMTS timestamp 

public:
  /** constructor */
  radioVector(const signalVector& wVector,
              UMTS::Time& wTime): signalVector(wVector),mTime(wTime) {};

  radioVector(unsigned wSz, UMTS::Time& wTime): signalVector(wSz),mTime(wTime) {};

  /** timestamp read and write operators */
  UMTS::Time time() const { return mTime;}
  void time(const UMTS::Time& wTime) { mTime = wTime;}

  /** comparison operator, used for sorting */
  bool operator>(const radioVector& other) const {return mTime > other.mTime;}

};

/** a priority queue of radioVectors, i.e. UMTS bursts, sorted so that earliest element is at top */
class VectorQueue : public InterthreadPriorityQueue<radioVector> {

public:

  /** the top element of the queue */
  UMTS::Time nextTime() const;

  /**
    Get stale burst, if any.
    @param targTime The target time.
    @return Pointer to burst older than target time, removed from queue, or NULL.
  */
  radioVector* getStaleBurst(const UMTS::Time& targTime);

  /**
    Get current burst, if any.
    @param targTime The target time.
    @return Pointer to burst at the target time, removed from queue, or NULL.
  */
  radioVector* getCurrentBurst(const UMTS::Time& targTime);


};

/** a FIFO of radioVectors */
class VectorFIFO {

private:

      PointerFIFO mQ;
      Mutex      mLock;

public:

      unsigned size() {return mQ.size();}

      void put(radioVector *ptr) {ScopedLock lock(mLock); mQ.put((void*) ptr);}

      radioVector *get() { ScopedLock lock(mLock); return (radioVector*) mQ.get();}

};

/** FIFO to store */
class RadioBurstFIFO : public InterthreadQueueWithWait<radioVector> {};

#if 0
{

private:

  PointerFIFO mQ;
  Mutex mLock;
  Signal updateSignal;

public:

  /** put burst*/
  void put(char *x) { ScopedLock lock(mLock); mQ.put((void *) x); fprintf(stdout,"IN PUT: %u\n",mQ.size()); updateSignal.signal();}

  /** get burst */
  char* get() { ScopedLock lock(mLock); fprintf(stdout,"IN GET\n"); if (!mQ.get()) updateSignal.wait(mLock); return (char *) mQ.get(); }

  unsigned size() {return mQ.size();}
};
#endif

/** the basestation clock class */
class RadioClock {

private:

  UMTS::Time mClock;
  Mutex mLock;
  Signal updateSignal;

public:

  /** Set clock */
  void set(const UMTS::Time& wTime) { ScopedLock lock(mLock); mClock = wTime; updateSignal.signal();}
  //void set(const UMTS::Time& wTime) { ScopedLock lock(mLock); mClock = wTime; updateSignal.broadcast();;}

  /** Increment clock */
  void incTN() { ScopedLock lock(mLock); mClock.incTN(); updateSignal.signal();}
  //void incTN() { ScopedLock lock(mLock); mClock.incTN(); updateSignal.broadcast();}

  /** Get clock value */
  UMTS::Time get() { ScopedLock lock(mLock); return mClock; }

  /** Wait until clock has changed */
  //void wait() {ScopedLock lock(mLock); updateSignal.wait(mLock,1);}
  // FIXME -- If we take away the timeout, a lot of threads don't start.  Why?
  void wait() {ScopedLock lock(mLock); updateSignal.wait(mLock);}

};

/** class to interface the transceiver with the USRP */
class RadioInterface {

private:

  Thread mAlignRadioServiceLoopThread;	      ///< thread that synchronizes transmit and receive sections

  VectorFIFO  mReceiveFIFO;		      ///< FIFO that holds receive  bursts

  RadioDevice *mRadio;			      ///< the USRP object
 
  short *sendBuffer; //[2*2*INCHUNK];
  unsigned sendCursor;

  short *rcvBuffer; //[2*2*OUTCHUNK];
  unsigned rcvCursor;

  signalVector *txHistoryVector;
  signalVector *rxHistoryVector;
  signalVector *inverseCICFilter;
  signalVector *rcvInverseCICFilter;
 
  bool underrun;			      ///< indicates writes to USRP are too slow
  bool overrun;				      ///< indicates reads from USRP are too slow
  TIMESTAMP writeTimestamp;		      ///< sample timestamp of next packet written to USRP
  TIMESTAMP readTimestamp;		      ///< sample timestamp of next packet read from USRP

  RadioClock mClock;                          ///< the basestation clock!

  int samplesPerSymbol;			      ///< samples per UMTS symbol
  int receiveOffset;                          ///< offset b/w transmit and receive UMTS timestamps, in timeslots
  int mRadioOversampling;

  bool mOn;				      ///< indicates radio is on

  double powerScaling;

  bool loadTest;
  int mNumARFCNs;
  signalVector *finalVec;

  /** format samples to USRP */ 
  short *radioifyVector(signalVector &wVector, short *shortVector, double scale, bool zeroOut);

  /** format samples from USRP */
  void unRadioifyVector(short *shortVector, signalVector &wVector);

  /** push UMTS bursts into the transmit buffer */
  void pushBuffer(void);

  /** pull UMTS bursts from the receive buffer */
  void pullBuffer(void);

public:

  /** start the interface */
  void start();

  /** constructor */
  RadioInterface(RadioDevice* wRadio = NULL,
		 int receiveOffset = 3,
		 int wRadioOversampling = SAMPSPERSYM,
		 bool wLoadTest = false,
		 UMTS::Time wStartTime = UMTS::Time(0));
    
  /** destructor */
  ~RadioInterface();

  void setSamplesPerSymbol(int wSamplesPerSymbol) {if (!mOn) samplesPerSymbol = wSamplesPerSymbol;}

  int getSamplesPerSymbol() { return samplesPerSymbol;}

  /** check for underrun, resets underrun value */
  bool isUnderrun() { bool retVal = underrun; underrun = false; return retVal;}
  
  /** attach an existing USRP to this interface */
  void attach(RadioDevice *wRadio, int wRadioOversampling) {if (!mOn) {mRadio = wRadio; mRadioOversampling = SAMPSPERSYM;} }

  /** return the receive FIFO */
  VectorFIFO* receiveFIFO() { return &mReceiveFIFO;}

  /** return the basestation clock */
  RadioClock* getClock(void) { return &mClock;};

 /** set receive gain */
  double setRxGain(double dB) {if (mRadio) return mRadio->setRxGain(dB); else return -1;}

  /** get receive gain */
  double getRxGain(void) {if (mRadio) return mRadio->getRxGain(); else return -1;}

  /** tune VCTCXO */
  bool setVCTCXO(unsigned int tuneVoltage);

  /** set transmit frequency */
  bool tuneTx(double freq, double adjFreq);

  /** set receive frequency */
  bool tuneRx(double freq, double adjFreq);

  /** drive transmission of UMTS bursts */
  void driveTransmitRadio(signalVector &radioBurst, bool zeroBurst);

  /** drive reception of UMTS bursts */
  void driveReceiveRadio(int guardPeriod = 0);

  void setPowerAttenuation(double atten);

  /** returns the full-scale transmit amplitude **/
  double fullScaleInputValue();

  /** returns the full-scale receive amplitude **/
  double fullScaleOutputValue();


protected:

  /** drive synchronization of Tx/Rx of USRP */
  void alignRadio();

  /** reset the interface */
  void reset();

  friend void *AlignRadioServiceLoopAdapter(RadioInterface*);

};

/** synchronization thread loop */
void *AlignRadioServiceLoopAdapter(RadioInterface*);

#endif
