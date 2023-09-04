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
  if [ -f $BANK2/usr/lib/opkg/info/adblock.control ]; then
    download https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/tch-gui-unhide-xtra.adblock $SCRIPT_DIR
    run_script $SCRIPT_DIR/tch-gui-unhide-xtra.adblock setup
  fi

  exclude="! -name adblock.control"
  if [ "$DEVICE_VERSION" != "17.2" -a "$DEVICE_VERSION" != "18.1.c" ]; then
    exclude="$exclude ! -name samba36-server.control"
  fi
  find $BANK2/usr/lib/opkg/info/ -type f -name '*.control' $exclude -exec basename {} .control \; | xargs -r opkg --force-overwrite install
  unset exclude

  log I "Restoring any additional packages configuration"
  restore_file /etc/config/adblock /etc/config/minidlna /etc/rsyncd.conf
fi