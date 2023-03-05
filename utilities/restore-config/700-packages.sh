#!/bin/sh

if [ $DEBUG = n ]; then
  run_script update-ca-certificates
else
  run_script update-ca-certificates -v
fi

log I "Removing any deleted packages"
find $BANK2/usr/lib/opkg/info/ -type c -name '*.control' -exec basename {} .control \; | xargs -rt opkg --force-depends remove

if [ -s $BANK2/etc/opkg/customfeeds.conf -a $(grep -v '^#' $BANK2/etc/opkg/customfeeds.conf 2>/dev/null | wc -l) -gt 0 ]; then
  log I "Installing any additional packages"
  if [ "$DEVICE_VERSION" = "17.2" -a -f $BANK2/usr/lib/opkg/info/adblock.control ]; then
    download https://repository.macoers.com/homeware/18/brcm63xx-tch/VANTW/packages/adblock_3.5.5-4_all.ipk /tmp
    opkg install /tmp/adblock_3.5.5-4_all.ipk
  fi
  if [ "$DEVICE_VERSION" != "17.2" -a "$DEVICE_VERSION" != "18.1.c" ]; then
    find $BANK2/usr/lib/opkg/info/ -type f -name '*.control' ! -name "samba36-server.control" -exec basename {} .control \; | xargs -r opkg --force-overwrite install
  else
    find $BANK2/usr/lib/opkg/info/ -type f -name '*.control' -exec basename {} .control \; | xargs -r opkg --force-overwrite install
  fi
  log I "Restoring any additional packages configuration"
  restore_file /etc/config/adblock /etc/config/minidlna /etc/rsyncd.conf
  if [ "$DEVICE_VERSION" = "17.2" -a -f $BANK2/usr/lib/opkg/info/adblock.control ]; then
    uci set adblock.global.adb_fetchutil='curl'
    uci -q commit adblock
  fi
fi