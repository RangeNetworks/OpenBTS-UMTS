# OpenBTS scripts for systemd
In this repository You will find OpenBTS related scripts.

The scripts is created to run OpenBTS services on Ubuntu 16.04 with ```systemd``` init system.

To run OpenBTS, Asterisk, SipAuthServe and Smqueue services on Ubuntu 16.04:
1. Copy all files from folder systemd to ```/etc/systemd/system/``` folder on your computer.
2. Execute ```systemctl reload``` and all services will be availible from ```systemctl``` utility.

To run/stop service with ```systemctl``` execute ```systemctl start service_name``` or ```systemctl stop service_name```

To start/stop all services, use ```openbts-start.sh``` and ```openbts-stop.sh```.
