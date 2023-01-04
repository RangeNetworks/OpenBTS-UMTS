#!/bin/bash
# This script has been tested in Ubuntu 16.04 and 18.04. Please report any problem you find.

# Install all package dependencies
sudo apt install autoconf libtool build-essential libuhd-dev uhd-host libzmq3-dev libosip2-dev libortp-dev libusb-1.0-0-dev asn1c libtool-bin libsqlite3-dev libreadline-dev

# Clone submodules from base repo
git submodule init
git submodule update
# Uncompress, configure, compile and install bundled version of ASN1 compiler, necessary in build-time
tar -xvzf asn1c-0.9.23.tar.gz
cd ./vlm-asn1c-0959ffb/
./configure
make
sudo make install
cd ../
# TODO: possibly more dependencies to be added here. Please suggest any requirement package you detect as missing.
