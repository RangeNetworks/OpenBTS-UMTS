# OpenBTS provides an open source alternative to legacy telco protocols and 
# traditionally complex, proprietary hardware systems.
#
# Copyright 2008 Free Software Foundation, Inc.
# Copyright 2010 Kestrel Signal Processing, Inc.
# Copyright 2011, 2014 Range Networks, Inc.
#
# This software is distributed under the terms of the GNU Affero General 
# Public License version 3. See the COPYING and NOTICE files in the main 
# directory for licensing information.
#
# This use of this software may be subject to additional restrictions.
# See the LEGAL file in the main directory for details.

include $(top_srcdir)/Makefile.common

EXTRA_DIST = README.Control

noinst_LTLIBRARIES = libcontrol.la

libcontrol_la_SOURCES = \
	TransactionTable.cpp \
	TMSITable.cpp \
	CallControl.cpp \
	SMSControl.cpp \
	ControlCommon.cpp \
	MobilityManagement.cpp \
	RadioResource.cpp \
	DCCHDispatch.cpp 


noinst_HEADERS = \
	ControlCommon.h \
	SMSControl.h \
	TransactionTable.h \
	TMSITable.h \
	RadioResource.h \
	MobilityManagement.h \
	CallControl.h \
	TMSITable.h
