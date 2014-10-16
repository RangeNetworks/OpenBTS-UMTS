/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2011-2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef _MEMORYLEAK_
#define _MEMORYLEAK_ 1
#include <map>
#include "ScalarTypes.h"
#include "Logger.h"

// pat 12-27-2012: Turning this off for UMTS because we are not currently looking for memory leaks,
// and there appear to be some memory corruptions somewhere, which I want to crash instead
// of just modifying this header.
#define RN_DISABLE_MEMORY_LEAK_TEST 0 

namespace Utils {

struct MemStats {
	// Enumerates the classes that are checked.
	// Redundancies are ok, for example, we check BitVector and also
	// several descendants of BitVector.
	enum MemoryNames {
		mZeroIsUnused,
		mVector,
		mVectorData,
		mBitVector,
		mByteVector,
		mByteVectorData,
		mRLCRawBlock,
		mRLCUplinkDataBlock,
		mRLCMessage,
		mRLCMsgPacketDownlinkDummyControlBlock,	// Redundant with RLCMessage
		mTBF,
		mLlcEngine,
		mSgsnDownlinkMsg,
		mRachInfo,
		mPdpPdu,
		mFECDispatchInfo,
		mRACHProcessorInfo,
		mDCHProcessorInfo,
		mL3Frame,
		msignalVector,
		mSoftVector,
		mScramblingCode,
		mURlcDownSdu,
		mURlcPdu,
		// Must be last:
		mMax,
	};
	int mMemTotal[mMax];	// In elements, not bytes.
	int mMemNow[mMax];
	const char *mMemName[mMax];
	MemStats();
	void memChkNew(MemoryNames memIndex, const char *id);
	void memChkDel(MemoryNames memIndex, const char *id);
	void text(std::ostream &os);
	// We would prefer to use an unordered_map, but that requires special compile switches.
	// What a super great language.
	typedef std::map<std::string,Int_z> MemMapType;
	MemMapType mMemMap;
};
extern struct MemStats gMemStats;

// This is a memory leak detector.
// Use by putting RN_MEMCHKNEW and RN_MEMCHKDEL in class constructors/destructors,
// or use the DEFINE_MEMORY_LEAK_DETECTOR class and add the defined class
// as an ancestor to the class to be memory leak checked.

struct MemLabel {
	std::string mccKey;
	virtual ~MemLabel() {
		// During startup there are constructor races where static instances of other classes (BitVector) 
		// are inited before MemoryLeak.  Such instances have an mccKey of "".  I have only seen that happen
		// during an exit() call, which doesnt matter much, but lets be neat and prevent a crash.
		if (mccKey == "") return;
		Int_z &tmp = Utils::gMemStats.mMemMap[mccKey]; tmp = tmp - 1;
	}
};

#if RN_DISABLE_MEMORY_LEAK_TEST
#define RN_MEMCHKNEW(type)
#define RN_MEMCHKDEL(type)
#define RN_MEMLOG(type,ptr)
#define DEFINE_MEMORY_LEAK_DETECTOR_CLASS(subClass,checkerClass) \
	struct checkerClass {};
#else

#define RN_MEMCHKNEW(type) { Utils::gMemStats.memChkNew(Utils::MemStats::m##type,#type); }
#define RN_MEMCHKDEL(type) { Utils::gMemStats.memChkDel(Utils::MemStats::m##type,#type); }

#define RN_MEMLOG(type,ptr) { \
	static std::string key = format("%s_%s:%d",#type,__FILE__,__LINE__); \
	(ptr)->/* MemCheck##type:: */ mccKey = key; \
	Utils::gMemStats.mMemMap[key]++; \
	}

// TODO: The above assumes that checkclass is MemCheck ## subClass
#define DEFINE_MEMORY_LEAK_DETECTOR_CLASS(subClass,checkerClass) \
	struct checkerClass : public virtual Utils::MemLabel { \
	    checkerClass() { RN_MEMCHKNEW(subClass); } \
		virtual ~checkerClass() { \
			RN_MEMCHKDEL(subClass); \
		} \
	};

#endif

}	// namespace Utils

#endif
