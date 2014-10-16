# OpenBTS provides an open source alternative to legacy telco protocols and 
# traditionally complex, proprietary hardware systems.
#
# Copyright 2014 Range Networks, Inc.
#
# This software is distributed under the terms of the GNU Affero General 
# Public License version 3. See the COPYING and NOTICE files in the main 
# directory for licensing information.
#
# This use of this software may be subject to additional restrictions.
# See the LEGAL file in the main directory for details.

include $(top_srcdir)/Makefile.common

noinst_LTLIBRARIES = libUMTS.la

libUMTS_la_CPPFLAGS = $(AM_CPPFLAGS) -I../TransceiverRAD1
libUMTS_la_CXXFLAGS = $(AM_CXXFLAGS) # -O3
libUMTS_la_SOURCES = \
	UMTSL1CC.cpp \
	UMTSL1Const.cpp \
	URRCTrCh.cpp \
	UMTSL1FEC.cpp \
	URRCMessages.cpp \
	URLC.cpp \
	URRC.cpp \
	UMTSPhCh.cpp \
	MACEngine.cpp \
	UMTSTransfer.cpp \
	UMTSConfig.cpp \
	UMTSLogicalChannel.cpp \
	UMTSRadioModemSequences.cpp \
	UMTSRadioModem.cpp \
	UMTSCodes.cpp \
	UMTSCommon.cpp \
	sigProcLib.cpp \
	IntegrityProtect.cpp \
	UMTSCLI.cpp \
	AsnHelper.cpp \
	RateMatch.cpp

noinst_HEADERS = \
	UMTSL1Const.h \
	UMTSL1CC.h \
	AsnHelper.h \
	MACEngine.h \
	UMTSCodes.h \
	UMTSCommon.h \
	UMTSConfig.h \
	UMTSLogicalChannel.h \
	UMTSRadioModem.h \
	UMTSRadioModemSequences.h \
	UMTSTransfer.h \
	URLC.h \
	URRC.h \
	URRCRB.h \
	URRCTrCh.h \
	URRCMessages.h \
	UMTSPhCh.h \
	sigProcLib.h \
	signalVector.h \
	RateMatch.h


