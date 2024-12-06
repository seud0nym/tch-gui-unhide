#!/bin/sh

adblock_supported_version="4.2"

if [ "$1" = "setup" ]; then
  case "$(grep 'CPU architecture' /proc/cpuinfo | cut -d: -f2 | sort -u | xargs)" in
    7) arch="arm_cortex-a9";;
    8) arch="arm_cortex-a53";;
    *) echo ">> Unknown CPU architecture - unable to complete setup"; exit;;
  esac
  grep -q '^extra_command()' /etc/rc.common || sed -e '/^$EXTRA_HELP/d' -e '/^EOF/a\echo -e "$EXTRA_HELP"' -e '/^depends()/i\extra_command() {\
        local cmd="$1"\
        local help="$2"\
        local extra="$(printf "%-8s%s" "${cmd}" "${help}")"\
        EXTRA_HELP="${EXTRA_HELP}\\t${extra}\\n"\
        EXTRA_COMMANDS="${EXTRA_COMMANDS} ${cmd}"\
}\
' -i /etc/rc.common
  echo ">> Determining installed and current versions of required packages..."
  coreutils_current="$(curl -skl https://raw.githubusercontent.com/seud0nym/tch-coreutils/master/repository/${arch}/packages/Packages | grep -m1 '^Version' | cut -d' ' -f2)"
  coreutils_installed="$(opkg list-installed | grep '^coreutils ' | cut -d- -f2 | xargs)"
  coresort_installed="$(opkg list-installed | grep '^coreutils-sort' | cut -d- -f3 | xargs)"
  adblock_installed="$(opkg list-installed | grep '^adblock ' | cut -d- -f2- | xargs)"
  ca_bundle_installed="$(opkg list-installed | grep '^ca-bundle ' | cut -d- -f3- | xargs)"
  ca_certificates_installed="$(opkg list-installed | grep '^ca-certificates ' | cut -d- -f3- | xargs)"
  openwrt_releases="$(curl -skL https://downloads.openwrt.org/releases/ | grep -Eo 'packages-[0-9][0-9]\.[0-9.]+' | sort -u)"
  openwrt_latest="$(echo "$openwrt_releases" | tail -n1)"
  i=1
  while [ -z "$adblock_current_filename" -a $i -lt $(echo "$openwrt_releases" | wc -l) ]; do
    openwrt_adblock="$(echo "$openwrt_releases" | tail -n$i | head -n1)"
    adblock_current_filename="$(curl -skL https://downloads.openwrt.org/releases/${openwrt_adblock}/arm_cortex-a9/packages/Packages | grep -E "^Filename: adblock_$adblock_supported_version.[-r0-9.]+_all.ipk" | cut -d' ' -f2)"
    i=$(( $i + 1 ))
  done
  adblock_current="$(echo $adblock_current_filename | grep -Eo '[0-9][-r0-9.]+')"
  ca_current="$(curl -skL https://downloads.openwrt.org/releases/${openwrt_latest}/arm_cortex-a9/base/Packages | grep 'Filename: ca-' | cut -d' ' -f2)"
  ca_bundle_current="$(echo "$ca_current" | grep bundle | grep -Eo '[0-9][-r0-9]+')"
  ca_certificates_current="$(echo "$ca_current" | grep certificates | grep -Eo '[0-9][-r0-9]+')"
  adblock_restart="n"
  if [ \( -n "$adblock_current" -a "$(echo $adblock_current | cut -d. -f1-2)" != $adblock_supported_version \) ]; then
    echo ">> Latest adblock version is $adblock_current but only version ${adblock_supported_version} is supported - unable to complete setup"
    exit 
  elif [ -z "$adblock_current" -a "$(echo $adblock_installed | cut -d. -f1-2)" != $adblock_supported_version ]; then
    echo ">> Unable to determin latest adblock version and version $adblock_installed installed but only version ${adblock_supported_version} is supported - unable to complete setup"
    exit 
  fi
  if [ -z "$coreutils_installed" ]; then
    if [ "$coreutils_current" != "$coreutils_installed" ]; then
      echo ">> Downloading coreutils v$coreutils_current"
      curl -kL https://raw.githubusercontent.com/seud0nym/tch-coreutils/master/repository/${arch}/packages/coreutils_${coreutils_current}_${arch}.ipk -o /tmp/coreutils_${coreutils_current}_${arch}.ipk || exit $?
      echo ">> Installing coreutils v$coreutils_current"
      opkg --force-overwrite install /tmp/coreutils_${coreutils_current}_${arch}.ipk
      rm /tmp/coreutils_${coreutils_current}_${arch}.ipk
    fi
    if [ "$coreutils_current" != "$coresort_installed" ]; then
      echo ">> Downloading coreutils-sort v$coreutils_current"
      curl -kL https://raw.githubusercontent.com/seud0nym/tch-coreutils/master/repository/${arch}/packages/coreutils-sort_${coreutils_current}_${arch}.ipk -o /tmp/coreutils-sort_${coreutils_current}_${arch}.ipk || exit $?
      echo ">> Installing coreutils-sort v$coreutils_current"
      opkg --force-overwrite install /tmp/coreutils-sort_${coreutils_current}_${arch}.ipk
      rm /tmp/coreutils-sort_${coreutils_current}_${arch}.ipk
    fi
  fi
  if [ -n "$ca_bundle_current" -a "$ca_bundle_current" != "$ca_bundle_installed" ]; then
    echo ">> Downloading ca-bundle v$ca_bundle_current"
    curl -kL https://downloads.openwrt.org/releases/${openwrt_latest}/arm_cortex-a9/base/ca-bundle_${ca_bundle_current}_all.ipk -o /tmp/ca-bundle_${ca_bundle_current}_all.ipk || exit $?
    echo ">> Installing ca-bundle v$ca_bundle_current"
    opkg --force-overwrite install /tmp/ca-bundle_${ca_bundle_current}_all.ipk
    rm /tmp/ca-bundle_${ca_bundle_current}_all.ipk
  fi
  if [ -n "$ca_certificates_current" -a "$ca_certificates_current" != "$ca_certificates_installed" ]; then
    echo ">> Downloading ca-certificates v$ca_certificates_current"
    curl -kL https://downloads.openwrt.org/releases/${openwrt_latest}/arm_cortex-a9/base/ca-certificates_${ca_certificates_current}_all.ipk -o /tmp/ca-certificates_${ca_certificates_current}_all.ipk || exit $?
    echo ">> Installing ca-certificates v$ca_certificates_current"
    opkg --force-overwrite install /tmp/ca-certificates_${ca_certificates_current}_all.ipk
    rm /tmp/ca-certificates_${ca_certificates_current}_all.ipk
  fi
  if [ -n "$adblock_current" -a "$adblock_current" != "$adblock_installed" ]; then
    echo ">> Downloading adblock v$adblock_current"
    curl -kL https://downloads.openwrt.org/releases/${openwrt_latest}/arm_cortex-a9/packages/adblock_${adblock_current}_all.ipk -o /tmp/adblock_${adblock_current}_all.ipk || exit $?
    echo ">> Installing adblock v$adblock_current"
    opkg --force-overwrite --no install /tmp/adblock_${adblock_current}_all.ipk
    rm /tmp/adblock_${adblock_current}_all.ipk
    adblock_restart="y"
  fi
  if [ -z "$(uci -q get adblock.global.adb_fetchparm)" ]; then
    echo ">> Fixing adb_fetchparm global configuration option"
    uci set adblock.global.adb_fetchparm="--cacert /etc/ssl/certs/ca-certificates.crt $(grep -FA2 '"curl"' /usr/bin/adblock.sh | grep -F adb_fetchparm | cut -d} -f2 | tr -d '"')"
    uci commit adblock
    adblock_restart="y"
  fi
  [ $adblock_restart = "y" ] && /etc/init.d/adblock restart
  /etc/init.d/adblock reload &
  if ! grep -q '/etc/init.d/adblock' /etc/crontabs/root; then
    mm=$(awk 'BEGIN{srand();print int(rand()*59);}')
    hh=$(awk 'BEGIN{srand();print int(rand()*4)+1;}')
    echo "$mm $hh * * * /etc/init.d/adblock reload" >> /etc/crontabs/root
    echo ">> adblock daily update has been scheduled to execute at $hh:$(printf '%02d' $mm)am every day"
    /etc/init.d/cron restart
  fi
  uci -q del_list system.@coredump[0].reboot_exceptions='sort'
  uci -q add_list system.@coredump[0].reboot_exceptions='sort'
  uci commit system
  SRV_system=$(( $SRV_system + 2 ))
  echo ">> Setup complete"
  exit
elif [ "$1" = "remove" ]; then
  echo ">> Removing adblock (if installed)"
  opkg list-installed | grep -q '^adblock ' && opkg remove adblock
  if grep -q '/etc/init.d/adblock' /etc/crontabs/root; then
    echo ">> Removing cron tasks"
    sed -e "/adblock/d" -i /etc/crontabs/root
    /etc/init.d/cron restart
  fi
  echo ">> Remove complete"
  exit
elif [ "$(basename $0)" = "tch-gui-unhide-xtra.adblock" -o -z "$FW_BASE" ]; then
  echo "ERROR: This script must NOT be executed, unless you specify either"
  echo "       the 'setup' or 'remove' options!"
  echo "       Place it in the same directory as tch-gui-unhide and it will"
  echo "       be applied automatically when you run tch-gui-unhide."
  exit 1
fi

# The tch-gui-unhide-xtra scripts should output a single line to indicate success or failure
# as the calling script has left a hanging echo -n. Include a leading space for clarity.

if [ -f /etc/init.d/adblock -a -z "$XTRAS_REMOVE" -a "$(opkg list-installed | grep adblock | cut -d' ' -f3 | cut -d. -f1-2)" = $adblock_supported_version ]; then
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
  if [ ! -f /www/docroot/modals/adblck-sources-modal.lp ]
  then
    cat <<"SRC" > /www/docroot/modals/adblck-sources-modal.lp
SRC
    chmod 644 /www/docroot/modals/adblck-sources-modal.lp
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
  if [ ! -f /www/snippets/tabs-adblock.lp ]
  then
    cat <<"TAB" > /www/snippets/tabs-adblock.lp
TAB
    chmod 644 /www/snippets/tabs-adblock.lp
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