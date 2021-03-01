#!/bin/sh

# Might as well clean up after ourselves, too.
rm -rf /var/run/asterisk
if pgrep transceiver; then killall transceiver; fi

systemctl stop asterisk
systemctl stop sipauthserve
systemctl stop smqueue
systemctl stop openbts
