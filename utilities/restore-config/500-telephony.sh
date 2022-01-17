#!/bin/sh

log I "Restoring telephony configuration..."
restore_file /etc/config/mmpbx*
log D "Fixing SIP Network User Agent"
sh /rom/etc/uci-defaults/*99-mmpbx
log D "Fixing SIP Network passwords"
uci -q revert mmpbxrvsipnet
for cfg in $(cat $CONFIG | gunzip | grep -E '^mmpbxrvsipnet\.[^\.]*\.password='); do
  uci_set "$cfg"
done
uci -q commit mmpbxrvsipnet
