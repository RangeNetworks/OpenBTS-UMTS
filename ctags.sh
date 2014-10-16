# OpenBTS provides an open source alternative to legacy telco protocols and 
# traditionally complex, proprietary hardware systems.
#
# Copyright 2014 Range Networks, Inc.

# This software is distributed under the terms of the GNU Affero General 
# Public License version 3. See the COPYING and NOTICE files in the main 
# directory for licensing information.

# This use of this software may be subject to additional restrictions.
# See the LEGAL file in the main directory for details.

DIRS="apps CLI CommonLibs Control Globals
	GSM SGSNGGSN SIP SMS SubscriberRegistry SR TransceiverRAD1 TRXManager UMTS"

#	ASN tools/

files=""
for dir in $DIRS;do
    files="$files $dir/*.h $dir/*.cpp"
done

eval echo $files
# Ignore PACKED keyword
# The --extra=+fq flag is for exuberant ctags, not the stock ctags.
# The stock ctags doesnt work on our files very well, maybe because of the namespace?
# Exuberant ctags used to be part of vim, but lately you must install it separately via: apt-get install exuberant-ctags.
eval ctags -I PACKED --extra=+fq $files

