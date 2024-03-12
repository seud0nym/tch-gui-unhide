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
  uci_set mesh_broker.mesh_broker.enable
  uci_set mesh_broker.mesh_common.agent_enabled
  uci_set mesh_broker.mesh_common.controller_enabled
  source_path=$($UCI show mesh_broker | grep "backhaul='1'" | cut -d. -f1-2)
  log D " >> Backhaul source path: $source_path"
  target_path=$(uci show mesh_broker | grep "backhaul='1'" | cut -d. -f1-2)
  log D " >> Backhaul target path: $target_path"
  if [ -n "$source_path" -a -n "$target_path" ]; then
    $UCI show $source_path | while read -r cfg; do
      value=$(echo "$cfg" | cut -d= -f2-)
      c=$(echo "$cfg" | cut -d= -f1 | cut -d. -f3)
      uci_set "${target_path}.$c=$value"
    done
    unset cfg value c
  fi
  uci -q commit mesh_broker
  unset source_path target_path
fi
