#ifndef __RADIOINTERFACE_H__
#define __RADIOINTERFACE_H__

#include "signalVector.h"
#include "LinkedLists.h"
#include "RadioDevice.h"
#include "Resampler.h"

#include <UMTSCommon.h>
#include <Interthread.h>

/** class used to organize UMTS bursts by UMTS timestamps */
class radioVector : public signalVector {
private:
  UMTS::Time mTime;   ///< the burst's UMTS timestamp 

public:
  /** constructor */
  radioVector(const signalVector& wVector,
              UMTS::Time& wTime): signalVector(wVector),mTime(wTime) { };

  radioVector(unsigned wSz, UMTS::Time& wTime)
    : signalVector(wSz), mTime(wTime) { };

  /** timestamp read and write operators */
  UMTS::Time time() const { return mTime; }
  void time(const UMTS::Time& wTime) { mTime = wTime; }

  /** comparison operator, used for sorting */
  bool operator>(const radioVector& other) const
  {
    return mTime > other.mTime;
  }
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
  Mutex mLock;

public:
  unsigned size() { return mQ.size(); }

  void put(radioVector *ptr)
  {
    ScopedLock lock(mLock);
    mQ.put((void*) ptr);
  }

  radioVector *get()
  {
    ScopedLock lock(mLock);
    return (radioVector*) mQ.get();
  }
};

/** FIFO to store */
class RadioBurstFIFO : public InterthreadQueueWithWait<radioVector> { };

/** the basestation clock class */
class RadioClock {
private:
  UMTS::Time mClock;
  Mutex mLock;
  Signal updateSignal;

public:
  /** Set clock */
  void set(const UMTS::Time& wTime)
  {
    ScopedLock lock(mLock);
    mClock = wTime;
    updateSignal.signal();
  }

  /** Increment clock */
  void incTN()
  {
    ScopedLock lock(mLock);
    mClock.incTN();
    updateSignal.signal();
  }

  /** Get clock value */
  UMTS::Time get()
  {
    ScopedLock lock(mLock);
    return mClock;
  }

  void wait()
  {
    ScopedLock lock(mLock);
    updateSignal.wait(mLock);
  }

  void signal()
  {
    ScopedLock lock(mLock);
    updateSignal.signal();
  }
};

/** class to interface the transceiver with the USRP */
class RadioInterface {
private:
  VectorFIFO  mReceiveFIFO;
  RadioDevice *mRadio;

  size_t sendCursor;
  size_t recvCursor;
  size_t inchunk, outchunk;

  bool underrun;
  bool overrun;
  long long writeTimestamp;
  long long readTimestamp;

  RadioClock mClock;
  int receiveOffset;

  bool mOn;

  /** base scaling assumes no digital attenuation */
  double baseScaling;

  /** digital scaling attenuation factor */
  double powerScaling;

  signalVector *finalVec;

  Resampler *upsampler;
  Resampler *dnsampler;

  signalVector *innerSendBuffer;
  signalVector *innerRecvBuffer;
  signalVector *outerSendBuffer;
  signalVector *outerRecvBuffer;

  short *convertSendBuffer;
  short *convertRecvBuffer;

  /** format samples to USRP */
  int radioifyVector(signalVector &wVector, float *shortVector, bool zero);

  /** format samples from USRP */
  void unRadioifyVector(float *floatVector, signalVector &wVector);

  /** push UMTS bursts into the transmit buffer */
  bool pushBuffer(void);

  /** pull UMTS bursts from the receive buffer */
  bool pullBuffer(void);

public:
  /** constructor */
  RadioInterface(RadioDevice* wRadio = NULL, int receiveOffset = 3,
                 UMTS::Time wStartTime = UMTS::Time(0));

  /** destructor */
  ~RadioInterface();

  bool init();

  /** start the interface */
  bool start();
  bool stop();

  /** check for underrun, resets underrun value */
  bool isUnderrun()
  {
    bool retVal = underrun;
    underrun = false;
    return retVal;
  }

  /** return the receive FIFO */
  VectorFIFO* receiveFIFO() { return &mReceiveFIFO;}

  /** return the basestation clock */
  RadioClock* getClock(void) { return &mClock;};

  /** set receive gain */
  double setRxGain(double dB)
  {
    if (mRadio)
      return mRadio->setRxGain(dB);
    else
      return -1;
  }

  /** get receive gain */
  double getRxGain(void)
  {
    if (mRadio)
      return mRadio->getRxGain();
    else
      return -1;
  }

  /** set transmit frequency */
  bool tuneTx(double freq);

  /** set receive frequency */
  bool tuneRx(double freq);

  /** drive transmission of UMTS bursts */
  void driveTransmitRadio(signalVector &radioBurst, bool zeroBurst);

  /** drive reception of UMTS bursts */
  void driveReceiveRadio(int guardPeriod = 0);

  int setPowerAttenuation(int atten);

  /** returns the full-scale transmit amplitude **/
  double fullScaleInputValue();

  /** returns the full-scale receive amplitude **/
  double fullScaleOutputValue();

  /** set thread priority on current thread */
  void setPriority() { mRadio->setPriority(); }

  /** get transport window type of attached device */
  enum RadioDevice::TxWindowType getWindowType()
  {
    return mRadio->getWindowType();
  }

protected:
  /** drive synchronization of Tx/Rx of USRP */
  void alignRadio();

  /** reset the interface */
  void reset();
};

#endif /* __RADIOINTERFACE_H__ */

