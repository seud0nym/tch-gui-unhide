#!/bin/sh

log D "Calculating de-telstra options"

options="--no-defaults --no-service-restart --no-password-remind -y"
alg=0

if [ $(ls -l $BANK2/etc/rc.d/ | grep ^c | grep -E ' S[0-9][0-9](cupsd|dumaos|lte-doctor-logger|mmpbxfwctl|mmpbxd|mmpbxbrcmdect|mud|multiap_agent|multiap_controller|multiap_vendorextensions|ndhttpd|telemetry-daemon|xl2tpd)' | wc -l) -gt 0 ]; then
  options="${options} -M"
fi
if grep -q hotspotd $BASE/.installed; then
  grep hotspotd $BASE/.packages | grep -q '^c' || options="${options} -ka"
fi
if grep -q cwmpd $BASE/.installed; then
  grep cwmpd $BASE/.packages | grep -q '^c' || options="${options} -kc"
fi
if grep -q 'set_voip_iface(x, mobileiface4, mobileiface6)' $BANK2/usr/lib/lua/wansensingfw/failoverhelper.lua; then
  options="${options} -kl"
fi
if grep -qE 'tr143|autoreset|wifi-doctor-agent'  $BASE/.installed; then
  grep -E 'tr143|autoreset|wifi-doctor-agent' $BASE/.packages | grep -q '^c' || options="${options} -km"
fi
if $UCI -q get system.ntp.server | grep -q 'telstra'; then
  options="${options} -kq"
fi
if $UCI show qos | grep -E "$($UCI show qos | grep =reclassify | cut -d= -f1 | xargs | tr " " "|")" | grep -q 'VoWiFi'; then
  options="${options} -kn"
fi
if $UCI -q show mountd | grep -qE 'mountd\.(ext[2-4]|fat|hfsplus(journal)*|ntfs)\.options=.*noexec.*'; then
  options="${options} -kx"
fi
if ! cmp -s /rom/etc/rtfd_persistent_filelist.conf $BANK2/etc/rtfd_persistent_filelist.conf; then
  options="${options} -Fy"
fi
hostname="$($UCI -q get system.@system[0].hostname | sed -e "s/$BACKUP_VARIANT/$DEVICE_VARIANT/")"
if [ -z "$hostname" ]; then
  log W "Failed to determine backup hostname: using $DEVICE_VARIANT"
  hostname="$DEVICE_VARIANT"
fi
options="${options} -h$hostname"; 
if [ $TEST_MODE = y -a "$DEVICE_SERIAL" != "$BACKUP_SERIAL" ]; then
  log D " ++ Adding -TEST to hostname"
  options="${options}-TEST"
fi
options="${options} -c$(config2yn dlnad.config.enabled)"
options="${options} -r$(config2yn printersharing.config.enabled)"
options="${options} -f$(config2yn samba.samba.filesharing)"
options="${options} -e$(config2yn mmpbx.dectemission.state)"
options="${options} -u$(config2yn upnpd.config.enable_upnp)"
options="${options} -l$(config2yn ledfw.syslog.trace)"
if uci -q get supervision; then
  case "$($UCI -q get supervision.global.mode)" in
    BFD) options="${options} -sb";;
    DNS) options="${options} -sd";;
    Disabled) options="${options} -sn";;
  esac
fi
if [ "$BACKUP_VERSION" = "17.2" -o "$BACKUP_VERSION" = "18.1.c" ]; then
  for g in $($UCI show firewall | grep '\.helper' | cut -d\' -f2); do
    key=$($UCI show firewall | grep '\.helper' | grep -i "\b$g" | sed 's/\(.*\)\.helper=.*/\1/')
    alg=$(( $alg + $($UCI -q get $key.enable || echo 1) ))
  done
else # 20.3.c
  wan_zone=$($UCI show firewall | grep @zone | grep -m 1 "wan='1'" | cut -d. -f1-2)
  alg=$($UCI -q get $wan_zone.helper | wc -w)
fi
[ $alg -eq 0 ] && options="${options} -an" || options="${options} -ay"
[ -e /etc/init.d/dumaos ] && options="${options} -g$(config2yn dumaos.tr69.dumaos_enabled)"
if [ -e /etc/config/multiap -a -e $BANK2/etc/config/multiap ]; then
  options="${options} -mu"
fi
[ -e /etc/config/nfc ] && options="${options} -q$(config2yn nfc.@nfc[0].enabled)"
if [ -s $BANK2/etc/opkg/customfeeds.conf -a $(grep -v '^#' $BANK2/etc/opkg/customfeeds.conf 2>/dev/null | wc -l) -gt 0 ]; then
  options="${options} -o"
  if [ "$DEVICE_VERSION" != "17.2" -a "$BACKUP_VERSION" != "17.2" -a "$(grep -om1 'homeware/[0-9][0-9]/' $BANK2/etc/opkg/customfeeds.conf)" != "homeware/$(echo $DEVICE_VERSION | cut -d. -f1)/" ]; then
    options="${options} -O$(grep -om1 'homeware/[0-9][0-9]/' $BANK2/etc/opkg/customfeeds.conf | cut -d/ -f2)"
  fi
fi
if ! $UCI show network | grep -qE '\.Guest.*=interface'; then
  options="${options} -G"
fi
if [ $TEST_MODE = y -a "$($UCI -q get mmpbx.global.enabled)" = "1" ]; then
  log W "TEST MODE: Disabling Telephony..."
  options="${options} -tn"
else
  options="${options} -t$(config2yn mmpbx.global.enabled)"
fi

run_script de-telstra $options

unset alg g hostname key options wan_zone