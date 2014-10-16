/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2009 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef OPENBTSCLI_H
#define OPENBTSCLI_H

#include <string>
#include <map>
#include <iostream>


namespace CommandLine {

enum CLIStatus { SUCCESS=0,
	BAD_NUM_ARGS=1,
	BAD_VALUE=2,
	NOT_FOUND=3,
	TOO_MANY_ARGS=4,
	FAILURE=5,
	CLI_EXIT=-1
	};


/** A table for matching strings to actions. */
typedef CLIStatus (*CLICommand)(int,char**,std::ostream&);
typedef std::map<std::string,CLICommand> ParseTable;

/** The help table. */
typedef std::map<std::string,std::string> HelpTable;

class Parser {

	private:

	ParseTable mParseTable;
	HelpTable mHelpTable;
	static const int mMaxArgs = 10;

	public:

	void addCommands();

	/**
		Process a command line.
		@return 0 on sucess, -1 on exit request, error codes otherwise
	*/
	CLIStatus process(const char* line, std::ostream& os) const;
	void startCommandLine();	// (pat) Start a simple command line processor.

	/** Add a command to the parsing table. */
	void addCommand(const char* name, CLICommand func, const char* helpString)
		{ mParseTable[name] = func; mHelpTable[name]=helpString; }

	ParseTable::const_iterator begin() const { return mParseTable.begin(); }
	ParseTable::const_iterator end() const { return mParseTable.end(); }

	/** Return a help string. */
	const char *help(const std::string& cmd) const;

	private:

	/**
		Parse and execute a command string.
		@line a writeable copy of the original line
		@cline the original line
		@os output stream
		@return status code
	*/
	CLIStatus execute(char* line, std::ostream& os) const;

};

extern CLIStatus printChansV4(std::ostream& os,bool showAll);

} 	// namespace CommandLine


#endif
// vim: ts=4 sw=4
