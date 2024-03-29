#!/bin/sh

log I "Restoring missing services..."
for service in $(find $BANK2/etc/init.d -type f | xargs -r -n 1 basename); do
  if [ ! -e /etc/init.d/$service ]; then
    log D " -- Restoring $service service"
    restore_file /etc/init.d/$service
  fi
done

log I "Restoring missing linked services..."
for service in $(find $BANK2/etc/init.d -type l | xargs -r -n 1 basename); do
  target=$(readlink $BANK2/etc/init.d/$service)
  if [ ! -L /etc/init.d/$service -a -n "$target" -a -e $target ]; then
    log D " -- Restoring $service linked service"
    ln -s /etc/init.d/$service $target
  fi
done

log I "Restoring service status..."
for service in $(find $BANK2/etc/rc.d -type l | xargs -r -n 1 basename | sed 's/^[SK][0-9][0-9]\(.*\)$/\1/' | sort -u); do
  if [ -n "$service" -a -e /etc/init.d/$service ]; then
    log D " -- Enabling $service service"
    /etc/init.d/$service enable
  fi
done
for service in $(find $BANK2/etc/rc.d -type c | xargs -r -n 1 basename | sed 's/^[SK][0-9][0-9]\(.*\)$/\1/' | sort -u); do
  if [ -n "$service" -a -e /etc/init.d/$service ]; then
    log D " -- Disabling $service service"
    /etc/init.d/$service disable
  fi
done

unset service target