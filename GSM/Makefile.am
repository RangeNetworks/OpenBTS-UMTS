# OpenBTS provides an open source alternative to legacy telco protocols and 
# traditionally complex, proprietary hardware systems.
#
# Copyright 2008 Free Software Foundation, Inc.
# Copyright 2014 Range Networks, Inc.
#
# This software is distributed under the terms of the GNU Affero General 
# Public License version 3. See the COPYING and NOTICE files in the main 
# directory for licensing information.
#
# This use of this software may be subject to additional restrictions.
# See the LEGAL file in the main directory for details.

include $(top_srcdir)/Makefile.common

noinst_LTLIBRARIES = libGSM.la

libGSM_la_CXXFLAGS = $(AM_CXXFLAGS) -O2
libGSM_la_SOURCES = \
	GSML3CCMessages.cpp \
	GSML3CCElements.cpp \
	GSML3MMMessages.cpp \
	GSML3MMElements.cpp \
	GSML3RRMessages.cpp \
	GSML3RRElements.cpp \
	GSML3CommonElements.cpp \
	GSML3Message.cpp \
	GSMCommon.cpp \
	GSMTransfer.cpp \
	GSMTAPDump.cpp

noinst_HEADERS = \
 	GSM610Tables.h \
	GSMCommon.h \
	GSML3CCElements.h \
	GSML3CCMessages.h \
	GSML3CommonElements.h \
	GSML3Message.h \
	GSML3MMElements.h \
	GSML3MMMessages.h \
	GSML3RRElements.h \
	GSML3RRMessages.h \
	GSMTransfer.h \
	PowerManager.h \
	GSMTAPDump.h \
	GSMSMSCBL3Messages.h \
	gsmtap.h \
	PhysicalStatus.h

