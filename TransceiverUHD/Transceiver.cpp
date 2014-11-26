/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 * Copyright 2014 Ettus Research LLC
 * 
 * This software is distributed under the terms of the GNU General Public 
 * License version 3. See the COPYING and NOTICE files in the current
 * directory for licensing information.
 * 
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include <stdio.h>
#include <Logger.h>
#include "Transceiver.h"

/* Clock indication reporting interval in frames */
#define CLK_IND_INTERVAL          100

/* Default attenuation value in dB */
#define DEFAULT_ATTEN             20

Transceiver::Transceiver(int wBasePort,
                         const char *wTRXAddress,
                         UMTS::Time wTransmitLatency,
                         RadioInterface *wRadioInterface)
  : mDataSocket(wBasePort + 2, wTRXAddress, wBasePort + 102),
    mControlSocket(wBasePort + 1, wTRXAddress, wBasePort + 101),
    mClockSocket(wBasePort, wTRXAddress, wBasePort + 100),
    mTxServiceLoopThread(NULL), mRxServiceLoopThread(NULL),
    mTransmitPriorityQueueServiceLoopThread(NULL),
    mControlServiceLoopThread(NULL), mOn(false), mPower(DEFAULT_ATTEN),
    mTransmitLatency(wTransmitLatency), mRadioInterface(wRadioInterface)
{
  signalVector emptyVector(UMTS::gSlotLen);
  UMTS::Time emptyTime(0, 0);
  mEmptyTransmitBurst = new radioVector((const signalVector&) emptyVector,
                                        (UMTS::Time&) emptyTime);
}

Transceiver::~Transceiver()
{
  stop();

  if (mControlServiceLoopThread) {
    mControlServiceLoopThread->cancel();
    mControlServiceLoopThread->join();
    delete mControlServiceLoopThread;
  }

  delete mEmptyTransmitBurst;
}

/*
 * Initialize transceiver
 *
 * Start or restart the control loop. Any further control is handled through the
 * socket API. Randomize the central radio clock set the downlink burst
 * counters. Note that the clock will not update until the radio starts, but we
 * are still expected to report clock indications through control channel
 * activity.
 */
void Transceiver::init(int wDelaySpread)
{
  if (wDelaySpread < 0)
    wDelaySpread = 0;

  stop();

  if (mControlServiceLoopThread) {
    mControlServiceLoopThread->cancel();
    mControlServiceLoopThread->join();
    delete mControlServiceLoopThread;
  }

  UMTS::Time time(random() % 1024, 0);
  mRadioInterface->getClock()->set(time);
  mTransmitDeadlineClock = time;
  mLastClockUpdateTime = time;
  mLatencyUpdateTime = time;

  mDelaySpread = wDelaySpread;
  mPower = mRadioInterface->setPowerAttenuation(mPower);

  mControlServiceLoopThread = new Thread(32768);
  mControlServiceLoopThread->start((void * (*)(void*))
                                   ControlServiceLoopAdapter, (void*) this);
}

/*
 * Start the transceiver
 *
 * Submit command(s) to the radio device to commence streaming samples and
 * launch threads to handle sample I/O. Re-synchronize the transmit burst
 * counters to the central radio clock here as well. Perform locking because the
 * stop request can occur simultaneously (i.e. shutdown).
 */
bool Transceiver::start()
{
  ScopedLock lock(mLock);

  if (mOn) {
    LOG(ERR) << "Transceiver already running";
    return false;
  }

  LOG(NOTICE) << "Starting the transceiver";

  UMTS::Time time = mRadioInterface->getClock()->get();
  mTransmitDeadlineClock = time;
  mLastClockUpdateTime = time;
  mLatencyUpdateTime = time;

  if (!mRadioInterface->start()) {
    LOG(ALERT) << "Device failed to start";
    return false;
  }

  /* Device is running - launch I/O threads */
  mTxServiceLoopThread = new Thread(8 * 32768);
  mRxServiceLoopThread = new Thread(8 * 32768);
  mTransmitPriorityQueueServiceLoopThread = new Thread(8 * 32768);

  mTxServiceLoopThread->start((void * (*)(void*))
                              TxServiceLoopAdapter, (void*) this);
  mRxServiceLoopThread->start((void * (*)(void*))
                              RxServiceLoopAdapter, (void*) this);
  mTransmitPriorityQueueServiceLoopThread->start((void * (*)(void*))
                         TransmitPriorityQueueServiceLoopAdapter, (void*) this);
  writeClockInterface();

  mOn = true;
  LOG(NOTICE) << "Transceiver running";
  return true;
}

/*
 * Stop the transceiver
 *
 * Perform stopping by disabling receive streaming and issuing cancellation
 * requests to running threads. Threads will timeout and terminate at the
 * cancellation points once the device is disabled. Perform locking since the
 * stop request can come from the destructor (i.e. shutdown) in addition to the
 * control thread.
 */
void Transceiver::stop()
{
  ScopedLock lock(mLock);

  if (!mOn)
    return;

  LOG(NOTICE) << "Stopping the transceiver";
  mTxServiceLoopThread->cancel();
  mRxServiceLoopThread->cancel();
  mTransmitPriorityQueueServiceLoopThread->cancel();

  LOG(INFO) << "Stopping the device";
  mRadioInterface->stop();

  LOG(INFO) << "Terminating threads";
  mRxServiceLoopThread->join();
  mTxServiceLoopThread->join();
  mTransmitPriorityQueueServiceLoopThread->join();

  delete mTxServiceLoopThread;
  delete mRxServiceLoopThread;
  delete mTransmitPriorityQueueServiceLoopThread;

  mTransmitPriorityQueue.clear();

  mOn = false;
  LOG(NOTICE) << "Transceiver stopped";
}

void Transceiver::addRadioVector(signalVector &burst, UMTS::Time &wTime)
{
  // modulate and stick into queue 
  radioVector *vec = new radioVector(burst, wTime);
  RN_MEMLOG(radioVector, vec);
  mTransmitPriorityQueue.write(vec);
}

void Transceiver::pushRadioVector(UMTS::Time &now)
{
  radioVector *stale, *next;

  // dump stale bursts, if any
  while ((stale = mTransmitPriorityQueue.getStaleBurst(now))) {
    LOG(NOTICE) << "dumping STALE burst in TRX->USRP interface burst:"
                << stale->time() << " now:" << now;
    writeClockInterface();
    delete stale;
  }

  // if queue contains data at the desired timestamp, stick it into FIFO
  if ((next = (radioVector*) mTransmitPriorityQueue.getCurrentBurst(now))) {
    mRadioInterface->driveTransmitRadio(*(next), false);
    delete next;
    return;
  }

  // Extremely rare that we get here. We need to send a blank burst to the
  // radio interface to update the timestamp.
  LOG(INFO) << "Sending empty burst at " << now;
  mRadioInterface->driveTransmitRadio(*(mEmptyTransmitBurst), true);
}

void Transceiver::reset()
{
  mTransmitPriorityQueue.clear();
}

void Transceiver::driveControl()
{
  int MAX_PACKET_LENGTH = 100;

  // check control socket
  char buffer[MAX_PACKET_LENGTH];
  int msgLen = -1;
  buffer[0] = '\0';

  msgLen = mControlSocket.read(buffer, 1000);

  if (msgLen < 1)
    return;

  char cmdcheck[4];
  char command[MAX_PACKET_LENGTH];
  char response[MAX_PACKET_LENGTH];

  sscanf(buffer, "%3s %s", cmdcheck, command);

  writeClockInterface();

  if (strcmp(cmdcheck, "CMD") != 0) {
    LOG(WARNING) << "bogus message on control interface";
    return;
  }
  LOG(NOTICE) << "command is " << buffer;

  if (!strcmp(command, "POWEROFF")) {
    stop();
    sprintf(response, "RSP POWEROFF 0");
  }
  else if (!strcmp(command, "POWERON")) {
    if (!start())
      sprintf(response, "RSP POWERON 1");
    else
      sprintf(response, "RSP POWERON 0");
  }
  else if (!strcmp(command, "SETRXGAIN")) {
    int newGain;
    sscanf(buffer, "%3s %s %d", cmdcheck, command, &newGain);
    newGain = mRadioInterface->setRxGain(newGain);
    sprintf(response, "RSP SETRXGAIN 0 %d", newGain);
  }
  else if (!strcmp(command, "SETTXATTEN")) {
    int power;
    sscanf(buffer, "%3s %s %d", cmdcheck, command, &power);
    mPower = mRadioInterface->setPowerAttenuation(power);
    sprintf(response, "RSP SETTXATTEN 0 %d", mPower);
  }
  else if (!strcmp(command, "SETFREQOFFSET")) {
      sprintf(response, "RSP SETFREQOFFSET 1");
  }
  else if (!strcmp(command, "SETPOWER")) {
    int power;
    sscanf(buffer, "%3s %s %d", cmdcheck, command, &power);
    mPower = mRadioInterface->setPowerAttenuation(power);
    sprintf(response, "RSP SETPOWER 0 %d", mPower);
  }
  else if (!strcmp(command, "ADJPOWER")) {
    int step;
    sscanf(buffer, "%3s %s %d", cmdcheck, command, &step);
    mPower = mRadioInterface->setPowerAttenuation(mPower + step);
    sprintf(response, "RSP ADJPOWER 0 %d", mPower);
  }
  else if (!strcmp(command, "RXTUNE")) {
    int freqkhz;
    sscanf(buffer, "%3s %s %d", cmdcheck, command, &freqkhz);
    if (!mRadioInterface->tuneRx(freqkhz * 1e3)) {
       LOG(ALERT) << "RX failed to tune";
       sprintf(response, "RSP RXTUNE 1 %d", freqkhz);
    }
    else
       sprintf(response, "RSP RXTUNE 0 %d", freqkhz);
  }
  else if (!strcmp(command, "TXTUNE")) {
    int freqkhz;
    sscanf(buffer, "%3s %s %d", cmdcheck, command, &freqkhz);
    if (!mRadioInterface->tuneTx(freqkhz * 1e3)) {
       LOG(ALERT) << "TX failed to tune";
       sprintf(response, "RSP TXTUNE 1 %d", freqkhz);
    }
    else
       sprintf(response, "RSP TXTUNE 0 %d", freqkhz);
  }
  else if (!strcmp(command, "SETFREQOFFSET")) {
      sprintf(response, "RSP SETFREQOFFSET 1");
  }
  else {
    LOG(WARNING) << "bogus command " << command << " on control interface.";
  }

  LOG(DEBUG) << "response: " << response;
  mControlSocket.write(response, strlen(response) + 1);
}

bool Transceiver::driveTransmitPriorityQueue()
{
  char buffer[MAX_UDP_LENGTH];

  // check data socket
  int msgLen = mDataSocket.read(buffer, 1000);
  if (msgLen < 0)
    return false;

  if (msgLen != 2 * UMTS::gSlotLen + 4) {
    LOG(ERR) << "badly formatted packet on UMTS->TRX interface";
    return false;
  }

  int timeSlot = (int) buffer[0];
  uint16_t frameNum = 0;
  for (int i = 0; i < 2; i++)
    frameNum = (frameNum << 8) | (0x0ff & buffer[i + 1]);

  static signalVector newBurst(UMTS::gSlotLen);
  signalVector::iterator itr = newBurst.begin();
  signed char *bufferItr = (signed char *) (buffer + 3);

  while (itr < newBurst.end()) {
    *itr++ = complex((float) *(bufferItr + 0),
                     (float) *(bufferItr + 1));
    bufferItr++;
    bufferItr++;
  }

  UMTS::Time currTime = UMTS::Time(frameNum, timeSlot);
  addRadioVector(newBurst, currTime);

  return true;
}

void Transceiver::driveReceiveFIFO()
{
  radioVector *rxBurst = NULL;
  int RSSI = 0;
  UMTS::Time burstTime;

  mRadioInterface->driveReceiveRadio(1024 + mDelaySpread);

  rxBurst = (radioVector *) mReceiveFIFO->get();
  if (!rxBurst)
    return;

  burstTime = rxBurst->time();
  size_t burstSize = 2 * rxBurst->size() + 3 + 1 + 1;
  char burstString[burstSize];

  burstString[0] = burstTime.TN();
  for (size_t i = 0; i < 2; i++)
    burstString[1 + i] = (burstTime.FN() >> ((1 - i) * 8)) & 0x0ff;

  burstString[3] = RSSI;
  radioVector::iterator burstItr = rxBurst->begin();
  char *burstPtr = burstString + 4;

  for (size_t i = 0; i < UMTS::gSlotLen + 1024 + mDelaySpread; i++) {
    *burstPtr++ = (char)  (int8_t) burstItr->real();
    *burstPtr++ = (char) -(int8_t) burstItr->imag();
    burstItr++;
  }

  burstString[burstSize - 1] = '\0';
  delete rxBurst;

  if (!burstTime.TN() && !(burstTime.FN() % CLK_IND_INTERVAL))
    writeClockInterface();

  mDataSocket.write(burstString, burstSize);
}

/*
 * Features a carefully controlled latency mechanism, to 
 * assure that transmit packets arrive at the radio/USRP
 * before they need to be transmitted.
 *
 * Deadline clock indicates the burst that needs to be
 * pushed into the FIFO right NOW.  If transmit queue does
 * not have a burst, stick in filler data.
 */
void Transceiver::driveTransmitFIFO()
{
  RadioClock *radioClock = mRadioInterface->getClock();

  if (!mOn)
    return;

  radioClock->wait();

  while (radioClock->get() + mTransmitLatency > mTransmitDeadlineClock) {
    // if underrun, then we're not providing bursts to radio/USRP fast
    //   enough.  Need to increase latency by one UMTS frame.
    if (mRadioInterface->getWindowType() == RadioDevice::TX_WINDOW_USRP1) {
      if (mRadioInterface->isUnderrun()) {
        // only do latency update every 10 frames, so we don't over update
        if (radioClock->get() > mLatencyUpdateTime + UMTS::Time(10, 0)) {
          mTransmitLatency = mTransmitLatency + UMTS::Time(1, 0);
          if (mTransmitLatency > UMTS::Time(15, 0))
            mTransmitLatency = UMTS::Time(15, 0);

          LOG(INFO) << "new latency: " << mTransmitLatency;
          mLatencyUpdateTime = radioClock->get();
        }
      } else {
        // if underrun hasn't occurred in the last sec (100 frames) drop
        //    transmit latency by a timeslot
        if (mTransmitLatency > UMTS::Time(1, 0)) {
          if (radioClock->get() > mLatencyUpdateTime + UMTS::Time(100, 0)) {
            mTransmitLatency.decTN();
            LOG(INFO) << "reduced latency: " << mTransmitLatency;
            mLatencyUpdateTime = radioClock->get();
          }
        }
      }
    }

    // time to push burst to transmit FIFO
    pushRadioVector(mTransmitDeadlineClock);
    mTransmitDeadlineClock.incTN();
  }
}

void Transceiver::writeClockInterface()
{
  char command[50];

  sprintf(command, "IND CLOCK %llu",
          (unsigned long long) (mTransmitDeadlineClock.FN() + 8));

  LOG(INFO) << "ClockInterface: sending " << command;

  mClockSocket.write(command, strlen(command) + 1);
  mLastClockUpdateTime = mTransmitDeadlineClock;
}

void *RxServiceLoopAdapter(Transceiver *transceiver)
{
  while (1) {
    transceiver->driveReceiveFIFO();
    pthread_testcancel();
  }
  return NULL;
}

void *TxServiceLoopAdapter(Transceiver *transceiver)
{
  while (1) {
    transceiver->driveTransmitFIFO();
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
    transceiver->driveTransmitPriorityQueue();
    pthread_testcancel();
  }
  return NULL;
}
