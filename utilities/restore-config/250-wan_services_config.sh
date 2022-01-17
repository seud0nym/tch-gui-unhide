#!/bin/sh

dev1st="$(echo "$DEVICE_VERSION" | cut -c1)"
bak1st="$(echo "$BACKUP_VERSION" | cut -c1)"

log I "Restoring Dynamic DNS configuration..."
restore_file /etc/config/ddns

log I "Restoring iperf configuration..."
restore_file /etc/config/iperf 

log I "Restoring WoL configuration..."
if [ "$DEVICE_VERSION" = "$BACKUP_VERSION" -o "$dev1st" = "$bak1st" ]; then
  restore_file /etc/config/wol
elif [ "$dev1st" != "1" -a "$bakfst" = "1" ]; then
  sed \
    -e 's/uci.wol.config.enabled/uci.wol.proxy.@wan2lan.enable/' \
    -e 's/uci.wol.config.src_dport/uci.wol.proxy.@wan2lan.src_port/' \
    -e 's/uci.wol.config/uci.wol.proxy.@wan2lan/' \
    $BANK2/etc/config/wol > /etc/config/wol
elif [ "$dev1st" = "1" -a "$bakfst" != "1" ]; then
  sed \
    -e 's/uci.wol.proxy.@wan2lan.enable/uci.wol.config.enabled/' \
    -e 's/uci.wol.proxy.@wan2lan.src_port/uci.wol.config.src_dport/' \
    -e 's/uci.wol.proxy.@wan2lan/uci.wol.config/' \
    $BANK2/etc/config/wol > /etc/config/wol
fi
