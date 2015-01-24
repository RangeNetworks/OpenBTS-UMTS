/**file Radiomodem, for physical later processing bits <--> chips */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include "UMTSRadioModemSequences.h"
#include "UMTSRadioModem.h"
#include "UMTSConfig.h"

#include "Transceiver.h"

//#include "RAD1Device.h"

#include "Logger.h"


using namespace UMTS;

UMTS::Time TxBitsQueue::nextTime() const
{
  UMTS::Time retVal;
  ScopedLock lock(mLock);
  while (mQ.size()==0) mWriteSignal.wait(mLock);
  return mQ.top()->time();
}

TxBitsBurst* TxBitsQueue::getStaleBurst(const UMTS::Time& targTime)
{
  ScopedLock lock(mLock);
  if ((mQ.size()==0)) {
    return NULL;
  }
  if (mQ.top()->time() < targTime) {
    TxBitsBurst* retVal = mQ.top();
    mQ.pop();
    return retVal;
  }
  return NULL;
}


TxBitsBurst* TxBitsQueue::getCurrentBurst(const UMTS::Time& targTime)
{
  ScopedLock lock(mLock);
  if ((mQ.size()==0)) {
    return NULL;
  }
  if (mQ.top()->time() == targTime) {
    TxBitsBurst* retVal = mQ.top();
    mQ.pop();
    return retVal;
  }
  return NULL;
}


// Assuming one sample per chip.  

Transceiver *trx;
RadioBurstFIFO *mT;
RadioBurstFIFO *mR;

const int FILTLEN = 17;
float invFilt[FILTLEN] = {0.010688,-0.021274,0.040037,-0.06247,0.10352,-0.15486,0.23984,-0.42166,0.97118,-0.42166,0.23984,-0.15486,0.10352,-0.06247,0.040037,-0.021274,0.010688};
float invFiltRcv[FILTLEN] = {0.017157,-0.028391,0.029818,-0.043222,0.047142,-0.063790,0.075942,-0.136568,0.964300,-0.136568,0.075942,-0.063790,0.047142,-0.043222,0.029818,-0.028391,0.017157};
signalVector *inverseCICFilter;
signalVector *rcvInverseCICFilter;
signalVector *txHistoryVector;
signalVector *rxHistoryVector;

RadioModem::RadioModem(UDPSocket& wDataSocket)
	:mDataSocket(wDataSocket)
{
  sigProcLibSetup(1);
  mUplinkPilotWaveformMap.clear();
  mUplinkScramblingCodes.clear();

inverseCICFilter = new signalVector(FILTLEN);
//RN_MEMLOG(signalVector,inverseCICFilter);
signalVector::iterator itr = inverseCICFilter->begin();
inverseCICFilter->isRealOnly(true);
inverseCICFilter->setSymmetry(ABSSYM);
for (int i = 0; i < FILTLEN; i++)
  *itr++ = complex(invFilt[i],0.0);
txHistoryVector = new signalVector(FILTLEN-1);

rcvInverseCICFilter = new signalVector(FILTLEN);
//RN_MEMLOG(signalVector,inverseCICFilter);
itr = rcvInverseCICFilter->begin();
rcvInverseCICFilter->isRealOnly(true);
rcvInverseCICFilter->setSymmetry(ABSSYM);
for (int i = 0; i < FILTLEN; i++)
  *itr++ = complex(invFiltRcv[i],0.0);
rxHistoryVector = new signalVector(FILTLEN-1);


  mTxQueue = new TxBitsQueue;

  for (int i = 0; i < 16; i++) {
	mRACHSignatureMask[i] = false;
        if (i < 12) mRACHSubchannelMask[i] = false;
  }

  mDelaySpread = gConfig.getNum("UMTS.Radio.MaxExpectedDelaySpread");

  mDownlinkScramblingCodeIndex = 16*gConfig.getNum("UMTS.Downlink.ScramblingCode");
  LOG(INFO) << "DownlinkScramblingCodeIndex: " << mDownlinkScramblingCodeIndex;
  mDownlinkScramblingCode = new DownlinkScramblingCode(mDownlinkScramblingCodeIndex);

  mUplinkPRACHScramblingCodeIndex = mDownlinkScramblingCodeIndex + gConfig.getNum("UMTS.PRACH.ScramblingCode"); //4.3.3.2 of 25.213
  mRACHSignatureMask[gConfig.getNum("UMTS.PRACH.Signature")] = true;
  mRACHSubchannelMask[gConfig.getNum("UMTS.PRACH.Subchannel")] = true;
  mAICHRACHOffset = UMTS::Time(0,cAICHRACHOffset); //FIXME:  make sure this is in the config and SIB5.
  mAICHSpreadingCodeIndex = cAICHSpreadingCodeIndex; // FIXME: needs to be in config and mirror what's in SIB5
  mRACHSearchSize = 100; //gConfig.getNum("UMTS.Radio.MaxExpectedDelaySpread");
  mRACHCorrelatorSize = 256*4; //256*4;
  mRACHPreambleOffset = 256;
  mRACHPilotsOffset = 256;
  mRACHMessageControlSpreadingCodeIndex = 16*gConfig.getNum("UMTS.PRACH.Signature")+15; // 4.3.1.3 of 25.213
  unsigned RACHsf = gConfig.getNum("UMTS.PRACH.SF");
  mRACHMessageDataSpreadingCodeIndex = RACHsf*gConfig.getNum("UMTS.PRACH.Signature")/16; // 4.3.1.3 of 25.213
  mRACHMessageControlSpreadingFactorLog2 = 8; // only possible spreading factor
  mRACHMessageDataSpreadingFactorLog2 = (int) round(log2(RACHsf)); 

  mRACHMessageSlots = 30/2; // FIXME:: needs this from config or higher layers

  mDPCCHCorrelationWindow = 0;
  mDPCCHSearchSize = 256; //2*256/8;
  mSSCHGroupNum = mDownlinkScramblingCodeIndex/128; // 3GPP 25.213 Sec. 5.2.2

  generateDownlinkPilotWaveforms();

  mLastTransmitTime = UMTS::Time(0,0);

  // 3GPP 25.213, Sec. 5.1.4
  // This looks like it doesn't hold true, that the alignment does not need to be shifted
  for (unsigned i = 0; i < gFrameLen; i++) {
    unsigned alignedIx = (i+gFrameLen-0*256) % gFrameLen;
    mDownlinkAlignedScramblingCodeI[i] = *(mDownlinkScramblingCode->ICode()+alignedIx);
    mDownlinkAlignedScramblingCodeQ[i] = *(mDownlinkScramblingCode->QCode()+alignedIx);
  }

  generateRACHPreambleTable(mRACHPreambleOffset,mRACHCorrelatorSize);
  generateRACHMessagePilots(mRACHCorrelatorSize);
}


// pat 1-5-2013: We cant start up a transceiver in the constructor above because other constructors
// at the same level (specifically, the socket that trx is going to use) are not initialized.
// See comments in ARFCNManager.
void RadioModem::radioModemStart()
{


  mDispatchQueue.clear();
  mFECDispatcher.start((void*(*)(void*)) FECDispatchLoopAdapter, this);
  mRACHQueue.clear();
  mRACHProcessor.start((void*(*)(void*)) RACHLoopAdapter, this);
  for (int i = 0; i < 100; i++) {
    mDCHQueue[i].clear();
    DCHLoopInfo *dli = new DCHLoopInfo; 
    dli->radioModem = (void *) this;
    dli->threadId = i;
    mDCHProcessor[i].start((void*(*)(void*)) DCHLoopAdapter, dli);
  }
}

void* FECDispatchLoopAdapter(RadioModem *modem)
{
	while(1) {
		FECDispatchInfo *q = (FECDispatchInfo*) (modem->mDispatchQueue).read();
		//printf("q: %0x %0x %0x\n",q,q->fec,q->burst); fflush(stdout);
		if (q!=NULL) { 
		  DCHFEC* fec = (DCHFEC*) (q->fec);
		  if (fec->active()) 
#define FRAMEBURSTS
#ifdef FRAMEBURSTS
			fec->l1WriteLowSideFrame(*(q->burst),(q->tfciFrame));
#else
			fec->l1WriteLowSide(*(q->burst));
#endif
		  // FIXME!!!  (pat) I am removing this delete[] because I dont think
		  // it is correct: the SoftVector at burst deletes its own memory.
		  // delete[] q->burst->begin();
		  // (harvind) the RxBitsBurst destructor does not destroy the inherited SoftVector's data.
		  // Instead of explicitly deleting it, we'll use the clear() command which safely deletes it.
		  //  dynamic_cast<SoftVector*>(q->burst)->clear();
		  // Nope. That doesn't work, nor do calls to clear and resize.  WTF?  Back to the old solution.
		  delete[] q->burst->begin();
		  delete q->burst;
		  delete q;
		}
	}
	return NULL;
}

void* RACHLoopAdapter(RadioModem *modem)
{
        while(1) {
                RACHProcessorInfo *q = (RACHProcessorInfo*) (modem->mRACHQueue).read();

		// if this is an access slot, then detect a RACH preamble.
  		modem->detectRACHPreamble(*((signalVector*) q->burst),q->burstTime,modem->mRACHThreshold);
  		//if (detectRACHPreamble(*wBurst,wTime,mRACHThreshold)) 
        	//LOG(INFO) << "RACH Enrg: " << wTime << " " << avgPwr;

  		// if RACH message part is expected, the decode one of the 15 or 30 consecutive slots./
  		modem->decodeRACHMessage(*((signalVector*)q->burst), q->burstTime, 5.0);

		delete (signalVector *) (q->burst);
		delete q;
        }
        return NULL;
}

void* DCHLoopAdapter(DCHLoopInfo *dli)
{
	UMTS::RadioModem *modem = (UMTS::RadioModem *) (dli->radioModem);
	int threadId = dli->threadId;
        while(1) {
                DCHProcessorInfo *q = (DCHProcessorInfo*) (modem->mDCHQueue[threadId]).read();
		signalVector *burstCopy = (signalVector *) q->burst;
		UMTS::Time wTime = q->burstTime;
        	DCHFEC *currDCH = (DCHFEC*) q->fec;
        	int slotIx = wTime.TN();
        	if ((slotIx==0) && (modem->gActiveDPDCH.find((void *)currDCH)==modem->gActiveDPDCH.end())) {
            		// add to DPDCH map
            		modem->gActiveDPDCH[(void *)currDCH] = new DPDCH((void*)currDCH,wTime);
        	}
        	if (modem->gActiveDPDCH.find((void *)currDCH)==modem->gActiveDPDCH.end()) continue;
        	//printf("time: %d, %d\n",wBurstI.time().FN(),wBurstI.time().TN());
        	DPDCH *currDPDCH = modem->gActiveDPDCH[(void *)currDCH];
        	if (!currDPDCH) {
			delete (signalVector *) (q->burst);
                	delete q;
			continue;
		}
        	if (slotIx==0) {
                	currDPDCH->frameTime = wTime;
                	currDPDCH->active = true;
			currDPDCH->bestSNR = -1000.0;
        	}
        	if (!currDPDCH->active) {
                        delete (signalVector *) (q->burst);
                        delete q;
                        continue;
                }
        	int uplinkScramblingCodeIndex = currDCH->getPhCh()->SrCode();
        	int numPilots = currDCH->getPhCh()->getUlDPCCH()->mNPilot;
			currDPDCH->active = modem->decodeDCH(*burstCopy,wTime,
                                      uplinkScramblingCodeIndex,
                                      numPilots,
                                      currDPDCH->descrambledBurst,
				      currDPDCH->rawBurst,
                                      currDPDCH->lastTOA,
				      currDPDCH->bestTOA,
				      currDPDCH->bestChannel,
			  	      currDPDCH->bestSNR,
                                      currDPDCH->tfciBits,
                                      currDPDCH->tpcBits);

        	if (slotIx == gFrameSlots-1) { //gots a frame, let's decode it
                	// First, need to figure out TFCI
                	int TFCI = findTfci(currDPDCH->tfciBits,currDCH->l1ul()->mNumTfc); 

                	// (pat) The uplink spreading factor can depend on the TFC of this particular uplink vector.
                	// We need to decode the DPCCH first then look up the SF based on the TFCI bits.  Someday.
                	int uplinkSpreadingFactorLog2 = currDCH->l1ul()->getFPI(0,TFCI)->mSFLog2;
                	int uplinkSpreadingCodeIndex = (1 << uplinkSpreadingFactorLog2)/4;
                	// 4.3.1.2.1 of 25.213
                	// DPDCH is always index of SF/4
                	// N DPDCH is always a SF of 4, index is 1 if N < 2, 3 if N < 4, 2 if N < 6 
                	// gonna assume single DPDCH per DCH                
			LOG(NOTICE) << "numTFCI: " << currDCH->l1ul()->mNumTfc << " TFCI: " << TFCI << ", SF: " << (1 << uplinkSpreadingFactorLog2) << ", scram: " << uplinkScramblingCodeIndex << ", code: " << uplinkSpreadingCodeIndex << ", time:" << wTime;
                	//LOG(INFO) << "TPC: " << currDPDCH->tpcBits[0] << " " << currDPDCH->tpcBits[1];
			
                	if (TFCI !=0) {
                        	currDCH->l1ul()->mReceived = true;
  				modem->decodeDPDCHFrame(*currDPDCH,uplinkScramblingCodeIndex,uplinkSpreadingFactorLog2,
							uplinkSpreadingCodeIndex);
                	}
        	}
                delete (signalVector *) (q->burst);
                delete q;
        }
        return NULL;
}

void RadioModem::generateRACHMessagePilots(int filtLen) 
{
    int pilotSeqLen = 8*256;
    radioData_t zeroIBurst[gSlotLen];
    memset(zeroIBurst,0,gSlotLen*sizeof(radioData_t));

    for (unsigned slot = 0; slot < gFrameSlots; slot++) {
	radioData_t pilotSeqQ[pilotSeqLen];
        memset(pilotSeqQ,0,pilotSeqLen*sizeof(radioData_t));
        
	radioData_t *IBurst = NULL;
	radioData_t *QBurst = NULL;

	spreadOneBranch((BitVector&) gRACHMessagePilots[slot],
                         (int8_t*) gOVSFTree.code(mRACHMessageControlSpreadingFactorLog2,
						   mRACHMessageControlSpreadingCodeIndex),
			 (1 << mRACHMessageControlSpreadingFactorLog2),
			 (radioData_t *) pilotSeqQ,
			 (1 << mRACHMessageControlSpreadingFactorLog2));

 	scramble(zeroIBurst, pilotSeqQ, pilotSeqLen, 
		 mRACHMessageAlignedScramblingCodeI+gSlotLen*slot,
		 mRACHMessageAlignedScramblingCodeQ+gSlotLen*slot,
 		 pilotSeqLen,&IBurst,&QBurst);

	signalVector RACHpilot(filtLen);
        signalVector::iterator itr = RACHpilot.begin();
        for (unsigned i = 0; i < RACHpilot.size(); i++)
          *itr++ = complex(IBurst[i+mRACHPilotsOffset],QBurst[i+mRACHPilotsOffset]);
        mRACHMessagePilotWaveforms[slot] = reverseConjugate(&RACHpilot);

	delete[] IBurst;
	delete[] QBurst;
    }
}

signalVector*  RadioModem::UplinkPilotWaveforms(int scramblingCode, int codeIndex, int numPilots, int slotIx) 
{

  int hashValue = waveformMapHash(scramblingCode,numPilots-3);
  if (mUplinkPilotWaveformMap.find(hashValue)!=mUplinkPilotWaveformMap.end()) {
	return mUplinkPilotWaveformMap[hashValue][slotIx];
  }
  else { // need to generate set of pilot vectors for scrambling code
    // NOTE: Pilot sequences are on the Q channel
    // Create a zeroed out I burst.
    radioData_t zeroIBurst[gSlotLen];
    memset(zeroIBurst,0,gSlotLen*sizeof(radioData_t));

    signalVector **newPilots = new signalVector*[gFrameSlots];

    // Figure out scrambling code 
    int Np = numPilots-3;
    if ( mUplinkScramblingCodes.find(scramblingCode)==mUplinkScramblingCodes.end() ) {
	mUplinkScramblingCodes[scramblingCode] = new UplinkScramblingCode(scramblingCode);
    }
    int8_t* scramI = (int8_t*) (mUplinkScramblingCodes[scramblingCode])->ICode();
    int8_t* scramQ = (int8_t*) (mUplinkScramblingCodes[scramblingCode])->QCode();
    unsigned seqSz = mDPCCHSearchSize; //numPilots*256;
    radioData_t pilotSeq[numPilots*256];
    for (unsigned slot = 0; slot < gFrameSlots; slot++) {
	memset(pilotSeq,0,numPilots*256*sizeof(radioData_t));
	spreadOneBranch((BitVector&) gPilotPatterns[Np][slot],
			 (int8_t *) gOVSFTree.code(8,codeIndex),
			 256,(radioData_t *) pilotSeq,numPilots*256);
        radioData_t *Iside = NULL;
	radioData_t *Qside = NULL;
	scramble((radioData_t *) zeroIBurst,(radioData_t *) pilotSeq, numPilots*256,
		scramI+gSlotLen*slot, scramQ+gSlotLen*slot, 
		numPilots*256, &Iside,&Qside);
	signalVector pilotChips(seqSz);
	signalVector::iterator itr = pilotChips.begin();
	// FIXME: need to specify correlation size here
	for (unsigned i = 0; i < seqSz; i++) 
	  *itr++ = complex(Iside[i+384],Qside[i+384]);
        newPilots[slot] = reverseConjugate(&pilotChips);
	delete[] Iside;
	delete[] Qside;
    }
    mUplinkPilotWaveformMap[hashValue] = newPilots;
    return newPilots[slotIx];
  }
}

void RadioModem::generateRACHPreambleTable(int startIx, int filtLen)
{
  unsigned scramblingCodeIx = mUplinkPRACHScramblingCodeIndex;
  LOG(INFO) << "RACH scramblingCodeIx: " << scramblingCodeIx;
  if (!mUplinkScramblingCodes[scramblingCodeIx]) {
      mUplinkScramblingCodes[scramblingCodeIx] = new UplinkScramblingCode(scramblingCodeIx);
      for (unsigned i = 0; i < gFrameLen; i++) {
          unsigned alignedIx = (i+4096);
          mRACHMessageAlignedScramblingCodeI[i] = *(mUplinkScramblingCodes[scramblingCodeIx]->ICode()+alignedIx);
          mRACHMessageAlignedScramblingCodeQ[i] = *(mUplinkScramblingCodes[scramblingCodeIx]->QCode()+alignedIx);
      }
  }

  for (int signature = 0; signature < 16; signature++) {
    radioData_t repeatedRACHPreambleI[256*16];
    for (unsigned int i = 0; i < 256*16; i++) {
      repeatedRACHPreambleI[i] = (gRACHSignatures[signature].bit(i % 16) ? -1 : 1);
    }
    radioData_t *RACHIside = NULL;

    scrambleRACH(repeatedRACHPreambleI, 4096, (int8_t *) mUplinkScramblingCodes[scramblingCodeIx]->ICode(),
	     256*16,&RACHIside);

    signalVector RACHmodBurst(filtLen);
    signalVector::iterator RACHmodBurstItr = RACHmodBurst.begin();
    for (int i = startIx; i < startIx+filtLen; i++) {
      float arg = ((float) M_PI/4.0F) + ((float) M_PI/2.0F) * (float) (i % 4);
      *RACHmodBurstItr = complex((float) RACHIside[i]*cos(arg),(float) RACHIside[i]*sin(arg));
      RACHmodBurstItr++;
    }
    delete[] RACHIside;
    mRACHTable[signature] = reverseConjugate(&RACHmodBurst);
  }
}

void RadioModem::generateDownlinkPilotWaveforms()
{

   radioData_t basicWaveformI[gSlotLen];
   radioData_t basicWaveformQ[gSlotLen];
   memset(basicWaveformI,0,gSlotLen*sizeof(radioData_t));
   memset(basicWaveformQ,0,gSlotLen*sizeof(radioData_t));

   // Note that P-SCH and S-SCH are multiplied by -1 to indicate that PCCPCH is not STTD encoded
   radioData_t aSTTD = -1;

   // Generate P-SCH
   PrimarySyncCode psc;
   for (int i = 0; i < 256; i++) { 
     basicWaveformI[i] = basicWaveformQ[i] = mPSCHAmplitude * aSTTD * ((psc.code())[i]);
     //LOG(INFO) << "basicWaveform[" << i << "] = " << (int) basicWaveformI[i];
   }

   for (unsigned slot = 0; slot < gFrameSlots; slot++) {
     // Generate S-SCH, using group number 
     SecondarySyncCode ssc(16*(gSSCAllocations[mSSCHGroupNum][slot]-1)); // 3GPP 25.213 Sec. 5.2.3.1
     mDownlinkSCHWaveformsI[slot] = new radioData_t[gSlotLen];  
     mDownlinkSCHWaveformsQ[slot] = new radioData_t[gSlotLen];
     memcpy(mDownlinkSCHWaveformsI[slot],basicWaveformI,gSlotLen*sizeof(radioData_t));
     memcpy(mDownlinkSCHWaveformsQ[slot],basicWaveformQ,gSlotLen*sizeof(radioData_t));
     for (int i = 0; i < 256; i++) { 
        //LOG(INFO) << "mDPWI[ " << slot << "][" << i << "] = " << (int) (mDownlinkSCHWaveformsI[slot])[i];
	(mDownlinkSCHWaveformsI[slot])[i] += mSSCHAmplitude * aSTTD * ((ssc.code())[i]);
        (mDownlinkSCHWaveformsQ[slot])[i] += mSSCHAmplitude * aSTTD * ((ssc.code())[i]);
	//LOG(INFO) << "mDPWI[ " << slot << "][" << i << "] = " << (int) (mDownlinkSCHWaveformsQ[slot])[i];
     }
   }

   // Generate CPICH, use 20 '0' bits per slot.
   // Primary-CPICH uses C_ch,256,0 channelization code
   BitVector CPICH("00000000000000000000");
   int8_t *code = (int8_t *) gOVSFTree.code(8,0);
   mDownlinkPilotWaveformsI = new radioData_t[gSlotLen];
   mDownlinkPilotWaveformsQ = new radioData_t[gSlotLen];
   memset(mDownlinkPilotWaveformsI,0,gSlotLen*sizeof(radioData_t));
   memset(mDownlinkPilotWaveformsQ,0,gSlotLen*sizeof(radioData_t));
   spread(CPICH, code, 256, mDownlinkPilotWaveformsI, mDownlinkPilotWaveformsQ, gSlotLen, mCPICHAmplitude);

   // Generate empty PICH, use 20 '0' bits per slot.  last 12 are supposed to be DTX, but oh well
   // PICH uses channelization code in SIB5, assumed its SF 256 and code 255.
   //BitVector PICH("00000000000000000000");
   //int8_t *codePICH = (int8_t *) gOVSFTree.code(8,255);
   //spread(PICH, codePICH, 256, mDownlinkPilotWaveformsI, mDownlinkPilotWaveformsQ, gSlotLen, mCPICHAmplitude);


}


/* NOTE: Make sure matchedfilter is already reversed and conjugated (i.e. run through reverseConjugate) */
float RadioModem::estimateChannel(signalVector *wBurst,
				 signalVector *matchedFilter,
				 unsigned maxTOA,
				 unsigned startTOA,
				 complex *channel,
			         float *TOA) 
{
    signalVector correlatedPilots(maxTOA);
    correlate(wBurst, matchedFilter, &correlatedPilots,
		CUSTOM, true, (matchedFilter->size()-1)+startTOA,maxTOA);
    if (channel && TOA) {
	float meanPower = 1.0;
	*channel = peakDetect(correlatedPilots,TOA,&meanPower);
	LOG(DEBUG) << "TOA: " << *TOA << "chan: " ;
	/*for(int i = -10; i < 10; i++) {
		if (floor(*TOA+i) >= maxTOA) continue;
		if (floor(*TOA+i) < 0) continue; 
		if (startTOA < 500.0) LOG(INFO) << correlatedPilots[floor(*TOA+i)] << " SNR " << correlatedPilots[floor(*TOA+i)].abs();
	}*/
	//LOG(INFO) << "TOA: " << *TOA;
	*TOA = *TOA + (float) startTOA;
	//LOG(INFO) << "TOA: " << *TOA;
	if (meanPower != 0.0) 
          return channel->norm2()/meanPower;
	else
	  return -100.0;
    }
    return 0.0;
}


void RadioModem::accumulate(radioData_t *addI, radioData_t *addQ, int addLen, radioData_t *accI, radioData_t *accQ)
{
      for (int i = 0; i < addLen;i++) {
	accI[i] += addI[i];
	accQ[i] += addQ[i];
      }
}


void RadioModem::spread(BitVector &wBurst, int8_t *code, int codeLen, radioData_t *accI, radioData_t *accQ, int accLen, radioData_t gain)
{
      int8_t *codePtrEnd = code+codeLen;
      for (unsigned i = 0; i < wBurst.size();i++) {
        unsigned byt = wBurst[i];
        if (byt == 0x7f) continue; // DTX symbol
        radioData_t *acc = ((i % 2 == 0) ? accI : accQ) + (i/2)*codeLen;
	int8_t *codePtr = code;
	radioData_t compositeGain = (2*(byt & 0x01)-1)*gain;
        while(codePtr < codePtrEnd) { 
            *acc += compositeGain * *codePtr++; 
	    acc++;
	}
      }
}

void RadioModem::spreadOneBranch(BitVector &wBurst, int8_t *code, int codeLen, radioData_t *acc, int accLen)
{
     // Assuming gains of each channel is +1.0
      for (unsigned i = 0; i < wBurst.size();i++) {
        bool invert = wBurst.bit(i);
        if (!invert) {
          for (int c = 0; c < codeLen; c++)
            acc[i*codeLen + c] += code[c];
        }
        else {
          for (int c = 0; c < codeLen; c++)
            acc[i*codeLen + c] -= code[c];
        }
      }
}


void RadioModem::scramble(radioData_t *wBurstI, radioData_t *wBurstQ, int len,
			  int8_t *codeI, int8_t *codeQ, int codeLen,
			  radioData_t **rBurstI, radioData_t **rBurstQ)

{
  if (*rBurstI == NULL) {
	*rBurstI = new radioData_t[len];
	memset(*rBurstI,0,sizeof(radioData_t)*len);
  } 
  if (*rBurstQ == NULL) {
	*rBurstQ = new radioData_t[len];
        memset(*rBurstQ,0,sizeof(radioData_t)*len);
  }
  radioData_t *IBurst = (*rBurstI);
  radioData_t *QBurst = (*rBurstQ);
  int8_t* codeEnd = codeI + codeLen;
  while (codeI < codeEnd) {
    *IBurst += (*wBurstI * *codeI - *wBurstQ * *codeQ);
    *QBurst += (*wBurstI * *codeQ + *wBurstQ * *codeI);
    IBurst++;wBurstI++;codeI++;
    QBurst++;wBurstQ++;codeQ++;
  }
}

void RadioModem::scrambleRACH(radioData_t *wBurstI, int len,
                              int8_t *codeI, int codeLen,
                              radioData_t **rBurstI)

{
  if (*rBurstI == NULL) {
        *rBurstI = new radioData_t[len];
        //memset(*rBurstI,0,sizeof(radioData_t)*len);
  }
  for (int i = 0; i < len; i++) {
    radioData_t cI = codeI[i];
    (*rBurstI)[i] = (wBurstI[i]*cI);
  }
}


signalVector* RadioModem::descramble(signalVector &wBurst, int8_t *codeI, int8_t *codeQ, signalVector *retVec) 
{
  if (retVec == NULL)
      retVec = new signalVector(wBurst.size());
 
  RN_MEMLOG(signalVector,retVec);
  signalVector::iterator wBurstItr = wBurst.begin();
  signalVector::iterator retItr = retVec->begin();

  for (unsigned int i = 0; i < wBurst.size(); i++) {
    *retItr++ = *wBurstItr++ * complex(*codeI++,- *codeQ++);
  }
  return retVec;
}

signalVector *RadioModem::despread(signalVector &wBurst,
				   const int8_t *code,
			           int codeLength,
				   bool useQ) 
{
  int finalLength = wBurst.size()/codeLength;
  signalVector *retVec = new signalVector(finalLength);
  RN_MEMLOG(signalVector,retVec);
  signalVector::iterator retItr = retVec->begin();
  signalVector::iterator burstItr = wBurst.begin();

  if (!useQ) {
    for (int i = 0; i < finalLength; i++) {
      *retItr = 0;
      for (int j = 0; j < codeLength; j++) {
	*retItr += (burstItr->real()*code[j]); 
	burstItr++;
      }
      retItr++;    
    }
  } 
  else {
    for (int i = 0; i < finalLength; i++) {
      *retItr = 0;
      for (int j = 0; j < codeLength; j++) {
        *retItr += (burstItr->imag()*code[j]); 
        burstItr++;
      }
      retItr++;
    }
  } 
 
  return retVec;

}

int consecutiveRACH = 0;
int consecutiveRACHTOA = 0;

bool RadioModem::detectRACHPreamble(signalVector &wBurst, UMTS::Time wTime, float detectionThreshold)
{

  if (mRACHMessagePending) return false;
  UMTS::Time cpTime = wTime;
  wTime = wTime + mAICHRACHOffset;

  // check that this is a valid access slot
  bool accessSlotSet1 = (wTime.FN() % 2 == 0) && (wTime.TN() % 2 == 0);
  bool accessSlotSet2 = (wTime.FN() % 2 == 1) && (wTime.TN() % 2 == 1);
  if (!(accessSlotSet1 || accessSlotSet2)) return false;
  
  for (int i = 0; i < 12; i++) {
    if (!mRACHSubchannelMask[i]) continue;
    bool validSlot = (accessSlotSet1 && (gRACHSubchannels[i][wTime.FN() % 8]*2 == (int) wTime.TN())) || 
                     (accessSlotSet2 && ((gRACHSubchannels[i][wTime.FN() % 8]*2 % gFrameSlots) == (int) wTime.TN()));
    if (!validSlot) continue;

    // correlate against preamble, is the max above the threshold?
    float SNR;
    for (int j = 0; j < 16;j++) {
      if (!mRACHSignatureMask[j]) continue;
      complex channel;
      float TOA;
      SNR = estimateChannel(&wBurst,mRACHTable[j],mRACHSearchSize,mRACHPreambleOffset,&channel,&TOA);
      TOA -= mRACHPreambleOffset;
      if (SNR > 6) LOG(INFO) << "signature: " << j << " SNR: " << SNR << " TOA: " << TOA << " time: " << wTime;
      if (SNR < detectionThreshold) {consecutiveRACH = 0; consecutiveRACHTOA = 0;}
      if (SNR > detectionThreshold) {
	if (fabs(consecutiveRACHTOA-TOA) > 2.0) {consecutiveRACH = 0; consecutiveRACHTOA = 0;}
	consecutiveRACH++;
	consecutiveRACHTOA = TOA;
	LOG(NOTICE) << "signature: " << j << " SNR: " << SNR << " TOA: " << TOA << " time: " << wTime << "cRACH: " << consecutiveRACH;

	consecutiveRACH = 0; consecutiveRACHTOA = 0;
        // if so, then send an AICH preamble ASAP.  Also be on guard to detect a 10ms/20ms RACH message part.
        // Sec. 7.3 of 25.211 states that AICH preamble must be sent 7680 (3 slots) or 12800 (5 slots) (AICH_Transmission_Timing) after RACH is received
        // Add AICH to priority queue for next available timestamp. 
        UMTS::Time mAICHResponseTime = wTime;

	while (mAICHResponseTime < gNodeB.clock().get() + UMTS::Time(1,9)) {
	  mAICHResponseTime = mAICHResponseTime + UMTS::Time(1,9); // 12 access slots 
        }
	//mAICHResponseTime = mAICHResponseTime + UMTS::Time(1,9);
	LOG(INFO) <<"Insert AICH"<<LOGVAR(SNR)<<LOGVAR(TOA)<< " at " << mAICHResponseTime << " and " << mAICHResponseTime + UMTS::Time(0,1) << ", last transmit: " << mLastTransmitTime << ", now: " << gNodeB.clock().get() << "rcvTime: " << cpTime;
	bool dummy;
        Time uselessTime;
	TxBitsBurst *out1 = new TxBitsBurst(gAICHSignatures[j].segment( 0,20),256,mAICHSpreadingCodeIndex,
		 		 mAICHResponseTime,false);
	RN_MEMLOG(TxBitsBurst,out1);
	addBurst(out1, dummy,uselessTime);
	TxBitsBurst *out2 = new TxBitsBurst(gAICHSignatures[j].segment(20,12),256,mAICHSpreadingCodeIndex,
		 		 mAICHResponseTime+UMTS::Time(0,1),false);
	RN_MEMLOG(TxBitsBurst,out2);
	addBurst(out2, dummy,uselessTime);
        // Indicated that a RACH message part is coming soon for demodulator
	mNextRACHMessageStart = mAICHResponseTime + UMTS::Time(0,3); // Sec. 7.3 of 25.211 
	mRACHMessagePending = true;
	mExpectedRACHTOA = TOA;
	return true;
      }
    } 

  }
  return false;
}


float RACHTFCI[32];
signalVector descrambledRACHFrame(gFrameLen);

bool RadioModem::decodeRACHMessage(signalVector &wBurst, UMTS::Time wTime, float detectionThreshold)
{
	if (!mRACHMessagePending) return false;

	if (wTime < mNextRACHMessageStart) return false;

	if (mNextRACHMessageStart + UMTS::Time(0,mRACHMessageSlots) <= wTime) {
		mRACHMessagePending = false;
		return false;
	}

  	float avgPwr;
	energyDetect(wBurst,(2560+1024)/4,10.0,&avgPwr);
	LOG(INFO) << "RACH Energy at " << wTime << " is: " << avgPwr;

	// correlate pilots on Q-channel for slot
	// use precomputed RACHpilots, which should include scrambling and spreading
	int slotIx = (wTime.TN() + gFrameSlots -mNextRACHMessageStart.TN()) % gFrameSlots;	
        complex channel;
        float TOA;
	float SNR = estimateChannel(&wBurst,mRACHMessagePilotWaveforms[slotIx],40,mRACHPilotsOffset+mExpectedRACHTOA-20.0,&channel,&TOA);
 	const float idealCorrelationAmplitude = 2*mRACHMessagePilotWaveforms[slotIx]->size();
 	channel = channel/idealCorrelationAmplitude;
	TOA -= mRACHPilotsOffset;
	LOG(INFO) << "RACH slotIx: " << slotIx << ", SNR: " << SNR << ", TOA: " << TOA << ", c: " << channel;
	// if viable correlation, demodulate data on I-channel and send to RACH decoder
	//if (SNR < detectionThreshold) return false;

	if (channel==complex(0,0)) channel = complex(1e6,1e6); // don't divide by zero.

	signalVector RACHBurst(wBurst);
	
	//scaleVector(RACHBurst,complex(1.0,0.0)/channel);
	delayVector(RACHBurst,round(-TOA));

	// FIXME: we should use segment or alias to avoid copy operations
	signalVector truncBurst(RACHBurst.begin(),0,gSlotLen); 
        scaleVector(truncBurst,complex(1.0,0.0)/channel);

        signalVector descrambledRACH = descrambledRACHFrame.segment(gSlotLen*slotIx,gSlotLen);

	descramble(truncBurst,
		   mRACHMessageAlignedScramblingCodeI+gSlotLen*slotIx,
		   mRACHMessageAlignedScramblingCodeQ+gSlotLen*slotIx,
		   &descrambledRACH);
						   
        signalVector *despreadRACHControl = despread(descrambledRACH,
                                                     gOVSFTree.code(mRACHMessageControlSpreadingFactorLog2,
                                                                    mRACHMessageControlSpreadingCodeIndex),
                                                     (1 << mRACHMessageControlSpreadingFactorLog2),
                                                     true);

	RACHTFCI[0+2*slotIx]= -0.5*((*despreadRACHControl)[8].real()/(float) (1 << mRACHMessageControlSpreadingFactorLog2))+0.5;
       	RACHTFCI[1+2*slotIx]= -0.5*((*despreadRACHControl)[9].real()/(float) (1 << mRACHMessageControlSpreadingFactorLog2))+0.5;

	delete despreadRACHControl;

	if (slotIx!=gFrameSlots-1) return true;

	unsigned tfci = findTfci(RACHTFCI,2);

	unsigned sfLog2 = mRACHMessageDataSpreadingFactorLog2+(tfci==0);
	unsigned sfIndex = mRACHMessageDataSpreadingCodeIndex*(1+(tfci==0));

        signalVector *despreadRACHData = despread(descrambledRACHFrame,
                                  		  gOVSFTree.code(sfLog2,sfIndex),
                                  		  (1 << sfLog2),
                                        	  false);

	// Where to send these results? the FEC is at gNodeB.mRachFec
	// FIXME: need to set RSSI
	unsigned slotSize = despreadRACHData->size()/gFrameSlots;
	for (unsigned j = 0; j < gFrameSlots; j++) {
		float dataBits[slotSize];
		for (unsigned i = 0; i < slotSize; i++) 
                        dataBits[i] = (-0.5)*((*despreadRACHData)[i+j*slotSize].real()/(float) (1 << sfLog2))+0.5;
		UMTS::Time slotTime = wTime;
		slotTime.decTN(mNextRACHMessageStart.TN()); // align time to aid L1 concatenation
		slotTime.decTN(gFrameSlots-1);
		slotTime.incTN(j);
		if (mNextRACHMessageStart.FN() % 2) slotTime.decTN(gFrameSlots); // FIXME!!
		RxBitsBurst dataBurst(sfLog2, dataBits, slotTime, TOA, 0);
		dataBurst.mTfciBits[0] = RACHTFCI[0+2*j];
                dataBurst.mTfciBits[1] = RACHTFCI[1+2*j];
		gNodeB.mRachFec->l1WriteLowSide(dataBurst);
	}

	delete despreadRACHData;

	return true;
}

bool RadioModem::decodeDCH(signalVector &wBurst,
			   UMTS::Time wTime,
                  	   int uplinkScramblingCodeIndex,
                  	   int numPilots,
			   signalVector &descrambledBurst,
			   signalVector &rawBurst,
			   float &guessTOA,
			   float &bestTOA,
			   complex &bestChannel,
			   float &bestSNR,
			   float *TFCI,
                           float *TPC)
{
	//LOG(INFO) << "decodeDCH start: " << wTime;
	// correlate pilots on Q-channel for slot
	int slotIx = wTime.TN();	
     	complex channel;
        float TOA;
	signalVector *uplinkPilots = UplinkPilotWaveforms(uplinkScramblingCodeIndex, 
							  0,
							  numPilots,slotIx);
	// FIXME: this start TOA should be adaptive based on previous TOA results
	float startTOA = (float) mDPCHOffset+384; // uplink DCH is offset by 1024 chips 
	startTOA += 10.0; // seems to be constant...Tx+Rx group delay of the RAD3 perhaps.
	float SNR;
	float corrWindow = 40.0;
	bool validTOAGuess = (guessTOA > -5000.0);
	if (validTOAGuess) { // guess is useful
		startTOA = guessTOA+384;
		corrWindow = 5.0;
	}
	SNR = estimateChannel(&wBurst,uplinkPilots,corrWindow*2+1,(startTOA-corrWindow),&channel,&TOA);
	
 	const float idealCorrelationAmplitude = 2*uplinkPilots->size();
 	channel = channel/idealCorrelationAmplitude;
        TOA = TOA-384;
	LOG(INFO) << "slotIx: " << slotIx << ", SNR: " << SNR << ", guessTOA: " << guessTOA << ", TOA: " << TOA << " " << corrWindow << ", c: " << channel << " abs: " << channel.abs();

	// if viable correlation, demodulate data on I-channel and send to RACH decoder
	//if (SNR < detectionThreshold) return false;

	if (channel==complex(0,0)) channel = complex(1e6,1e6); // don't divide by zero.

	signalVector rawData = rawBurst.segment(gSlotLen*slotIx,wBurst.size());	
	wBurst.copyTo(rawData);

	//scaleVector(wBurst,complex(1.0,0.0)/channel);
 	delayVector(wBurst,-TOA); //round(-TOA));

	// FIXME: we should use segment or alias to avoid copy operations
	signalVector truncBurst(wBurst.begin(),0,gSlotLen); 
        scaleVector(truncBurst,complex(1.0,0.0)/channel);

        if (!mUplinkScramblingCodes[uplinkScramblingCodeIndex])
          mUplinkScramblingCodes[uplinkScramblingCodeIndex] = new UplinkScramblingCode(uplinkScramblingCodeIndex);

	signalVector descrambleResult = descrambledBurst.segment(gSlotLen*slotIx,gSlotLen);

	//LOG(INFO) << "des start: " << wTime;
	descramble(truncBurst,
		   (int8_t *) mUplinkScramblingCodes[uplinkScramblingCodeIndex]->ICode()+gSlotLen*slotIx,
		   (int8_t *) mUplinkScramblingCodes[uplinkScramblingCodeIndex]->QCode()+gSlotLen*slotIx,
		   &descrambleResult);

        //if ((wTime.FN() % 100 == 0) && (!wTime.TN())) 
	//	LOG(INFO) << "despread DCH data: " << *despreadDCHData;

	signalVector descrambleResultTFCITPC = descrambleResult.segment((1<<8)*0,(1<<8)*10);

        signalVector *despreadDCHControl = despread(descrambleResultTFCITPC,
                                  		     gOVSFTree.code(8,0),
                                  		     (1 << 8),
                                        	     true);
        //LOG(INFO) << "desp stop: " << wTime;

	//if (slotIx == 14) 
	//	LOG(INFO) << "despread DCH ctrl: " << *despreadDCHControl << ", " << numPilots << " pilots should be: " << gPilotPatterns[numPilots-3][slotIx]; 

	// FIXME: assume slot format 0...need to adapt accordingly
	float bitscale = -0.5*(1.0/(float) (1 << 8)/2.0);
	TFCI[0+2*slotIx] = bitscale*((*despreadDCHControl)[0+6].real()) +0.5;
        TFCI[1+2*slotIx] = bitscale*((*despreadDCHControl)[1+6].real()) +0.5;
	TPC[0+2*slotIx] =  bitscale*((*despreadDCHControl)[2+6].real()) +0.5;
        TPC[1+2*slotIx] =  bitscale*((*despreadDCHControl)[3+6].real()) +0.5;

        //LOG(INFO) << "DCH TFCI: " << TFCI[0+2*slotIx] << " " << TFCI[1+2*slotIx];

        delete despreadDCHControl;
        //LOG(INFO) << "decodeDCH stop: " << wTime;

        if (slotIx!=0) return true;

	if (bestSNR < SNR) {
		bestTOA = TOA;
		bestSNR = SNR;
		bestChannel = channel;
	}

	if (SNR > 3.0) {
		guessTOA = TOA;
        }
        else {
		guessTOA = -10000.0; 
	}
	
	return true;
}	

bool RadioModem::decodeDPDCHFrame(DPDCH &frame,
				  int uplinkScramblingCodeIndex,
                                  int uplinkSpreadingFactorLog2,
				  int uplinkSpreadingCodeIndex)
{

        delayVector(frame.rawBurst,-frame.bestTOA); //round(-TOA));

        // FIXME: we should use segment or alias to avoid copy operations
        signalVector truncBurst(frame.rawBurst.begin(),0,gFrameLen);
        scaleVector(truncBurst,complex(1.0,0.0)/frame.bestChannel);

        if (!mUplinkScramblingCodes[uplinkScramblingCodeIndex])
          mUplinkScramblingCodes[uplinkScramblingCodeIndex] = new UplinkScramblingCode(uplinkScramblingCodeIndex);

        signalVector descrambleResult(truncBurst.size());

        //LOG(INFO) << "des start: " << wTime;
        descramble(truncBurst,
                   (int8_t *) mUplinkScramblingCodes[uplinkScramblingCodeIndex]->ICode(),
                   (int8_t *) mUplinkScramblingCodes[uplinkScramblingCodeIndex]->QCode(),
                   &descrambleResult);


	signalVector *despreadDCHData = despread(descrambleResult,
                                                gOVSFTree.code(uplinkSpreadingFactorLog2,
                                                               uplinkSpreadingCodeIndex),
                                                (1 << uplinkSpreadingFactorLog2),
                                                false);

	//LOG(INFO) << "despreadDCHData: " << despreadDCHData->segment(0,1000);

	//FIXME: need to set RSSI
        float bitScale = -0.5/((float) (1 << uplinkSpreadingFactorLog2)*2.0);

#ifndef FRAMEBURSTS
	unsigned numBitsSlot = despreadDCHData->size()/gFrameSlots;
	for (unsigned j = 0; j < gFrameSlots; j++) {
	  float *dataBits = new float[numBitsSlot];
	  for (unsigned i = 0; i < numBitsSlot; i++) { 
	      dataBits[i] = bitScale* ((*despreadDCHData)[i+numBitsSlot*j].real()) + 0.5;
          }
          RxBitsBurst* dataBurst = new RxBitsBurst(uplinkSpreadingFactorLog2, 
						   dataBits, 
						   UMTS::Time(frame.frameTime.FN(),j), 0 /*TOA*/, 
						   0 /*RSSI*/);
	  //if (j == 0) LOG(INFO) << "rxbits: " << *(dynamic_cast<SoftVector*>(dataBurst));
          dataBurst->mTfciBits[0] = frame.tfciBits[0+2*j];
          dataBurst->mTfciBits[1] = frame.tfciBits[1+2*j];
          FECDispatchInfo *q = new FECDispatchInfo;
          q->fec = (void*) frame.fec;
          q->burst = dataBurst;
          RN_MEMLOG(RxBitsBurst,dataBurst);
          mDispatchQueue.write(q);
	}
#else
        unsigned numBitsFrame = despreadDCHData->size();
	LOG(INFO) << "numBitsFrame: " << numBitsFrame;
        float *dataBits = new float[numBitsFrame];
        for (unsigned i = 0; i < numBitsFrame; i++) {
            dataBits[i] = bitScale* ((*despreadDCHData)[i].real()) + 0.5;
        }
        RxBitsBurst* dataBurst = new RxBitsBurst(uplinkSpreadingFactorLog2,
                                                   dataBits,
						   numBitsFrame,
                                                   UMTS::Time(frame.frameTime.FN(),0), 0 /*TOA*/,
                                                   0 /*RSSI*/);
        //if (j == 0) LOG(INFO) << "rxbits: " << *(dynamic_cast<SoftVector*>(dataBurst));
        FECDispatchInfo *q = new FECDispatchInfo;
        q->fec = (void*) frame.fec;
        q->burst = dataBurst;
	memcpy(q->tfciFrame,frame.tfciBits,30*sizeof(float));
        RN_MEMLOG(RxBitsBurst,dataBurst);
        mDispatchQueue.write(q);
#endif

	delete despreadDCHData;
 
	return true;

}


void RadioModem::receiveBurst(void)
{
        char buffer[MAX_UDP_LENGTH];
        int msgLen = mDataSocket.read(buffer);

        if (msgLen<=0) SOCKET_ERROR;

        // decode
        unsigned char *rp = (unsigned char*)buffer;
        // timeslot number
        unsigned int TN = *rp++;
        // frame number
        int16_t FN = *rp++;
        FN = (FN<<8) + (*rp++);
        // soft symbols
	unsigned int burstLen = gSlotLen+1024+mDelaySpread;
	signalVector *dataBurst = new signalVector(burstLen);
	RN_MEMLOG(signalVector,dataBurst);
  	complex *burstPtr = dataBurst->begin();
        for (unsigned int i=0; i<burstLen; i++) {
	  *burstPtr++ = complex((float) ((radioData_t) (signed char) (*rp)), 
				(float) ((radioData_t) (signed char) (*(rp+1)))); //complex(dataI[i],dataQ[i]);
	  rp++; rp++;
        }
	receiveSlot(dataBurst, UMTS::Time(FN,TN));
	delete dataBurst;
	//bool underrun;
}

void RadioModem::receiveSlot(signalVector *wwBurst, UMTS::Time wTime) 
{
  //float avgPwr;
  //energyDetect(*wBurst,50,10.0,&avgPwr);
  //if (avgPwr > 20000.0) LOG(INFO) << "Enrg: " << wTime << " " << avgPwr;

  signalVector *wBurst = wwBurst;

  RACHProcessorInfo *q = new RACHProcessorInfo;
  q->burst = (void*) new signalVector(*wBurst);
  q->burstTime = wTime;
  RN_MEMLOG(signalVector,wBurst);
  mRACHQueue.write(q);

#if 1 
  // gActiveDCH...a list of active DCH FEC objects.
  // go through list and demodulate for each DCH. 
  DCHListType::const_iterator DCHBegin, DCHEnd;
  {
    ScopedLock lock(gActiveDCH.mLock);
    if ((wTime.TN()==0) && (wTime.FN() % 4 == 0) && (gActiveDPDCH.size() > gActiveDCH.size())) {
           // more than one DCH just closed, need to rebuild map
           gActiveDPDCH.clear();
    }
    DCHBegin = gActiveDCH.begin();
    DCHEnd = gActiveDCH.end();
    gActiveDCH.inRxUse = true;
  }

  int threadCtr = 0;
  for (DCHListType::const_iterator DCHItr = DCHBegin;
       DCHItr != DCHEnd;
       DCHItr++)	 
  {
        DCHFEC *currDCH = *DCHItr;
        if (!currDCH->active()) continue;
 	DCHProcessorInfo *q = new DCHProcessorInfo;
	q->burst = (void *) new signalVector(*wBurst);
	q->burstTime = wTime;
	q->fec = *DCHItr;
	RN_MEMLOG(signalVector,(signalVector*) (q->burst));
	mDCHQueue[threadCtr++].write(q);
  }
  {
    ScopedLock lock(gActiveDCH.mLock);
    gActiveDCH.inRxUse = false;
  }
#endif

  return;
} 




radioData_t waveformI[gSlotLen];
radioData_t waveformQ[gSlotLen];
radioData_t* finalWaveformI = new radioData_t[gSlotLen];
radioData_t* finalWaveformQ = new radioData_t[gSlotLen];


void RadioModem::transmitSlot(UMTS::Time nowTime, bool &underrun)
{

  //LOG(DEBUG) << "RM transmitting at time: " << nowTime;

  // start with pilot waveforms
  int slotIx = nowTime.TN();
  memcpy(waveformI,mDownlinkPilotWaveformsI,sizeof(radioData_t)*gSlotLen);
  memcpy(waveformQ,mDownlinkPilotWaveformsQ,sizeof(radioData_t)*gSlotLen);

  underrun = false;

  // dump stale bursts, if any
  while (TxBitsBurst* staleBurst = mTxQueue->getStaleBurst(nowTime)) {
    LOG(NOTICE) << "dumping STALE burst in UMTS Tx Queue, nowTime: " << nowTime << ", burst time:" << staleBurst->time();
    delete staleBurst;
    underrun = true;
  }

  std::map<unsigned int,bool> receivedBursts;
  receivedBursts.clear();
  // if queue contains data at the desired timestamp, spread it and accumulate
  while (TxBitsBurst* next = (TxBitsBurst *) mTxQueue->getCurrentBurst(nowTime)) {
    //LOG(INFO) << "transmitFIFO: wrote burst " << next << " at time: " << nowTime;
    unsigned int startIx = (next->rightJustified()) ? (gSlotLen-(next->size()/2*next->SF())) : 0;
    if (next->DCH()) //(next->SF()!=256) 
	LOG(INFO) << "time: " << nowTime << ", spreading " << next->log2SF() << ", " << next->codeIndex() << ", " << next->size() << ", " << next->rightJustified() << ", " << next->DCH();
    spread(*next, 
	   (int8_t *) gOVSFTree.code (next->log2SF(),next->codeIndex()),
	   next->SF(), waveformI+startIx, waveformQ+startIx, gSlotLen, next->DCH() ? mDCHAmplitude : mCCPCHAmplitude);
    receivedBursts[(1 << next->log2SF()) + (next->codeIndex() << 16)] = true;
    delete next;
  }

#if 1 
  // stick in DPCCH pilots for any active DCH channels, UE needs these for synchronization in CELL_DCH state
  // need to stick in TPC bits and TFCI bits too.
  DCHListType::const_iterator DCHItr, DCHEnd;
  {
    //LOG(NOTICE) << "waiting for lock";
    ScopedLock lock(gActiveDCH.mLock);
    //LOG(NOTICE) << "got lock";
    DCHItr = gActiveDCH.begin();
    DCHEnd = gActiveDCH.end();
    gActiveDCH.inTxUse = true;
  }
  while (DCHItr != DCHEnd) {
        DCHFEC *currDCH = *DCHItr;	//dynamic_cast<DCHFEC*>(*DCHItr);	// (pat) cast should not be needed?
	if (!currDCH->active()) {DCHItr++; continue;}
        int downlinkSpreadingFactor = currDCH->getPhCh()->getDlSF();
	int downlinkLog2SF = currDCH->getPhCh()->getDlSFLog2();
        int downlinkSpreadingCodeIndex = currDCH->getPhCh()->SpCode();
	SlotFormat *dlslot = currDCH->getPhCh()->getDlSlot();
	const unsigned bitsInSlot = dlslot->mBitsPerSlot;
        const unsigned npilot = dlslot->mNPilot;
        const unsigned pi = dlslot->mPilotIndex;
	const unsigned ndata1 = dlslot->mNData1;
	const unsigned ntpc = dlslot->mNTpc;
	const unsigned ntfci = dlslot->mNTfci;
        unsigned tpcField = ((1 << ntpc)-1);
	BitVector TPCBits(ntpc);
        TPCBits.fillField(0,tpcField,ntpc);
        int startIx = downlinkSpreadingFactor * (ndata1/2);
        spread(TPCBits,
               (int8_t *) gOVSFTree.code(downlinkLog2SF,downlinkSpreadingCodeIndex),
               downlinkSpreadingFactor,
               waveformI+startIx, waveformQ+startIx, gSlotLen, mDCHAmplitude);

        if (receivedBursts.find(downlinkSpreadingFactor + (downlinkSpreadingCodeIndex << 16)) != receivedBursts.end()) {
                DCHItr++;
                continue;
        }

        BitVector TFCIBits(ntfci);
        TFCIBits.fillField(0,0,ntfci);
        startIx += downlinkSpreadingFactor * (ntpc/2);
        spread(TFCIBits,
               (int8_t *) gOVSFTree.code(downlinkLog2SF,downlinkSpreadingCodeIndex),
               downlinkSpreadingFactor,
               waveformI+startIx, waveformQ+startIx, gSlotLen, mDCHAmplitude);

        BitVector radioSlotBits(npilot);
	startIx = downlinkSpreadingFactor * (bitsInSlot-npilot)/2;
        radioSlotBits.fillField(0, TrCHConsts::sDlPilotBitPattern[pi][slotIx], npilot);
	// spread pilot symbols and accumulate
	spread(radioSlotBits,
	       (int8_t *) gOVSFTree.code(downlinkLog2SF,downlinkSpreadingCodeIndex),
	       downlinkSpreadingFactor,
	       waveformI+startIx, waveformQ+startIx, gSlotLen, mDCHAmplitude);
	DCHItr++;
  }
#endif
  {
    ScopedLock lock(gActiveDCH.mLock);
    gActiveDCH.inTxUse = false;
  }

  // scramble
  // start with the P-SCH + S-SCH waveforms as they are unscrambled
  // NOTE: The scrambling code is aligned 256 chips into the slot, so that chip "0" falls upon the first non-zero chip of the P-CCPCH.

  memcpy(finalWaveformI, mDownlinkSCHWaveformsI[slotIx],sizeof(radioData_t)*gSlotLen);
  memcpy(finalWaveformQ, mDownlinkSCHWaveformsQ[slotIx],sizeof(radioData_t)*gSlotLen);
  scramble(waveformI, waveformQ, gSlotLen,
	   mDownlinkAlignedScramblingCodeI+gSlotLen*slotIx,
	   mDownlinkAlignedScramblingCodeQ+gSlotLen*slotIx,
	   gSlotLen,&finalWaveformI,&finalWaveformQ);

  static const int bufferSize = 2*gSlotLen+3+1;
//  char *buffer = new char[bufferSize];
  char *buffer = (char *) malloc(sizeof(char)*bufferSize);
  unsigned char *wp = (unsigned char*)buffer;
  // slot
  *wp++ = nowTime.TN();
  // frame number
  uint16_t FN = nowTime.FN();
  *wp++ = (FN>>8) & 0x0ff;
  *wp++ = (FN) & 0x0ff;

  // copy data
  signed char *wpp = (signed char *) wp;
  for (unsigned i=0; i<gSlotLen; i++) {
     *wpp++ = (signed char) finalWaveformI[i];
     *wpp++ = (signed char) finalWaveformQ[i];
  }

  buffer[bufferSize-1] = '\0';

  // write to the socket
  mDataSocket.write(buffer,bufferSize);
  delete []buffer;

  mLastTransmitTime = nowTime;
  //LOG(INFO) << LOGVAR(mLastTransmitTime) <<LOGVAR2("clock.FN",gNodeB.clock().FN());

}

void RadioModem::addBurst (TxBitsBurst *wBurst, bool &underrun, Time& updateTime)
{
  underrun = false;
  if (wBurst->time() > mLastTransmitTime) {
    mTxQueue->write(wBurst);
    assert(mTxQueue->size() < 10000);       // (pat) FIXME: Make sure someone is on the other end.
  } else {
    underrun = true;
    updateTime = mLastTransmitTime;
  }
}

