The YSF Gateway interfaces the MMDVM Host to the open source YSF Reflector system using the standard Wires-X commands from the radio. It also gateways position information to aprs.fi if sent.

The file YSFHosts.txt holds information about the reflectors available, and the gateway has the ability to reload this file at intervals to ensure that it is always up to date.

It is expected that a call to retrieve this file is done via some mechanism such as cron on Linux. The URLs to retrieve the latest file are [http://register.ysfreflector.de/export_csv.php](http://register.ysfreflector.de/export_csv.php), or [https://register.ysfreflector.de/export_csv.php](https://register.ysfreflector.de/export_csv.php).

An example would be following line in root's crontab, that fetches the reflector-list each 5 minutes:
> sudo crontab -e

add following line:

> */5 * * * * wget -O /var/YSFGateway/YSFHosts.txt http://register.ysfreflector.de/export_csv.php
