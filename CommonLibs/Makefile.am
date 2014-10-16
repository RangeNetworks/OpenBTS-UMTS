# OpenBTS provides an open source alternative to legacy telco protocols and 
# traditionally complex, proprietary hardware systems.
#
# Copyright 2008, 2009 Free Software Foundation, Inc.
# Copyright 2011, 2014 Range Networks, Inc.
#
# This software is distributed under the terms of the GNU Affero General 
# Public License version 3. See the COPYING and NOTICE files in the main 
# directory for licensing information.
#
# This use of this software may be subject to additional restrictions.
# See the LEGAL file in the main directory for details.

include $(top_srcdir)/Makefile.common

EXTRA_DIST = \
	example.config \
	README.common

noinst_LTLIBRARIES = libcommon.la

libcommon_la_CXXFLAGS = $(AM_CXXFLAGS) -O3 -lsqlite3
libcommon_la_SOURCES = \
	BitVector.cpp \
	TurboCoder.cpp \
	ByteVector.cpp \
	LinkedLists.cpp \
	Sockets.cpp \
	Threads.cpp \
	Timeval.cpp \
	Logger.cpp \
	URLEncode.cpp \
	Configuration.cpp \
	sqlite3util.cpp \
	Utils.cpp

noinst_PROGRAMS = \
	BitVectorTest \
	InterthreadTest \
	SocketsTest \
	TimevalTest \
	RegexpTest \
	VectorTest \
	ConfigurationTest \
	LogTest \
	URLEncodeTest \
	F16Test

noinst_HEADERS = \
	BitVector.h \
	TurboCoder.h \
	ByteVector.h \
	Interthread.h \
	LinkedLists.h \
	Sockets.h \
	Threads.h \
	Timeval.h \
	Regexp.h \
	Vector.h \
	Configuration.h \
	F16.h \
	URLEncode.h \
	Logger.h \
	Utils.h \
	ScalarTypes.h \
	sqlite3util.h

URLEncodeTest_SOURCES = URLEncodeTest.cpp
URLEncodeTest_LDADD = libcommon.la

BitVectorTest_SOURCES = BitVectorTest.cpp
BitVectorTest_LDADD = libcommon.la

InterthreadTest_SOURCES = InterthreadTest.cpp
InterthreadTest_LDADD = libcommon.la
InterthreadTest_LDFLAGS = -lpthread

SocketsTest_SOURCES = SocketsTest.cpp
SocketsTest_LDADD = libcommon.la
SocketsTest_LDFLAGS = -lpthread

TimevalTest_SOURCES = TimevalTest.cpp
TimevalTest_LDADD = libcommon.la

VectorTest_SOURCES = VectorTest.cpp
VectorTest_LDADD = libcommon.la

RegexpTest_SOURCES = RegexpTest.cpp
RegexpTest_LDADD = libcommon.la

ConfigurationTest_SOURCES = ConfigurationTest.cpp
ConfigurationTest_LDADD = libcommon.la

LogTest_SOURCES = LogTest.cpp
LogTest_LDADD = libcommon.la

F16Test_SOURCES = F16Test.cpp

MOSTLYCLEANFILES += testSource testDestination


