#!/bin/sh

if [ ":$1" = ":remove" ]; then
  XTRAS_REMOVE="Y"
elif [ "$(basename $0)" = "tch-gui-unhide-xtra.wireguard" -o -z "$FW_BASE" ]; then
  echo "ERROR: This script must NOT be executed!"
  echo "       Place it in the same directory as tch-gui-unhide and it will"
  echo "       be applied automatically when you run tch-gui-unhide."
  exit
fi

# The tch-gui-unhide-xtra scripts should output a single line to indicate success or failure
# as the calling script has left a hanging echo -n. Include a leading space for clarity.

FW_LAN_ZONE=$(uci show firewall | grep @zone | grep -m 1 "network='lan'" | cut -d. -f1-2)

if [ -f /usr/bin/wireguard-go -a -f /usr/bin/wg-go -a -f /lib/netifd/proto/wireguard.sh -a -z "$XTRAS_REMOVE" ]; then
  echo " Adding wireguard support..."

  cat <<"CA" > /usr/share/transformer/commitapply/uci_wireguard.ca
CA
  cat <<"RPC" > /usr/share/transformer/mappings/rpc/gui.wireguard.map
RPC
  cat <<"UCI" > /usr/share/transformer/mappings/uci/wireguard.map
UCI
  cat <<"SH" > /usr/share/transformer/scripts/reload_wireguard.sh
SH
  cat <<"CRD" > /www/cards/004_WireGuard.lp
CRD
  cat <<"AJX" > /www/docroot/ajax/wireguard-status.lua
AJX
  cat <<"ZIP" > /www/docroot/js/jszip.min.js
ZIP
  cat <<"QR" > /www/docroot/js/qrcode.min.js
QR
  cat <<"MOD" > /www/docroot/modals/wireguard-modal.lp
MOD
  cat <<"HLP" > /www/lua/wireguard_helper.lua
HLP

  chmod 644 /usr/share/transformer/commitapply/uci_wireguard.ca
  chmod 644 /usr/share/transformer/mappings/rpc/gui.wireguard.map
  chmod 644 /usr/share/transformer/mappings/uci/wireguard.map
  chmod 755 /usr/share/transformer/scripts/reload_wireguard.sh
  chmod 644 /www/cards/004_WireGuard.lp
  chmod 644 /www/docroot/ajax/wireguard-status.lua
  chmod 644 /www/docroot/js/jszip.min.js
  chmod 644 /www/docroot/js/qrcode.min.js
  chmod 644 /www/docroot/modals/wireguard-modal.lp
  chmod 644 /www/lua/wireguard_helper.lua

  SRV_transformer=$(( $SRV_transformer + 1 ))

  if [ -z "$(uci -q get network.wg0)" ]; then
    LAN_IPv6="$(uci -q get network.lan.ipv6)"
    OCTET_3="$(printf '%d' 0x$(uci get env.var.local_eth_mac | cut -d: -f6))"
    uci set network.wg0="interface"
    uci set network.wg0.proto="wireguard"
    uci set network.wg0.private_key="$(/usr/bin/wg-go genkey)"
    uci set network.wg0.listen_port="51820"
    uci add_list network.wg0.addresses="172.23.${OCTET_3}.1/24"
    if [ -z "$LAN_IPv6" -o "$LAN_IPv6" = "1" ]; then
      ULA_PREFIX="$(uci -q get network.globals.ula_prefix)"
      if [ -z "$ULA_PREFIX" ]; then
        # https://tools.ietf.org/html/rfc4193 (sort of...)
        MAC=$(uci -q get env.var.local_eth_mac)
        case $(echo $MAC | cut -c2-2) in
          0) SECOND_REV=2;; 2) SECOND_REV=0;; 4) SECOND_REV=6;; 6) SECOND_REV=4;; 8) SECOND_REV=a;; a) SECOND_REV=8;; c) SECOND_REV=e;; e) SECOND_REV=c;; *) SECOND_REV=$OPTARG;;
        esac
        EUI64="$(echo $MAC | cut -c1-1)${SECOND_REV}$(echo $MAC | cut -c3-6)fffe$(echo $MAC | cut -c7-12)"
        GLOBALID=$(echo -ne "$(date +%s | md5sum | cut -c1-16)${EUI64}" | sed "s/../\\x&/g" | sha256sum | cut -c23-32)
        ULA_PREFIX="$(echo fd${GLOBALID} | sed 's|\(....\)\(....\)\(....\)|\1:\2:\3::/48|' | sed -e 's/:0*/:/g')"
        uci set network.globals.ula_prefix="$ULA_PREFIX"
      fi
      uci add_list network.wg0.addresses="$(echo $ULA_PREFIX | cut -d: -f1-3):23$(uci get env.var.local_eth_mac | cut -d: -f6)::1/120"
    fi
    uci set network.wg0.ipv6="$LAN_IPv6"
    uci set network.wg0.enabled="0"
    uci commit network
    SRV_network=$(( $SRV_network + 1 ))
  fi

  if [ -z "$(uci -q get firewall.wg0)" ]; then
    uci set firewall.wg0="rule"
    uci set firewall.wg0.name="Allow-WireGuard"
    uci set firewall.wg0.src="wan"
    uci set firewall.wg0.dest_port="51820"
    uci set firewall.wg0.proto="udp"
    uci set firewall.wg0.target="ACCEPT"
    uci commit firewall
    SRV_firewall=$(( $SRV_firewall + 1 ))
  fi

  if ! uci -q get $FW_LAN_ZONE.network | grep -qE "\bwg0\b"; then
    uci add_list $FW_LAN_ZONE.network="wg0"
    uci commit firewall
    SRV_firewall=$(( $SRV_firewall + 1 ))
  fi

  sed \
    -e '/ActiveEthernetNumberOfEntries/a\    numVPNClient = "rpc.gui.wireguard.server_active_peers",' \
    -e '/local nWiFi/a\  local nVPN = tonumber(devices_data["numVPNClient"]) or 0' \
    -e "/WireGuard/a\  html[#html+1] = '<span class=\"simple-desc\">'\
  html[#html+1] = '<i class=\"icon-cloud status-icon\"></i>'\
  html[#html+1] = format(N('<strong %1\$s>%2\$d WireGuard peer</strong> connected','<strong %1\$s>%2\$d WireGuard peers</strong> connected',nVPN),'class=\"modal-link\" data-toggle=\"modal\" data-remote=\"modals/wireguard-modal.lp\" data-id=\"wireguard-modal\"',nVPN)\
  html[#html+1] = '</span>'\
  " -i /www/lua/devicescard_helper.lua

  for f in /www/docroot/landingpage.lp /www/docroot/loginbasic.lp; do
    sed -e '/^local proxy/a\local function preLoadWG()\
  proxy.get("rpc.gui.wireguard.client_active_peers")\
end\
ngx.timer.at(0, preLoadWG)' -i $f
  done
else
  WG_INSTALLED=0

  if uci -q get $FW_LAN_ZONE.network | grep -qE "\bwg0\b"; then
    WG_INSTALLED=1
    uci -q del_list $FW_LAN_ZONE.network="wg0"
    uci commit firewall
    SRV_firewall=$(( $SRV_firewall + 1 ))
  fi

  if [ "$(uci -q get firewall.wg0)" = "rule" ]; then
    WG_INSTALLED=1
    uci -q delete firewall.wg0=
    uci commit firewall
    SRV_firewall=$(( $SRV_firewall + 1 ))
  fi

  if [ "$(uci -q get network.wg0)" = "interface " ]; then
    WG_INSTALLED=1
    for WG_PEER in $(uci show network | grep "=wireguard_wg0\$" | cut -d= -f1); do
        uci -q delete $WG_PEER
    done
    uci -q delete network.wg0
    uci commit network
    SRV_network=$(( $SRV_network + 1 ))
  fi

  if [ $WG_INSTALLED -eq 0 ]; then
    echo " SKIPPED - openwrt-wireguard-go not found"
  else
    echo " Removed wireguard support"
  fi
fi
