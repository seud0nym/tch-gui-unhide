#!/bin/sh

restore_file /etc/config/bqos
if [ "$DEVICE_VERSION" != "17.2" -o "$BACKUP_VERSION" != "17.2" ]; then
  log I "Restoring QoS Egress Shapers..."
  uci -q revert qos
  uci_copy qos swshaper
  for shaper in $($UCI show qos | grep "=swshaper$" | cut -d. -f2); do
    for device in $($UCI show qos | grep "^qos\.[^\.]*\.swshaper='$shaper'" | cut -d. -f2); do
      uci_set qos.$device.swshaper
    done
  done
  uci -q commit qos
fi
