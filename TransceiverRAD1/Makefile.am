# OpenBTS provides an open source alternative to legacy telco protocols and 
# traditionally complex, proprietary hardware systems.
#
# Copyright 2008 Free Software Foundation, Inc.
# Copyright 2014 Range Networks, Inc.
#
# This software is distributed under the terms of the GNU General Public 
# License version 3. See the COPYING and NOTICE files in the current 
# directory for licensing information.
#
# This use of this software may be subject to additional restrictions.
# See the LEGAL file in the main directory for details.

if !HAVE_UHD
include $(top_srcdir)/Makefile.common

DESTDIR = 

EXTRA_DIST = \
	README \
	README.Talgorithm

noinst_LTLIBRARIES = libtransceiver.la libumtstransceiver.la

libtransceiver_la_CPPFLAGS = $(AM_CPPFLAGS) $(USB_INCLUDES) $(WITH_INCLUDES)
libtransceiver_la_CXXFLAGS = $(AM_CXXFLAGS) -DOMNITHREAD_POSIX=1 -lusb-1.0  # (oley) -O3 is the default
libtransceiver_la_SOURCES = \
	rnrad1Core.cpp                   \
        fusb.cpp                         \
        rnrad1Rx.cpp                     \
        rnrad1Tx.cpp \
	radioInterface.cpp \
	sigProcLib.cpp \
	Transceiver.cpp \
	RAD1Device.cpp \
	FactoryCalibration.cpp \
	DummyLoad.cpp

# pat removed sigProcLib which is duplicated in UMTS dir.
libumtstransceiver_la_CPPFLAGS = $(AM_CPPFLAGS) $(USB_INCLUDES) $(WITH_INCLUDES)
libumtstransceiver_la_CXXFLAGS = $(AM_CXXFLAGS) -DOMNITHREAD_POSIX=1 -lusb-1.0
libumtstransceiver_la_SOURCES = \
	rnrad1Core.cpp                   \
        fusb.cpp                         \
        rnrad1Rx.cpp                     \
        rnrad1Tx.cpp \
	radioInterface.cpp \
	Transceiver.cpp \
	RAD1Device.cpp \
	FactoryCalibration.cpp \
	DummyLoad.cpp


noinst_PROGRAMS = \
	RAD1ping \
	transceiver \
	RAD1Cmd \
	RAD1SN

# (pat) Note that Harvind moved sigProgLib.h and signalVector.h to the UMTS dir
# so I deleted them here.
noinst_HEADERS = \
        ad9862.h                        \
        fusb.h                          \
        fpga_regs.h                     \
        ids.h                           \
        i2c.h                           \
        interfaces.h                    \
        spi.h				\
        rnrad1Core.h			\
	rnrad1.h			\
	Complex.h \
	radioInterface.h \
	radioDevice.h \
	Transceiver.h \
	RAD1Device.h \
	FactoryCalibration.h \
	DummyLoad.h

RAD1ping_SOURCES = RAD1ping.cpp
RAD1ping_LDADD = \
	libtransceiver.la \
	$(COMMON_LA) $(SQLITE_LA) 

RAD1Cmd_SOURCES = RAD1Cmd.cpp
RAD1Cmd_LDADD = \
        libtransceiver.la \
        $(COMMON_LA) $(SQLITE_LA)

RAD1SN_SOURCES = RAD1SN.cpp
RAD1SN_LDADD = \
        libtransceiver.la \
        $(COMMON_LA) $(SQLITE_LA)


transceiver_SOURCES = runTransceiver.cpp ../apps/GetConfigurationKeys.cpp
transceiver_LDADD = \
	libtransceiver.la \
	$(UMTS_LA) $(GSM_LA)\
	$(COMMON_LA) $(SQLITE_LA) 


MOSTLYCLEANFILES +=

#radioInterface.cpp
#ComplexTest.cpp
#sigProcLibTest.cpp
#sweepGenerator.cpp
#testRadio.cpp

install: transceiver
	@mkdir -p "$(DESTDIR)/OpenBTS/"
	install transceiver "$(DESTDIR)/OpenBTS/"
	install ezusb.ihx "$(DESTDIR)/OpenBTS/"
	install fpga.rbf "$(DESTDIR)/OpenBTS/"
endif
