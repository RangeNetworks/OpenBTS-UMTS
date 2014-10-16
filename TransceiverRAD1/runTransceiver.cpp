/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2014 Range Networks, Inc.
 * 
 * This software is distributed under the terms of the GNU General Public 
 * License version 3. See the COPYING and NOTICE files in the current
 * directory for licensing information.
 * 
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

/* while (1) sleep(1); */

#include <stdlib.h>

#include "Transceiver.h"
#include "RAD1Device.h"
#include "DummyLoad.h"

#include <time.h>
#include <signal.h>

#include <UMTSCommon.h>
#include <Logger.h>
#include <Configuration.h>
#include <FactoryCalibration.h>

using namespace std;

ConfigurationKeyMap getConfigurationKeys2();
ConfigurationTable gConfig("/etc/OpenBTS/OpenBTS-UMTS.db","transceiver", getConfigurationKeys2());
FactoryCalibration gFactoryCalibration;

volatile bool gbShutdown = false;
static void ctrlCHandler(int signo)
{
   cout << "Received shutdown signal" << endl;;
   gbShutdown = true;
}


int main(int argc, char *argv[])
{
	try {

  if ( signal( SIGINT, ctrlCHandler ) == SIG_ERR )
  {
    cerr << "Couldn't install signal handler for SIGINT" << endl;
    exit(1);
  }

  if ( signal( SIGTERM, ctrlCHandler ) == SIG_ERR )
  {
    cerr << "Couldn't install signal handler for SIGTERM" << endl;
    exit(1);
  }
  // Configure logger.
  gLogInit("transceiver",gConfig.getStr("Log.Level").c_str(),LOG_LOCAL7);

  gFactoryCalibration.readEEPROM();

  int numARFCN=1;
  if (argc>1) numARFCN = atoi(argv[1]);

#ifdef SINGLEARFCN
  numARFCN=1;
#endif

  srandom(time(NULL));

  RAD1Device *usrp = new RAD1Device(3.84e6);

  //DummyLoad *usrp = new DummyLoad(mOversamplingRate*1625.0e3/6.0);
  usrp->make(); 


  RadioInterface* radio = new RadioInterface(usrp,0 /*1024*/,SAMPSPERSYM,false);
  Transceiver *trx = new Transceiver(5700,"127.0.0.1",SAMPSPERSYM,UMTS::Time(2,0),radio);
  trx->receiveFIFO(radio->receiveFIFO());

/*
  signalVector *gsmPulse = generateGSMPulse(2,1);
  BitVector normalBurstSeg = "0000101010100111110010101010010110101110011000111001101010000";
  BitVector normalBurst(BitVector(normalBurstSeg,gTrainingSequence[0]),normalBurstSeg);
  signalVector *modBurst = modulateBurst(normalBurst,*gsmPulse,8,1);
  signalVector *modBurst9 = modulateBurst(normalBurst,*gsmPulse,9,1);
  signalVector *interpolationFilter = createLPF(0.6/mOversamplingRate,6*mOversamplingRate,1);
  signalVector totalBurst1(*modBurst,*modBurst9);
  signalVector totalBurst2(*modBurst,*modBurst);
  signalVector totalBurst(totalBurst1,totalBurst2);
  scaleVector(totalBurst,usrp->fullScaleInputValue());
  double beaconFreq = -1.0*(numARFCN-1)*200e3;
  signalVector finalVec(625*mOversamplingRate);
  for (int j = 0; j < numARFCN; j++) {
	signalVector *frequencyShifter = new signalVector(625*mOversamplingRate);
	frequencyShifter->fill(1.0);
	frequencyShift(frequencyShifter,frequencyShifter,2.0*M_PI*(beaconFreq+j*400e3)/(1625.0e3/6.0*mOversamplingRate));
  	signalVector *interpVec = polyphaseResampleVector(totalBurst,mOversamplingRate,1,interpolationFilter);
	multVector(*interpVec,*frequencyShifter);
	addVector(finalVec,*interpVec); 	
  }
  signalVector::iterator itr = finalVec.begin();
  short finalVecShort[2*finalVec.size()];
  short *shortItr = finalVecShort;
  while (itr < finalVec.end()) {
	*shortItr++ = (short) (itr->real());
	*shortItr++ = (short) (itr->imag());
	itr++;
  }
  usrp->loadBurst(finalVecShort,finalVec.size());
*/
  trx->start();
  int i = 0;
  while(!gbShutdown) { sleep(1); } // i++; if (i==60) exit(1);}

  cout << "Shutting down transceiver..." << endl;

//  trx->stop();
  delete trx;
//  delete radio;

	} catch (ConfigurationTableKeyNotFound e) {
		LOG(EMERG) << "required configuration parameter " << e.key() << " not defined, aborting";
		//gReports.incr("OpenBTS-UMTS.Exit.Error.ConfigurationParamterNotFound");
	}
}

ConfigurationKeyMap getConfigurationKeys2()
{
	extern ConfigurationKeyMap getConfigurationKeys();
	ConfigurationKeyMap map = getConfigurationKeys();
	ConfigurationKey *tmp;

	tmp = new ConfigurationKey("TRX.RadioFrequencyOffset","128",
		"~170Hz steps",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"96:160",// educated guess
		true,
		"Fine-tuning adjustment for the transceiver master clock.  "
			"Roughly 170 Hz/step.  "
			"Set at the factory.  "
			"Do not adjust without proper calibration."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("TRX.TxAttenOffset","0",
		"dB of attenuation",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:100",// educated guess
		true,
		"Hardware-specific gain adjustment for transmitter, matched to the power amplifier, expessed as an attenuationi in dB.  "
			"Set at the factory.  "
			"Do not adjust without proper calibration."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("TRX.RadioNumber","0",
		"",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:9",		// Not likely to have 10 radios on the same computer.  Not likely to have >1
		true,
		"If non-0, use multiple radios on the same cpu, numbered 1-9.  Must change TRX.Port also.  Provide a separate config file for each OpenBTS+Radio combination using the environment variable or --config command line option."
	);
	map[tmp->getName()] = *tmp;
	delete(tmp);

	return map;
}
