/**@file Global system parameters. */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Processing, Inc.
 * Copyright 2011-2021 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include "config.h"
#include <Globals.h>
#include <CLI.h>
#include <TMSITable.h>
#include <URLEncode.h>

#define PROD_CAT "P"

#define FEATURES ""

const char *gVersionString = "release " VERSION FEATURES " " PROD_CAT " built " TIMESTAMP_ISO " " REPO_REV " ";

const char *gOpenWelcome =
	"OpenBTS, OpenBTS-UMTS\n"
	"Copyright 2008, 2009, 2010 Free Software Foundation, Inc.\n"
	"Copyright 2010 Kestrel Signal Processing, Inc.\n"
	"Copyright 2011-2021 Range Networks, Inc.\n"
	"Release " VERSION " " PROD_CAT " formal build date " TIMESTAMP_ISO " " REPO_REV "\n"
	"\"OpenBTS\" is a trademark of Range Networks, Inc.\n"
	"\"OpenBTS-UMTS\" is a trademark of Range Networks, Inc.\n"
	// (pat) Normally this should also say: "All Rights Reserved",
	// but that is a little complicated in this because they are not.
	"\nContributors:\n"
	"  Range Networks, Inc.:\n"
	"    David Burgess, Harvind Samra, Donald Kirker, Doug Brown, Pat Thompson, Michael Iedema\n"
	"  Kestrel Signal Processing, Inc.:\n"
	"    David Burgess, Harvind Samra, Raffi Sevlian, Roshan Baliga\n"
	"  GNU Radio:\n"
	"    Johnathan Corgan\n"
	"  Others:\n"
	"    Anne Kwong, Jacob Appelbaum, Joshua Lackey, Alon Levy\n"
	"    Alexander Chemeris, Alberto Escudero-Pascual\n"
	"Incorporated GPL libraries and components:\n"
	"  libosip2 (LGPL), liportp2 (LGPL)"
#ifdef HAVE_LIBREADLINE
	", readline"
#endif
	"\n"
	"\nThis program comes with ABSOLUTELY NO WARRANTY.\n"
	"\nUse of this software may be subject to other legal restrictions,\n"
	"including patent licsensing and radio spectrum licensing.\n"
	"All users of this software are expected to comply with applicable\n"
	"regulations and laws.  See the LEGAL file in the source code for\n"
	"more information."
;


CommandLine::Parser gParser;

