#!/bin/sh

# Since Ubuntu clears /var/run on reboot, create this before we try to start 
if [ ! -e /var/run/OpenBTS ]; then
	mkdir /var/run/OpenBTS
fi

if [ ! -e /var/run/rrlp ]; then
	mkdir /var/run/rrlp
	chmod 777 /var/run/rrlp
fi

# place for CRD data
if [ ! -e /var/lib/OpenBTS ]; then
	mkdir /var/lib/OpenBTS
fi

if [ ! -d /var/run/asterisk ]; then
	mkdir -p asterisk /var/run/asterisk
	chown asterisk: /var/run/asterisk
fi

# Make sure permissions are set up correctly
if [ -d /var/lib/asterisk/sqlite3dir ]; then
	chown -R asterisk:www-data /var/lib/asterisk/sqlite3dir
	chmod 775 /var/lib/asterisk/sqlite3dir
	chmod 664 /var/lib/asterisk/sqlite3dir/sqlite3*
fi

systemctl start asterisk
systemctl start sipauthserve
systemctl start smqueue
systemctl start openbts

echo -n "Asterisk PID: " ; echo `ps -ef | grep "asterisk" | grep -wv 'grep\|vi\|vim'` | awk '{print $2}'
echo -n "Sipauthserve PID: " ; echo `ps -ef | grep "sipauthserve" | grep -wv 'grep\|vi\|vim'` | awk '{print $2}'
echo -n "Smqueue PID: " ; echo `ps -ef | grep "smqueue" | grep -wv 'grep\|vi\|vim'` | awk '{print $2}'
echo -n "OpenBTS PID: " ; echo `ps -ef | grep "OpenBTS" | grep -wv 'grep\|vi\|vim'` | awk '{print $2}'
