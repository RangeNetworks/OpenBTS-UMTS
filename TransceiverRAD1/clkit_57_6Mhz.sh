#!/bin/bash

# OpenBTS provides an open source alternative to legacy telco protocols and 
# traditionally complex, proprietary hardware systems.
#
# Copyright 2014 Range Networks, Inc.
#
# This software is distributed under the terms of the GNU General Public 
# License version 3. See the COPYING and NOTICE files in the current 
# directory for licensing information.
#
# This use of this software may be subject to additional restrictions.
# See the LEGAL file in the main directory for details.


# RangeNetworks - clock set utility to force 57.6 MHz operation from 13Mhz clock
# 12/27/2011 
# out0 & out1 = 13MHz, out2 & out3 = 57.6MHz
# the PLL mult is set first then each output divider value
# 0x70 is the PLL address, then you have [reg# in hex][value in hex]
# as the 2nd arg
#
# sudo ./RAD1Cmd -x load_firmware /usr/local/share/usrp/rev4/std.ihx
# uses RAD1Cmd instead of usrper

echo "Loading std.ihx firmware file... resetting EZ-USB"
sudo ./RAD1Cmd -x load_firmware ezusb.ihx 
sleep 5
echo "3G PLL register update utility for 57.6Mhz"
echo "Begin updating Si5338 registers"
sudo ./RAD1Cmd i2c_write 0x70 4baa
echo "MultiSynth2 frequency Conf MS2_P1[7:0]" 
sleep 1
sudo ./RAD1Cmd i2c_write 0x70 4c13
echo "MultiSynth2 frequency Conf MS2_P1[15:8]" 
sleep 1
sudo ./RAD1Cmd i2c_write 0x70 4d08
echo "MultiSynth2 frequency Conf MS2_P2[5:0] MS2_P1[17:18] " 
echo "Setting clock output 2 to 57.6Mhz complete"
sleep 1
sudo ./RAD1Cmd i2c_write 0x70 5103
echo "MultiSynth2 frequency Conf MS2_P3[7:0]"
sleep 1
sudo ./RAD1Cmd i2c_write 0x70 56aa
echo "MultiSynth3 frequency Conf MS3_P1[7:0]" 
sleep 1
sudo ./RAD1Cmd i2c_write 0x70 5713
echo "MultiSynth3 frequency Conf MS3_P1[15:8]" 
sleep 1
sudo ./RAD1Cmd i2c_write 0x70 5808
echo "MultiSynth3 frequency Conf MS3_P2[5:0] MS3_P1[17:18] " 
sleep 1
sudo ./RAD1Cmd i2c_write 0x70 5c03
echo "MultiSynth3 frequency Conf MS3_P3[7:0]"
echo "Setting clock output 3 to 57.6Mhz complete"
sleep 1
echo "Completed 57.6MHz configuration... soft resetting"
sudo ./RAD1Cmd i2c_write 0x70 f602
sleep 1
echo "Soft reset complete... exiting"
exit 0
