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

#include "UMTSCommon.h"

using namespace UMTS;
using namespace std;

unsigned UMTS::channelFreqKHz(UMTSBand band, unsigned ARFCN)
{
	switch (band) {
		case UMTS850:
			assert((ARFCN<=4458) && (ARFCN>=4357));
			return ARFCN*1000/5;
		case UMTS900:
                        assert((ARFCN<=3088) && (ARFCN>=2937));
                        return (ARFCN+340*5)*1000/5;
		case UMTS1700:
			assert((ARFCN<=1738) && (ARFCN>=1537));
                        return (ARFCN+1805*5)*1000/5;
		case UMTS1800:
                        assert((ARFCN<=1513) && (ARFCN>=1162));
                        return (ARFCN+1575*5)*1000/5;
                case UMTS1900:
                        assert((ARFCN<=9938) && (ARFCN>=9662));
                        return (ARFCN)*1000/5;
                case UMTS2100:
                        assert((ARFCN<=10838) && (ARFCN>=10562));
                        return (ARFCN)*1000/5;
		default:
			assert(0);
	}
	//return ARFCN * 200;
}


unsigned UMTS::uplinkOffsetKHz(UMTSBand band)
{
	switch (band) {
		case UMTS850:  return 45000;
		case UMTS900:  return 45000;
		case UMTS1700: return 400000;
		case UMTS1800: return 95000;
		case UMTS1900: return 80000;
		case UMTS2100: return 190000;
		default: assert(0);
	}
}


ostream& UMTS::operator<<(ostream& os, const Time& t)
{
        os << t.TN() << ":" << t.FN();
        return os;
}

// (pat) This was called with a frame number argument, which did an auto-conversion to Time.
// I changed the name and modified the arguments to match the call to clarify.
void UMTS::Clock::setFN(unsigned wFN)
{
	int oldFN = FN();	// Debugging.
	// (pat) Timeval is a surprisingly expensive operation, so I moved outside the lock period.
	Timeval now(0);
	mLock.lock();
	mBaseTime = now;
	mBaseFN = wFN;
	mLock.unlock();

	{ // Debugging:
		int diff = FNDelta(oldFN,wFN);
		if (diff > 1 || diff < -1) {
			LOG(NOTICE) << "clock set FN:"<<LOGVAR(oldFN) <<LOGVAR2("newFN",wFN) <<LOGVAR(diff)
				<<LOGVAR2("BaseTime.sec",mBaseTime.sec())<<LOGVAR2(".usec",mBaseTime.usec()) << LOGVAR2("t",format("%.2f",timef()));
		}
	}
}


// If fractionUSecs is non-null, return the fraction into the next cycle in usecs.
int32_t UMTS::Clock::FN(uint32_t *fractionUSecs) const
{
	Timeval now; // (pat) Timeval is a surprisingly expensive operation, so I moved outside the lock period.
	mLock.lock();	// (pat) lock synchronizes changes to mBaseTime and mBaseFN by setFN().
	int32_t deltaSec = now.sec() - mBaseTime.sec();
	int32_t deltaUSec = (signed)now.usec() - (signed)mBaseTime.usec();	// (pat) added the (signed) cast.
	int64_t elapsedUSec = 1000000LL*deltaSec + deltaUSec;
	int64_t elapsedFrames = elapsedUSec / UMTS::gFrameMicroseconds;
	int32_t currentFN = (mBaseFN + elapsedFrames) % UMTS::gHyperframe;
	if (fractionUSecs) { *fractionUSecs = (uint32_t) elapsedUSec / UMTS::gFrameMicroseconds; }
	mLock.unlock();

	{ // Debugging: Time must be monotonically increasing.
		static int prevFN = -1;
		static Timeval prev;
		static uint32_t prevdeltaSec, prevdeltaUSec;
		static uint64_t prevelapsedUSec, prevelapsedFrames;
		bool backwards = (prevFN >= 0 && FNDelta(currentFN,prevFN) < 0);
		if (backwards) {
			LOG(NOTICE)<<(backwards ? "TIME RAN BACKWARDS:": "TIME:")
				<<LOGVAR(prevFN)<<LOGVAR2("prev.sec",prev.sec())<<LOGVAR2("usec",prev.usec())
				<<LOGVAR(currentFN)<<LOGVAR2("now.sec",now.sec())<<LOGVAR2("usec",now.usec())
				<<LOGVAR(deltaSec)<<LOGVAR(deltaUSec)<<LOGVAR(elapsedUSec)<<LOGVAR(elapsedFrames)
				<< endl;
			LOG(NOTICE) <<LOGVAR2("basetime.sec",mBaseTime.sec())<<LOGVAR2("usec",mBaseTime.usec()) << endl;
			LOG(NOTICE) << LOGVAR(prevdeltaSec) << LOGVAR(prevdeltaUSec) << LOGVAR(prevelapsedUSec) << LOGVAR(prevelapsedFrames);
		}
		prevFN = currentFN;
		prevdeltaSec = deltaSec; prevdeltaUSec = deltaUSec;
		prevelapsedUSec = elapsedUSec; prevelapsedFrames = elapsedFrames;
		prev = now;
	}
	return currentFN;
}

int16_t UMTS::FNDelta(int16_t v1, int16_t v2)
{
        static const int16_t halfModulus = gHyperframe/2;
        int16_t delta = v1-v2;
        if (delta>=halfModulus) delta -= gHyperframe;
        else if (delta<-halfModulus) delta += gHyperframe;
        return (int16_t) delta;
}

int UMTS::FNCompare(int16_t v1, int16_t v2)
{
        int16_t delta = FNDelta(v1,v2);
        if (delta>0) return 1;
        if (delta<0) return -1;
        return 0;
}



// (pat 12-13-2012) Formerly this waited an integral number of whole frames, so if it was in the middle
// of a frame, it waited until the middle of the target frame.
// Now it waits until the start of the specified 'when' frame.
void UMTS::Clock::wait(const Time& when) const
{
	uint32_t fraction;
	int16_t now = FN(&fraction);
	int16_t target = when.FN();
	// (pat) I think this was a bug because it should use modulo arith, and fixed:
	//if (now>=target) return;
	int16_t delta = FNDelta(target,now);
	if (delta <= 0) { return; }	// (pat) added
	// The usleep amount is limited to 1e6.
	// 9-8-2012 pat: usleep value is limited to 1000000, so switch to nanosleep.
	// Also, nanosleep solves the problem that these calls may return early on interrupt,
	// which is difficult to correct with usleep.
	// The max sleep time is 2048 * 1e6 usecs, which just fits in 31 bits.
	// 12-30-2012, pat: I tried leaving the fraction of a cycle out to debug why the
	// stupid Sierra Wireless Modem is so flaky, but did not help.
	uint32_t totalUsecs = delta * UMTS::gFrameMicroseconds - fraction;
	struct timespec howlong, rem;
	howlong.tv_sec = totalUsecs/1000000;
	howlong.tv_nsec = (totalUsecs - 1000000 * howlong.tv_sec) * 1000;
	while (0 != nanosleep(&howlong, &rem)) { howlong = rem; }

	//static const int32_t maxSleep = 100;
	//if (delta>maxSleep) delta=maxSleep;
	//usleep(delta*UMTS::gFrameMicroseconds);
}

