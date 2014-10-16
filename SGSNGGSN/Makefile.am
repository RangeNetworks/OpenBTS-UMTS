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

noinst_LTLIBRARIES = libSGSNGGSN.la

libSGSNGGSN_la_CXXFLAGS = $(AM_CXXFLAGS) # -O2
libSGSNGGSN_la_SOURCES = \
	Sgsn.cpp \
	Ggsn.cpp \
	GPRSL3Messages.cpp \
	iputils.cpp \
	miniggsn.cpp \
	LLC.cpp \
	SgsnCli.cpp

noinst_HEADERS = \
	Ggsn.h \
	GPRSL3Messages.h \
	LLC.h \
	miniggsn.h \
	SgsnBase.h \
	Sgsn.h
