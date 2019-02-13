#!/bin/bash
sudo apt-get install git autoconf libtool libtool-bin gpp g++ libzmq3-dev pkg-config libosip2-dev libortp-dev libusb-dev libusb-1.0-0-dev libreadline-dev  libsqlite3-dev libuhd-dev -y
git submodule init
git submodule update

tar -xvzf asn1c-0.9.23.tar.gz
cd ./vlm-asn1c-0959ffb/
./configure
make
sudo make install
cd ../
