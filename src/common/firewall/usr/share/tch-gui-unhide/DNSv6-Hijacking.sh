#!/bin/sh

. $IPKG_INSTROOT/lib/functions.sh

_DNS6=""
_ZONES="lan"

find_dns() {
  _DNS6="$_DNS6 $1"
}

find_dns_servers() {
  local dhcp="$1"
  local dns dhcp_option
  config_list_foreach "$dhcp" "dns" find_dns
}

find_guest_zones() {
  local config="$1"
  local zone enabled wan
  config_get zone "$config" "name"
  [ "$zone" = "loopback" -o "$zone" = "lan" ] && return
  config_get enabled "$config" "enabled" "1"
  [ "$enabled" != "0" ] || return
  config_get wan "$config" "wan" "0"
  [ "$wan" = "1" ] && return
  _ZONES="$_ZONES $zone"
}

# Delete any existing rules
for table in mangle filter; do
  ip6tables -t $table -S | grep "DNSv6-Hijacking" | while read action params; do
    not_quoted=$(echo "$params" | cut -d'"' -f1)
    quoted=$(echo "$params" | cut -d'"' -f2)
    if [ "$not_quoted" = "$quoted" ]; then
      ip6tables -t $table -D $not_quoted
    else
      ip6tables -t $table -D $not_quoted "$quoted"
    fi
  done
done

# See if IPv6 DNS Hijacking is enabled
config_load tproxy
config_get _HIJACK6 "dnsv6" "enabled" "0"

[ "$_HIJACK6" = "1" ] || exit

# Load the proxy address
config_get targetPort "dnsv6" "targetPort" 
if [ -z "$targetPort" -o $targetPort = "53" ]; then
  config_get _DNS6 "dnsv6" "destIP" 
  if [ -z "$_DNS6" ]; then
    config_load network
    config_get wan_ifname "wan" "ifname"
    _DNS6="$(ip -6 -o a show scope global | grep -v "$wan_ifname" | awk '{split($4,a,"/");print a[1]}' | xargs)"
  fi
fi

# Load the IP addresses of DNS servers configured via DHCP
config_load dhcp
config_foreach find_dns_servers "dhcp"

# Find Guest zones
config_load firewall
config_foreach find_guest_zones "zone"

# First insert the logging rules
ip6tables -t mangle -nL PREROUTING --line-numbers  | grep '!tproxy-go/dnsv6@.*TPROXY' | sort -nr | sed -e 's|/\* !tproxy-go/dnsv6@.*||' | while read num target prot src dst proto dpt ipset; do
  [ -n "$ipset" ] && ipset="-m set $(echo "$ipset" | sed -e 's/match-set/--&/')"
  ip6tables -t mangle -I PREROUTING $num -p $prot -m $proto $(echo $dpt | sed -e 's/dpt:/--dport /') $ipset -m comment --comment 'DNSv6-Hijacking' -j LOG --log-prefix '** HIJACKED ** '
done
for zone in $_ZONES; do
  ip6tables -t filter -nL zone_${zone}_forward --line-numbers | grep Hijack-Block | sed -e 's| /\* !fw3: .* \*/||' | sort -nr | while read num target prot src dst proto dpt ipset; do
    [ -n "$ipset" ] && ipset="-m set $(echo "$ipset" | sed -e 's/match-set/--&/')"
    ip6tables -t filter -I zone_${zone}_forward $num -p $prot -m $proto $(echo $dpt | sed -e 's/dpt:/--dport /') $ipset -m comment --comment 'DNSv6-Hijacking' -j LOG --log-prefix '** BLOCKED ** '
  done
done

# Create the rules to always accept defined DNS servers
for dns in $_DNS6; do
  for proto in udp tcp; do
    ip6tables -t mangle -I PREROUTING -d $dns/128 -p $proto -m $proto --dport 53 -m comment --comment "DNSv6-Hijacking" -j ACCEPT 2>/dev/null
  done
done

exit