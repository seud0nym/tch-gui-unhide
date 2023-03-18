#!/bin/sh
. /lib/functions.sh

[ "$1" = "update" ] && FORCE=y || FORCE=n

config_load intrusion_protect
config_get enabled config enabled 0

if [ "$enabled" = "0" ]; then
  iptables -nL zone_wan_input | grep -q Intrusion_Protection && iptables -D zone_wan_input -j Intrusion_Protection
  iptables -nL zone_wan_forward | grep -q Intrusion_Protection && iptables -D zone_wan_forward -j Intrusion_Protection
  iptables -nL | grep -q "Chain Intrusion_Protection" && { iptables -F Intrusion_Protection; iptables -X Intrusion_Protection; }
  exit 0
fi

iptables -nL | grep -q "Chain Intrusion_Protection" || iptables -N Intrusion_Protection
iptables -nL zone_wan_input | grep -q Intrusion_Protection || iptables -I zone_wan_input 1 -j Intrusion_Protection
iptables -nL zone_wan_forward | grep -q Intrusion_Protection || iptables -I zone_wan_forward 1 -j Intrusion_Protection
iptables -F Intrusion_Protection

make_rules() {
  local name="$1"
  local action="$2"
  local logging="$3"
  local set_name="$4"

  if [ "$logging" = "1" -a "$action" = "DROP" ]; then
    iptables -A Intrusion_Protection -m set --match-set $set_name src,dst -m limit --limit "10/minute" -j LOG --log-prefix "BLOCK $name "
  fi
  iptables -A Intrusion_Protection -m set --match-set $set_name src -j $action
  iptables -A Intrusion_Protection -m set --match-set $set_name dst -j $action
}

make_ipset() {
  local name="$1"
  local raw_list="$2"
  local action="$3"
  local logging="$4"
  local set_name="$5"
  local tmp_name="tmp_${name}"
  local list="$(mktemp)"
  local list_size hash_size tmp_ipset ip

  [ -z "$action" ] && action="DROP"
  [ -z "$set_name" ] && set_name="IP_${name}"

  egrep "^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}(/[0-9]{1,2})?$" < "$raw_list" | sort -u >"$list"
  list_size=$(cat "$list" | wc -l)

  if [ "$list_size" -gt 0 ]; then
    hash_size=$(( $list_size / 2))

    tmp_ipset="$(mktemp)"
    echo "create $tmp_name hash:net family inet hashsize $hash_size maxelem $list_size" >>"$tmp_ipset"
    while read ip; do
      echo "add $tmp_name $ip" >>"$tmp_ipset"
    done <"$list"
    echo "swap $tmp_name $set_name" >>"$tmp_ipset"
    echo "destroy $tmp_name" >>"$tmp_ipset"

    ipset -q list "$tmp_name" >/dev/null && ipset destroy "$tmp_name"
    ipset -q list "$set_name" >/dev/null || ipset create "$set_name" hash:net family inet
    ipset -! -q restore -f "$tmp_ipset"
    rm -f "$tmp_ipset"

    make_rules "$name" "$action" "$logging" "$set_name"
  fi

  rm -f "$list"

  return 0
}

download() {
  local name="$1"
  local set_name="IP_${name}"
  local tmp_name="tmp_${name}"
  local enabled logging url raw_list headers
  
  config_get enabled "$name" enabled 0
  config_get logging "$name" logging 0
  config_get url "$name" url 

  [ "$enabled" = "0" -o -z "$url" ] && return 1

  if [ $FORCE = n -a  $(ipset -q list $set_name | egrep "^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}(/[0-9]{1,2})?$" | wc -l) -gt 0 ]; then
    make_rules "$name" "DROP" "$logging" "$set_name"
    return 1
  fi

  raw_list="$(mktemp)"
  headers="$(mktemp)"

  echo Downloading $url..
  curl -skLv "$url" >"$raw_list" 2>"$headers"
  if grep -qi 'HTTP/[0-9\.]* 200 OK' "$headers"; then
    make_ipset "$name" "$raw_list" "DROP" "$logging" "$set_name"
  fi

  rm -f "${raw_list}" "${headers}"

  return 0
}

write_ip_to_tmp() {
  local ip="$1"
  local raw_list="$2"
  echo "$ip" >>"$raw_list"
}

manual() {
  local cfg="$1"
  local action="$2"
  local set_name="IP_${cfg}"
  local enabled logging raw_list

  config_get enabled "$cfg" enabled 0

  [ "$enabled" = 0 ] && return 1

  config_get logging "$cfg" logging 0
  raw_list="$(mktemp)"
  config_list_foreach "$cfg" ip write_ip_to_tmp "$raw_list"
  make_ipset "$set_name" "$raw_list" "$action" "$logging" "$set_name"
  rm -f "$raw_list"

  return 0
}

manual WhiteList RETURN
manual BlackList DROP
config_foreach download blocklist

exit 0