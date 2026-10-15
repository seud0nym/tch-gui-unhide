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
# Firmware with this flag (e.g. 20.3.c MR22.1) pre-fills the default password on the login pages while it is 0
# Set it from the restored verifier (the backup may predate the flag, or hold a stale 0), with the same check as the
# firmware's own upgrade conversion (/usr/lib/parameter_conversion/verify_password_change.lua)
if [ -n "$(uci -q get web.usr_admin.password_changed)" ]; then
  if lua -e '
    local srp = require("srp")
    local salt = io.popen("uci -q get web.usr_admin.srp_salt"):read("*l")
    local verifier = io.popen("uci -q get web.usr_admin.srp_verifier"):read("*l")
    if not salt or not verifier then os.exit(1) end
    local user, A = srp.User("admin", "Telstra")
    local server, B = srp.Verifier("admin", salt, verifier, A)
    local _, err = server:verify(user:get_M(salt, B))
    os.exit(err and 0 or 1)'; then
    log V " ++ Setting web.usr_admin.password_changed='1' (restored password is not the default)"
    uci set web.usr_admin.password_changed='1'
  fi
fi
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