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



#include "Timeval.h"

using namespace std;

void Timeval::future(unsigned offset)
{
	now();
	unsigned sec = offset/1000;
	unsigned msec = offset%1000;
	mTimeval.tv_usec += msec*1000;
	mTimeval.tv_sec += sec;
	if (mTimeval.tv_usec>1000000) {
		mTimeval.tv_usec -= 1000000;
		mTimeval.tv_sec += 1;
	}
}


struct timespec Timeval::timespec() const
{
	struct timespec retVal;
	retVal.tv_sec = mTimeval.tv_sec;
	retVal.tv_nsec = 1000 * (long)mTimeval.tv_usec;
	return retVal;
}


bool Timeval::passed() const
{
	Timeval nowTime;
	if (nowTime.mTimeval.tv_sec < mTimeval.tv_sec) return false;
	if (nowTime.mTimeval.tv_sec > mTimeval.tv_sec) return true;
	if (nowTime.mTimeval.tv_usec > mTimeval.tv_usec) return true;
	return false;
}

double Timeval::seconds() const
{
	return ((double)mTimeval.tv_sec) + 1e-6*((double)mTimeval.tv_usec);
}



long Timeval::delta(const Timeval& other) const
{
	// 2^31 milliseconds is just over 4 years.
	long deltaS = other.sec() - sec();
	long deltaUs = other.usec() - usec();
	return 1000*deltaS + deltaUs/1000;
}
	



ostream& operator<<(ostream& os, const Timeval& tv)
{
	os.setf( ios::fixed, ios::floatfield );
	os << tv.seconds();
	return os;
}


ostream& operator<<(ostream& os, const struct timespec& ts)
{
	os << ts.tv_sec << "," << ts.tv_nsec;
	return os;
}



// vim: ts=4 sw=4
