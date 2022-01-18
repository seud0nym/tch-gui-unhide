#!/bin/sh

if [ -e /etc/config/multiap -a -e $BANK2/etc/config/multiap ]; then
  log I "Restoring multiap configuration..."
  agent_mac="$(uci -q get multiap.agent.macaddress)"
  ctrlr_mac="$(uci -q get multiap.controller.macaddress)"
  restore_file /etc/config/multiap
  uci -q revert multiap
  uci_set multiap.agent.macaddress="$agent_mac"
  uci_set multiap.controller.macaddress="$ctrlr_mac"
  uci -q commit multiap
  
  log I "Restoring BackHaul SSID state"
  bkup_ap=$($UCI show wireless | grep "='wl1_2'" | cut -d. -f1-2)
  this_ap=$($UCI show wireless | grep "='wl1_2'" | cut -d. -f1-2)
  if [ -n "$bkup_ap" -a -n "$this_ap" ]; then
    uci_set "$this_ap.state='$($UCI -q get $bkup_ap.state)'"
    uci -q commit wireless
  fi
fi
