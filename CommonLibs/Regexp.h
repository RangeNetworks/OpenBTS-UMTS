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



#ifndef REGEXPW_H
#define REGEXPW_H

#include <regex.h>
#include <iostream>
#include <stdlib.h>



class Regexp {

	private:

	regex_t mRegex;


	public:

	Regexp(const char* regexp, int flags=REG_EXTENDED)
	{
		int result = regcomp(&mRegex, regexp, flags);
		if (result) {
			char msg[256];
			regerror(result,&mRegex,msg,255);
			std::cerr << "Regexp compilation of " << regexp << " failed: " << msg << std::endl;
			abort();
		}
	}

	~Regexp()
		{ regfree(&mRegex); }

	bool match(const char *text, int flags=0) const
		{ return regexec(&mRegex, text, 0, NULL, flags)==0; }

};


#endif
