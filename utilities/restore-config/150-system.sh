#!/bin/sh

log I "Restoring system configuration..."
uci -q revert system
uci_set system.@coredump[0].reboot
uci_set system.@system[0].log_size
uci_set system.@system[0].log_port y
uci_set system.@system[0].log_ip y
uci_set system.@system[0].conloglevel y
uci_set system.@system[0].network_timezone
if [ "$($UCI -q get system.@system[0].network_timezone)" = "0" ]; then
  uci_set system.@system[0].timezone
  uci_set system.@system[0].zonename
fi
if [ "$BACKUP_SERIAL" = "$DEVICE_SERIAL" ]; then
  uci_set system.@system[0].hw_reboot_count
  uci_set system.@system[0].sw_reboot_count
fi
uci -q commit system

log I "Restoring ledfw configuration..."
if [ "$DEVICE_VERSION" = "$BACKUP_VERSION" ]; then
  restore_file /etc/config/ledfw
fi
restore_file /etc/ledfw/stateMachines.lua

log I "Restoring /etc/rc.local..."
restore_file /etc/rc.local
