/**@file Global system parameters. */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009 Free Software Foundation, Inc.
 * Copyright 2011-2021 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

/*
	This file keeps global system parameters.
*/

#ifndef GLOBALS_H
#define GLOBALS_H

#include "Defines.h"
#include <Configuration.h>
#include <CLI.h>
#include <PhysicalStatus.h>
#include <TMSITable.h>



/** Date-and-time string, defined in OpenBTS.cpp. */
extern const char* gDateTime;

/**
	Just about everything goes into the configuration table.
	This should be defined in the main body of the top-level application.
*/
extern ConfigurationTable gConfig;

/** The OpenBTS welcome message. */
extern const char *gOpenWelcome;

/** The OpenBTS-UMTS version string. */
extern const char *gVersionString;

/** The central parser. */
extern CommandLine::Parser gParser;

/** The global TMSI table. */
extern Control::TMSITable gTMSITable;

/** The physical status reporting table */
extern GSM::PhysicalStatus gPhysStatus;

#endif
