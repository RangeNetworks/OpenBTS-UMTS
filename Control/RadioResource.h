/**@file UMTS RRC Procedures, 3GPP 25.331 */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
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

#ifndef RADIORESOURCE_H
#define RADIORESOURCE_H

#include <list>

#include <GSML3CommonElements.h>
#include <UMTSCommon.h>



namespace GSM {
class L3MobileIdentity;
class L3PagingResponse;
}

namespace Control {
class TransactionEntry;
}

namespace UMTS {
class DCCHLogicalChannel;
}





namespace RRC {



/** Find and complete the in-process transaction associated with a paging repsonse. */
//void PagingResponseHandler(const GSM::L3PagingResponse*, UMTS::DCCHLogicalChannel*); // FIXME: stubbed out


/**@ Access Grant mechanisms */
//@{


/** Decode RACH bits and send an immediate assignment; may block waiting for a channel. */
//void AccessGrantResponder(
//	unsigned requestReference, const GSM::Time& when,
//	float RSSI, float timingError);


/** This record carries all of the parameters associated with a RACH burst. */
class ChannelRequestRecord {

	private:

	unsigned mRA;		///< request reference
	UMTS::Time mFrame;	///< receive timestamp
	float mRSSI;		///< dB wrt full scale
	float mTimingError;	///< correlator timing error in symbol periods

	public:

	ChannelRequestRecord(
		unsigned wRA, const UMTS::Time& wFrame,
		float wRSSI, float wTimingError)
		:mRA(wRA), mFrame(wFrame),
		mRSSI(wRSSI), mTimingError(wTimingError)
	{ }

	unsigned RA() const { return mRA; }
	const UMTS::Time& frame() const { return mFrame; }
	float RSSI() const { return mRSSI; }
	float timingError() const { return mTimingError; }

};


/** A thread to process contents of the channel request queue. */
void* AccessGrantServiceLoop(void*);


//@}




/**@ Paging mechanisms */
//@{

/** An entry in the paging list. */
class PagingEntry {

	private:

	GSM::L3MobileIdentity mID;		///< The mobile ID.
	UMTS::ChannelTypeL3 mType;		///< The needed channel type.
	unsigned mTransactionID;		///< The associated transaction ID.
	Timeval mExpiration;			///< The expiration time for this entry.

	public:

	/**
		Create a new entry, with current timestamp.
		@param wID The ID to be paged.
		@param wLife The number of milliseconds to keep paging.
	*/
	PagingEntry(const GSM::L3MobileIdentity& wID, UMTS::ChannelTypeL3 wType,
			unsigned wTransactionID, unsigned wLife)
		:mID(wID),mType(wType),mTransactionID(wTransactionID),mExpiration(wLife)
	{}

	/** Access the ID. */
	const GSM::L3MobileIdentity& ID() const { return mID; }

	/** Access the channel type needed. */
	UMTS::ChannelTypeL3 type() const { return mType; }

	unsigned transactionID() const { return mTransactionID; }

	/** Renew the timer. */
	void renew(unsigned wLife) { mExpiration = Timeval(wLife); }

	/** Returns true if the entry is expired. */
	bool expired() const { return mExpiration.passed(); }

};

typedef std::list<PagingEntry> PagingEntryList;


/**
	The pager is a global object that generates paging messages on the CCCH.
	To page a mobile, add the mobile ID to the pager.
	The entry will be deleted automatically when it expires.
	All pager operations are linear time.
	Not much point in optimizing since the main operation is inherently linear.
*/
class Pager {

	private:

	PagingEntryList mPageIDs;				///< List of ID's to be paged.
	mutable Mutex mLock;					///< Lock for thread-safe access.
	Signal mPageSignal;						///< signal to wake the paging loop
	Thread mPagingThread;					///< Thread for the paging loop.
	volatile bool mRunning;

	public:

	Pager()
		:mRunning(false)
	{}

	/** Set the output FIFO and start the paging loop. */
	void start();

	/**
		Add a mobile ID to the paging list.
		@param addID The mobile ID to be paged.
		@param chanType The channel type to be requested.
		@param transaction The transaction record, which will be modified.
		@param wLife The paging duration in ms, default is SIP Timer B.
	*/
	void addID(
		const GSM::L3MobileIdentity& addID,
		UMTS::ChannelTypeL3 chanType,
		Control::TransactionEntry& transaction,
		unsigned wLife=gConfig.getNum("SIP.Timer.B")
	);

	/**
		Remove a mobile ID.
		This is used to stop the paging when a phone responds.
		@return The transaction ID associated with this entry.
	*/
	unsigned removeID(const GSM::L3MobileIdentity&);

	private:

	/**
		Traverse the paging list, paging all IDs.
		@return Number of IDs paged.
	*/
	unsigned pageAll();

	/** A loop that repeatedly calls pageAll. */
	void serviceLoop();

	/** C-style adapter. */
	friend void *PagerServiceLoopAdapter(Pager*);

public:

	/** return size of PagingEntryList */
	size_t pagingEntryListSize();

	/** Dump the paging list to an ostream. */
	void dump(std::ostream&) const;
};


void *PagerServiceLoopAdapter(RRC::Pager*);


//@}	// paging mech



/** A loop to geneate the SIB list. */
void BCCHGenerator();


} // namespace RRC


#endif
