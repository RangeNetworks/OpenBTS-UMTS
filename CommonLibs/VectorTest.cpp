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


#include "Vector.h"
#include <iostream>

// We must have a gConfig now to include Vector.
#include "Configuration.h"
ConfigurationTable gConfig;

using namespace std;

typedef Vector<int> TestVector;

int main(int argc, char *argv[])
{
	TestVector test1(5);
	for (int i=0; i<5; i++) test1[i]=i;
	TestVector test2(5);
	for (int i=0; i<5; i++) test2[i]=10+i;

	cout << test1 << endl;
	cout << test2 << endl;

	{
		TestVector testC(test1,test2);
		cout << testC << endl;
		cout << testC.head(3) << endl;
		cout << testC.tail(3) << endl;
		testC.fill(8);
		cout << testC << endl;
		test1.copyToSegment(testC,3);
		cout << testC << endl;

		TestVector testD(testC.segment(4,3));
		cout << testD << endl;
		testD.fill(9);
		cout << testC << endl;
		cout << testD << endl;
	}

	return 0;
}
