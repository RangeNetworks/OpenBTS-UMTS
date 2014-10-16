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

/*
	Compilation switches
	TRANSMIT_LOGGING	write every burst on the given slot to a log
*/

#include "radioInterface.h"
#include "Interthread.h"
#include "UMTSCommon.h"
#include "Sockets.h"

#include <sys/types.h>
#include <sys/socket.h>

/** Define this to be the slot number to be logged. */
//#define TRANSMIT_LOGGING 1

/** The Transceiver class, responsible for physical layer of basestation */
class Transceiver {
  
private:

  UMTS::Time mTransmitLatency;     ///< latency between basestation clock and transmit deadline clock
  UMTS::Time mLatencyUpdateTime;   ///< last time latency was updated

  UDPSocket mDataSocket;	  ///< socket for writing to/reading from UMTS core
  UDPSocket mControlSocket;	  ///< socket for writing/reading control commands from UMTS core
  UDPSocket mClockSocket;	  ///< socket for writing clock updates to UMTS core

  VectorQueue  mTransmitPriorityQueue;   ///< priority queue of transmit bursts received from UMTS core
  VectorFIFO*  mTransmitFIFO;     ///< radioInterface FIFO of transmit bursts 
  VectorFIFO*  mReceiveFIFO;      ///< radioInterface FIFO of receive bursts 

  RadioBurstFIFO  mT;
  RadioBurstFIFO  mR;

  Thread *mFIFOServiceLoopThread;  ///< thread to push/pull bursts into transmit/receive FIFO
  Thread *mRFIFOServiceLoopThread;  ///< thread to push/pull bursts into transmit/receive FIFO
  Thread *mControlServiceLoopThread;       ///< thread to process control messages from UMTS core
  Thread *mTransmitPriorityQueueServiceLoopThread;///< thread to process transmit bursts from UMTS core

  UMTS::Time mTransmitDeadlineClock;       ///< deadline for pushing bursts into transmit FIFO 
  UMTS::Time mLastClockUpdateTime;         ///< last time clock update was sent up to core
  radioVector *mEmptyTransmitBurst;

  RadioInterface *mRadioInterface;	  ///< associated radioInterface object
  double txFullScale;                     ///< full scale input to radio
  double rxFullScale;                     ///< full scale output to radio

  /** modulate and add a burst to the transmit queue */
  void addRadioVector(signalVector &burst,
		      UMTS::Time &wTime);

  /** Push modulated burst into transmit FIFO corresponding to a particular timestamp */
  void pushRadioVector(UMTS::Time &nowTime);

  /** send messages over the clock socket */
  void writeClockInterface(void);

  int mSamplesPerSymbol;               ///< number of samples per UMTS symbol

  bool mOn;			       ///< flag to indicate that transceiver is powered on
  double mTxFreq;                      ///< the transmit frequency
  double mRxFreq;                      ///< the receive frequency
  int mPower;                          ///< the transmit power in dB

  int mDelaySpread;			///< maximum expected delay spread, i.e. extend buffer when sending upstream

public:

  /** Transceiver constructor 
      @param wBasePort base port number of UDP sockets
      @param TRXAddress IP address of the TRX manager, as a string
      @param wSamplesPerSymbol number of samples per UMTS symbol
      @param wTransmitLatency initial setting of transmit latency
      @param radioInterface associated radioInterface object
  */
  Transceiver(int wBasePort,
	      const char *TRXAddress,
	      int wSamplesPerSymbol,
	      UMTS::Time wTransmitLatency,
	      RadioInterface *wRadioInterface);
   
  /** Destructor */
  ~Transceiver();

  /** start the Transceiver */
  void start();

  /** attach the radioInterface receive FIFO */
  void receiveFIFO(VectorFIFO *wFIFO) { mReceiveFIFO = wFIFO;}

  /** attach the radioInterface transmit FIFO */
  void transmitFIFO(VectorFIFO *wFIFO) { mTransmitFIFO = wFIFO;}

  RadioBurstFIFO* highSideTransmitFIFO(void) { return &mT;}
  RadioBurstFIFO* highSideReceiveFIFO(void)  { return &mR;}


protected:

  /** drive reception and demodulation of UMTS bursts */ 
  void driveReceiveFIFO();

  /** drive transmission of UMTS bursts */
  void driveTransmitFIFO();

  /** drive handling of control messages from UMTS core */
  void driveControl();

  /**
    drive modulation and sorting of UMTS bursts from UMTS core
    @return true if a burst was transferred successfully
  */
  bool driveTransmitPriorityQueue();

  friend void *FIFOServiceLoopAdapter(Transceiver *);

  friend void *RFIFOServiceLoopAdapter(Transceiver *);

  friend void *ControlServiceLoopAdapter(Transceiver *);

  friend void *TransmitPriorityQueueServiceLoopAdapter(Transceiver *);

  void reset();
};

/** FIFO thread loop */
void *FIFOServiceLoopAdapter(Transceiver *);
void *RFIFOServiceLoopAdapter(Transceiver *);

/** control message handler thread loop */
void *ControlServiceLoopAdapter(Transceiver *);

/** transmit queueing thread loop */
void *TransmitPriorityQueueServiceLoopAdapter(Transceiver *);

