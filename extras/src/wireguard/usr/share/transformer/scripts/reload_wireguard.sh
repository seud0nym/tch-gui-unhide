#!/bin/sh

. $IPKG_INSTROOT/lib/functions.sh
. $IPKG_INSTROOT/lib/functions/network.sh

[ -e /tmp/reload_wireguard.debug ] && DEBUG=y

handle_interface() {
  local interface="$1"

  config_get proto $interface proto
  config_get enabled $interface enabled

  if [ "{$proto}" = "{wireguard}" ]; then
    if /bin/grep -qE "^${interface}$" /tmp/.wg_uci_modified_ifnames; then
      [ "$DEBUG" ] && /usr/bin/logger -t wireguard -p daemon.debug "reload_wireguard.handle_interface: FOUND $interface in /tmp/.wg_uci_modified_ifnames [enabled=$enabled]"
      /bin/sed -e "/^${interface}$/d" -i /tmp/.wg_uci_modified_ifnames
      /sbin/ifdown $interface
      [ "{$enabled}" != "{0}" ] && ifup $interface
    else
      [ "$DEBUG" ] && /usr/bin/logger -t wireguard -p daemon.debug "reload_wireguard.handle_interface: SKIPPED $interface [enabled=$enabled]"
    fi
  fi
}

{
  /usr/bin/flock -x 3

  config_load network
  config_foreach handle_interface interface

  # Anything left must have been deleted
  for interface in $(/bin/cat /tmp/.wg_uci_modified_ifnames | sort -u); do
    [ "$DEBUG" ] && /usr/bin/logger -t wireguard -p daemon.debug "reload_wireguard: FOUND $interface in /tmp/.wg_uci_modified_ifnames but not in /etc/config/network"
    /bin/sed -e "/^${interface}$/d" -i /tmp/.wg_uci_modified_ifnames
    /sbin/ifdown $interface
  done
} 3>/var/lock/uci.wireguard.lock

exit 0
