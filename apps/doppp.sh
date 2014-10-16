#!/bin/sh

# OpenBTS provides an open source alternative to legacy telco protocols and 
# traditionally complex, proprietary hardware systems.
#
# Copyright 2014 Range Networks, Inc.

# This software is distributed under the terms of the GNU Affero General 
# Public License version 3. See the COPYING and NOTICE files in the main 
# directory for licensing information.

# This use of this software may be subject to additional restrictions.
# See the LEGAL file in the main directory for details.


# The ppp session remains active until the modem is unplugged or: pkill pppd
# You can turn it on and off with ifconfig ppp0 up or ifconfig ppp0 down, just like ifconfig eth0 up/down.
# pppd runs as a daemon, so this script returns instantly, but pppd is still working.
# Progress messages go to /var/log/syslog.

#/usr/sbin/pppd /dev/modem dump 115200 connect "/usr/sbin/chat -s -V -t 10 -e -f /OpenBTS/ppp/chatscript"
#/usr/sbin/pppd /dev/ttyUSB0 dump 115200 connect "/usr/sbin/chat -s -V -t 10 -e -f /OpenBTS/ppp/chatscript"

# We need to gprs deregister (+CGATT) first or the GSM registartion (+COPS)
# will fail (report ERROR) and I dont know how to distinguish
# that failure from actual registration failure.
# Update: Maybe the above is not necessary, and the COPS registration
# failure is a bug in the GSM stack?
# Chat has a big foobar in that to send a caret (^) you must write \136
#CONNECT_SCRIPT="
#	'' ATZ
#	OK AT
#	TIMEOUT 30
#	OK AT+CGDCONT=1,\"\"IP\"\",\"internet\",\"0.0.0.0\",0,0
#	OK AT+CGATT=0
#	OK AT+COPS=1,2,00101
#	OK AT+CGDATA=\"PPP\"
#	OK ATDT*99***1#
#	CONNECT '' "

CONNECT_SCRIPT="
       '' ATZ
       OK AT
       TIMEOUT 30
       OK ATDT*99***1#
       CONNECT '' "

DISCONNECT_SCRIPT="
	'' +++\\\\d\\\\dATH0
	'NO CARRIER' '' "

# The modem does not support compression so we diable those just to save time in the negotiation.
# The modem takes a long time to init, so LCP IP address negotiation may fail on the first invocation
# of pppd but then succeed on the second invocation unless ipcp-max-configure is specified.
# If you dont include the disconnect script then the immediately following connect will fail.
# Removed: auth
	# user pat password "" \
/usr/sbin/pppd /dev/ttyUSB0 115200 debug dump usepeerdns defaultroute \
	login \
	ipcp-max-configure 20 logfile ppp.log\
	nodeflate nobsdcomp nopcomp novj novjccomp \
	connect "/usr/sbin/chat -v -e $(echo $CONNECT_SCRIPT)" \
	disconnect "/usr/sbin/chat -v -e $(echo $DISCONNECT_SCRIPT)"


#tail -f /var/log/syslog
tail -f ppp.log
