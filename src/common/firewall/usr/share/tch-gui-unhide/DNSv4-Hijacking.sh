#!/bin/sh

. $IPKG_INSTROOT/lib/functions.sh

_DNS4="$(ip -4 -o a show dev br-lan scope global | awk '{split($4,a,"/");print a[1]}' | xargs)"
_ZONES="lan"
  
find_dhcp_option() {
  local value="$1"
  local ip="${value#6,}"
  [ "$ip" = "$value" ] && return
  _DNS4="$_DNS4 $(echo $ip | tr ',' ' ')"
}

find_dns_servers() {
  local dhcp="$1"
  local dns dhcp_option
  config_list_foreach "$dhcp" "dhcp_option" find_dhcp_option
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
iptables -t nat -S | grep "DNSv4-Hijacking" | while read action params; do
  iptables -t nat -D $params
done

# See if IPv4 DNS Hijacking is enabled
config_load firewall
config_get _HIJACK4 "dns_int" "enabled" "0"
  
[ "$_HIJACK4" = "1" ] || exit

# Find Guest zones
config_foreach find_guest_zones "zone"

# Load the IP addresses of DNS servers configured via DHCP
config_load dhcp
config_foreach find_dns_servers "dhcp"

# First insert the logging rules
iptables -t nat -S | grep -E '(Redirect|Intercept)-DNS' | sed -e 's/-m comment.*$//' | while read action params; do 
  iptables -t nat -I $params -m comment --comment "DNSv4-Hijacking" -j LOG --log-prefix '** HIJACKED ** ' 
done

# Create the rules to always accept these DNS servers
for dns in $_DNS4; do
  for proto in udp tcp; do
    for zone in $_ZONES; do
      iptables -t nat -I zone_${zone}_prerouting -d $dns/32 -p $proto -m $proto --dport 53 -m comment --comment "DNSv4-Hijacking" -j ACCEPT 2>/dev/null
    done
  done
done

exit
