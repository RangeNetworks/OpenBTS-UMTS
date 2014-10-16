/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2009 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2011-2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#include <iostream>
#include <iterator>

#include "Logger.h"
#include "Configuration.h"

ConfigurationTable gConfig;
//ConfigurationTable gConfig("example.config");

void printAlarms()
{
    std::ostream_iterator<std::string> output( std::cout, "\n" );
    std::list<std::string> alarms = gGetLoggerAlarms();
    std::cout << "# alarms = " << alarms.size() << std::endl;
    std::copy( alarms.begin(), alarms.end(), output );
}

int main(int argc, char *argv[])
{
	gLogInit("LogTest","NOTICE",LOG_LOCAL7);

	LOG(EMERG) << " testing the logger.";
	LOG(ALERT) << " testing the logger.";
	LOG(CRIT) << " testing the logger.";
	LOG(ERR) << " testing the logger.";
	LOG(WARNING) << " testing the logger.";
	LOG(NOTICE) << " testing the logger.";
	LOG(INFO) << " testing the logger.";
	LOG(DEBUG) << " testing the logger.";
    std::cout << "\n\n\n";
    std::cout << "testing Alarms\n";
    std::cout << "you should see three lines:" << std::endl;
    printAlarms();
    std::cout << "----------- generating 20 alarms ----------" << std::endl;
    for (int i = 0 ; i < 20 ; ++i) {
        LOG(ALERT) << i;
    }
    std::cout << "you should see ten lines with the numbers 10..19:" << std::endl;
    printAlarms();
}



