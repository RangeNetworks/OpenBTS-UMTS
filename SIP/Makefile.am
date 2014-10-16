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

noinst_LTLIBRARIES = libSIP.la

libSIP_la_CPPFLAGS = $(AM_CPPFLAGS) \
	     -I$(OSIP_INCLUDEDIR) $(OSIP_CPPFLAGS) \
	     -I$(ORTP_INCLUDEDIR) $(ORTP_CPPFLAGS)
libSIP_la_CXXFLAGS = $(AM_CXXFLAGS) -Wextra
libSIP_la_SOURCES = \
	SIPEngine.cpp \
	SIPInterface.cpp \
	SIPMessage.cpp \
	SIPUtility.cpp

noinst_HEADERS = \
	SIPEngine.h \
	SIPInterface.h \
	SIPMessage.h \
	SIPUtility.h
