#!/bin/bash
# This script has been tested in Ubuntu 16.04 and 18.04. Please report any problem you find.

# Latest UHD from Ettus PPA
sudo add-apt-repository ppa:ettusresearch/uhd
sudo apt-get update
sudo apt-get install libuhd-dev libuhd003 uhd-host

# Other dependencies, mostly build-time
sudo apt-get install git autoconf libtool libtool-bin gpp g++ libzmq3-dev pkg-config libosip2-dev libortp-dev libusb-dev libusb-1.0-0-dev libreadline-dev  libsqlite3-dev -y
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
