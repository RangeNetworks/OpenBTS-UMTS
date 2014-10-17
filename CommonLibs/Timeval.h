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


#ifndef TIMEVAL_H
#define TIMEVAL_H

#include <unistd.h>
#include <stdint.h>
#include "sys/time.h"
#include <iostream>



/** A wrapper on usleep to sleep for milliseconds. */
inline void msleep(long v) { usleep(v*1000); }


/** A C++ wrapper for struct timeval. */
class Timeval {

	private:

	struct timeval mTimeval;

	public:

	/** Set the value to gettimeofday. */
	void now() { gettimeofday(&mTimeval,NULL); }

	/** Set the value to gettimeofday plus an offset. */
	void future(unsigned ms);

	//@{
	Timeval(unsigned sec, unsigned usec)
	{
		mTimeval.tv_sec = sec;
		mTimeval.tv_usec = usec;
	}

	Timeval(const struct timeval& wTimeval)
		:mTimeval(wTimeval)
	{}

	/**
		Create a Timeval offset into the future.
		@param offset milliseconds
	*/
	Timeval(unsigned offset=0) { future(offset); }
	//@}

	/** Convert to a struct timespec. */
	struct timespec timespec() const;

	/** Return total seconds. */
	double seconds() const;

	time_t sec() const { return mTimeval.tv_sec; }
	uint32_t usec() const { return mTimeval.tv_usec; }

	/** Return differnce from other (other-self), in ms. */
	long delta(const Timeval& other) const;

	/** Elapsed time in ms. */
	long elapsed() const { return delta(Timeval()); }

	/** Remaining time in ms. */
	long remaining() const { return -elapsed(); }

	/** Return true if the time has passed, as per gettimeofday. */
	bool passed() const;

	/** Add a given number of minutes to the time. */
	void addMinutes(unsigned minutes) { mTimeval.tv_sec += minutes*60; }

};

std::ostream& operator<<(std::ostream& os, const Timeval&);

std::ostream& operator<<(std::ostream& os, const struct timespec&);


#endif
// vim: ts=4 sw=4
