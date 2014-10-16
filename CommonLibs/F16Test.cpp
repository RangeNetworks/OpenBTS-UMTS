/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2009 Free Software Foundation, Inc.
 * Copyright 2011-2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#include "F16.h"


#include <iostream>

using namespace std;

int main(int argc, char **argv)
{

	F16 a = 2.5;
	F16 b = 1.5;
	F16 c = 2.5 * 1.5;
	F16 d = c + a;
	F16 e = 10;
	cout << a << ' ' << b << ' ' << c << ' ' << d << ' ' << e << endl;

	a *= 3;
	b *= 0.3;
	c *= e;
	cout << a << ' ' << b << ' ' << c << ' ' << d << endl;

	a /= 3;
	b /= 0.3;
	c = d * 0.05;
	cout << a << ' ' << b << ' ' << c << ' ' << d << endl;

	F16 f = a/d;
	cout << f << ' ' << f+0.5 << endl;
}
