#!/bin/sh

. $IPKG_INSTROOT/lib/functions.sh
. $IPKG_INSTROOT/lib/functions/network.sh

[ -e /tmp/reload_wireguard.debug ] && DEBUG=y

handle_interface() {
  local interface="$1"

  config_get proto   $interface proto   unknown
  config_get enabled $interface enabled 0

  if [ "$proto" = "wireguard" ]; then
    config_get log_level $interface log_level 
    [ "$log_level" = "debug" ] && DEBUG=y
    if /bin/grep -qE "^${interface}$" /tmp/.wg_uci_modified_ifnames; then
      [ "$DEBUG" ] && /usr/bin/logger -t wireguard -p daemon.debug "reload_wireguard.handle_interface: Interface '$interface' found in /tmp/.wg_uci_modified_ifnames [enabled=$enabled]"
      /bin/sed -e "/^${interface}$/d" -i /tmp/.wg_uci_modified_ifnames
      uci -q del_list dhcp.main.interface="$interface"
      [ "$enabled" = "1" ] && ifup $interface && uci -q add_list dhcp.main.interface="$interface"
    elif [ "$enabled" = "1" -a "$(ifstatus $interface | jsonfilter -e '@.up')" = "false" -a "$(ifstatus $interface | jsonfilter -e '@.pending')" = "false" ]; then
      [ "$DEBUG" ] && /usr/bin/logger -t wireguard -p daemon.debug "reload_wireguard.handle_interface: Interface '$interface' enabled but down! Bringing it back up..."
      uci -q del_list dhcp.main.interface="$interface"
      uci -q add_list dhcp.main.interface="$interface"
    elif [ "$enabled" = "1" -a -z "$(ifstatus $interface | jsonfilter -e '@.up')" ]; then
      [ "$DEBUG" ] && /usr/bin/logger -t wireguard -p daemon.debug "reload_wireguard.handle_interface: Interface '$interface' enabled but does not exist? Starting network..."
      NETWORK_ACTION="restart"
    else
      [ "$DEBUG" ] && /usr/bin/logger -t wireguard -p daemon.debug "reload_wireguard.handle_interface: Interface '$interface' SKIPPED [enabled=$enabled]"
    fi
  fi
}

{
  /usr/bin/flock -x 3

  NETWORK_ACTION="reload"

  config_load network
  config_foreach handle_interface interface

  # Anything left must have been deleted
  for interface in $(/bin/cat /tmp/.wg_uci_modified_ifnames | sort -u); do
    [ "$DEBUG" ] && /usr/bin/logger -t wireguard -p daemon.debug "reload_wireguard: Interface '$interface' found in /tmp/.wg_uci_modified_ifnames but not in /etc/config/network"
    /bin/sed -e "/^${interface}$/d" -i /tmp/.wg_uci_modified_ifnames
    uci -q del_list dhcp.main.interface="$interface"
  done

  uci commit dhcp
  /etc/init.d/dnmasq reload
  /etc/init.d/network $NETWORK_ACTION
} 3>/var/lock/uci.wireguard.lock

exit 0
