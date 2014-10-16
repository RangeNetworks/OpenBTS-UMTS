/**@file Simplified Vector template with aliases. */

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


#ifndef VECTOR_H
#define VECTOR_H

#include "MemoryLeak.h"
#include <string.h>
#include <iostream>
#include <assert.h>
// We cant use Logger.h in this file...
extern int gVectorDebug;
#define BVDEBUG(msg) if (gVectorDebug) {std::cout << msg;}

DEFINE_MEMORY_LEAK_DETECTOR_CLASS(Vector,MemCheckVector)

/**
	A simplified Vector template with aliases.
	Unlike std::vector, this class does not support dynamic resizing.
	Unlike std::vector, this class does support "aliases" and subvectors.
*/
template <class T> class Vector : public MemCheckVector {

	// TODO -- Replace memcpy calls with for-loops.

	public:

	/**@name Iterator types. */
	//@{
	typedef T* iterator;
	typedef const T* const_iterator;
	//@}

	protected:

	T* mData;		///< allocated data block, if any
	T* mStart;		///< start of useful data
	T* mEnd;		///< end of useful data + 1

	public:

	/** Return the size of the Vector. */
	size_t size() const
	{
		assert(mStart>=mData);
		assert(mEnd>=mStart);
		return mEnd - mStart;
	}

	/** Return size in bytes. */
	size_t bytes() const { return size()*sizeof(T); }

	/** Change the size of the Vector, discarding content. */
	void resize(size_t newSize)
	{
		if (mData!=NULL) {
			delete[] mData;
			RN_MEMCHKDEL(VectorData);
		}
		if (newSize==0) {
			mData=NULL;
		} else {
			mData = new T[newSize];
			RN_MEMCHKNEW(VectorData);
		}
		mStart = mData;
		mEnd = mStart + newSize;
	}

	/** Release memory and clear pointers. */
	void clear() { resize(0); }


	/** Copy data from another vector. */
	void clone(const Vector<T>& other)
	{
		resize(other.size());
		memcpy(mData,other.mStart,other.bytes());
	}




	//@{

	/** Build an empty Vector of a given size. */
	Vector(size_t wSize=0):mData(NULL) { resize(wSize); }

	/** Build a Vector by shifting the data block. */
	Vector(Vector<T>& other)
		:mData(other.mData),mStart(other.mStart),mEnd(other.mEnd)
	{ BVDEBUG("v1\n"); other.mData=NULL; }

	/** Build a Vector by copying another. */
	Vector(const Vector<T>& other):mData(NULL) { BVDEBUG("v2\n"); clone(other); }

	/** Build a Vector with explicit values. */
	Vector(T* wData, T* wStart, T* wEnd)
		:mData(wData),mStart(wStart),mEnd(wEnd)
	{ }

	/** Build a vector from an existing block, NOT to be deleted upon destruction. */
	Vector(T* wStart, size_t span)
		:mData(NULL),mStart(wStart),mEnd(wStart+span)
	{ }

	/** Build a Vector by concatenation. */
	Vector(const Vector<T>& other1, const Vector<T>& other2)
		:mData(NULL)
	{
		resize(other1.size()+other2.size());
		memcpy(mStart, other1.mStart, other1.bytes());
		memcpy(mStart+other1.size(), other2.mStart, other2.bytes());
	}

	//@}

	/** Destroy a Vector, deleting held memory. */
	virtual ~Vector() { clear(); }




	//@{

	/** Assign from another Vector, shifting ownership. */
	void operator=(Vector<T>& other)
	{
		BVDEBUG("v=\n");
		clear();
		mData=other.mData;
		mStart=other.mStart;
		mEnd=other.mEnd;
		other.mData=NULL;
	}

	/** Assign from another Vector, copying. */
	void operator=(const Vector<T>& other) { BVDEBUG("vc=\n"); clone(other); }

	//@}


	//@{

	/** Return an alias to a segment of this Vector. */
	Vector<T> segment(size_t start, size_t span)
	{
		T* wStart = mStart + start;
		T* wEnd = wStart + span;
		assert(wEnd<=mEnd);
		return Vector<T>(NULL,wStart,wEnd);
	}

	/** Return an alias to a segment of this Vector. */
	const Vector<T> segment(size_t start, size_t span) const
	{
		T* wStart = mStart + start;
		T* wEnd = wStart + span;
		assert(wEnd<=mEnd);
		return Vector<T>(NULL,wStart,wEnd);
	}

	Vector<T> head(size_t span) { return segment(0,span); }
	const Vector<T> head(size_t span) const { return segment(0,span); }
	Vector<T> tail(size_t start) { return segment(start,size()-start); }
	const Vector<T> tail(size_t start) const { return segment(start,size()-start); }

	/**
		Copy part of this Vector to a segment of another Vector.
		@param other The other vector.
		@param start The start point in the other vector.
		@param span The number of elements to copy.
	*/
	void copyToSegment(Vector<T>& other, size_t start, size_t span) const
	{
		T* base = other.mStart + start;
		assert(base+span<=other.mEnd);
		assert(mStart+span<=mEnd);
		memcpy(base,mStart,span*sizeof(T));
	}

	/** Copy all of this Vector to a segment of another Vector. */
	void copyToSegment(Vector<T>& other, size_t start=0) const { copyToSegment(other,start,size()); }

	void copyTo(Vector<T>& other) const { copyToSegment(other,0,size()); }

	/**
		Copy a segment of this vector into another.
		@param other The other vector (to copt into starting at 0.)
		@param start The start point in this vector.
		@param span The number of elements to copy.
	*/
	void segmentCopyTo(Vector<T>& other, size_t start, size_t span) const
	{
		const T* base = mStart + start;
		assert(base+span<=mEnd);
		assert(other.mStart+span<=other.mEnd);
		memcpy(other.mStart,base,span*sizeof(T));
	}

	void fill(const T& val)
	{
		T* dp=mStart;
		while (dp<mEnd) *dp++=val;
	}

	void fill(const T& val, unsigned start, unsigned length)
	{
		T* dp=mStart+start;
		T* end=dp+length;
		assert(end<=mEnd);
		while (dp<end) *dp++=val;
	}


	//@}


	//@{

	T& operator[](size_t index)
	{
		assert(mStart+index<mEnd);
		return mStart[index];
	}

	const T& operator[](size_t index) const
	{
		assert(mStart+index<mEnd);
		return mStart[index];
	}

	const T* begin() const { return mStart; }
	T* begin() { return mStart; }
	const T* end() const { return mEnd; }
	T* end() { return mEnd; }
	//@}

	/* FIRST steps two pointers through a mapping, one pointer into the interleaved
	 * data and the other through the uninterleaved data.  The fifth argument, COPY,
	 * determines whether the copy is from interleaved to uninterleaved, or back.
	 * FIRST assumes no padding is necessary.
	 * The reason for the define is to minimize the cost of parameterization and
	 * function calls, as this is meant for L1 code, while also minimizing the
	 * duplication of code.
	 */

	#define FIRST(UNINTERLEAVED,UNINTERLEAVEDP,INTERLEAVED,INTERLEAVEDP,COPY) \
		assert(UNINTERLEAVED.size() == INTERLEAVED.size()); \
		unsigned rows = UNINTERLEAVED.size() / columns; \
		assert(rows * columns == UNINTERLEAVED.size()); \
		const char *colp = permutation; \
		T *INTERLEAVEDP = &INTERLEAVED[0]; \
		for (unsigned i = 0; i < columns; i++) { \
			T *UNINTERLEAVEDP = &UNINTERLEAVED[*colp++]; \
			for (unsigned j = 0; j < rows; j++) { \
				COPY; \
				UNINTERLEAVEDP += columns; \
			} \
		}

	/** interleaving with No Padding */
	void interleavingNP(const unsigned columns, const char *permutation, Vector<T> &out)
	{
		FIRST((*this), inp, out, outp, *outp++ = *inp)
	}

	/** de-interleaving with No Padding */
	void deInterleavingNP(const unsigned columns, const char *permutation, Vector<T> &out)
	{
		FIRST(out, outp, (*this), inp, *outp = *inp++)
	}

	/* SECOND steps two pointers through a mapping, one pointer into the interleaved
	 * data and the other through the uninterleaved data.  The fifth argument, COPY,
	 * determines whether the copy is from interleaved to uninterleaved, or back.
	 * SECOND pads if necessary.
	 * The reason for the define is to minimize the cost of parameterization and
	 * function calls, as this is meant for L1 code, while also minimizing the
	 * duplication of code.
	 */

	#define SECOND(UNINTERLEAVED,UNINTERLEAVEDP,INTERLEAVED,INTERLEAVEDP,COPY) \
		assert(UNINTERLEAVED.size() == INTERLEAVED.size()); \
		int R2 = (UNINTERLEAVED.size() + columns - 1) / columns; \
		int padding = columns * R2 - UNINTERLEAVED.size(); \
		int rows = R2; \
		int firstPaddedColumn = columns - padding; \
		const char *colp = permutation; \
		T *UNINTERLEAVEDP = &UNINTERLEAVED[0]; \
		for (int i = 0; i < columns; i++) { \
			int trows = rows - (*colp >= firstPaddedColumn); \
			T *INTERLEAVEDP = &INTERLEAVED[*colp++]; \
			for (int j = 0; j < trows; j++) { \
				COPY; \
				INTERLEAVEDP += columns; \
			} \
		}

	/** interleaving With Padding */
	void interleavingWP(const int columns, const char *permutation, Vector<T> &out)
	{
		SECOND((*this), inp, out, outp, *outp = *inp++)
	}

	/** de-interleaving With Padding */
	void deInterleavingWP(const int columns, const char *permutation, Vector<T> &out)
	{
		SECOND(out, outp, (*this), inp, *outp++ = *inp)
	}

};




/** Basic print operator for Vector objects. */
template <class T>
std::ostream& operator<<(std::ostream& os, const Vector<T>& v)
{
	for (unsigned i=0; i<v.size(); i++) os << v[i] << " ";
	return os;
}



#endif
// vim: ts=4 sw=4
