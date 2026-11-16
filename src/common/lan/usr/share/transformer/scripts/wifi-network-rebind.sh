#!/bin/sh
# An SSID moved to another interface (Local Network > Wi-Fi Networks, or any other change of a wifi-iface
# "network" option). netifd reads that option only at a network reload, which the stock commitapply rule for
# wireless does not run. The reload can also leave the SSID's old interface up on the SSID's device
# (on a DJA0231 with 20.3.c, in 16 of 32 test moves of a guest SSID): netifd keeps the link although the
# old interface no longer names the SSID. Such an interface has no device of its own any more, so take it
# down, and note it. An interface noted that way is brought back up as soon as it has a device again: an SSID
# names it, or it lists a device in ifname ("recover" runs only that part, after an interface's ifname changes).
# Runs in the background, so that the rest of the commit and apply carries on.
[ "$1" = "-f" ] || { sh "$0" -f "$1" </dev/null >/dev/null 2>&1 & exit 0; }
MODE=$2
DOWN=/var/run/wifi-network-rebind.down
LOCK=/var/lock/wifi-network-rebind

# Nothing noted, so nothing to bring back up: a recover run then needs neither the lock nor the wait. (An
# interface that a running move notes later is brought back up by that move itself, which reads the committed
# ifname after it takes the interface down.)
[ "$MODE" = "recover" ] && [ ! -f $DOWN ] && exit 0

log() {
  logger -t wifi-network-rebind "$*"
}

# One run at a time: a second move saved while the first is still being handled waits for it.
# lock leaves a holder process that keeps the lock until `lock -u`, and ash skips the EXIT trap when a signal
# kills it, so exit through the trap on a signal. Both traps come first: a run killed while it waits still gets
# the lock (ash runs the trap once lock returns) and then releases it.
trap 'lock -u $LOCK' EXIT
trap 'exit 1' INT TERM HUP
lock $LOCK

# Let the hostapd and network reloads of the same commit finish first: wait (at most 30 s) until none has
# run for 3 s in a row, since the network reload of the same commit can start after this script
i=0
quiet=0
while [ $i -lt 30 ] && [ $quiet -lt 3 ]; do
  if ps w | grep -E '/etc/init\.d/(hostapd|network) reload' | grep -qv grep; then
    quiet=0
  else
    quiet=$((quiet+1))
  fi
  sleep 1
  i=$((i+1))
done

# Enabled access-point SSIDs whose network option names one interface (a station, disabled or multi-network
# SSID is left alone)
vifs() {
  for vif in $(uci -q show wireless | sed -n "s/^wireless\.\([^.=]*\)=wifi-iface$/\1/p"); do
    [ "$(uci -q get wireless.$vif.mode)" = "ap" ] && [ "$(uci -q get wireless.$vif.state)" != "0" ] || continue
    case "$(uci -q get wireless.$vif.network)" in ""|*" "*) continue ;; esac
    echo $vif
  done
}

has_word() {
  for w in $1; do
    [ "$w" = "$2" ] && return 0
  done
  return 1
}

noted() {
  [ -f $DOWN ] && grep -qx "$1" $DOWN
}

changed=0
if [ "$MODE" != "recover" ]; then
  ubus call network reload
  sleep 5
  dump=$(ubus call network.interface dump)
  for vif in $(vifs); do
    [ -d /sys/class/net/$vif ] || continue
    net=$(uci -q get wireless.$vif.network)
    for intf in $(echo "$dump" | jsonfilter -e "@.interface[@.l3_device='$vif'].interface"); do
      [ "$intf" = "$net" ] && continue
      # an alias interface (ifname @<interface>) shares its parent's device: leave it
      case "$(uci -q get network.$intf.ifname)" in @*) continue ;; esac
      if [ "$(uci -q get network.$intf.type)" != "bridge" ] && ! has_word "$(uci -q get network.$intf.ifname)" $vif; then
        log "$vif belongs to $net: taking down $intf, which netifd left on $vif"
        ifdown $intf
        noted $intf || echo $intf >> $DOWN
        changed=1
      else
        log "$vif belongs to $net, but $intf also names it"
      fi
    done
  done
fi

# Bring back up the interfaces this script took down, once they have a device again
if [ -f $DOWN ]; then
  ssid_nets=" $(for vif in $(vifs); do uci -q get wireless.$vif.network; done | tr '\n' ' ') "
  for intf in $(cat $DOWN); do
    if [ "$(uci -q get network.$intf)" != "interface" ]; then
      sed -i "/^$intf\$/d" $DOWN
    elif case "$ssid_nets" in *" $intf "*) true ;; *) false ;; esac || [ -n "$(uci -q get network.$intf.ifname)" ]; then
      if [ "$(uci -q get network.$intf.auto)" != "0" ]; then
        log "$intf has a device again: bringing it up"
        ifup $intf
        changed=1
      fi
      sed -i "/^$intf\$/d" $DOWN
    fi
  done
  [ -s $DOWN ] || rm -f $DOWN
fi
[ $changed = 1 ] && sleep 5
[ "$MODE" = "recover" ] && exit 0

# Last resort: rebind the radio of an SSID that has still not reached its own interface
rebind=""
for vif in $(vifs); do
  [ -d /sys/class/net/$vif ] || continue
  net=$(uci -q get wireless.$vif.network)
  if [ "$(uci -q get network.$net.type)" = "bridge" ]; then
    master=$(readlink /sys/class/net/$vif/master)
    [ "${master##*/}" = "br-$net" ] && continue
  else
    status=$(ubus call network.interface.$net status 2>/dev/null) || continue
    [ "$(echo "$status" | jsonfilter -e '@.autostart')" = "true" ] || continue
    # netifd reports l3_device only while the interface is up; a pending interface (a DHCP client, say) is
    # still coming up on its device
    [ "$(echo "$status" | jsonfilter -e '@.pending')" = "true" ] && continue
    [ "$(echo "$status" | jsonfilter -e '@.up')" = "true" ] && [ "$(echo "$status" | jsonfilter -e '@.l3_device')" = "$vif" ] && continue
  fi
  rebind="$rebind $(uci -q get wireless.$vif.device)"
done
for radio in $(echo $rebind | tr ' ' '\n' | sort -u); do
  log "rebinding the SSIDs of $radio"
  ubus call network.wireless down "{\"device\":\"$radio\"}"
  sleep 2
  ubus call network.wireless up "{\"device\":\"$radio\"}"
done
