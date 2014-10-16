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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "radioDevice.h"


#include <sys/time.h>
#include <math.h>
#include <string>
#include <iostream>


/** A class to handle a USRP rev 4, with a two RFX900 daughterboards */
class DummyLoad: public RadioDevice {

private:

  double sampleRate; 	///< the desired sampling rate
  unsigned long long samplesRead;	///< number of samples read from USRP
  unsigned long long samplesWritten;	///< number of samples sent to USRP

  Mutex underrunLock;

  struct timeval startTime, currTime; 

  TIMESTAMP currstamp;
  short *dummyBurst;
  int dummyBurstSz;
  int dummyBurstCursor;
  bool underrun;

  void updateTime(void);

 public:

  /** Object constructor */
  DummyLoad (double _desiredSampleRate);

  int loadBurst(short *wDummyBurst, int len);

  /** Instantiate the USRP */
  bool make(bool skipRx = false); 

  /** Start the USRP */
  bool start();

  /** Stop the USRP */
  bool stop();

  /**
	Read samples from the USRP.
	@param buf preallocated buf to contain read result
	@param len number of samples desired
	@param overrun Set if read buffer has been overrun, e.g. data not being read fast enough
	@param timestamp The timestamp of the first samples to be read
	@param underrun Set if USRP does not have data to transmit, e.g. data not being sent fast enough
	@param RSSI The received signal strength of the read result
	@return The number of samples actually read
  */
  int  readSamples(short *buf, int len, bool *overrun, 
		   TIMESTAMP timestamp = 0xffffffff,
		   bool *underrun = NULL,
		   unsigned *RSSI = NULL);
  /**
        Write samples to the USRP.
        @param buf Contains the data to be written.
        @param len number of samples to write.
        @param underrun Set if USRP does not have data to transmit, e.g. data not being sent fast enough
        @param timestamp The timestamp of the first sample of the data buffer.
        @param isControl Set if data is a control packet, e.g. a ping command
        @return The number of samples actually written
  */
  int  writeSamples(short *buf, int len, bool *underrun, 
		    TIMESTAMP timestamp = 0xffffffff,
		    bool isControl = false);
 
  /** Update the alignment between the read and write timestamps */
  bool updateAlignment(TIMESTAMP timestamp);
  
  /** Set the transmitter frequency */
  bool setTxFreq(double wFreq);

  /** Set the receiver frequency */
  bool setRxFreq(double wFreq);

  /** Returns the starting write Timestamp*/
  TIMESTAMP initialWriteTimestamp(void) { return 20000;}

  /** Returns the starting read Timestamp*/
  TIMESTAMP initialReadTimestamp(void) { return 20000;}

  /** returns the full-scale transmit amplitude **/
  double fullScaleInputValue() {return 13500.0;}

  /** returns the full-scale receive amplitude **/
  double fullScaleOutputValue() {return 9450.0;}

  /** Return internal status values */
  inline double getTxFreq() { return 0;}
  inline double getRxFreq() { return 0;}
  inline double getSampleRate() {return sampleRate;}
  inline double numberRead() { return samplesRead; }
  inline double numberWritten() { return samplesWritten;}

};

