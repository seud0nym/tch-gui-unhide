#!/bin/sh

if [ "$(basename $0)" = "tch-gui-unhide-xtra.adblock" -o -z "$FW_BASE" ]; then
  echo "ERROR: This script must NOT be executed!"
  echo "       Place it in the same directory as tch-gui-unhide and it will"
  echo "       be applied automatically when you run tch-gui-unhide."
  exit
fi

# The tch-gui-unhide-xtra scripts should output a single line to indicate success or failure
# as the calling script has left a hanging echo -n. Include a leading space for clarity.

if [ -f /etc/init.d/adblock -a -z "$XTRAS_REMOVE" -a "$(opkg list-installed | grep adblock | cut -d' ' -f3 | cut -d. -f1-2)" = "3.5" ]; then
  echo " Adding adblock support..."

  if [ ! -f /usr/share/transformer/commitapply/uci_adblock.ca ]; then
    cat <<"CA" > /usr/share/transformer/commitapply/uci_adblock.ca
CA
    SRV_transformer=$(( $SRV_transformer + 1 ))
  fi

  if [ ! -f /usr/share/transformer/mappings/uci/adblock.map ]; then
    cat <<"UCI" > /usr/share/transformer/mappings/uci/adblock.map
UCI
    chmod 644 /usr/share/transformer/mappings/uci/adblock.map
    SRV_transformer=$(( $SRV_transformer + 1 ))
  fi

  if [ ! -f /usr/share/transformer/mappings/rpc/gui.adblock.map ]; then
    cat <<"RPC" > /usr/share/transformer/mappings/rpc/gui.adblock.map
RPC
    chmod 644 /usr/share/transformer/mappings/rpc/gui.adblock.map
    SRV_transformer=$(( $SRV_transformer + 1 ))
  fi

  if [ ! -f /www/lua/adblock_helper.lua ]
  then
    cat <<"HLP" > /www/lua/adblock_helper.lua
HLP
    chmod 644 /www/lua/adblock_helper.lua
  fi

  # The modals are named adblck-* otherwise some browser extensions block it!
  if [ ! -f /www/docroot/modals/adblck-config-modal.lp ]
  then
    cat <<"CFG" > /www/docroot/modals/adblck-config-modal.lp
CFG
    chmod 644 /www/docroot/modals/adblck-config-modal.lp
  fi
  if [ ! -f /www/docroot/modals/adblck-lists-modal.lp ]
  then
    cat <<"LST" > /www/docroot/modals/adblck-lists-modal.lp
LST
    chmod 644 /www/docroot/modals/adblck-lists-modal.lp
  fi
  if [ ! -f /www/docroot/ajax/adblck-status.lua ]
  then
    cat <<"AJX" > /www/docroot/ajax/adblck-status.lua
AJX
    chmod 644 /www/docroot/ajax/adblck-status.lua
  fi

  if [ ! -f /www/cards/008_adblock.lp ]
  then
    cat <<"CRD" > /www/cards/008_adblock.lp
CRD
    chmod 644 /www/cards/008_adblock.lp
  fi

  grep -q "/etc/init.d/adblock" /etc/crontabs/root
  if [ $? -eq 1 ]; then
    mm=$(awk 'BEGIN{srand();print int(rand()*59);}')
    hh=$(awk 'BEGIN{srand();print int(rand()*3)+2;}')
    echo "$mm $hh * * * /etc/init.d/adblock reload" >> /etc/crontabs/root
    SRV_cron=$(( $SRV_cron + 1 ))
  fi

  grep -q "json_load_file()" /etc/init.d/adblock
  if [ $? -eq 1 ]; then
    sed -e '/boot()/i\json_load_file() { json_load "$(cat $1)"; }' -i /etc/init.d/adblock
  fi

  q=$(grep -n 'query "${1}"' /etc/init.d/adblock | cut -d: -f1)
  p=$(( $q - 1 ))
  if [ "$(grep -n '[ -s "${adb_pidfile}" ] && return 1' /etc/init.d/adblock  | cut -d: -f1 | grep -E "^$p$")" = "$p" ]; then
    sed \
      -e '/query "${1}"/i\        local rtfile_content rtfile="$(uci_get adblock extra adb_rtfile)"' \
      -e '/query "${1}"/i\        rtfile="${rtfile:-"/tmp/adb_runtime.json"}"' \
      -e '/query "${1}"/i\        rtfile_content=$(cat "$rtfile")' \
      -e '/query "${1}"/a\        echo "$rtfile_content" > "$rtfile"' \
      -i /etc/init.d/adblock
  fi
else
  grep -q "/etc/init.d/adblock" /etc/crontabs/root
  if [ $? -eq 0 ]; then
    echo " adblock removed - Cleaning up"
    rm $(find /usr/share/transformer/ /www -type f -name '*adbl*' | grep -v '/www/nd-js/blockadblock.js' | xargs)
    sed -e '/\/etc\/init.d\/adblock/d' -i /etc/crontabs/root
    SRV_transformer=$(( $SRV_transformer + 1 ))
    SRV_cron=$(( $SRV_cron + 1 ))
  else
    echo " SKIPPED because adblock 3.5 not installed"
  fi
fi