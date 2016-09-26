#!/bin/bash
### BEGIN INIT INFO
#
# Provides:             YSFReflector
# Required-Start:       $all
# Required-Stop:        
# Default-Start:        2 3 4 5
# Default-Stop:         0 1 6
# Short-Description:    Example startscript YSFReflector

#
### END INIT INFO
## Fill in name of program here.
PROG="YSFReflector"
PROG_PATH="/usr/local/bin/"
PROG_ARGS="/etc/YSFReflector.ini"
PIDFILE="/var/run/YSFReflector.pid"
USER="root"

start() {
      if [ -e $PIDFILE ]; then
          ## Program is running, exit with error.
          echo "Error! $PROG is currently running!" 1>&2
          exit 1
      else
          cd $PROG_PATH
          ./$PROG $PROG_ARGS
          echo "$PROG started"
          touch $PIDFILE
      fi
}

stop() {
      if [ -e $PIDFILE ]; then
          ## Program is running, so stop it
         echo "$PROG is running"
         rm -f $PIDFILE
         killall $PROG
         echo "$PROG stopped"
      else
          ## Program is not running, exit with error.
          echo "Error! $PROG not started!" 1>&2
          exit 1
      fi
}

## Check to see if we are running as root first.
## Found at
## http://www.cyberciti.biz/tips/shell-root-user-check-script.html
if [ "$(id -u)" != "0" ]; then
      echo "This script must be run as root" 1>&2
      exit 1
fi

case "$1" in
      start)
          start
          exit 0
      ;;
      stop)
          stop
          exit 0
      ;;
      reload|restart|force-reload)
          stop
          sleep 5
          start
          exit 0
      ;;
      **)
          echo "Usage: $0 {start|stop|reload}" 1>&2
          exit 1
      ;;
esac
exit 0
### END
