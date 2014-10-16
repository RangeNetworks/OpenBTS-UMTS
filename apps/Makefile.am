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

noinst_PROGRAMS = \
	OpenBTS-UMTS \
	OpenBTS-UMTSDo \
	OpenBTS-UMTSCLI

OpenBTS_UMTS_SOURCES = OpenBTS-UMTS.cpp GetConfigurationKeys.cpp
OpenBTS_UMTS_LDADD = \
	$(GLOBALS_LA) \
	$(CLI_LA) \
	$(TRX_LA) \
	$(SIP_LA) \
	$(UMTS_LA) \
	$(CONTROL_LA) \
	$(SGSNGGSN_LA) \
	$(ASN_LA) \
	$(GSM_LA) \
	$(SMS_LA) \
	$(NODEMANAGER_LA) \
	$(OSIP_LIBS) \
	$(ORTP_LIBS) \
	$(COMMON_LA)
OpenBTS_UMTS_CPPFLAGS = $(STD_DEFINES_AND_INCLUDES)

OpenBTS_UMTSCLI_SOURCES = OpenBTS-UMTSCLI.cpp
OpenBTS_UMTSCLI_LDADD = -lreadline
OpenBTS_UMTSDo_SOURCES = OpenBTS-UMTSDo.cpp

EXTRA_DIST = \
	OpenBTS-UMTS.example.sql \
	openbts-umts.conf

install: OpenBTS-UMTS OpenBTS-UMTSCLI OpenBTS-UMTSDo
	mkdir -p "$(DESTDIR)/OpenBTS/"
	install OpenBTS-UMTS "$(DESTDIR)/OpenBTS/"
	install OpenBTS-UMTSCLI "$(DESTDIR)/OpenBTS/"
	install OpenBTS-UMTSDo "$(DESTDIR)/OpenBTS/"
	mkdir -p "$(DESTDIR)/etc/init/"
	install openbts-umts.conf "$(DESTDIR)/etc/init/"
	mkdir -p "$(DESTDIR)/etc/OpenBTS/"
#	install iptables.rules "$(DESTDIR)/etc/OpenBTS/"
	install OpenBTS-UMTS.example.sql "$(DESTDIR)/etc/OpenBTS/"
