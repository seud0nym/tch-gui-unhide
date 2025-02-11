#!/bin/sh

. $IPKG_INSTROOT/lib/functions.sh

_DNS4=""
_ZONES=""
_IP=""

find_dhcp_option() {
  local value="$1"
  local ip="${value#6,}"
  [ "$ip" = "$value" ] && return
  _DNS4="${_DNS4},${ip}"
}

find_dns_servers() {
  local config="$1"
  local dns dhcp_option interface
  config_get interface "$config" "interface" "dummy"
  _DNS4="$_DNS4 $interface"
  config_list_foreach "$config" "dhcp_option" find_dhcp_option
}

add_zone_network() {
  local network="$1"
  _ZONES="${_ZONES},${network}"
}

find_guest_zones() {
  local config="$1"
  local zone enabled wan ifname
  config_get zone "$config" "name"
  [ "$zone" = "loopback" ] && return
  config_get enabled "$config" "enabled" "1"
  [ "$enabled" != "0" ] || return
  config_get wan "$config" "wan" "0"
  [ "$wan" = "1" ] && return
  _ZONES="$_ZONES ${zone}"
  config_list_foreach "$config" network add_zone_network
}

find_ip_address() {
  local config="$1"
  [ "$config" = "loopback" ] && return
  local proto ipaddr
  config_get proto "$config" "proto"
  [ "$proto" = "static" ] || return
  config_get ipaddr "$config" "ipaddr"
  _IP="$_IP ${config},${ipaddr}"
}

# Delete any existing rules
for table in nat filter; do
  iptables -t $table -S | grep "DNSv4-Hijacking" | while read action params; do
    not_quoted=$(echo "$params" | cut -d'"' -f1)
    quoted=$(echo "$params" | cut -d'"' -f2)
    if [ "$not_quoted" = "$quoted" ]; then
      iptables -t $table -D $not_quoted
    else
      iptables -t $table -D $not_quoted "$quoted"
    fi
  done
done

# See if IPv4 DNS Hijacking is enabled
config_load firewall
config_get _HIJACK4 "dns_int" "enabled" "0"
  
[ "$_HIJACK4" = "1" ] || exit

# Find Guest zones
config_foreach find_guest_zones "zone"

# Find interface IP addresses
config_load network
config_foreach find_ip_address "interface"

# Load the IP addresses of DNS servers configured via DHCP
config_load dhcp
config_foreach find_dns_servers "dhcp"

# First insert the logging rules
for zone in $_ZONES; do
  iptables -t nat -nL zone_${zone}_prerouting --line-numbers | grep Hijack-DNS | sed -e 's| /\* !fw3: .*$||' | sort -nr | while read num target prot opt src dst proto dpt ipset; do
    [ -n "$ipset" ] && ipset="-m set $(echo "$ipset" | sed -e 's/match-set/--&/')"
    iptables -t nat -I zone_${zone}_prerouting $num -p $prot -m $proto $(echo $dpt | sed -e 's/dpt:/--dport /') $ipset -m comment --comment 'DNSv4-Hijacking' -j LOG --log-prefix '** HIJACKED ** '
  done
  iptables -t filter -nL zone_${zone}_forward --line-numbers | grep Hijack-Block | sed -e 's| /\* !fw3: .* \*/||' | sort -nr | while read num target prot opt src dst proto dpt ipset; do
    [ -n "$ipset" ] && ipset="-m set $(echo "$ipset" | sed -e 's/match-set/--&/')"
    iptables -t filter -I zone_${zone}_forward $num -p $prot -m $proto $(echo $dpt | sed -e 's/dpt:/--dport /') $ipset -m comment --comment 'DNSv4-Hijacking' -j LOG --log-prefix '** BLOCKED ** '
  done
done

# Create the rules to always accept defined DNS servers
for z in $_ZONES; do
  zone=$(echo "$z" | cut -d, -f1)
  networks=$(echo "$z" | cut -d, -f2-)
  for network in $(echo $networks | tr ',' ' '); do
    d=$(echo "$_DNS4" | grep -oE "\b${network},[^ ]*" | cut -d, -f2-)
    [ -z "$d" ] && d=$(echo "$_IP" | grep -oE "\b${network},[^ ]*" | cut -d, -f2-)
    for dns in $(echo "$d" | tr ',' ' '); do
      for proto in udp tcp; do
        iptables -t nat -I zone_${zone}_prerouting -d $dns/32 -p $proto -m $proto --dport 53 -m comment --comment "DNSv4-Hijacking" -j ACCEPT 2>/dev/null
      done
    done
  done
done

exit
