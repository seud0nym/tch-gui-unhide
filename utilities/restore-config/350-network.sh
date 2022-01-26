#!/bin/sh

log I "Restoring network configuration..."
LAN_IP_ADDR="$(uci get network.lan.ipaddr)"
LAN_IP_MASK="$(uci get network.lan.netmask)"

for cfg in dhcp network wireless user_friendly_name; do
  uci -q revert $cfg
  restore_file /etc/config/$cfg
done

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
  uci set wireless.radio_2G.state='0'
  uci set wireless.radio_5G.state='0'
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
