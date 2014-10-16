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

#include <stdio.h>
#include <stdlib.h>
#include <list>

#include "RadioResource.h"

#include <UMTSCommon.h>
#include <UMTSLogicalChannel.h>
#include <UMTSConfig.h>

#include "TransactionTable.h"

#include <Logger.h>
#undef WARNING


using namespace std;
using namespace UMTS;
using namespace RRC;
using namespace Control;




void Pager::addID(const GSM::L3MobileIdentity& newID, UMTS::ChannelTypeL3 chanType,
		TransactionEntry& transaction, unsigned wLife)
{
	transaction.GSMState(GSM::Paging);
	transaction.setTimer("3113",wLife);
	// Add a mobile ID to the paging list for a given lifetime.
	ScopedLock lock(mLock);
	// If this ID is already in the list, just reset its timer.
	// Uhg, another linear time search.
	// This would be faster if the paging list were ordered by ID.
	// But the list should usually be short, so it may not be worth the effort.
	for (PagingEntryList::iterator lp = mPageIDs.begin(); lp != mPageIDs.end(); ++lp) {
		if (lp->ID()==newID) {
			LOG(DEBUG) << newID << " already in table";
			lp->renew(wLife);
			mPageSignal.signal();
			return;
		}
	}
	// If this ID is new, put it in the list.
	mPageIDs.push_back(PagingEntry(newID,chanType,transaction.ID(),wLife));
	LOG(INFO) << newID << " added to table";
	mPageSignal.signal();
}


unsigned Pager::removeID(const GSM::L3MobileIdentity& delID)
{
	// Return the associated transaction ID, or 0 if none found.
	LOG(INFO) << delID;
	ScopedLock lock(mLock);
	for (PagingEntryList::iterator lp = mPageIDs.begin(); lp != mPageIDs.end(); ++lp) {
		if (lp->ID()==delID) {
			unsigned retVal = lp->transactionID();
			mPageIDs.erase(lp);
			return retVal;
		}
	}
	return 0;
}



unsigned Pager::pageAll()
{
	// Traverse the full list and page all IDs.
	// Remove expired IDs.
	// Return the number of IDs paged.
	// This is a linear time operation.

	ScopedLock lock(mLock);

	// Clear expired entries.
	PagingEntryList::iterator lp = mPageIDs.begin();
	while (lp != mPageIDs.end()) {
		if (!lp->expired()) ++lp;
		else {
			LOG(INFO) << "erasing " << lp->ID();
			// Non-responsive, dead transaction?
			gTransactionTable.removePaging(lp->transactionID());
			// remove from the list
			lp=mPageIDs.erase(lp);
		}
	}

	LOG(INFO) << "paging " << mPageIDs.size() << " mobile(s)";

	// Page remaining entries, two at a time if possible.
	// These PCH send operations are non-blocking.
	lp = mPageIDs.begin();
	while (lp != mPageIDs.end()) {
		// TODO UMTS -- This needs a rewrite for UMTS.
		assert(0);
	}

	return mPageIDs.size();
}

size_t Pager::pagingEntryListSize()
{
	ScopedLock lock(mLock);
	return mPageIDs.size();
}

void Pager::start()
{
	if (mRunning) return;
	mRunning=true;
	mPagingThread.start((void* (*)(void*))PagerServiceLoopAdapter, (void*)this);
}



void* RRC::PagerServiceLoopAdapter(Pager *pager)
{
	pager->serviceLoop();
	return NULL;
}

void Pager::serviceLoop()
{
	while (mRunning) {

		LOG(DEBUG) << "Pager blocking for signal";
		mLock.lock();
		while (mPageIDs.size()==0) mPageSignal.wait(mLock);
		mLock.unlock();

		// page everything
		pageAll();

		// Wait for pending activity to clear the channel.
		// TODO UMTS -- Needs to be written for UMTS.
		// This wait is what causes PCH to have lower priority than AGCH.
		//unsigned load = gNodeB.getPCH()->load();
		//LOG(DEBUG) << "Pager waiting for " << load << " multiframes";
		//if (load) usleep(UMTS::gFrameMicroseconds*load);
	}
}



void Pager::dump(ostream& os) const
{
	ScopedLock lock(mLock);
	PagingEntryList::const_iterator lp = mPageIDs.begin();
	while (lp != mPageIDs.end()) {
		os << lp->ID() << " " << lp->type() << " " << lp->expired() << endl;
		++lp;
	}
}




// vim: ts=4 sw=4
