#!/bin/sh

log I "Restoring scheduled cron jobs..."
cat /etc/crontabs/root | while read -r mm hh day mth wkday cmd params; do
  set -- $params
  while [ "/$1/" != "//" -a "/$(echo -e "$1" | cut -c1)/" = "/-/" ]; do shift; done
  case "$cmd" in
    sh) echo $*;;
    /usr/bin/lua) echo $1;;
    ubus) echo $2;;
    *) echo $cmd
  esac
done > /tmp/restore-config_cron-exceptions
grep -vwf /tmp/restore-config_cron-exceptions $BANK2/etc/crontabs/root >> /etc/crontabs/root
rm /tmp/restore-config_cron-exceptions
