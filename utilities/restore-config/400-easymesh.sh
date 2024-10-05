#!/bin/sh

if [ -e /etc/config/multiap -a -e $BANK2/etc/config/multiap ]; then
  log I "Restoring MultiAP configuration..."
  agent_mac="$(uci -q get multiap.agent.macaddress)"
  ctrlr_mac="$(uci -q get multiap.controller.macaddress)"
  uci -q revert multiap
  restore_file /etc/config/multiap /etc/config/vendorextensions
  uci_set multiap.agent.macaddress="$agent_mac"
  uci_set multiap.controller.macaddress="$ctrlr_mac"
  uci -q commit multiap
	unset agent_mac ctrlr_mac
fi

if [ -e /etc/config/mesh_broker -a -e $BANK2/etc/config/mesh_broker ]; then
  log I "Restoring EasyMesh configuration..."
  restore_file /etc/config/mesh_broker
fi
