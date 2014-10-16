/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2011-2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#include "URLEncode.h"
#include <string>
#include <iostream>


using namespace std;


int main(int argc, char *argv[])
{

	string test = string("Testing: !@#$%^&*() " __DATE__ " " __TIME__);
	cout << test << endl;
	cout << URLEncode(test) << endl;
}

