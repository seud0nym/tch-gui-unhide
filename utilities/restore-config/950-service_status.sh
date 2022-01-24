#!/bin/sh

log I "Restoring Service status..."
for service in $(ls -l $BANK2/etc/rc.d | grep ^c | grep -oE '[KS][0-9][0-9].*$' | cut -c4- | sort -u); do
  if [ -n "$service" -a -e /etc/init.d/$service ]; then
    log D " -- Disabling $service service"
    /etc/init.d/$service disable
  fi
done

unset service