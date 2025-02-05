#!/bin/sh

. $IPKG_INSTROOT/lib/functions.sh

_DNS6=""

find_dns() {
  _DNS6="$_DNS6 $1"
}

find_dns_servers() {
  local dhcp="$1"
  local dns dhcp_option
  config_list_foreach "$dhcp" "dns" find_dns
}

# Delete any existing rules
ip6tables -t mangle -S PREROUTING | grep "DNSv6-Hijacking" | while read action params; do
  ip6tables -t mangle -D $params
done

# See if IPv6 DNS Hijacking is enabled
config_load tproxy
config_get _HIJACK6 "dnsv6" "enabled" "0"

[ "$_HIJACK6" = "1" ] || exit

# Load the proxy address
config_get _DNS6 "dnsv6" "destIP" 
[ -z "$_DNS6" ] && _DNS6="$(ip -6 -o a show scope global | grep -v "$(uci -q get network.wan.ifname)" | awk '{split($4,a,"/");print a[1]}' | xargs)"
# Load the IP addresses of DNS servers configured via DHCP
config_load dhcp
config_foreach find_dns_servers "dhcp"

# First insert the logging rules
ip6tables -t mangle -S PREROUTING | grep '!tproxy-go/dnsv6@.*-j TPROXY' | sed -e 's/-m comment.*$//' | while read action params; do 
  ip6tables -t mangle -I $params  -m comment --comment "DNSv6-Hijacking" -j LOG --log-prefix '** HIJACKED ** ' 
done

# Create the rules to always accept these DNS servers
for dns in $_DNS6; do
  for proto in udp tcp; do
    ip6tables -t mangle -I PREROUTING -d $dns/128 -p $proto -m $proto --dport 53 -m comment --comment "DNSv6-Hijacking" -j ACCEPT 2>/dev/null
  done
done

exit