/**@file UMTS master state and resource management object. */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008-2010 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef UMTSCONFIG_H
#define UMTSCONFIG_H

#include "ByteVector.h"
//#include <vector>
#include <Interthread.h>

#include <GSML3CommonElements.h>

#include <RadioResource.h>
#include <UMTSCommon.h>
#include <UMTSL1FEC.h>

//#include "TRXManager.h"

// Note: These ASN includes are not needed for anything in UMTSConfig,
// or any checked in code, but DAB just added something here recently
// so I assume there is unchecked in code that still needs this.
#include "asn_system.h"	// Dont let other includes land in namespace ASN.
#include <assert.h>		// Must include before ASN.
namespace ASN {
#include "asn_SEQUENCE_OF.h"
};

class ARFCNManager;	// Not in namespace UMTS.

namespace UMTS {

//using namespace ASN;

class BCHFEC;
class CCCHLogicalChannel;
class RACHFEC;
class FACHFEC;
class PCHFEC;
class DCCHLogicalChannel;
class DTCHLogicalChannel;

//class DCCHList : public std::vector<DCCHLogicalChannel*> {};
class DTCHList : public std::vector<DTCHLogicalChannel*> {};

//class PhyChanMap : public std::map<PhyChanDesc,PhyChanParams*> {};

const unsigned cAICHSpreadingCodeIndex = 2;
const unsigned cAICHRACHOffset = 5;

/**
	This object carries the top-level UMTS air interface configuration.
	It serves as a central clearinghouse to get access to everything else in the UMTS code.
*/
class UMTSConfig {
	bool mInited;	// Has this class been initialized?

	/** The paging mechanism is built-in. */
	RRC::Pager mPager;

	mutable Mutex mLock;		///< multithread access control

	BCHFEC *mBCH;
	CCCHLogicalChannel *CCCH;

	public:
	RACHFEC *mRachFec;	// The one and only one for now.
	FACHFEC *mFachFec;	// The one and only one for now.
	PCHFEC *mPchFec;	// The one and only one for now, except we dont have any.
	// See gChannelTree for DCHFEC allocation.

	private:
	/**@name Allocatable channel pools. */
	//@{
	//DCCHList mDCCHPool;
	DTCHList mDTCHPool;	// (pat 9-2012) This is not yet hooked up to anything.
	//@}



	UMTSBand mBand;		///< NodeB operating band

	UMTS::Clock mClock;		///< local copy of NodeB master clock

	time_t mStartTime;

	bool mHold;		///< If true, do not respond to RACH bursts.

	Thread mAccessGrantThread;

	//PhyChanMap mPhyChanMap; ///< Map containing the physical channel parameters

	GSM::L3LocationAreaIdentity mLAI;

	public:

	/** All parameters come from gConfig. */
	UMTSConfig();

	void init(ARFCNManager* downstream=NULL);	// For things to do between class constructor and start().

	/** Start the all the threads. */
	void start(ARFCNManager* C0);

	/** Get the a copy of the current master clock value. */
	const UMTS::Clock& clock() const { return mClock; }

	UMTS::Clock& clock() { return mClock; }

	int32_t time() const { return mClock.FN(); }

	/**@name Accessors. */
	//@{
	RRC::Pager& pager() { return mPager; }
	UMTSBand band() const { return mBand; }
	unsigned getSrncId() { return gConfig.getNum("UMTS.SRNC_ID"); }
	//@}

	/**
		Re-encode the L2Frames for system information messages.
		Called whenever a beacon parameter is changed.
	*/
	void regenerateBeacon();

	/**
		Hold off on channel allocations; don't answer RACH.
		@param val true to hold, false to clear hold
	*/
	void hold(bool val);

	/**
		Return true if we are holding off channel allocation.
	*/
	bool hold() const;


	/**
		Given an SFN, return a pointer to an SIB transport block ready for transmission.
		Must call fillSIBSchedule first for this to work.
	*/
	const TransportBlock* getTxSIB(unsigned SFN);


	/** Populate the MIB into the scheduling table. */
	//void fillMIBSchedule();

	/** Enqueue a RACH channel request; to be deleted when dequeued later. */
	//void channelRequest(RRC::ChannelRequestRecord *req)
	//	{ mChannelRequestQueue.write(req); }

	//RRC::ChannelRequestRecord* nextChannelRequest()
	//	{ return mChannelRequestQueue.read(); }

	//void flushChannelRequests()
	//	{ mChannelRequestQueue.clear(); }


	/**@name Manage DCCH Pool. */
	//@{
	/** Add a new DCCH to the channel allocation pool. */
	//void addDCCH(DCCHLogicalChannel *wDCCH) { ScopedLock lock(mLock); mDCCHPool.push_back(wDCCH); }
	/** Return a pointer to a usable channel. */
	//DCCHLogicalChannel *getDCCH();
	/** Return true if an DCCH is available, but do not allocate it. */
	size_t DCCHAvailable() const { return true; }
	/** Return number of total DCCH. */
	//unsigned DCCHTotal() const { return mDCCHPool.size(); }
	/** Return number of active DCCH. */
	//unsigned DCCHActive() const;
	/** Just a reference to the DCCH pool. */
	//const DCCHList& DCCHPool() const { return mDCCHPool; }
	//@}

	/**@name Manage DTCH pool. */
	//@{
	/** Add a new DTCH to the channel allocation pool. */
	//void addDTCH(DTCHLogicalChannel *wDTCH) { ScopedLock lock(mLock); mDTCHPool.push_back(wDTCH); }
	/** Return a pointer to a usable channel. */
	//DTCHLogicalChannel *getDTCH();
	/** Return true if an DTCH is available, but do not allocate it. */
	size_t DTCHAvailable() const;
	/** Return number of total DTCH. */
	//unsigned DTCHTotal() const { return mDTCHPool.size(); }
	/** Return number of active DTCH. */
	unsigned DTCHActive() const;	// TODO - see this func
	/** Just a reference to the DTCH pool. */
	//const DTCHList& DTCHPool() const { return mDTCHPool; }
	//@}

        /**@name Manage Phy params map. */
		// (pat) It looks like David started this but I cant figure out how it was supposed to work.
		// These functions are unreferenced.
		// I preserved all this code in UMTSPhCh.h in case someone wants to resurrect it,
		// but see gChannelTree class to allocate DCH channels.
#if 0
        //@{
 	/** Add a new physical channel to the map */
        void addPhyChan(PhyChanDesc wDesc, PhyChanParams *wParams) {
                mPhyChanMap[wDesc] = wParams;
        }
	/** Remove channel from map */
        void killPhyChan(PhyChanDesc wDesc) {
                mPhyChanMap.erase(wDesc);
        }
	/** Get a channel that matches given descriptor from map */
        PhyChanParams *getPhyChan(PhyChanDesc wDesc) {
                if (mPhyChanMap.count(wDesc) == 0) return NULL;
                return mPhyChanMap[wDesc];
        }

	/** Return reference to the params map */
        const PhyChanMap& getPhyChanMap() { return mPhyChanMap;}
	//@}
#endif

	/** Return number of seconds since starting. */
	time_t uptime() const { return ::time(NULL)-mStartTime; }

	/** Current uplink interference level in dBm. */
	long getULInterference() const;


	const GSM::L3LocationAreaIdentity& LAI() const { return mLAI; }

};



}	// UMTS


/**@addtogroup Globals */
//@{
/** A single global UMTSConfig object in the global namespace. */
extern UMTS::UMTSConfig gNodeB;
//@}


#endif


// vim: ts=4 sw=4
