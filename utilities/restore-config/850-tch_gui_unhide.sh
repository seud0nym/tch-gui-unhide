#!/bin/sh

log I "Restoring subset of GUI files..."
find /www/cards/ /www/docroot/modals/ -maxdepth 1 -type f -exec rm {} \;
restore_directory /www/cards -maxdepth 1
restore_directory /www/docroot/modals -maxdepth 1
restore_file /www/docroot/gateway.lp /www/docroot/css/gw-telstra.css /www/docroot/css/gw.css /www/docroot/css/responsive.css /usr/share/transformer/mappings/rpc/gui.map /etc/tch-gui-unhide.theme /etc/config/tch_gui_unhide

log I "Restoring GUI configuration..."
uci -q revert web
uci_copy web rule 'dumaos' "roles"
for cfg in $($UCI show web | grep 'web\.card_'); do
  uci_set $cfg
done
uci_set web.usr_admin.srp_salt
uci_set web.usr_admin.srp_verifier
uci -q commit web

uci -q revert system
uci_set system.config.export_plaintext
uci_set system.config.export_unsigned
uci_set system.config.import_plaintext
uci_set system.config.import_unsigned
uci -q commit system

log D "Calculating tch-gui-unhide options"
options="--no-service-restart -y"

[ -n "$($UCI -q get web.uidefault.defaultuser)" -a -n "$($UCI -q get web.default.default_user)" ]  && options="$options -dy" || options="$options -dn"
[ "$($UCI -q get web.uidefault.upgradefw)" = "1" ] && options="$options -fy" || options="$options -fn"

[ -e /etc/config/adblock ] && options="$options -x adblock"
[ -e /etc/config/minidlna ] && options="$options -x minidlna"
[ -e /etc/rsyncd.conf ] && options="$options -x rsyncd"
[ -e /usr/sbin/smbd ] && /usr/sbin/smbd -V | grep -q '^Version 3\.6\.' && options="$options -x samba36-server"
[ -e /usr/bin/wireguard-go ] && options="$options -x wireguard"

title="$(grep -o 'title>.*</title' /www/docroot/gateway.lp | cut -d'>' -f2 | cut -d'<' -f1 | grep -v 'ngx.print')"
if [ -n "$title" ]; then
  options="$options -h$(echo "$title" | sed -e "s/$BACKUP_VARIANT/$DEVICE_VARIANT/")"
  [ $TEST_MODE = y ] && options="${options}-TEST"
fi

run_script tch-gui-unhide-$DEVICE_VERSION $options

unset cfg options title