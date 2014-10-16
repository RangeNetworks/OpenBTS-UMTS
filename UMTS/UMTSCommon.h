/**@file UMTS common-use objects and functions. */

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

#ifndef UMTSCOMMON_H
#define UMTSCOMMON_H

#include <Interthread.h>
#include <BitVector.h>
#include <Timeval.h>
#include <ostream>

#include <GSMCommon.h>


// If USE_OLD_FEC is 0, USE_OLD_DCH must be 0 too.
#define USE_OLD_FEC 0	// BCH, FACH and RACH
#define USE_OLD_DCH 0	// DCH can use old or new L1 separately from BCH, FACH, RACH


namespace UMTS {

class TrCHFEC;
class L1CCTrCh;

#if USE_OLD_FEC
class TrCHFEC;
typedef TrCHFEC L1FEC_t;
#else
class L1CCTrCh;
typedef L1CCTrCh L1FEC_t;
#endif

const unsigned gSlotLen = 2560;
const unsigned gFrameMicroseconds = 10000;
const unsigned gHyperframe = 4096;
const unsigned gFrameSlots = 15;
const unsigned gFrameLen = 38400;
const unsigned gSlotMicroseconds = gFrameMicroseconds/gFrameSlots;	// Integer is approximate

// (pat) Physical channel types.
enum PhChType {
	CPICHType,		// Common pilot channel, carries sync, not a TrCh.
	PCCPCHType,		// Primary common control physical channel, carries beacon.
	SCCPCHType,		// carries downlink FACH (common downlink, control or data) and PCH (paging)
	PRACHType,		// carries uplink RACH
	DPDCHType 		// dedicated physical data channel, bidirectional, carries DCH.
};

// Warning: The GSM directory defines GSM::RACHType that has nothing to do with this.
enum TrChType {
	TrChInvalid,	// To mark an unspecified value.
	TrChBCHType,
	TrChPCHType,
	TrChRACHType,
	TrChFACHType,
	// TrChDCHType,		// bidirectional DCH.
	TrChDlDCHType,		// downlink DCH
	TrChUlDCHType		// uplink DCH
	// And unimplemented: EDCH and HSDSCH
};

// (pat) Logical channel types.
// 25.331 10.2 Radio Resource Control Messages - each one specifies which logical channel it is sent on.
// Warning: The GSM directory redefines several of these in that namespace.
enum ChannelTypeL3 {
	BCCHType,		// (pat) Used exclusively for SIB messages.  Sent on BCH but may also be sent on FACH (why?)
	CCCHType,		// (pat) For UE in unconnected-mode; Uses SRB0
	MCCHType,
	MSCHType,
	MTCHType,
	CTCHType,
	DTCCH_FACHType,	// (pat) do not need this, use the TrChType.
	DTCCH_RACHType,	// (pat) do not need this, use the TrChType.
	DCCHType,		// (pat) For UE in connected-mode; Uses SRB1, 2, 3, 4
	DTCHType,
	UNDEFINED_CHANNEL
};

enum UMTSBand {
	UMTS850=850,  // Band V
	UMTS900=900,  // Band VII
	UMTS1700=1700, // Band IV
	UMTS1800=1800, // Band III
	UMTS1900=1900, // Band II
	UMTS2100=2100  // Band I
};


/**@name Actual radio carrier frequencies, in kHz, 3GPP 25.104 5. */
//@{
unsigned channelFreqKHz(UMTSBand wBand, unsigned wARFCN);
unsigned uplinkOffsetKHz(UMTSBand);
//@}

/** Get a clock difference, within the modulus, v1-v2. */
int16_t FNDelta(int16_t v1, int16_t v2);

/**
        Compare two frame clock values.
        @return 1 if v1>v2, -1 if v1<v2, 0 if v1==v2
*/
int FNCompare(int16_t v1, int16_t v2);

class Time {

	private:

	int16_t mFN;			///< frame number
	int mTN;			///< UMTS radio slot number

	public:

	Time(uint16_t wFN=0, unsigned wTN=0)
		:mFN(wFN % gHyperframe),mTN(wTN)
	{ }


	Time slot(unsigned s) const { assert(s<gFrameSlots); return Time(mFN,s); }

	/**@name Accessors. */
	//@{
	int16_t FN() const { return mFN; }
	void FN(uint16_t wFN) { mFN = wFN; }
	unsigned TN() const { return mTN; }
	//void TN(int wTN) { mTN=wTN; }
	//@}

	/**@name Arithmetic. */
	//@{

	Time& operator++()
	{
		mFN = (mFN+1) % gHyperframe;
		return *this;
	}

	Time& decTN(unsigned step=1)
	{
		mTN -= step;
		if (mTN<0) {
			mTN+=gFrameSlots;
			mFN--;
			if (mFN<0) mFN+=gHyperframe;
		}
		return *this;
	}

	Time& incTN(unsigned step=1)
	{
		mTN += step;
		if (mTN>=(int)gFrameSlots) {
			mTN-=gFrameSlots;
			mFN = (mFN+1) % gHyperframe;
		}
		return *this;
	}

	Time& operator+=(int step)
	{
		// Remember the step might be negative.
		mFN += step;
        	if (mFN<0) mFN+=gHyperframe;
                mFN = mFN % gHyperframe;
		return *this;
	}

	Time operator-(int step) const
		{ return operator+(-step); }

	Time operator+(int step) const
	{
		Time newVal = *this;
		newVal += step;
		return newVal;
	}

	Time operator+(const Time& other) const
	{
		unsigned sTN = mTN+other.mTN;
		unsigned newTN = sTN % gFrameSlots;
		int16_t newFN = mFN+other.mFN + (sTN/gFrameSlots);
		newFN = newFN % gHyperframe;
		return Time(newFN,newTN);
	} 

	int operator-(const Time& other) const
		{ return FNDelta(mFN,other.mFN); }	

	//@}


	/**@name Comparisons. */
	//@{

	bool operator<(const Time& other) const
	{
		if (mFN==other.mFN) return (mTN<other.mTN);
		return FNCompare(mFN,other.mFN) < 0;
	}

	bool operator>(const Time& other) const
	{
		if (mFN==other.mFN) return (mTN>other.mTN);
		return FNCompare(mFN,other.mFN) > 0;
	}

	bool operator<=(const Time& other) const
	{
		if (mFN==other.mFN) return (mTN<=other.mTN);
		return FNCompare(mFN,other.mFN) <=0;
	}

	bool operator>=(const Time& other) const
	{
		if (mFN==other.mFN) return (mTN>=other.mTN);
		return FNCompare(mFN,other.mFN)>=0;
	}

	bool operator==(const Time& other) const
		{ return (mFN == other.mFN) && (mTN==other.mTN); }

	//@}

};

std::ostream& operator<<(std::ostream& os, const Time& ts);

/**
	A class for calculating the current UMTS frame number.
	Has built-in concurrency protections.
*/
class Clock {

	private:

	mutable Mutex mLock;
	int64_t mBaseFN;
	Timeval mBaseTime;

	public:

	Clock(const UMTS::Time& when = UMTS::Time(0))
		:mBaseFN(when.FN())
	{}

	/** Set the clock to a value. */
	// (pat) This is called by TRXManager.cpp:TransceiverManager::clockHandler()
	// However, I was seeing it called regularly, which is a bad thing.
	void setFN(unsigned wFN);

	/** Read the clock. */
	int32_t FN(uint32_t *fractionUSecs = NULL) const;

	/** Read the clock. */
	UMTS::Time get() const { return UMTS::Time(FN()); }

	/** Block until the clock passes a given time. */
	void wait(const UMTS::Time&) const;
};


// (pat 3-24-2012) This is currently unused.
enum PhyChanBranch {
	I_ONLY = 0,
	Q_ONLY = 1,
	I_AND_Q = 2
};

class TrCHFEC;

class PhyChanParams {

	private:
	
        TrCHFEC *mHighSideFEC;
	UMTS::Time mStartTime;
        UMTS::Time mLastDetectedTime;
	UMTS::Time mFirstDetectedTime;
	bool mRACH;
        float mLastTOA;
	
	public:

	PhyChanParams(TrCHFEC* wHighSideFEC, UMTS::Time wStartTime, bool isRACH = false): 
		mHighSideFEC(wHighSideFEC), mStartTime(wStartTime), 
		mLastDetectedTime(Time(0,0)), mFirstDetectedTime(Time(0,0)),
		mRACH(isRACH), mLastTOA(0.0) {};

	TrCHFEC *highSideFEC() {return mHighSideFEC;}

	UMTS::Time startTime() {return mStartTime;}
	
	UMTS::Time firstDetection() {return mFirstDetectedTime;}

	UMTS::Time lastDetection() {return mLastDetectedTime;}

	bool isRACH() { return mRACH;}

	float lastTOA() { return mLastTOA;}

	void firstDetection(UMTS::Time wTime) {mFirstDetectedTime = wTime;}

	void lastDetection(UMTS::Time wTime) {mLastDetectedTime = wTime;}

	void lastTOA(float wTOA) {mLastTOA  = wTOA;}

};

// (pat) This class was unused, so I removed it.  The original code is preserved in UMTSPhCh.h
//class PhyChanDesc;


} // namespace UMTS


#endif
