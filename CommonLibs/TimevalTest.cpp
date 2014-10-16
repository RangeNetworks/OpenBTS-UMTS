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
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{

	Timeval then(10000);
	cout << then.elapsed() << endl;

	while (!then.passed()) {
		cout << "now: " << Timeval() << " then: " << then << " remaining: " << then.remaining() << endl;
		usleep(500000);
	}
	cout << "now: " << Timeval() << " then: " << then << " remaining: " << then.remaining() << endl;
}
