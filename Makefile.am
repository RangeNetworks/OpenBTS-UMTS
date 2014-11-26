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

ACLOCAL_AMFLAGS = -I config

# Order must be preserved
SUBDIRS = \
	config \
	ASN \
	CommonLibs \
	Globals \
	CLI \
	SIP \
	GSM \
	UMTS \
	SGSNGGSN \
	SMS \
	TransceiverRAD1 \
	TransceiverUHD \
	TRXManager \
	Control \
	NodeManager \
	apps \
	doc

EXTRA_DIST = \
	AUTHORS \
	ChangeLog \
	COPYING \
	INSTALL \
	LEGAL \
	README

install: all
	$(MAKE) -C ./apps install
	$(MAKE) -C ./TransceiverRAD1 install
	$(MAKE) -C ./TransceiverUHD install

FORCE:
