#!/bin/sh

LAN_IP_ADDR="$(uci get network.lan.ipaddr)"
LAN_IP_MASK="$(uci get network.lan.netmask)"

if uci -q get mobiled.@profile[0] > /dev/null; then
  log I "Restoring mobile operators and profiles..."
  uci_copy mobiled profile
  uci_copy mobiled operator
  uci_set mobiled.@device[0].mcc
  uci_set mobiled.@device[0].mnc
  uci -q commit mobiled
fi

log I "Restoring network configuration..."
for cfg in dhcp network user_friendly_name; do
  uci -q revert $cfg
  restore_file /etc/config/$cfg
done

if [ "$DEVICE_VERSION" = "$BACKUP_VERSION" -o \( "$DEVICE_VERSION_NUMBER" -le 2004000 -a "$BACKUP_VERSION_NUMBER" -le 2004000 \) -o \( "$DEVICE_VERSION_NUMBER" -ge 2004000 -a "$BACKUP_VERSION_NUMBER" -ge 2004000 \) ]; then
  log I "Restoring wireless configuration..."
  uci -q revert wireless
  restore_file /etc/config/wireless
fi

log I "Configuring intercept daemon..."
uci -q revert intercept
uci_set intercept.config.enabled
uci -q commit intercept

if [ $TEST_MODE = y ]; then
  log W "TEST MODE: Disabling Dynamic DNS..."
  uci set ddns.myddns_ipv4.enabled='0'
  uci set ddns.myddns_ipv6.enabled='0'
  uci -q commit ddns

  log W "TEST MODE: Disabling Wi-Fi..."
  if [ -n "$(uci -q get wireless.radio_2G.state)" ]; then
    uci set wireless.radio_2G.state='0'
    uci set wireless.radio_5G.state='0'
  elif [ -n "$(uci -q get wireless.radio0.state)" ]; then
    uci set wireless.radio0.state='0'
    uci set wireless.radio1.state='0'
  fi
  uci -q commit wireless

  if [ -z "$IPADDR" ]; then
    log W "TEST MODE: Retaining current LAN IPv4 Address..."
    uci set network.lan.ipaddr="$LAN_IP_ADDR"
    uci set network.lan.netmask="$LAN_IP_MASK"
    uci -q commit network
  fi
fi

if [ -n "$IPADDR" ]; then
  log I "Setting LAN IPv4 Address to $IPADDR"
  uci set network.lan.ipaddr="$IPADDR"
  uci -q commit network
fi

log I "Configuring WAN Sensing..."
uci -q revert wansensing
uci_set wansensing.global.enable
uci_set wansensing.global.autofailover
uci_set wansensing.global.voiceonfailover
uci_set wansensing.global.autofailovermaxwait
uci -q commit wansensing

log I "Configuring Multicast Snooping..."
uci -q revert mcastsnooping
uci_set mcastsnooping.lan.igmp_snooping
uci_set mcastsnooping.lan.mcast_flooding
uci_set mcastsnooping.lan.mld_snooping
uci_set mcastsnooping.lan.mcast6_flooding
uci -q commit mcastsnooping

if [ -f $BANK2/usr/bin/ip6neigh-setup ]; then
  log I "Restoring ip6neigh..."
  $BANK2/usr/bin/ip6neigh-setup install
  restore_file /etc/config/ip6neigh
fi
