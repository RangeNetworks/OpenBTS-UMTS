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

#ifndef UMTSRADIOMODEM_H
#define UMTSRADIOMODEM_H

#include "sigProcLib.h"
//#include <map>
#include "LinkedLists.h"
#include "Sockets.h"
#include "UMTSCodes.h"
#include <Configuration.h>

extern ConfigurationTable gConfig;


namespace UMTS {

typedef int16_t radioData_t;

/** a priority queue of radioVectors, i.e. UMTS bursts, sorted so that earliest element is at top */
class TxBitsQueue : public InterthreadPriorityQueue<TxBitsBurst> {

public:

  /** the top element of the queue */
  UMTS::Time nextTime() const;

  /**
    Get stale burst, if any.
    @param targTime The target time.
    @return Pointer to burst older than target time, removed from queue, or NULL.
  */
  TxBitsBurst* getStaleBurst(const UMTS::Time& targTime);

  /**
    Get current burst, if any.
    @param targTime The target time.
    @return Pointer to burst at the target time, removed from queue, or NULL.
  */
  TxBitsBurst* getCurrentBurst(const UMTS::Time& targTime);

};

struct FECDispatchInfo {
	void *fec; // actually DCHFEC;
	RxBitsBurst *burst;
	float tfciFrame[30];
	FECDispatchInfo() { RN_MEMCHKNEW(FECDispatchInfo); }
	~FECDispatchInfo() { RN_MEMCHKDEL(FECDispatchInfo); }
};

struct RACHProcessorInfo {
        void *burst; // actually DCHFEC;
        UMTS::Time burstTime;
        RACHProcessorInfo() { RN_MEMCHKNEW(RACHProcessorInfo); }
        ~RACHProcessorInfo() { RN_MEMCHKDEL(RACHProcessorInfo); }
};

struct DCHProcessorInfo {
        void *burst; // actually DCHFEC;
        UMTS::Time burstTime;
	void *fec;
        DCHProcessorInfo() { RN_MEMCHKNEW(DCHProcessorInfo); }
        ~DCHProcessorInfo() { RN_MEMCHKDEL(DCHProcessorInfo); }
};

struct DCHLoopInfo {
	void *radioModem;
	int threadId;
};

// Assuming one sample per chip.  

        class DPDCH
        {
        public:
        void* fec;
        UMTS::Time frameTime;
        signalVector descrambledBurst;
	signalVector rawBurst;
        float tfciBits[32];
        float tpcBits[30];
        bool active;
	float bestTOA;
	complex bestChannel;
	float bestSNR;
        float lastTOA;
        float powerMultiplier;

        DPDCH(void* wFEC, UMTS::Time wTime): fec(wFEC), frameTime(wTime)
        {
		active = true; 
	 	descrambledBurst = signalVector(gFrameLen); 
	 	rawBurst = signalVector(gFrameLen+gSlotLen); 
	 	lastTOA = bestTOA = -10000.0; 
	 	bestChannel = 1.0; 
	 	powerMultiplier=1.0;
        }

        ~DPDCH()
        {}

        };


// TODO: The RadioModem needs a loop to call transmitSlot repeatedly.
class RadioModem

{

public:

/*	class DPDCH
	{
        public:
        void* fec;
        UMTS::Time frameTime;
        signalVector descrambledBurst;
        float tfciBits[32];
	float tpcBits[30];    
        bool active;
	float TOA;
	float powerMultiplier;

        DPDCH(void* wFEC, UMTS::Time wTime): fec(wFEC), frameTime(wTime)
        {active = true; descrambledBurst = signalVector(gFrameLen); TOA = -10000.0;powerMultiplier=1.0;}

        ~DPDCH()
        {}

	};
*/
	std::map<void*,DPDCH*> gActiveDPDCH;


	UDPSocket& mDataSocket;


        RadioModem(UDPSocket& wDataSocket);

        /* (pointer to channel map,
                    map of scrambling codes,
                    priority queue of TxBitsBurst objects)
        */
        // gather up submitted slots for transmission at timestamp
        // return underrun to indicate that queue contains bursts that are too old
        void transmitSlot(UMTS::Time timestamp, bool &underrun);

        // public method to add TxBitsBurst burst for transmission
        // return underrun to indicate that burst is too late, and what time the clock should be updated to
        void addBurst (TxBitsBurst *wBurst, bool &underrun, Time &updateTime);

	// receive burst from UDP packet
        void receiveBurst(void);

        /*struct FECDispatchInfo {
                DCHFEC *fec;
                RxBitsBurst *burst;
        };*/

        InterthreadQueueWithWait<FECDispatchInfo> mDispatchQueue;
        InterthreadQueueWithWait<RACHProcessorInfo> mRACHQueue;
        InterthreadQueueWithWait<DCHProcessorInfo> mDCHQueue[100];

        friend void *FECDispatchLoopAdapter(RadioModem*);
        friend void *RACHLoopAdapter(RadioModem*);
        friend void *DCHLoopAdapter(DCHLoopInfo*);

        static constexpr float mRACHThreshold = 10.0;


private:

	// receive data
        void receiveSlot (signalVector *wBurst, UMTS::Time wTime);


        // map between a hash and an array of 15 signalVectors of varying length
        // hash function is (scramblingcode*6)+nP
        std::map<int,signalVector**> mUplinkPilotWaveformMap;
        std::map<int,UplinkScramblingCode*> mUplinkScramblingCodes;

        inline int waveformMapHash(int scramblingCode, int nP) { return scramblingCode*6+nP;}

        signalVector *mRACHTable[16];

        //      ChannelMap   *mMap; // ???
        TxBitsQueue *mTxQueue;

        // Going to assume we are only using one signature
        bool mRACHSignatureMask[16];
        bool mRACHSubchannelMask[12];

        // indices into scrambling tables
        int mUplinkRACHScramblingCodeIndex;
        int mUplinkPRACHScramblingCodeIndex;
        UMTS::Time mAICHRACHOffset;
        int mAICHSpreadingCodeIndex;
        int mRACHSearchSize;
        // RACH correlator length
        int mRACHCorrelatorSize;
	int mRACHPreambleOffset;
	int mRACHPilotsOffset;
	signalVector *mRACHMessagePilotWaveforms[gFrameSlots];
	int mRACHMessageControlSpreadingFactorLog2;
	int mRACHMessageDataSpreadingFactorLog2;
	int mRACHMessageControlSpreadingCodeIndex;
	int mRACHMessageDataSpreadingCodeIndex;
	int mRACHMessageSlots;
	bool mRACHMessagePending;
	UMTS::Time mNextRACHMessageStart;
        int8_t mRACHMessageAlignedScramblingCodeI[gFrameLen];
        int8_t mRACHMessageAlignedScramblingCodeQ[gFrameLen];
	double mExpectedRACHTOA;

        int mDownlinkScramblingCodeIndex;
        DownlinkScramblingCode* mDownlinkScramblingCode;
        int8_t mDownlinkAlignedScramblingCodeI[gFrameLen];
        int8_t mDownlinkAlignedScramblingCodeQ[gFrameLen];
        int mDownlinkSpreadingCodeIndex;

        // uplink is behind the downlink for DPCH.
        static const int mDPCHOffset = 1024;

        // latest transmit timestamp
	public:
        UMTS::Time mLastTransmitTime;
		unsigned txqsize() { return mTxQueue->size(); }

	private:
 
       	int mDelaySpread;
        int mDPCCHCorrelationWindow;
        int mDPCCHSearchSize;

        int mSSCHGroupNum;
	radioData_t *mDownlinkSCHWaveformsI[gFrameSlots];
	radioData_t *mDownlinkSCHWaveformsQ[gFrameSlots];
        radioData_t *mDownlinkPilotWaveformsI;
        radioData_t *mDownlinkPilotWaveformsQ;

	static const radioData_t mCPICHAmplitude = 5;
  	static const radioData_t mPSCHAmplitude = 2;  // usually 3dB below CPICH, but can be signifcantly lower (PSCH not scrambled)
  	static const radioData_t mSSCHAmplitude = 5;  // usually 3dB below CPICH (keep in mind SSCH not scrambled)
  	static const radioData_t mCCPCHAmplitude = 2; // e.g. typically CPCCH is 5 dB below CPICH, AICH level is set in SIB5, etc.
	static const radioData_t mAICHAmplitude = 20; // FIXME: Is this right?
	static const radioData_t mDCHAmplitude = 10;
	Thread mFECDispatcher;
	Thread mRACHProcessor;
	Thread mDCHProcessor[100];

	/* Generate a table of pilot sequences for lookup and later correlation 
	   Defined Sec. 5.2.1.1 of 25.211, dependes upon higher layer parameters and the slot */
	signalVector* UplinkPilotWaveforms(int scramblingCode, int codeIndex, int numPilots, int slotIx);

	/* Generate a table of RACH preambles
           Need to know scrambling code assigned to RACH preambles and message part */
	void generateRACHPreambleTable(int startIx, int filtLen);

	void generateRACHMessagePilots(int filtLen);

	/* Generate and combine SCH (P-SCH and S-SCH) and CPICH waveforms for repeated transmission */
	void generateDownlinkPilotWaveforms();

	/* Estimate the channel (phase, amplitude, time offset) */
	float estimateChannel(signalVector *wBurst,
                                 signalVector *matchedFilter,
                                 unsigned maxTOA,
                                 unsigned startTOA,
                                 complex *channel,
                                 float *TOA);

	/* Accumulate a vector into an existing vector */
	void accumulate(radioData_t *addI, radioData_t *addQ, int addLen, radioData_t *accI, radioData_t *accQ);


        /* Scramble a transmit burst...essentially a series of sign changes on array of floats. 
           Sign change can be implemented simply as a bit flip. */
	void scramble(radioData_t *wBurstI, radioData_t *wBurstQ, int len,
                          int8_t *codeI, int8_t *codeQ, int codeLen,
                          radioData_t **rBurstI, radioData_t **rBurstQ);

	/* Used to scramble pre-computed RACH waveforms */
	void scrambleRACH(radioData_t *wBurstI, int len,
                              int8_t *codeI, int codeLen,
                              radioData_t **rBurstI);


	/* Descramble a receive burst...essentially a series of sign changes on array of floats?  Nope, they are complex multiplies.*/
	signalVector* descramble(signalVector &wBurst, int8_t *codeI, int8_t *codeQ, signalVector *retVec = NULL);

	/* Despread a descrambled burst...essentially an integrate(add/subtract) and dump operation on floats. */
	signalVector *despread(signalVector &wBurst, 
                               const int8_t *code,
                               int codeLength,
			       bool useQ);
	
	/* Spread a scrambled burst...essentially an Kronecker product */
	void spread(BitVector &wBurst, int8_t *code, int codeLen, radioData_t *accI, radioData_t *accQ, int accLen, radioData_t gain = 1);

	void spreadOneBranch(BitVector &wBurst, int8_t *code, int codeLen, radioData_t *acc, int accLen);

	public: 

	/* Detect a RACH preamble, and return AICH in following access slot */
	bool detectRACHPreamble(signalVector &wBurst, UMTS::Time wTime, float detectionThreshold);

	/* Decode expected RACH message */
        bool decodeRACHMessage(signalVector &wBurst, UMTS::Time wTime, float detectionThreshold);

	/* Decode expected DCH burst */
	bool decodeDCH(signalVector &wBurst,
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
			   float *TPC);

	bool decodeDPDCHFrame(DPDCH &frame,
			      int uplinkScramblingCodeIndex,
                              int uplinkSpreadingFactorLog2,
			      int uplinkSpreadingCodeIndex);
	void radioModemStart();
};	

//void* FECDispatchLoopAdapter(RadioModem* rm);


}

void* FECDispatchLoopAdapter(UMTS::RadioModem* rm);

void* RACHLoopAdapter(UMTS::RadioModem* rm);

void* DCHLoopAdapter(UMTS::DCHLoopInfo* dli);


#endif
