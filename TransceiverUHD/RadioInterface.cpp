/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009 Free Software Foundation, Inc.
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

#include "RadioInterface.h"
#include <Logger.h>

extern "C" {
#include "convert.h"
}

/*
 * Downlink digital gain scaling
 *
 * This value scales floating point transmit values prior fixed-point
 * conversion. The current value provides a significant amount of backoff to
 * avoid reaching compression across general device-daughterboard combinations.
 * More optimal values to target peak gain or maximum dynamic range will be
 * device and daughterboard combination specific.
 * */
#define TX_BASE_SCALING            150.0f

/*
 * Resampling ratio for 25 MHz base clocking
 *
 * This resampling factor absorbs the sample rate differences between UMTS chip
 * rate of 3.84 Mcps and the device clocking rate. USRP N200 clock operates at a
 * rate of 100 MHz with factor of 16 downsampling. B200 uses 25 MHz FPGA
 * clocking with downsampling by 4.
 */
#define RESAMP_INRATE              384
#define RESAMP_OUTRATE             625

/*
 * Number of taps per resampler-RRC filter partitions
 *
 * The number of taps per polyphase partition filter can be adjusted to suit
 * processing or spectrum requirements. Note that the filter length, and
 * associated group delay, affects timing.
 */
#define RESAMP_TAP_LEN             20

/* Chunk multiplier with resampler rates determine the chunk sizes */
#define CHUNK_MUL                  2

/* Universal resampling parameters */
#define NUMCHUNKS                  32

/* Receive scaling factor for 16 to 8 bits */
#define CONVERT_RX_SCALE           (128.0 / 32768.0)

UMTS::Time VectorQueue::nextTime() const
{
  UMTS::Time retVal;
  ScopedLock lock(mLock);

  while (!mQ.size())
    mWriteSignal.wait(mLock);

  return mQ.top()->time();
}

radioVector* VectorQueue::getStaleBurst(const UMTS::Time& targTime)
{
  ScopedLock lock(mLock);
  if (!mQ.size())
    return NULL;

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
  if (!mQ.size())
    return NULL;

  if (mQ.top()->time() == targTime) {
    radioVector* retVal = mQ.top();
    mQ.pop();
    return retVal;
  }
  return NULL;
}

RadioInterface::RadioInterface(RadioDevice *wRadio, int wReceiveOffset,
                               UMTS::Time wStartTime)
  : mRadio(wRadio), sendCursor(0), recvCursor(0),
    underrun(false), receiveOffset(wReceiveOffset),
    mOn(false), powerScaling(TX_BASE_SCALING),
    finalVec(NULL), upsampler(NULL), dnsampler(NULL),
    innerSendBuffer(NULL), innerRecvBuffer(NULL),
    outerSendBuffer(NULL), outerRecvBuffer(NULL),
    convertSendBuffer(NULL), convertRecvBuffer(NULL)
{
  mClock.set(wStartTime);
  inchunk = RESAMP_INRATE * CHUNK_MUL;
  outchunk = RESAMP_OUTRATE * CHUNK_MUL;
}

RadioInterface::~RadioInterface(void)
{
  delete convertSendBuffer;
  delete convertRecvBuffer;

  delete innerSendBuffer;
  delete outerSendBuffer;
  delete innerRecvBuffer;
  delete outerRecvBuffer;

  delete dnsampler;
  delete upsampler;
}

/*
 * Allocate resamplers and I/O buffers
 *
 * Setup multiple chunks on buffers except for the outer receive buffer, which
 * always receives a fixed number of samples. Conversion buffers are fixed point
 * used directly by the device interface.
 */
bool RadioInterface::init()
{
  dnsampler = new Resampler(RESAMP_INRATE, RESAMP_OUTRATE, RESAMP_TAP_LEN);
  if (!dnsampler->init(Resampler::FILTER_TYPE_RRC)) {
    LOG(ALERT) << "Rx resampler failed to initialize";
    return false;
  }

  upsampler = new Resampler(RESAMP_OUTRATE, RESAMP_INRATE, RESAMP_TAP_LEN);
  if (!upsampler->init(Resampler::FILTER_TYPE_RRC)) {
    LOG(ALERT) << "Tx resampler failed to initialize";
    return false;
  }

  outerSendBuffer = new signalVector(NUMCHUNKS * outchunk);
  innerRecvBuffer = new signalVector(NUMCHUNKS * inchunk);

  /* Buffers that feed the resampler require filter length headroom */
  innerSendBuffer = new signalVector(NUMCHUNKS * inchunk + upsampler->len());
  outerRecvBuffer = new signalVector(outchunk + dnsampler->len());

  convertSendBuffer = new short[outerSendBuffer->size() * 2];
  convertRecvBuffer = new short[outerRecvBuffer->size() * 2];

  return true;
}

/*
 * Start the radio device
 *
 * When the device start returns, initialize the timestamp counters with values
 * reported by the device. This avoids having to reset the device clock (if even
 * possible) and guessing at latency and corresponding initial values.
 *
 * Note that we do not advance the initial timestamp relative to receive here
 * so the initial data chunk will submit with a late timestamp. Some timing
 * compensation will occur with the TX_PACKET_SYNC alignment, which removes
 * packets from the downlink stream, while the remaining timing adjustment will
 * be absorbed by UHD and on the device itself.
 */
bool RadioInterface::start()
{
  if (mOn)
    return true;

  LOG(INFO) << "Starting radio device";

  if (!mRadio->start())
    return false;

  sendCursor = 0;
  recvCursor = 0;
  writeTimestamp = mRadio->initialWriteTimestamp();
  readTimestamp = mRadio->initialReadTimestamp();
  mOn = true;

  LOG(INFO) << "Radio started";
  return true;
}

/*
 * Stop the radio device
 *
 * This is a pass-through call to the device interface. Because the underlying
 * stop command issuance generally doesn't return confirmation on device status,
 * this call will only return false if the device is already stopped. 
 */
bool RadioInterface::stop()
{
  if (!mOn || !mRadio->stop())
    return false;

  mOn = false;
  return true;
}

double RadioInterface::fullScaleInputValue(void)
{
  return mRadio->fullScaleInputValue();
}

double RadioInterface::fullScaleOutputValue(void)
{
  return mRadio->fullScaleOutputValue();
}

int RadioInterface::setPowerAttenuation(int atten)
{
  double rfGain, digAtten;

  if (atten < 0)
    atten = 0;

  rfGain = mRadio->setTxGain(mRadio->maxTxGain() - (double) atten);
  digAtten = (double) atten - mRadio->maxTxGain() + rfGain;

  if (digAtten < 1.0)
    powerScaling = TX_BASE_SCALING;
  else
    powerScaling = TX_BASE_SCALING / sqrt(pow(10, (digAtten / 10.0)));

  return atten;
}

int RadioInterface::radioifyVector(signalVector &wVector,
                                   float *retVector, bool zero)
{
  if (zero)
    memset(retVector, 0, wVector.size() * sizeof(Complex<float>));
  else
    memcpy(retVector, wVector.begin(), wVector.size() * sizeof(Complex<float>));

  return wVector.size();
}

void RadioInterface::unRadioifyVector(float *floatVector,
                                      signalVector& newVector)
{
  memcpy(newVector.begin(), floatVector,
         newVector.size() * sizeof(Complex<float>));
}

bool RadioInterface::pushBuffer(void)
{
  int rc, chunks;
  int inner_len, outer_len;

  if (sendCursor < inchunk)
    return true;
  if (sendCursor > innerSendBuffer->size()) {
    LOG(ALERT) << "Send buffer overflow";
  }

  chunks = sendCursor / inchunk;
  inner_len = chunks * inchunk;
  outer_len = chunks * outchunk;

  /* Input from the buffer with number of taps length headroom */
  float *resamp_in = (float *) (innerSendBuffer->begin() + upsampler->len());
  float *resamp_out = (float *) outerSendBuffer->begin();

  rc = upsampler->rotate(resamp_in, inner_len, resamp_out, outer_len);
  if (rc < 0) {
    LOG(ALERT) << "Sample rate downsampling error";
    return false;
  }

  convert_float_short(convertSendBuffer,
                      (float *) outerSendBuffer->begin(),
                      powerScaling, outer_len * 2);

  mRadio->writeSamples(convertSendBuffer, outer_len,
                       &underrun, writeTimestamp);

  /* Shift remaining samples to beginning of buffer */
  memmove(innerSendBuffer->begin() + upsampler->len(),
          innerSendBuffer->begin() + upsampler->len() + inner_len,
          (sendCursor - inner_len) * 2 * sizeof(float));

  writeTimestamp += outer_len;
  sendCursor -= inner_len;

  return true;
}

bool RadioInterface::pullBuffer(void)
{
  bool localUnderrun;

  if (recvCursor > innerRecvBuffer->size() - inchunk)
    return true;

  /* Outer buffer access size is fixed */
  size_t num_recv = mRadio->readSamples(convertRecvBuffer,
                                        outchunk,
                                        &overrun,
                                        readTimestamp,
                                        &localUnderrun);
  if (num_recv != outchunk) {
    LOG(ALERT) << "Receive error " << num_recv;
    return false;
  }

  short *convert_in = convertRecvBuffer;
  float *convert_out = (float *) (outerRecvBuffer->begin() + dnsampler->len());

  convert_short_float(convert_out, convert_in, CONVERT_RX_SCALE, outchunk * 2);

  underrun |= localUnderrun;
  readTimestamp += outchunk;

  /* Write to the end of the inner receive buffer */
  float *resamp_in = (float *) (outerRecvBuffer->begin() + dnsampler->len());
  float *resamp_out = (float *) (innerRecvBuffer->begin() + recvCursor);

  int rc = dnsampler->rotate(resamp_in, outchunk,
                             resamp_out, inchunk);
  if (rc < 0) {
    LOG(ALERT) << "Sample rate upsampling error";
    return false;
  }

  recvCursor += inchunk;

  return true;
}

bool RadioInterface::tuneTx(double freq)
{
  return mRadio->setTxFreq(freq);
}

bool RadioInterface::tuneRx(double freq)
{
  return mRadio->setRxFreq(freq);
}

void RadioInterface::driveTransmitRadio(signalVector &radioBurst,
                                        bool zeroBurst)
{
  if (!mOn)
    return;

  /* Buffer write position */
  float *pos = (float *) (innerSendBuffer->begin() +
                          upsampler->len() + sendCursor);

  radioifyVector(radioBurst, pos, zeroBurst);

  sendCursor += radioBurst.size();
  pushBuffer();
}

void RadioInterface::driveReceiveRadio(int guardPeriod)
{
  if (!mOn)
    return;

  pullBuffer();

  UMTS::Time recvClock = mClock.get();
  int recvSz = recvCursor;
  int readSz = 0;
  const int symbolsPerSlot = UMTS::gSlotLen;

  // while there's enough data in receive buffer, form received 
  //    UMTS bursts and pass up to Transceiver
  int vecSz = symbolsPerSlot + guardPeriod;
  while (recvSz > vecSz) {
    UMTS::Time tmpTime = recvClock;
    mClock.incTN();
    recvClock.incTN();

    if (recvClock.FN() >= 0) {
      radioVector *rxBurst = new radioVector(vecSz, tmpTime);
      memcpy(rxBurst->begin(),
             innerRecvBuffer->begin() + readSz,
             rxBurst->size() * sizeof(Complex<float>));

      mReceiveFIFO.put(rxBurst);
    }

    readSz += symbolsPerSlot;
    recvSz -= symbolsPerSlot;
  }

  if (!readSz)
    return;

  memmove(innerRecvBuffer->begin(),
          innerRecvBuffer->begin() + readSz,
          (recvCursor - readSz) * sizeof(Complex<float>));

  recvCursor -= readSz;
}
