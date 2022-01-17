#!/bin/sh

if [ "$DEVICE_VERSION" = "$BACKUP_VERSION" -o \( "$DEVICE_VERSION" = "17.2" -a "$BACKUP_VERSION" = "18.1.c" \) -o \( "$BACKUP_VERSION" = "17.2" -a "$DEVICE_VERSION" = "18.1.c" \) ]; then
  restore_file /etc/config/power
fi
