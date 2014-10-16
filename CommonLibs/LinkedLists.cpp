/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008 Free Software Foundation, Inc.
 * Copyright 2011-2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#include "LinkedLists.h"

void PointerFIFO::push_front(void* val)	// by pat
{
	// Pat added this routine for completeness, but never used or tested.
	// The first person to use this routine should remove this assert.
	ListNode *node = allocate();
	node->data(val);
	node->next(mHead);
	mHead = node;
	if (!mTail) mTail=node;
	mSize++;
}

void PointerFIFO::put(void* val)
{
	ListNode *node = allocate();
	node->data(val);
	node->next(NULL);
	if (mTail!=NULL) mTail->next(node);
	mTail=node;
	if (mHead==NULL) mHead=node;
	mSize++;
}

/** Take an item from the FIFO. */
void* PointerFIFO::get()
{
	// empty list?
	if (mHead==NULL) return NULL;
	// normal case
	ListNode* next = mHead->next();
	void* retVal = mHead->data();
	release(mHead);
	mHead = next;
	if (next==NULL) mTail=NULL;
	mSize--;
	return retVal;
}



ListNode *PointerFIFO::allocate()
{
	if (mFreeList==NULL) return new ListNode;
	ListNode* retVal = mFreeList;
	mFreeList = mFreeList->next();
	return retVal;
}

void PointerFIFO::release(ListNode* wNode)
{
	wNode->next(mFreeList);
	mFreeList = wNode;
}

