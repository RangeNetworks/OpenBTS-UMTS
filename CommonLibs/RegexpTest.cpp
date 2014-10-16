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


#include "Regexp.h"
#include <iostream>

using namespace std;


int main(int argc, char *argv[])
{

	Regexp email("^[[:graph:]]+@[[:graph:]]+ ");
	Regexp simple("^dburgess@");

	const char text1[] = "dburgess@jcis.net test message";
	const char text2[] = "no address text message";

	cout << email.match(text1) << " " << text1 << endl;
	cout << email.match(text2) << " " << text2 << endl;

	cout << simple.match(text1) << " " << text1 << endl;
	cout << simple.match(text2) << " " << text2 << endl;
}
