#!/bin/sh

. $IPKG_INSTROOT/lib/functions.sh

_DNS4=""

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

# Delete any existing rules
iptables -t nat -S zone_lan_prerouting | grep "No-Hijack-DHCP-DNSv4" | while read action params; do
  iptables -t nat -D $params
done

# See if IPv4 DNS Hijacking is enabled
config_load firewall
config_get _HIJACK4 "dns_int" "enabled" "0"
  
[ "$_HIJACK4" = "1" ] || exit

# Load the device IPs if no proxy specified
[ -z "$_DNS4" ] && _DNS4="$(ip -4 -o a show dev br-lan scope global | awk '{split($4,a,"/");print a[1]}' | xargs)"
# Load the IP addresses of DNS servers configured via DHCP
config_load dhcp
config_foreach find_dns_servers "dhcp"

# First insert the logging rules
iptables -t nat -S | grep -E '(Redirect|Intercept)-DNS' | sed -e 's/-m comment.*$//' | while read action params; do 
  iptables -t nat -I $params -m comment --comment "No-Hijack-DHCP-DNSv4" -j LOG --log-prefix '** HIJACKED ** ' 
done

# Create the rules to always accept these DNS servers
for dns in $_DNS4; do
  for proto in udp tcp; do
    iptables -t nat -I zone_lan_prerouting -d $dns/32 -p $proto -m $proto --dport 53 -m comment --comment "No-Hijack-DHCP-DNSv4" -j ACCEPT 2>/dev/null
  done
done

exit
