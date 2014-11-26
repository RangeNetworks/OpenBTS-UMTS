#ifndef TRANSCEIVER_H
#define TRANSCEIVER_H

#include "RadioInterface.h"
#include "Interthread.h"
#include "UMTSCommon.h"
#include "Sockets.h"

#include <sys/types.h>
#include <sys/socket.h>

/** The Transceiver class, responsible for physical layer of basestation */
class Transceiver {
private:
  UDPSocket mDataSocket;           ///< socket for writing to/reading from UMTS core
  UDPSocket mControlSocket;        ///< socket for writing/reading control commands from UMTS core
  UDPSocket mClockSocket;          ///< socket for writing clock updates to UMTS core

  VectorQueue  mTransmitPriorityQueue;   ///< priority queue of transmit bursts received from UMTS core
  VectorFIFO*  mTransmitFIFO;      ///< radioInterface FIFO of transmit bursts 
  VectorFIFO*  mReceiveFIFO;       ///< radioInterface FIFO of receive bursts 

  RadioBurstFIFO  mT;
  RadioBurstFIFO  mR;

  Thread *mTxServiceLoopThread;    ///< thread to push/pull bursts into transmit/receive FIFO
  Thread *mRxServiceLoopThread;    ///< thread to push/pull bursts into transmit/receive FIFO
  Thread *mTransmitPriorityQueueServiceLoopThread;///< thread to process transmit bursts from UMTS core
  Thread *mControlServiceLoopThread;       ///< thread to process control messages from UMTS core

  bool mOn;                        ///< flag to indicate that transceiver is powered on
  int mPower;                      ///< the transmit power in dB
  int mDelaySpread;                ///< maximum expected delay spread, i.e. extend buffer when sending upstream

  UMTS::Time mTransmitLatency;     ///< latency between basestation clock and transmit deadline clock
  UMTS::Time mLatencyUpdateTime;   ///< last time latency was updated

  UMTS::Time mTransmitDeadlineClock;       ///< deadline for pushing bursts into transmit FIFO 
  UMTS::Time mLastClockUpdateTime;         ///< last time clock update was sent up to core
  radioVector *mEmptyTransmitBurst;

  RadioInterface *mRadioInterface; ///< associated radioInterface object

  /** modulate and add a burst to the transmit queue */
  void addRadioVector(signalVector &burst, UMTS::Time &wTime);

  /** Push modulated burst into transmit FIFO corresponding to a particular timestamp */
  void pushRadioVector(UMTS::Time &nowTime);

  /** send messages over the clock socket */
  void writeClockInterface(void);

  /** Start and stop I/O threads through the control socket API */
  bool start();
  void stop();

  /** Protect destructor accessable stop call */
  Mutex mLock;

public:
  /** Transceiver constructor 
      @param wBasePort base port number of UDP sockets
      @param TRXAddress IP address of the TRX manager, as a string
      @param wTransmitLatency initial setting of transmit latency
      @param radioInterface associated radioInterface object
  */
  Transceiver(int wBasePort,
        const char *TRXAddress,
        UMTS::Time wTransmitLatency,
        RadioInterface *wRadioInterface);

  /** Destructor */
  ~Transceiver();

  /** Start the control loop */
  void init(int wDelaySpread);

  /** attach the radioInterface receive FIFO */
  void receiveFIFO(VectorFIFO *wFIFO) { mReceiveFIFO = wFIFO; }

  /** attach the radioInterface transmit FIFO */
  void transmitFIFO(VectorFIFO *wFIFO) { mTransmitFIFO = wFIFO; }

  RadioBurstFIFO* highSideTransmitFIFO(void) { return &mT; }
  RadioBurstFIFO* highSideReceiveFIFO(void)  { return &mR; }

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

  friend void *TxServiceLoopAdapter(Transceiver *);
  friend void *RxServiceLoopAdapter(Transceiver *);
  friend void *ControlServiceLoopAdapter(Transceiver *);
  friend void *TransmitPriorityQueueServiceLoopAdapter(Transceiver *);

  void reset();
};

/** FIFO thread loop */
void *TxServiceLoopAdapter(Transceiver *);
void *RxServiceLoopAdapter(Transceiver *);

/** control message handler thread loop */
void *ControlServiceLoopAdapter(Transceiver *);

/** transmit queueing thread loop */
void *TransmitPriorityQueueServiceLoopAdapter(Transceiver *);

#endif /* TRANSCEIVER_H */
