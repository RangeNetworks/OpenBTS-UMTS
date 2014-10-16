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

#include <URLEncode.h>
#include <string>
#include <string.h>
#include <ctype.h>

using namespace std;

//based on javascript encodeURIComponent()
string URLEncode(const string &c)
{
	static const char *digits = "01234567890ABCDEF";
	string retVal="";
	for (unsigned i=0; i<c.length(); i++)
	{
		const char ch = c[i];
		if (isalnum(ch) || strchr("-_.!~'()",ch)) {
			retVal += ch;
		} else {
			retVal += '%';
			retVal += digits[(ch>>4) & 0x0f];
			retVal += digits[ch & 0x0f];
		}
	}
	return retVal;
}

