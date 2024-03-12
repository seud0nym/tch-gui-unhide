#!/bin/sh

if [ $DEBUG = n ]; then
  run_script update-ca-certificates
else
  run_script update-ca-certificates -v
fi

log I "Removing any deleted packages"
find $BANK2/usr/lib/opkg/info/ -type c -name '*.control' -exec basename {} .control \; | xargs -rt opkg --force-depends remove

if [ -s $BANK2/etc/opkg/customfeeds.conf -a $(grep -v '^#' $BANK2/etc/opkg/customfeeds.conf 2>/dev/null | wc -l) -gt 0 ]; then
  log I "Checking for additional packages"
  if [ -f $BANK2/usr/lib/opkg/info/adblock.control ]; then
    log I "Installing adblock"
    download https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/tch-gui-unhide-xtra.adblock $SCRIPT_DIR
    run_script $SCRIPT_DIR/tch-gui-unhide-xtra.adblock setup
  fi

  exclude="! -name adblock.control ! -name ca-certificates.control ! -name ca-bundle.control"
  [ "$DEVICE_VERSION" != "17.2" -a "$DEVICE_VERSION" != "18.1.c" ] && exclude="$exclude ! -name samba36-server.control"
  include=$(find $BANK2/usr/lib/opkg/info/ -type f -name '*.control' $exclude -exec basename {} .control \; | xargs)
  if [ -n "$include" ]; then
    log I "Installing additional packages"
    opkg --no-check-certificate update
    opkg --no-check-certificate --force-overwrite install $include
    log I "Restoring additional packages configuration"
    restore_file /etc/config/adblock /etc/config/minidlna /etc/rsyncd.conf
  else
    log I "No additional packages found to install"
  fi
  unset exclude include
fi

if [ -e $BANK2/etc/nginx/OpenSpeedTest-Server.conf ]; then
  log I "Restoring OpenSpeedTest"
  restore_directory /usr/share/nginx
  restore_file /etc/nginx/OpenSpeedTest-Server.conf /etc/nginx/nginx.conf
fi