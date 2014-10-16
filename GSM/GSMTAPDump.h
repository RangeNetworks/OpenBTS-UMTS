/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2009 Free Software Foundation, Inc.
 * Copyright 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#ifndef GSMTAPDUMP_H
#define GSMTAPDUMP_H

#include "gsmtap.h"
#include "GSMTransfer.h"


void gWriteGSMTAP(unsigned ARFCN, unsigned TS, unsigned FN, const GSM::L2Frame& frame);


#endif

// vim: ts=4 sw=4
