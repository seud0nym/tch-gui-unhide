#!/bin/sh
. /lib/functions.sh

_ssh_ports=$(uci show dropbear | grep Port | cut -d= -f2 | tr -d "'" | sort -u)
_gui_ports=$(grep listen /etc/nginx/nginx.conf | grep -v ':\|55555' | grep -o '[0-9]*' | sort -u)

add_input_port_rule() {
  local cmd="$1"
  local chain="$2"
  local allowed_ip="$3"
  local protocols="$4"
  local port protocol
  shift 4

  for port in $*; do
    for protocol in $(echo $protocols | tr -s ',' ' '); do
      $cmd -A "$chain" --protocol "$protocol" --dport "$port" -s "$allowed_ip" -j reject
    done
  done
}

delete_chain() {
  local cmd="$1"
  local interface="$2"
  local chain="$3"

  $cmd -D "${chain}_rule" -i "${interface}"  -j "${interface}_${chain}" 2>/dev/null
  $cmd -F "${interface}_${chain}" 2>/dev/null
  $cmd -X "${interface}_${chain}" 2>/dev/null
}

handle_peer() {
  local peer_config="$1"
  local interface="$2"
  local ipv6="$3"
  local allowed_ip allowed_ips lan_access wan_access port

  config_get allowed_ips "$peer_config" "allowed_ips"
  config_get lan_access "$peer_config" "lan_access" "1"
  config_get wan_access "$peer_config" "wan_access" "1"
  config_get ssh_access "$peer_config" "ssh_access" "1"
  config_get gui_access "$peer_config" "gui_access" "1"

  [ -z "${allowed_ips}" -o \( "$lan_access" = "1" -a "$wan_access" = "1" -a "$ssh_access" = "1" -a "$gui_access" = "1" \) ] && return

  for allowed_ip in ${allowed_ips}; do
    case "${allowed_ip}" in
      *:*)
        if [ "$ipv6" != "0" ]; then
          [ "$lan_access" != "1" ] && ip6tables -A "${interface}_forwarding" -o br-lan -s "$allowed_ip" -j reject
          [ "$wan_access" != "1" ] && ip6tables -A "${interface}_forwarding" ! -o br-lan -s "$allowed_ip" -j reject
          [ "$ssh_access" != "1" ] && add_input_port_rule ip6tables "${interface}_input" "$allowed_ip" "tcp,udp" $_ssh_ports
          [ "$gui_access" != "1" ] && add_input_port_rule ip6tables "${interface}_input" "$allowed_ip" "tcp" $_gui_ports
        fi;;
      *.*)
        [ "$lan_access" != "1" ] && iptables -A "${interface}_forwarding" -o br-lan -s "$allowed_ip" -j reject
        [ "$wan_access" != "1" ] && iptables -A "${interface}_forwarding" ! -o br-lan -s "$allowed_ip" -j reject
        [ "$ssh_access" != "1" ] && add_input_port_rule iptables "${interface}_input" "$allowed_ip" "tcp,udp" $_ssh_ports
        [ "$gui_access" != "1" ] && add_input_port_rule iptables "${interface}_input" "$allowed_ip" "tcp" $_gui_ports
        ;;
    esac
  done
}

handle_interface() {
  local interface="$1"
  local enabled proto listen_port ipv6 cmd chain

  config_get proto "$interface" "proto" "unknown"
  config_get listen_port "$interface" "listen_port"
  [ "$proto" = "wireguard" -a -n "$listen_port" ] || return

  config_get ipv6 "$interface" "ipv6" "1"
  if [ "$ipv6" = "1" ]; then
    commands="iptables ip6tables"
  else
    commands="iptables"
    for chain in input forwarding; do
      delete_chain ip6tables "${interface}" "${chain}"
    done
  fi

  for cmd in $commands; do
    for chain in input forwarding; do
      $cmd -N "${interface}_${chain}" 2>/dev/null
      $cmd -C "${chain}_rule" -i "${interface}" -j "${interface}_${chain}" 2>/dev/null || $cmd -A "${chain}_rule" -i "${interface}" -j "${interface}_${chain}"
      $cmd -F "${interface}_${chain}"
    done
  done

  config_get enabled "$interface" "enabled" "1"
  if [ "$enabled" = "1" -a "$(ifstatus $interface | jsonfilter -e '@.up')" = "true" ]; then
    /usr/bin/logger -t "wireguard-peer-firewall.sh" -p daemon.notice "Applying peer firewall rules on interface '$interface'"
    for cmd in $commands; do
      for chain in input forwarding; do
        for protocol in tcp udp; do
          $cmd -I "${interface}_${chain}" --protocol "$protocol" --dport 53 -j ACCEPT
        done
      done
    done
    config_foreach handle_peer "wireguard_$interface" "$interface" "$ipv6"
  else
    /usr/bin/logger -t "wireguard-peer-firewall.sh" -p daemon.notice "Removing peer firewall rules on interface '$interface'"
    for cmd in iptables ip6tables; do
      for chain in input forwarding; do
        delete_chain "$cmd" "${interface}" "${chain}"
      done
    done
  fi
}

config_load network
config_foreach handle_interface "interface"

exit 0
