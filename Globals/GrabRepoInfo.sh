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

cd $1

INFO=""
if [ -d ./.svn ]; then
	INFO="r$(svn info . | grep "Last Changed Rev:" | cut -d " " -f 4) CommonLibs:r$(svn info ./CommonLibs | grep "Last Changed Rev:" | cut -d " " -f 4)"
elif [ -d ./.git ]; then
	INFO="$(git rev-parse --short=10 HEAD) CommonLibs:$(cd CommonLibs; git rev-parse --short=10 HEAD)"
fi

echo $INFO
