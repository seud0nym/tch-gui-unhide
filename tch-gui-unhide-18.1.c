#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 18.1.c - Release 2021.09.27
RELEASE='2021.09.27@17:58'
# Make sure that we are running on Telstra firmware
if [ "$(uci -q get env.var._provisioning_code)" != "Telstra" ]
then
  echo "ERROR! This script is intended for devices with Telstra firmware. Exiting"
  exit 1
fi

# Based on various pages https://hack-technicolor.readthedocs.io/en/stable/

TARGET_VERSION=""
for x in $(basename $0 | tr '-' "$IFS")
do 
  TARGET_VERSION=$x
done
echo "$TARGET_VERSION" | grep -q '\.'
if [ $? -eq 1 ]
then
  echo "ERROR: Unable to determine Target Version!"
  echo "       Did the script get renamed??"
  echo "       Aborting..."
  exit 1
fi
SCRIPT=$(basename $0 | sed "s/-$TARGET_VERSION//")

DFLT_USR=$(uci -q get web.uidefault.defaultuser)
FW_UPGRD=$(uci -q get web.uidefault.upgradefw)
VARIANT=$(uci -q get env.var.variant_friendly_name | sed -e 's/TLS//')
MAC=$(uci -q get env.var.ssid_mac_postfix_r0)
HOSTNAME=$(uci -q get system.@system[0].hostname)
tmpTITLE="$(grep 'title>.*</title' /www/docroot/gateway.lp | cut -d'>' -f2 | cut -d'<' -f1)"
echo $tmpTITLE | grep -q ngx.print
if [ $? -eq 1 ]; then
  TITLE="$tmpTITLE"
else
  TITLE=""
fi
unset tmpTITLE

if [ -f /usr/tch-gui-unhide.theme ]; then
  mv /usr/tch-gui-unhide.theme /etc/tch-gui-unhide.theme
fi
if [ -f /etc/tch-gui-unhide.theme ]; then
  . /etc/tch-gui-unhide.theme
fi
if [ -z "$THEME" ]; then
  KEEPLP=n
  grep -q 'body{background-color:#353c42;' /www/docroot/css/gw.css
  if [ $? -eq 0 ]; then
    THEME=night
  else
    grep -q 'body{background-color:#fff;' /www/docroot/css/gw.css
    if [ $? -eq 0 ]; then
      THEME=light
    else
      THEME=telstra
      grep -q 'body.landingpage #detailed_info_mobile>div:nth-child(5)>form>center>div{background-color' /www/docroot/css/gw-telstra.css
      if [ $? -eq 1 -a -f /www/snippets/tabs-home.lp ]; then
        grep -q 'T"Boost Your Wi-Fi"' /www/snippets/tabs-home.lp
        if [ $? -eq 1 ]; then
          KEEPLP=y
        else
          KEEPLP=n
        fi
      fi
    fi
  fi
  case "$(grep '.smallcard .header{background-color:#......;}' /www/docroot/css/gw.css  | cut -d# -f2 | cut -d\; -f1)" in
    "005c32") COLOR=green;;
    "662e91") COLOR=purple;;
    "f36523") COLOR=orange;;
    "008fd5") COLOR=blue;;
    "272c30") COLOR=monochrome;;
    *) if [ $THEME = light ]; then COLOR=monochrome; else COLOR=blue; fi;;
  esac
  ICONS="$(grep '.card_bg:after{visibility:' /www/docroot/css/gw.css | cut -d: -f3 | cut -d\; -f1)"
fi

grep -q 'rpc.gui.pwd.@' /www/docroot/modals/mmpbx-profile-modal.lp
if [ $? -eq 0 ]; then
  SIP_PWDS=y
else
  SIP_PWDS=n
fi

grep -q '@media screen and (min-width:1200px) and (max-width:1499px){' /www/docroot/css/responsive.css
if [ $? -eq 0 ]; then
  ACROSS=5
else
  ACROSS=4
fi

INSTALLED_RELEASE="$(grep -o -m 1 -E '[0-9][0-9][0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]@[0-9][0-9]:[0-9][0-9]' /usr/share/transformer/mappings/rpc/gui.map 2>/dev/null)"

if [ -z "$INSTALLED_RELEASE" ]; then
  CHART_CARDS='i'
elif [ -e /www/cards/000_Charts.lp ]; then
  CHART_CARDS='s'
elif [ -e /www/cards/000_CPU.lp ]; then
  CHART_CARDS='i'
else
  CHART_CARDS='n'
fi

grep -q 'rpc.gui.UpdateAvailable' /www/docroot/gateway.lp
if [ $? -eq 0 ]; then
  UPDATE_BTN=y
else
  grep -q 'tch-gui-unhide' /www/docroot/gateway.lp
  if [ $? -eq 0 ]; then
    UPDATE_BTN=n
  else
    UPDATE_BTN=y
  fi
fi

FIX_DFLT_USR=n
FIX_FW_UPGRD=n

if [ -z "$INSTALLED_RELEASE" -a -f .defaults.tch-gui-unhide ]; then
  . ./.defaults.tch-gui-unhide
fi

if [ "$(uci -q get dropbear.lan.enable)" = "1" -a "$(uci -q get dropbear.lan.PasswordAuth)" = "on" -a "$(uci -q get dropbear.lan.RootPasswordAuth)" = "on" -a "$(uci -q get dropbear.lan.RootLogin)" = "1" ]; then
  FIX_SSH=n
else
  FIX_SSH=y
fi
if [ "$(uci -q get system.config.export_plaintext)" = "1" -a "$(uci -q get system.config.export_unsigned)" = "1" -a "$(uci -q get system.config.import_plaintext)" = "1" -a "$(uci -q get system.config.import_unsigned)" = "1" ]; then
  FIX_CFG_PORT=n
else
  FIX_CFG_PORT=y
fi
echo "$(uci -q get web.parentalblock.roles)" | grep -q "admin"
if [ $? -eq 0 ]; then
  FIX_PARENT_BLK=n
else
  FIX_PARENT_BLK=y
fi

# Keep count of changes so we know whether to restart services
SRV_cron=0
SRV_dropbear=0
SRV_firewall=0
SRV_nginx=0
SRV_power=0
SRV_system=0
SRV_transformer=0

check_pwr_setting() {
  section="$1"
  option="$2"
  text="$3"
  if [ -z "$(uci -q get power.$section.$option)" ]; then
    pwrctl show | grep "$text" | grep -q DISABLED
    value=$?
    [ "$DEBUG" = "V" ] && echo "001@$(date +%H:%M:%S): - power.$section.$option=$value"
    uci set power.$section.$option="$value"
    SRV_power=$(( $SRV_power + 1 ))
  fi
}

apply_service_changes() {
  echo 001@$(date +%H:%M:%S): Applying service changes if required...
  if [ $SRV_transformer -gt 0 ]; then
    # Need to stop watchdog whilst we restart transformer, because if it does not find /var/run/transformer.pid, it will reboot the system!
    /etc/init.d/watchdog-tch stop >/dev/null 2>&1
    /etc/init.d/transformer restart
    /etc/init.d/watchdog-tch start >/dev/null 2>&1
  fi
  [ $SRV_cron -gt 0 ] && /etc/init.d/cron restart
  [ $SRV_dropbear -gt 0 ] && /etc/init.d/dropbear restart
  [ $SRV_system -gt 0 ] && /etc/init.d/system reload
  [ $SRV_nginx -gt 0 ] && /etc/init.d/nginx restart
  [ $SRV_firewall -gt 0 ] && /etc/init.d/firewall reload 2> /dev/null

  echo 001@$(date +%H:%M:%S): Applying power settings...
  if [ -z "$(uci -q get power.cpu)" ];then
    uci set power.cpu='cpu'
  fi

  if [ -z "$(uci -q get power.cpu.cpuspeed)" ]; then
    case "$(pwrctl show | grep "CPU Speed Divisor" | tr -s " " | cut -d" " -f4)" in
      1) uci set power.cpu.cpuspeed="1";;
      2) uci set power.cpu.cpuspeed="2";;
      4) uci set power.cpu.cpuspeed="4";;
      5) uci set power.cpu.cpuspeed="8";;
      *) uci set power.cpu.cpuspeed="256";;
    esac
    SRV_power=$(( $SRV_power + 1 ))
  fi
  check_pwr_setting "cpu" "wait" "CPU Wait"
  check_pwr_setting "ethernet" "ethapd" "Auto Power"
  check_pwr_setting "ethernet" "eee" "Energy Efficient Ethernet"
  check_pwr_setting "ethernet" "autogreeen" "AutoGreeen"
  check_pwr_setting "ethernet" "dgm" "Deep Green Mode"
  [ $SRV_power -gt 0 ] && uci commit power
}

DEBUG=""
RESTORE=n
WRAPPER=n
YES=n
THEME_ONLY='n'

FILENAME=$(basename $0)

usage() {
  echo "Optional parameters:"
  echo " Control Options:"
  echo " -d y|n           : Enable (y) or Disable (n) Default user (i.e. GUI access without password)"
  echo "                     (Default is current setting)"
  echo " -f y|n           : Enable (y) or Disable (n) firmware upgrade in the web GUI"
  echo "                     (Default is current setting)"
  echo " -p y|n           : Use decrypted text (y) or masked password (n) field for SIP Profile passwords"
  echo "                     (Default is current setting i.e (n) by default)"
  echo " -v y|n           : Enable (y) check for new releases and show 'Update Available' button in GUI, or Disable (n)"
  echo "                     (Default is current setting or (y) for first time installs)"
  echo " Theme Options:"
  echo " -a 4|5           : Set the number of cards across on screen width greater than 1200px"
  echo "                     (Default is current setting, or 4 for first time installs)"
  echo " -c b|o|g|p|r|m|M : Set the theme highlight colour"
  echo "                     b=blue o=orange g=green p=purple r=red m=monochrome M=monochrome (with monochrome charts)"
  echo "                     (Default is current setting, or (m) for light theme or (b) for night theme)"
  echo " -h d|s|n|\"txt\" : Set the browser tabs title (Default is current setting)"
  echo "                     (d)=$VARIANT (s)=$VARIANT-$MAC (n)=$HOSTNAME (\"txt\")=Specified \"txt\""
  echo " -i y|n           : Show (y) or hide (n) the card icons"
  echo "                      (Default is current setting, or (n) for light theme and (y) for night theme)"
  echo " -l y|n           : Keep the Telstra landing page (y) or de-brand the landing page (n)"
  echo "                      (Default is current setting, or (n) if no theme has been applied)"
  echo " -t l|n|t|m       : Set a light (l), night (n), or Telstra-branded Classic (t) or Modern (m) theme"
  echo "                      (Default is current setting, or Telstra Classic if no theme has been applied)"
  echo " -C n|s|i         : Keep or remove chart cards"
  echo "                     n=No Chart Cards will be available"
  echo "                     s=Only the Summary Chart Card will be available"
  echo "                     i=Only the Individual Chart Cards will be available (default)"
  echo " -T               : Apply theme ONLY - bypass all other processing"
  echo " Update Options:"
  echo " -u               : Check for and download any changes to this script (may be newer than latest release version)"
  echo "                      When specifying the -u option, it must be the ONLY parameter specifed."
  if [ $WRAPPER = y ]; then
  echo " -U               : Download the latest release, including utility scripts (will overwrite all existing script versions)."
  echo "                      After download, tch-gui-unhide will be automatically executed."
  fi
  echo " Miscellaneous Options:"
  echo " -r               : Restore changed GUI files to their original state (config changes are NOT restored)"
  echo "                     When specifying the -r option, it must be the ONLY parameter specifed."
  echo " -y               : Bypass confirmation prompt (answers 'y')"
  echo " -V               : Show the release number of this script, the current installed release, and the latest available release on GitHub"
  echo 
  echo "NOTE #1: Theme (-t) does not need to be re-specified when re-running the script: current state will be 'remembered'"
  echo "         between executions (unless you execute with the -r option, which will remove all state information)"
  echo "NOTE #2: Use tch-gui-unhide-cards to set card order and visibility"
  echo
  exit
}

while getopts :a:c:d:f:h:i:l:p:rt:uv:yC:TVW-: option
do
 case "${option}" in
   -) case "${OPTARG}" in 
        debug)  DEBUG="V";; 
        *)      usage;; 
      esac;;
   a) if [ "${OPTARG}" -eq 4 -o "${OPTARG}" -eq 5 ]; then ACROSS=$OPTARG; else echo "ERROR: Cards across must be either 4 or 5"; exit 2; fi;;
   c) case "$(echo ${OPTARG} | sed 's/\(.\)\(.*\)/\1/')" in
        b) COLOR=blue;; 
        g) COLOR=green;; 
        o) COLOR=orange;; 
        p) COLOR=purple;; 
        r) COLOR=red;; 
        m) COLOR=monochrome;;
        M) COLOR=MONOCHROME;;
        *) echo "ERROR: Unknown color option - $OPTARG"; exit 2;;
      esac;;
   d) case "${OPTARG}" in y|Y) FIX_DFLT_USR=y; DFLT_USR='admin';; n|N) FIX_DFLT_USR=y; DFLT_USR='';;  *) echo 'WARNING: -d valid options are y or n'; exit 2;; esac;;
   f) case "${OPTARG}" in y|Y) FIX_FW_UPGRD=y; FW_UPGRD='1';;     n|N) FIX_FW_UPGRD=y; FW_UPGRD='0';; *) echo 'WARNING: -f valid options are y or n'; exit 2;; esac;;
   h) case "${OPTARG}" in
        d) TITLE="$VARIANT";;
        n) TITLE="$HOSTNAME";;
        s) TITLE="$VARIANT-$MAC";;
        *) TITLE="$OPTARG";;
      esac;;
   i) if [ "$(echo ${OPTARG} | tr "YN" "yn" | sed 's/\(.\)\(.*\)/\1/')" = "y" ]; then ICONS=visible; else ICONS=hidden; fi;;
   l) if [ "$(echo ${OPTARG} | tr "YN" "yn" | sed 's/\(.\)\(.*\)/\1/')" = "y" ]; then KEEPLP=y; else KEEPLP=n; fi;;
   p) if [ "$(echo ${OPTARG} | tr "YN" "yn" | sed 's/\(.\)\(.*\)/\1/')" = "y" ]; then SIP_PWDS=y; else SIP_PWDS=n; fi;;
   r) RESTORE=y;;
   t) case "${OPTARG}" in
        g) THEME=night; COLOR=green; KEEPLP=n; echo WARNING: -tg deprecated. Use -tn -cg in future;; 
        o) THEME=night; COLOR=orange; KEEPLP=n; echo WARNING: -to deprecated. Use -tn -co in future;; 
        p) THEME=night; COLOR=purple; KEEPLP=n; echo WARNING: -tp deprecated. Use -tn -cp in future;; 
        l) THEME=light; COLOR=monochrome; KEEPLP=n;;
        m) THEME=telstramodern; COLOR=blue; KEEPLP=y;;
        n) THEME=night; COLOR=blue; KEEPLP=n;;
        *) THEME=telstra; COLOR=blue; KEEPLP=y;;
      esac;;
   u) RESULT=$(curl -m 5 -s -k -L -I https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/$FILENAME | sed 's/\r//')
      if [ $? -ne 0 ]
      then
        echo "002@$(date +%H:%M:%S): GitHub check of $FILENAME failed with an unknown error. Do you have an internet connection?"
        return 5
      else 
        STATUS=$(echo $RESULT | grep '^HTTP' | cut -d' ' -f2)
        LENGTH=$(echo $RESULT | grep '^Content-Length' | cut -d' ' -f2)
        next=''
        for t in $(echo $RESULT | tr " " "$IFS")
        do
          case "$next" in
            s)  STATUS="$t";next='';;
            l)  LENGTH="$t";next='';;
            *)  case "$t" in
                  "HTTP/1.1") next='s';;
                  "Content-Length:") next='l';;
                  *) next='';;
                esac;;
          esac
        done
        case "$STATUS" in
          200)  if [ -f $FILENAME ]
                then
                  SIZE=$(ls -l $FILENAME | tr -s ' ' | cut -d' ' -f5)
                  if [ $SIZE -eq $LENGTH ]
                  then
                  echo "002@$(date +%H:%M:%S): Size of $FILENAME matches GitHub version - No update required"
                  return 0
                  fi
                fi
                curl -k -L https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/$FILENAME > $FILENAME
                if [ $? -eq 0 ]
                then
                  chmod +x $FILENAME
                  echo "002@$(date +%H:%M:%S): Successfully updated $FILENAME."
                  return 0
                else
                  echo "002@$(date +%H:%M:%S): Failed to download updated version of $FILENAME."
                  return 2
                fi;;
          404)  echo "002@$(date +%H:%M:%S): Platform script $FILENAME not found!!!"
                return 4;;
          *)    echo "002@$(date +%H:%M:%S): GitHub check of $FILENAME returned $STATUS"
                return 5;;
        esac
      fi
      exit;;
   v) if [ "$(echo ${OPTARG} | tr "YN" "yn" | sed 's/\(.\)\(.*\)/\1/')" = "y" ]; then UPDATE_BTN=y; else UPDATE_BTN=n; fi;;
   y) YES=y;;
   C) case "${OPTARG}" in
        n|s|i) CHART_CARDS="$OPTARG";; 
        *) echo "ERROR: Unknown chart card option - $OPTARG"; exit 2;;
      esac;;
   T) THEME_ONLY=y;;
   V) LATEST_RELEASE=$(curl -m 1 -q -s -k -L -r0-9 https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/VERSION.txt)
      echo "002@$(date +%H:%M:%S): This Release       $RELEASE"
      if [ -z "$INSTALLED_RELEASE" ]; then
        echo "002@$(date +%H:%M:%S): Installed Release  NOT INSTALLED"
      else
        echo "002@$(date +%H:%M:%S): Installed Release  $INSTALLED_RELEASE"
      fi
      if [ -z "$LATEST_RELEASE" ]; then
        echo "002@$(date +%H:%M:%S): Latest Release     UNKNOWN (No internet access?)"
      else
        echo "002@$(date +%H:%M:%S): Latest Release     $LATEST_RELEASE"
      fi
      exit;;      
   W) WRAPPER=y;;
   ?) usage;;
 esac
done

if [ "$ICONS" = "" ]; then
  if [ "$THEME" = "light" ]; then
    ICONS=hidden
  else
    ICONS=visible
  fi
fi

if [ $THEME_ONLY = n ]; then
restore_www() {
  # File deployed to incorect location by releases prior to 2021.02.06
  if [ -f /usr/share/transformer/mappings/iperf.map ]; then
    rm /usr/share/transformer/mappings/iperf.map
  fi
  # Need to remove existing version or it doesn't get replaced?? 
  if [ -f /usr/sbin/traffichistory.lua ]; then
    if [ "$1" = "V" ]; then
      echo 010@$(date +%H:%M:%S): Removing file /usr/sbin/traffichistory.lua
    fi
    rm /usr/sbin/traffichistory.lua
  fi
  grep -q "/usr/sbin/traffichistory.lua" /etc/crontabs/root
  if [ $? -eq 0 ]; then
    sed -e '/traffichistory.lua/d' -i /etc/crontabs/root
    SRV_cron=$(( $SRV_cron + 1 ))
  fi
  # Add individual files to be restored here
  for t in /etc/init.d/power /usr/lib/lua/web/post_helper.lua
  do
    s=/rom$t
    if [ -f $s ]; then
      cmp -s "$s" "$t"
      if [ $? -ne 0 ]; then
        echo 010@$(date +%H:%M:%S): Restoring file $t
        cp -f -p "$s" "$t"
      fi
    fi
  done
  # Add directories to be restored here
  for d in www/cards www/docroot www/lua www/snippets usr/share/transformer/commitapply usr/share/transformer/mappings/rpc usr/share/transformer/mappings/uci
  do
    echo 010@$(date +%H:%M:%S): Restoring directory /$d
    for s in $(find /rom/$d -type f | grep -v -E \(/rom/www/docroot/help\))
    do
      if [ "$s" = "/rom/www/cards/010_lte.lp" ]
      then
        t="/www/cards/003_lte.lp"
      else
        t=$(echo "$s" | cut -c5-)
      fi
      cmp -s "$s" "$t"
      if [ $? -ne 0 ]; then
        if [ "$1" = "V" ]; then
          echo 014@$(date +%H:%M:%S): Restoring file $t
        fi
        cp -f -p "$s" "$t"
        if [ $d = "usr/share/transformer" ]; then
          SRV_transformer=$(( $SRV_transformer + 1 ))
        fi
      fi
    done

    for f in $(find /$d -type f ! -path '/www/docroot/help*' ! -name '003_lte.lp')
    do
      if [ ! -f "/rom$f" -a -f "$f" ]; then
        if [ "$1" = "V" ]; then
          echo 019@$(date +%H:%M:%S): Removing file $f
        fi
        rm -f "$f"
        if [ $d = "usr/share/transformer" ]; then
          SRV_transformer=$(( $SRV_transformer + 1 ))
        fi
      fi
    done
  done
}

if [ $RESTORE = y ]; then
  restore_www V
  for f in /etc/tch-gui-unhide.ignored_release /etc/tch-gui-unhide.theme
  do
    if [ -f "$f" ]
    then
      echo 019@$(date +%H:%M:%S): Removing $f
      rm "$f"
    fi
  done
  for d in /www/docroot/css/light /www/docroot/css/night /www/docroot/css/telstra /www/docroot/img/light /www/docroot/img/night /www/docroot/css/Telstra
  do
    if [ -d "$d" ]
    then
      echo 019@$(date +%H:%M:%S): Removing empty directory $d
      rmdir "$d"
    fi
  done
  for s in $(uci show web | grep normally_hidden | cut -d. -f2)
  do
    echo 019@$(date +%H:%M:%S): Removing config entry web.$s
    uci -q delete web.$s
    uci -q del_list web.ruleset_main.rules="$s"
    SRV_nginx=$(( $SRV_nginx + 2 ))
  done
  for s in $(uci show web | grep '=card' | cut -d= -f1)
  do
    echo 019@$(date +%H:%M:%S): Removing config entry $s
    uci -q delete $s
    SRV_nginx=$(( $SRV_nginx + 1 ))
  done
  RULES=$(uci get web.ruleset_main.rules)
  for s in $(echo $RULES | tr " " "\n" | grep -v dumaos | sort -u)
  do
    TARGET=$(uci -q get web.$s.target)
    if [ -n "$TARGET" -a ":$TARGET" != ":/" -a ! -f /www/docroot$TARGET -a ! -f /www$TARGET -a ! -f /www/docroot/ajax$TARGET ]; then
      echo 019@$(date +%H:%M:%S): Removing config entry web.$s
      uci -q delete web.$s
      uci -q del_list web.ruleset_main.rules="$s"
      SRV_nginx=$(( $SRV_nginx + 2 ))
    else
      ROLE=$(uci -q get web.$s.roles)
      if [ -n "$ROLE" -a "$ROLE" = "nobody" ]; then
        echo 019@$(date +%H:%M:%S): Resetting admin role on config entry web.$s.roles
        uci -q delete web.$s.roles
        uci add_list web.$s.roles="admin"
        SRV_nginx=$(( $SRV_nginx + 2 ))
      fi
    fi
  done
  uci commit web
  sed -e '/lua_shared_dict *TGU_/d' -i /etc/nginx/nginx.conf
  SRV_nginx=$(( $SRV_nginx + 1 ))
  apply_service_changes
  echo "************************************************************"
  echo "* Restore complete. You should clear your browser cache of *"
  echo "* images and files to make sure you can see the changes.   *"
  echo "* NOTE: No config changes have been restored, except to    *"
  echo "*       remove added entries to display hidden modals.     *"
  echo "************************************************************"
  exit
fi

VERSION=$(uci get version.@version[0].version | cut -d- -f1)
FW_BASE=$(uci get version.@version[0].marketing_version)
if [ "$FW_BASE" = "$TARGET_VERSION" ]
then
  echo 020@$(date +%H:%M:%S): Current version matches target \($TARGET_VERSION\)
else
  echo 020@$(date +%H:%M:%S): WARNING: This script was developed for the $TARGET_VERSION firmware.
  echo 020@$(date +%H:%M:%S): It MAY work on your $VARIANT with $FW_BASE firmware, but it also may not.
  echo 020@$(date +%H:%M:%S): If you still wish to proceed, reply FORCE \(in capitals as shown\).
  read
  if [ "$REPLY" != "FORCE" ]
  then
    echo 020@$(date +%H:%M:%S): Good choice.
    exit
  fi
fi

echo "030@$(date +%H:%M:%S): This script will perform the following actions:"
if [ $FIX_SSH = y ]; then
echo "030@$(date +%H:%M:%S):  - Properly enable SSH access over LAN"
fi
echo "030@$(date +%H:%M:%S):  - Preserve the password files and SSH configuration to prevent root loss on RTFD"
if [ $FIX_CFG_PORT = y ]; then
echo "030@$(date +%H:%M:%S):  - Enable unsigned configuration export/import in the web GUI"
fi
if [ $FIX_FW_UPGRD = y ]; then
if [ "$FW_UPGRD" = "1" ]; then
echo "030@$(date +%H:%M:%S):  - Enable firmware upgrade in the web GUI"
else
echo "030@$(date +%H:%M:%S):  - DISABLE firmware upgrade in the web GUI"
fi
fi
if [ $FIX_DFLT_USR = y ]; then
if [ "$DFLT_USR" = "admin" ]; then
echo "030@$(date +%H:%M:%S):  - ENABLE the default user in the web GUI (i.e. GUI access without password)"
else
echo "030@$(date +%H:%M:%S):  - Disable the default user in the web GUI"
fi
fi
if [ $FIX_PARENT_BLK = y ]; then
echo "030@$(date +%H:%M:%S):  - Ensure admin role can administer the parental block"
fi
if [ -z "$INSTALLED_RELEASE" ]; then
echo "030@$(date +%H:%M:%S):  - Install tch-gui-unhide release $RELEASE"
else
echo "030@$(date +%H:%M:%S):  - Replace tch-gui-unhide release $INSTALLED_RELEASE with $RELEASE"
fi
echo "030@$(date +%H:%M:%S):  - Allow editing of various settings that are not exposed in the stock GUI"
echo "030@$(date +%H:%M:%S):  - Unhide various cards and tabs contained in the stock GUI that are normally hidden"
echo "030@$(date +%H:%M:%S):  - Add new cards and screens, and modified cards and screens from the Ansuel tch-nginx-gui"
echo "030@$(date +%H:%M:%S):  - Pretty up the GUI screens a bit"
echo "030@$(date +%H:%M:%S):  - Apply the $THEME theme with $COLOR highlights and $ICONS card icons"
echo "030@$(date +%H:%M:%S):  - Allow $ACROSS cards across the page on wide screens"
case "$CHART_CARDS" in 
  n)  echo "030@$(date +%H:%M:%S):  - All chart cards will be removed";;
  s)  echo "030@$(date +%H:%M:%S):  - Only the Summary Chart Card will be available";;
  i)  echo "030@$(date +%H:%M:%S):  - The Individual Chart Cards will be available";;
esac
if [ -f /www/docroot/landingpage.lp -a $KEEPLP = n ]; then
echo "030@$(date +%H:%M:%S):  - Theme and de-brand the Telstra Landing Page"
fi
if [ -n "$TITLE" ]; then
echo "030@$(date +%H:%M:%S):  - Set the browser tabs titles to $TITLE"
fi
if [ "$SIP_PWDS" = y ]; then
echo "030@$(date +%H:%M:%S):  - SIP Profile passwords will be decrypted and displayed in text fields rather than password fields"
fi
if [ "$UPDATE_BTN" = y ]; then
echo "030@$(date +%H:%M:%S): New release checking is ENABLED and 'Update Available' will be shown in GUI when new version released"
else
echo "030@$(date +%H:%M:%S): New release checking is DISABLED! 'Update Available' will NOT be shown in GUI when new version released"
fi

echo 030@$(date +%H:%M:%S): If you wish to proceed, enter y otherwise just press [Enter] to stop.
if [ $YES = y ]; then
  REPLY=y
else
  read
fi
if [ "$REPLY" != "y" -a "$REPLY" != "Y" ]; then
  exit
fi

echo 030@$(date +%H:%M:%S): IMPORTANT NOTE - You can restore changed GUI files to their original state by running: $0 -r

# Package repository for Homeware 18 moved 31/03/2021 (Quick fix in case de-telstra not run)
if [ $(grep -c 'www.macoers.com/repository' /etc/opkg/customfeeds.conf) -gt 0 ]; then
  echo 040@$(date +%H:%M:%S): Fixing opkg repository
  sed -e 's|www.macoers.com/repository|repository.macoers.com|' -i /etc/opkg/customfeeds.conf
fi

if [ $FIX_SSH = y ]; then
  echo 040@$(date +%H:%M:%S): Properly enabling SSH access over LAN
  # We need to enable this properly as you can enable/disable via the GUI, and if done
  # without allowing password auth and root login, it can accidently prevent SSH access
  uci set dropbear.lan.enable='1'
  uci set dropbear.lan.PasswordAuth='on'
  uci set dropbear.lan.RootPasswordAuth='on'
  uci set dropbear.lan.RootLogin='1'
  uci commit dropbear
  SRV_dropbear=$(( $SRV_dropbear + 4 ))
fi
  
if [ $FIX_CFG_PORT = y ]; then
  echo 040@$(date +%H:%M:%S): Enabling unsigned configuration export/import in the web GUI
  uci set system.config.export_plaintext='1'
  uci set system.config.export_unsigned='1'
  uci set system.config.import_plaintext='1'
  uci set system.config.import_unsigned='1'
  uci commit system
  SRV_system=$(( $SRV_system + 4 ))
fi

if [ $FIX_FW_UPGRD = y ]; then
  if [ $FW_UPGRD = 1 ]; then
    echo 040@$(date +%H:%M:%S): Enabling firmware upgrade in the web GUI
    uci set web.uidefault.upgradefw='1'
    uci set web.uidefault.upgradefw_role='admin'
    SRV_nginx=$(( $SRV_nginx + 2 ))
  else
    echo 040@$(date +%H:%M:%S): DISABLING firmware upgrade in the web GUI
    uci set web.uidefault.upgradefw='0'
    SRV_nginx=$(( $SRV_nginx + 1 ))
  fi
  uci commit web
fi

if [ $FIX_DFLT_USR = y ]; then
  if [ "$DFLT_USR" = "admin" ]; then
    echo 040@$(date +%H:%M:%S): Enabling the default user in the web GUI
    uci set web.default.default_user='usr_admin'
    uci set web.uidefault.defaultuser='admin'
  else
    echo 040@$(date +%H:%M:%S): Disabling the default user in the web GUI
    uci -q delete web.default.default_user
    uci -q delete web.uidefault.defaultuser
  fi
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 2 ))
fi

if [ $FIX_PARENT_BLK = y ]; then
  echo 040@$(date +%H:%M:%S): Ensuring admin role can administer the parental block
  uci -q del_list web.parentalblock.roles='admin'
  uci add_list web.parentalblock.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 2 ))
fi

grep -q -E "lua_shared_dict *TGU_CPU" /etc/nginx/nginx.conf
if [ $? -eq 1 ]; then
  echo 040@$(date +%H:%M:%S): Creating shared dictionary for CPU calculations
  sed -e '/^http/a\    lua_shared_dict TGU_CPU 10m;' -i /etc/nginx/nginx.conf
  SRV_nginx=$(( $SRV_nginx + 1 ))
fi

grep -q -E "lua_shared_dict *TGU_MbPS" /etc/nginx/nginx.conf
if [ $? -eq 1 ]; then
  echo 040@$(date +%H:%M:%S): Creating shared dictionary for throughput calculations
  sed -e '/^http/a\    lua_shared_dict TGU_MbPS 10m;' -i /etc/nginx/nginx.conf
  SRV_nginx=$(( $SRV_nginx + 1 ))
fi

grep -q -E "lua_shared_dict *TGU_Theme" /etc/nginx/nginx.conf
if [ $? -eq 1 ]; then
  echo 040@$(date +%H:%M:%S): Creating shared dictionary for theme
  sed -e '/^http/a\    lua_shared_dict TGU_Theme 10m;' -i /etc/nginx/nginx.conf
  SRV_nginx=$(( $SRV_nginx + 1 ))
fi

echo 040@$(date +%H:%M:%S): Preserving password files and SSH configuration to prevent root loss on RTFD
for f in /etc/passwd /etc/shadow /etc/config/dropbear /etc/dropbear/* /etc/rc.d/*dropbear
do
  grep -q -E "^$f$" /etc/rtfd_persistent_filelist.conf
  if [ $? -eq 1 ]
  then
    echo "$f">>/etc/rtfd_persistent_filelist.conf
  fi
done

echo 050@$(date +%H:%M:%S): Ensuring card sequence and visibility is up to date
ALLCARDRULES="$(uci show web | grep =card)"
for CARDFILE in $(find /www/cards/ -maxdepth 1 -type f | sort)
do
  CARD="${CARDFILE#*_}"
  CARDRULE="card_$(basename $CARD .lp)"
  MODAL=$(grep createCardHeader $CARDFILE | grep -o "modals/.*\.lp")
  MODALRULE=""
  HIDDEN=$(uci -q get web.${CARDRULE}.hide)
  if [ -z "$MODAL" ]; then
    MODAL=$(grep '\(modalPath\|modal_link\)' $CARDFILE | grep -m 1 -o "modals/.*\.lp")
  fi
  if [ -n "$MODAL" ]; then
    MODALRULE=$(uci show web | grep $MODAL | grep -m 1 -v card_ | cut -d. -f2)
    if [ -n "$MODALRULE" -a -n "$(uci -q get web.$MODALRULE.roles | grep -v -E 'admin|guest')" ]; then
      echo "050@$(date +%H:%M:%S):  - Converting $CARD card visibility from modal-based visibility"
      HIDDEN=1
      uci add_list web.$MODALRULE.roles='admin'
      SRV_nginx=$(( $SRV_nginx + 1 ))
    fi
  elif [ "$CARDRULE" = "card_CPU" ]; then
    MODALRULE='gatewaymodal'
    [ -z "$HIDDEN" ] && HIDDEN=$(uci -q get web.card_gateway.hide)
  elif [ "$CARDRULE" = "card_RAM" ]; then
    MODALRULE='gatewaymodal'
    [ -z "$HIDDEN" ] && HIDDEN=$(uci -q get web.card_gateway.hide)
  elif [ "$CARDRULE" = "card_WANDown" ]; then
    MODALRULE='broadbandmodal'
    [ -z "$HIDDEN" ] && HIDDEN=$(uci -q get web.card_broadband.hide)
  elif [ "$CARDRULE" = "card_WANUp" ]; then
    MODALRULE='broadbandmodal'
    [ -z "$HIDDEN" ] && HIDDEN=$(uci -q get web.card_broadband.hide)
  fi
  if [ -z "$HIDDEN" -o \( "$HIDDEN" != "0" -a "$HIDDEN" != "1" \) ]; then
    HIDDEN=0
  fi
  [ "$DEBUG" = "V" ] && echo "050@$(date +%H:%M:%S): - Card Rule $CARDRULE: card=$(basename $CARDFILE) hide=$HIDDEN modal=$MODALRULE"
  uci set web.${CARDRULE}=card
  uci set web.${CARDRULE}.card="$(basename $CARDFILE)"
  uci set web.${CARDRULE}.hide="$HIDDEN"
  if [ -z "$MODALRULE" ]; then
    uci -q delete web.${CARDRULE}.modal
  else
    uci set web.${CARDRULE}.modal="$MODALRULE"
  fi
  SRV_nginx=$(( $SRV_nginx + 1 ))
done
uci commit web

# Do the restore
restore_www
echo 050@$(date +%H:%M:%S): Pre-update restore completed

echo BLD@$(date +%H:%M:%S): Deploying modified GUI code
echo 'QlpoOTFBWSZTWZVO944BuiH/////VWP///////////////8UXO2cv6DUSxlWsaRJnDdYY32eHvXm
fdwJ6KUoecr6M6UtbQe260du77sopXzvO8qQVjaBdfeZ5SV9A0a1BwfbvTb2OhV73doHsxye2a91
ru6QQPN3NVDtqUXr3b09z7fezth877Pu3x6hVnvty9a7OZavW6Y3dw4H2YHqX0AegbtKIHlF5ABo
BPHpVBSvvvfeD5mPZ19ngVvo6N421Ju5uvez3YvdO60dG4MUPM2l7nvMt33fXw+1Kqr3m6nrJ9NO
AAB6HroAfcFPvvBVV0d33ABdotHTeOe713D6e8ze93kg8Z77vbbY993HbN61u977gPV89a7vOh59
eO2r74ZzRVPdK8u9ttuvXnz1Pre92V997hu7ncx23nYzzPLV3HR2NO9m99jdect8eCUuund1x13Z
3Y043l2NTVzls6sdVqjcA96+uq8+fPffe+fAA1VDqhI0BbdN8ejfe6ABDsAeg1ShqpJrdsIIUChv
YAAAAe09477UofHO9Pvdt74oAAGevttZQF714ZSgFBKl6NfWFAusAKPgPbi7Hq+88H3119Dxqk+9
7zLd33fTV3zvve3U4vrRm8+8neV72Dl2962644EPQu7d3boAu7mgAodsAGlFs33Nwa00xo3TS8tb
MPpqCqHRqIPun3vDpj253Nx1rSCARKKABQBRju3BvfeXzXp8NB0y3cXk77z5KigUeh9t9Wweg3vP
vuD2z3b3Zt54V57u6vd73eL5591z3vu730eTPbdvr1yfc1bn3vdtHWfe4XZCQ0BhesdrADOytC2+
u7d3cyARUpIUCm2thp3YPcM+ceaOCRX2WGrBm2SNC9udvfc+7ve3cnvi773Pnvr7tw81mlA9jdmu
6c1XV1XrqumdbljUrae17PC8hQtvbc5pddvLXelndZsqoYwVWbu462etPeaBzSqN7nbC+3RT7w9P
vobvcKebzD6vND0Sdb763b0fDqIntpKHuzlU2bvgfW70AogoO7ujKgNSFbLBY21fd1x3q956mTN3
Mfe+vvfHT3ffN4+n0Ftt06UENqNUSI7BoSQgAUTvXOnD77vr5EFV2e++G4C+7XfTr73aj0ADQAod
5K7rdQCvfXwcAA6SUUBShRSoA2NR15ve+83z3zWVuD0Dbvte2rm5fX2e59t9Pc3VPWuqnobXR711
33vur7e24066vt99jeZ99uIPI+g22J6fTQbt6+RKjX33yfeD3mNvvePuYPr4vm3p67e7vstq9Ovo
88tvr57x72fWnr0m2bd246AD1bj1Hsb2MnrPW6tTPpueGk9jw+m3ae9and9t19h9tGu8+gvYRAHP
K+G97q85sN7OtJZKBPPZw0ds9J9Dr59l13RxBvLj7xfeXtGo89e99fIrltq2rXmu+7q72y7773K+
fMvPO0927ed7pvvY+tPsZdHxx71iIVA6+VUWuIHbM5ru1dp26MVO7Ar7uXKoDb3B0CkuzToFjuzT
IKFra7agyoAtgYnVa3c3Biu+3LetnoPRmPL3ar2St776+nH3rbHj6u5O3Z0Ct20oa209PHG+7fO9
3Q1l5zR0O2btrveNbcA5WsHrTCQqVIp216r3Z3d3Lc+u9s8033cFmrq6NQlu12NDve7vpy7fd10s
rXbO7ulths0V02cOzugx89NwsREEAoBIoEVUKRfe57waDY+5o777Pele3vd3vbue7rqzVL27ZuUY
zDznt993HbUXz7unxUvXc+ltN3KOx4QB1tHaPNUq0pvbXWrXezvXs93vB7l1Vd7e9t4wO7StuuvO
2O7ubuZbe2uodttuijxkdg0jXdehpXl3WAAOd23cuhGxb7zwAM4x7N9zp9qy90664C0Ni3QGTYM9
cu29O7Ztz3s48umVOnQBp1r7fXq9q2fCChEKa6FFz3OI93evPb729bIBttPrlrbu07dzEHd61UPp
W331XeL2GO8BUgAoOuLVXqhzNTTWQIuLdlbw++deG19Budr7e54UAddbPTJz2m2ZptbO92271WCW
vUeuzc8pL1znQaiL2nti59O9br7vnb7p6O33e7NW+0qqPvYTi1h7RMNaor7euAbQ9A4gFKlRVXdz
brVgSnbeufX33Hvux7sbq2zSgAD6y6Z72KXArztstOdrtDgJEEokCuxnbfB7mfXusFJ9ddUD7q9e
9uhtj3u8wno1O95o9bNQAHdjZzbqa4nsbvZJYpgABuwBRoKO5HenvXtteWzr6u72vtGNsp112q2m
1gtq0CgzdwAOXQdVI+73aoe53DQCdZxEbOtS+7PT1z7rUbu6dauy5cvcsbju773exl7uAynTq7Ff
LU9enOqWrkz6yjyPZbTqlCSSlGjpz5FZXjqvdnPO9NG9bG7ui7crONdzdND172tF1hdzm9T2zaaB
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
DunK62IqQg6fg92e2+mSHHx1zlhdh/BF9XeXzY2HKm71fThmIiPdZ9ugOhYipjFWN5+n8j8sNnz6
Y+rotwRNDf+jdsr346i4bvbXQAtWTuwGkN3Upm0uIUe/3T/l2WfEETlw/XtueXlXbZ5UJ6hIQgSP
d/xCMRKKqh4o9bHofHzH4xvnOBE2Yamyi2RvNl9gpjRE0OvCboDkqIgW+G2pAcokpw20DfGxHPk9
pApWCEki3/YR5dB+O67bNn6AN5HKP5+ZpP3ID8Ev83N+cBA8UqD2KgKUH2IHCRiAaBgljECBEuKd
3ftr6KDlOydNu3Ulr7MaX+8mzaZ23+JaOjWsbz8Ednk73ZpnR5wi79HbGD6iiG+dn2xYqC/jBjB/
lYGUCJV0aHQIDDMKdjHFfFAZGwW3q5ZvsjxNmJ4gnwmz096+09OnCHX477+aTecXInu5KjGayDNn
B0W5QRHFK07GQEstIGgxIj1dcZGGWQtD+n9hw+u/5Pwa8MHuNj+Lca1EsenydRb6uf09YymnzxVX
tB7BRHQCEDUqfOPcBKnJCG7gjogd5E/zdh0/vvsRr/qYf+yDw/2f4/9j/Z/2v+Hwf5vZ+np/2/7M
/4L/1Wf+L/i/5f9v/k//n/yf7fyfD3dP+/3eyz/w//V6NT/8v/N/xf/x/s//D/3T/hP/l/f9H/H/
0f6//Nh6P/jb7P/P/s/F/x/+v0f2fP/z/9f/+cf+3/x/x/Ts/L/1/7P+T/n/8X+7/un/Uv/R/t/6
fD7/83/+9P+/v4f+jPv/z+3/p7f+r/m/2/+X/m/6f+r/2/yf/n/Z/ze//X/7f+X/8P7P9v/Y/s+5
/c/r6f/9/N2/+P/+9PL+v+z/MH/gj/L+PD7fRORCf5PwZ/3o/2VI3PuRoP9rpTfENl2/9Dmi/YQ/
d4yLu/+bTpjvVhABh/upMAw//MgzpwEKVnqBn5TyHGJCnDsQT0ZGuBIM8SNZXXp/zsklnAjgOpRw
i2a+wSWzVw5fYgz1A3WBdzSduNhs7piBbEGMrkVMy/9/oYPTv/w2degeW2blq7LnRyBDhtuOMm3/
isvNTma/6Y6oG//md2zoOZGgaWUqbTdUtjW0jiqqFtqGoM5MYULxyZA5o1HQtxQ0KhsIJr0mAsNM
lyBmuhwmnX/3/Jn82H/X/AhiH+kFQH2fR/2g1wRwaQyEoQD/4Aiv/x0K2Fa07kzgF0+vcNHAzr/6
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
qCpB+jYBBP/+LuSKcKEhKp3vHA==' | base64 -d | bzcat | tar -xf - -C /

# Run any extra feature scripts
for s in $(ls tch-gui-unhide-xtra.* 2>/dev/null)
do
  echo -n 060@$(date +%H:%M:%S): Executing extra feature script for $(echo $s | cut -d. -f2-):
  . ./$s
done

if [ -f /www/docroot/booster.lp ]
then
  echo 065@$(date +%H:%M:%S): Importing Booster screen into advanced view
  sed \
    -e '/lp.include("header.lp")/,/lp.include("message.lp")/d' \
    -e '/local lp = require("web.lp")/i \    ngx.print(ui_helper.createHeader(T"Wi-Fi Boosters", false, false, nil, nil) )' \
    -e 's#<div class="container">#<div class="modal-body update">#' \
    -e 's#<div class="row">#<div>#' \
    -e 's#lp.include("tabs-home.lp")#lp.include("tabs-boosters.lp")#' \
    -e 's#lp.include("footer.lp")#ngx.print(ui_helper.createFooter())#' \
    -e 's#/booster.lp#/modals/wireless-boosters-boosters-modal.lp#' \
    /www/docroot/booster.lp > /www/docroot/modals/wireless-boosters-boosters-modal.lp
fi
if [ -f /www/docroot/boosterstatus.lp ]
then
  echo 065@$(date +%H:%M:%S): Importing Booster status screen into advanced view
  sed \
    -e '/lp.include("header.lp")/,/lp.include("message.lp")/d' \
    -e '/require("web.content_helper")/a local ui_helper = require("web.ui_helper")' \
    -e '/local type_convert/i \    ngx.print(ui_helper.createHeader(T"Wi-Fi Boosters", false, false, nil, nil) )' \
    -e 's#<div class="container">#<div class="modal-body update">#' \
    -e 's#<div class="row">#<div>#' \
    -e 's#networkmap span12#networkmap#' \
    -e 's#lp.include("tabs-home.lp")#lp.include("tabs-boosters.lp")#' \
    -e 's#lp.include("footer.lp")#ngx.print(ui_helper.createFooter())#' \
    /www/docroot/boosterstatus.lp > /www/docroot/modals/wireless-boosters-status-modal.lp
fi
if [ -f /www/docroot/wifidevices.lp ]
then
  echo 065@$(date +%H:%M:%S): Importing Booster Wi-Fi devices screen into advanced view
  sed \
    -e '/lp.include("header.lp")/,/lp.include("message.lp")/d' \
    -e '/local content =/i \    ngx.print(ui_helper.createHeader(T"Wi-Fi Boosters", false, false, nil, nil) )' \
    -e 's#<div class="container">#<div class="modal-body update">#' \
    -e 's#<div class="row">#<div>#' \
    -e 's#lp.include("tabs-home.lp")#lp.include("tabs-boosters.lp")#' \
    -e 's#lp.include("footer.lp")#ngx.print(ui_helper.createFooter())#' \
    /www/docroot/wifidevices.lp > /www/docroot/modals/wireless-boosters-devices-modal.lp
fi

grep -q swshaper /usr/share/transformer/mappings/uci/qos.map
if [ $? -eq 1 ]; then
  echo 066@$(date +%H:%M:%S): Configure transformer for QoS shaping
  sed \
    -e 's/"force_pcp" }/"force_pcp", "swshaper" }/' \
    -e '$a \\' \
    -e '$a \--uci.qos.swshaper' \
    -e '$a \local qos_swshaper = {' \
    -e '$a \  config = config_qos,' \
    -e '$a \  type = "swshaper",' \
    -e '$a \  options = { "enable", "max_bit_rate" }' \
    -e '$a \}' \
    -e '$a \mapper("uci_1to1").registerNamedMultiMap(qos_swshaper)' \
    -i /usr/share/transformer/mappings/uci/qos.map
  SRV_transformer=$(( $SRV_transformer + 1 ))
fi

echo 070@$(date +%H:%M:%S): Creating QoS Reclassify Rules modal
sed \
  -e 's/\(classify[\.-_%]\)/re\1/g' \
  -e 's/Classify/Reclassify/' \
  /www/docroot/modals/qos-classify-modal.lp > /www/docroot/modals/qos-reclassify-modal.lp

echo 075@$(date +%H:%M:%S): Importing Traffic Monitor into Diagnostics
sed \
  -e '/lp.include("header.lp")/,/lp.include("message.lp")/d' \
  -e '/^local attributes/i \    ngx.print(ui_helper.createHeader(T"Diagnostics", false, false, nil, nil) )' \
  -e '/"tabs-services/i <div class="modal-body update">\\' \
  -e '/^<div class="container toplevel">/d' \
  -e '/^<div class="row">/d' \
  -e '/^<div class="span11">/d' \
  -e '/^<fieldset>/,/^\\/d' \
  -e '/^<\/script>/a <\/script>\\' \
  -e '/^<\/script>/a <\/div>\\' \
  -e '/^<\/script>/,/^\\/d' \
  -e 's|tabs-services|tabs-diagnostics|' \
  -e 's|lp.include("footer.lp")|ngx.print(ui_helper.createFooter())|' \
  -e 's|800px;|900px;margin:0 auto;|' \
  -e 's|traffic.lp|modals/diagnostics-traffic-modal.lp|' \
  -e 's|<a href="%s" target="_self">|<a href="#" data-remote="%s">|' \
  -e 's|@wwan.up")\[1\].value|@wwan.up")|' \
  -e 's|if wwan_up|if wwan_up and wwan_up[1].value|' \
  -e 's/count == 5/count < 5/' \
  -e '/ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)/,/^\s*$/d' \
  -e '/proxy.set("rpc.system.lock.free", "0")/i end' \
  -e 's/\(iface_total\[v.iface  .. "_" .. direct\]\)/(tonumber(string.untaint(\1)) or 0)/' \
  /www/docroot/traffic.lp > /www/docroot/modals/diagnostics-traffic-modal.lp

# Initial invocation of transformer code will fail if history directory does not exist
if [ ! -d /root/trafficmon/history ]; then
  echo 075@$(date +%H:%M:%S): Create directory to retain traffic monitor daily history
  mkdir -p /root/trafficmon/history
fi
if [ ! -f /root/trafficmon/history/.wan_rollover ]; then
  echo 075@$(date +%H:%M:%S): Create WAN traffic monitor history configuration file
  echo "1" > /root/trafficmon/history/.wan_rollover
fi
if [ ! -f /root/trafficmon/history/.wwan_rollover ]; then
  echo 075@$(date +%H:%M:%S): Create WWAN traffic monitor history configuration file
  echo "1" > /root/trafficmon/history/.wwan_rollover
fi

grep -q "/usr/sbin/traffichistory.lua" /etc/crontabs/root
if [ $? -eq 1 ]; then
  echo 075@$(date +%H:%M:%S): Create cron job to retain traffic monitor daily history
  echo "9,19,29,39,49,59 * * * * /usr/sbin/traffichistory.lua" >> /etc/crontabs/root
  SRV_cron=$(( $SRV_cron + 1 ))
fi

echo 075@$(date +%H:%M:%S): Only lock files for 600 seconds
sed -e 's/lfs.lock_dir(datadir)/lfs.lock_dir(datadir,600)/' -i /usr/share/transformer/mappings/rpc/system.lock.map
SRV_transformer=$(( $SRV_transformer + 1 ))

echo 080@$(date +%H:%M:%S): Use the nicer green spinner
for lp in $(grep -l -r 'spinner.gif' /www 2>/dev/null | sort | xargs)
do
  sed -e 's/spinner\.gif/spinner-green.gif/' -i $lp
done

echo 080@$(date +%H:%M:%S): Update gateway card and status ajax with hardware temperature monitors
elements=""
for m in $(find /sys/devices/ -name temp1_input)
do
  elements="$elements\"$m\","
done
for f in /www/cards/001_gateway.lp /www/docroot/ajax/gateway-status.lua
do
  sed -e "s|\(^local temp1_input = {\)|\1$elements|" -i $f
done

SERIAL=$(uci get env.var.serial)
echo 080@$(date +%H:%M:%S): Change config export filename from config.bin to $VARIANT-$SERIAL-$VERSION@YYMMDD.bin
echo 080@$(date +%H:%M:%S): Add reset/upgrade warnings
sed \
  -e "s/=config.bin/=$VARIANT-$SERIAL-$VERSION@\" .. os.date(\"%Y%m%d\") .. \".bin/" \
  -e '/local basic =/i local lose_root_warning = { alert = { class = "alert-info", style = "margin-bottom:5px;" }, }' \
  -e '/T"Reset"/i \    html[#html + 1] = ui_helper.createAlertBlock(T"Root access <i>should</i> be preserved when using the <b><i class=\\"icon-bolt\\" style=\\"width:auto;\\">\</i> Reset</b> button. You can use the <i>reset-to-factory-defaults-with-root</i> utility script from the command line to have more control over the factory reset and still retain root access.", lose_root_warning)' \
  -e '/T"Upgrade"/i \          html[#html + 1] = ui_helper.createAlertBlock(T"<b>WARNING!</b> Upgrading firmware using this method will cause loss of root access! Use the <i>reset-to-factory-defaults-with-root</i> utility script with the -f option from the command line to upgrade to the firmware and still retain root access.", lose_root_warning)' \
  -e '/"uci.versioncusto.override.fwversion_override"/a \   unhide_version = "rpc.gui.UnhideVersion",' \
  -e '/"Serial Number"/i \    html[#html + 1] = ui_helper.createLabel(T"tch-gui-unhide Version", content["unhide_version"], basic)' \
  -e '/Global Information/d' \
  -i /www/docroot/modals/gateway-modal.lp

if [ "$UPDATE_BTN" = y ]; then
  echo 080@$(date +%H:%M:%S): Add update available button
  sed \
    -e '/uci.version.version.@version\[0\].timestamp/a\    updatable = "rpc.gui.UpdateAvailable",' \
    -e '/isBridgedMode/i\        if cui.updatable == "1" then' \
    -e "/isBridgedMode/i\          html[#html + 1] = '<div data-toggle=\"modal\" class=\"btn\" data-remote=\"/modals/tch-gui-unhide-update-modal.lp\" data-id=\"tch-gui-unhide-update-modal\">'" \
    -e "/isBridgedMode/i\          html[#html + 1] = '<i class=\"icon-info-sign orange\"></i>&nbsp;'" \
    -e "/isBridgedMode/i\          html[#html + 1] = T\"Update Available\"" \
    -e "/isBridgedMode/i\          html[#html + 1] = '</div>'" \
    -e '/isBridgedMode/i\        end' \
    -e '/local getargs/a\if getargs and getargs.ignore_update then' \
    -e '/local getargs/a\  local proxy = require("datamodel")' \
    -e '/local getargs/a\  proxy.set("rpc.gui.IgnoreCurrentRelease", getargs.ignore_update)' \
    -e '/local getargs/a\end' \
    -i /www/docroot/gateway.lp
else
  echo 080@$(date +%H:%M:%S): Update available button will NOT be shown in GUI
fi

echo 080@$(date +%H:%M:%S): Add auto-refresh management, chart library, and wait indicator when opening modals
sed \
  -e '/<title>/i \    <script src="/js/tch-gui-unhide.js"></script>\\' \
  -e '/<title>/i \    <script src="/js/chart-min.js"></script>\\' \
  -e '/id="waiting"/a \    <script>$(".smallcard .header,.modal-link").click(function(){if ($(this).attr("data-remote")||$(this).find("[data-remote]").length>0){$("#waiting").fadeIn();}});</script>\\' \
  -i /www/docroot/gateway.lp

echo 080@$(date +%H:%M:%S): Add improved debugging when errors cause cards to fail to load
sed \
  -e '/lp.include(v)/i\         local success,msg = xpcall(function()' \
  -e '/lp.include(v)/a\         end, function(err)' \
  -e '/lp.include(v)/a\          ngx.print("<pre>", debug.traceback(err),"</pre>")' \
  -e '/lp.include(v)/a\          ngx.log(ngx.ERR, debug.traceback(err))' \
  -e '/lp.include(v)/a\         end)' \
  -i /www/docroot/gateway.lp

echo 080@$(date +%H:%M:%S): Fix uptime on basic Broadband tab
sed -e 's/days > 1/days > 0/' -i /www/docroot/broadband.lp

echo 085@$(date +%H:%M:%S): Decrease LOW and MEDIUM LED levels
sed -e 's/LOW = "2"/LOW = "1"/' -e 's/MID = "5"/MID = "4"/' -i /www/docroot/modals/gateway-modal.lp

LTE_CARD="$(find /www/cards -type f -name '*lte.lp')"
echo 085@$(date +%H:%M:%S): Fix display bug on Mobile card, hide if no devices found and stop refresh when any modal displayed
sed \
  -e '/<script>/a var divs = $("#mobiletab .content").children("div");if(divs.length>0){var p=$("#mobiletab .content .subinfos");divs.appendTo(p);}\\' \
  -e '/var refreshInterval/a window.intervalIDs.push(refreshInterval);\\' \
  -e '/require("web.lte-utils")/a local result = utils.getContent("rpc.mobiled.DeviceNumberOfEntries")' \
  -e '/require("web.lte-utils")/a local devices = tonumber(result.DeviceNumberOfEntries)' \
  -e '/require("web.lte-utils")/a if devices and devices > 0 then' \
  -e '$ a end' \
  -i $LTE_CARD
echo 085@$(date +%H:%M:%S): Show Mobile Only Mode status
sed \
  -e 's/height: 25/height:20/' \
  -e '/\.card-label/a margin-bottom:0px;\\' \
  -e '/local light/i local proxy = require("datamodel")' \
  -e '/local light/i local primarywanmode = proxy.get("uci.wansensing.global.primarywanmode")' \
  -e "/card_bg/a ');" \
  -e '/card_bg/a if primarywanmode then' \
  -e '/card_bg/a   if primarywanmode[1].value == "MOBILE" then' \
  -e '/card_bg/a     ngx.print(ui_helper.createSimpleLight("1", T("Mobile Only Mode enabled")) );' \
  -e '/card_bg/a   else' \
  -e '/card_bg/a     ngx.print(ui_helper.createSimpleLight("0", T("Mobile Only Mode disabled")) );' \
  -e '/card_bg/a   end' \
  -e '/card_bg/a end' \
  -e "/card_bg/a ngx.print('\\\\" \
  -i $LTE_CARD

echo 085@$(date +%H:%M:%S): Show LTE Network Registration and Service status
sed \
  -e 's/\(, signal_quality\)/\1, registration_status, service_status/' \
  -e '/radio_interface_map/a\			result = utils.getContent(rpc_path .. "network.serving_system.nas_state")' \
  -e '/radio_interface_map/a\			registration_status = utils.nas_state_map[result.nas_state]' \
  -e '/radio_interface_map/a\			result = utils.getContent(rpc_path .. "network.serving_system.service_state")' \
  -e '/radio_interface_map/a\			service_status = utils.service_state_map[result.service_state]' \
  -e '/local data =/a\	registration_status = registration_status or "",' \
  -e '/local data =/a\	service_status = service_status or "",' \
  -i /www/docroot/ajax/mobiletab.lua
sed \
  -e '/"subinfos"/a\				<div style="height: 20px;" data-bind="visible: registrationStatus().length > 0">\\' \
  -e "/\"subinfos\"/a\					<label class=\"card-label\">');  ngx.print( T\"Network\"..\":\" ); ngx.print('</label>\\\\" \
  -e '/"subinfos"/a\					<div class="controls">\\' \
  -e '/"subinfos"/a\						<strong data-bind="text: registrationStatus"></strong>\\' \
  -e '/"subinfos"/a\					</div>\\' \
  -e '/"subinfos"/a\				</div>\\' \
  -e '/"subinfos"/a\				<div style="height: 20px;" data-bind="visible: serviceStatus().length > 0">\\' \
  -e "/\"subinfos\"/a\					<label class=\"card-label\">');  ngx.print( T\"Service\"..\":\" ); ngx.print('</label>\\\\" \
  -e '/"subinfos"/a\					<div class="controls">\\' \
  -e '/"subinfos"/a\						<strong data-bind="text: serviceStatus"></strong>\\' \
  -e '/"subinfos"/a\					</div>\\' \
  -e '/"subinfos"/a\				</div>\\' \
  -e '/this.deviceStatus/a\			this.registrationStatus = ko.observable("");\\' \
  -e '/this.deviceStatus/a\			this.serviceStatus = ko.observable("");\\' \
  -e '/elementCSRFtoken/a\					if(data.registration_status != undefined) {\\' \
  -e '/elementCSRFtoken/a\						self.registrationStatus(data.registration_status);\\' \
  -e '/elementCSRFtoken/a\					}\\' \
  -e '/elementCSRFtoken/a\					if(data.service_status != undefined) {\\' \
  -e '/elementCSRFtoken/a\						self.serviceStatus(data.service_status);\\' \
  -e '/elementCSRFtoken/a\					}\\' \
  -i $LTE_CARD

echo 085@$(date +%H:%M:%S): Add Device Capabilities and LTE Band Selection to Mobile Configuration screen
sed \
  -e 's/getValidateCheckboxSwitch()/validateBoolean/' \
  -e 's/createCheckboxSwitch/createSwitch/' \
  -e '/local function get_session_info_section/i \local function get_device_capabilities_section(page, html)' \
  -e '/local function get_session_info_section/i \	local section = {}' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.arfcn_selection_support ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"ARFCN Selection Support" .. ":", page.device.capabilities.arfcn_selection_support))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.band_selection_support ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"Band Selection Support" .. ":", page.device.capabilities.band_selection_support))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.manual_plmn_selection ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"Manual PLMN Selection" .. ":", page.device.capabilities.manual_plmn_selection))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.strongest_cell_selection ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"Strongest Cell Selection" .. ":", page.device.capabilities.strongest_cell_selection))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.supported_modes ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"Supported Modes" .. ":", page.device.capabilities.supported_modes))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.supported_bands_cdma ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"Supported CDMA Bands" .. ":", page.device.capabilities.supported_bands_cdma))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.supported_bands_gsm ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"Supported GSM Bands" .. ":", page.device.capabilities.supported_bands_gsm))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.supported_bands_lte ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"Supported LTE Bands" .. ":", page.device.capabilities.supported_bands_lte))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.max_data_sessions ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"Max Data Sessions" .. ":", page.device.capabilities.max_data_sessions))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.sms_reading ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"SMS Reading" .. ":", page.device.capabilities.sms_reading))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if page.device.capabilities.sms_sending ~= "" then' \
  -e '/local function get_session_info_section/i \		tinsert(section, ui_helper.createLabel(T"SMS Sending" .. ":", page.device.capabilities.sms_sending))' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \	if utils.Len(section) > 0 then' \
  -e '/local function get_session_info_section/i \		tinsert(html, "<fieldset><legend>" .. T"Device Capabilities" .. "</legend>")' \
  -e '/local function get_session_info_section/i \		tinsert(html, section)' \
  -e '/local function get_session_info_section/i \		tinsert(html, "</fieldset>")' \
  -e '/local function get_session_info_section/i \	end' \
  -e '/local function get_session_info_section/i \end' \
  -e '/local function get_session_info_section/i \\' \
  -e '/local function get_profile_select/i \local function validate_lte_bands(device)' \
  -e '/local function get_profile_select/i \    local choices = setmetatable({}, untaint_mt)' \
  -e '/local function get_profile_select/i \    local b' \
  -e '/local function get_profile_select/i \    for b in string.gmatch(device.capabilities.supported_bands_lte, "%d+") do' \
  -e '/local function get_profile_select/i \      choices[string.untaint(b)] = true' \
  -e '/local function get_profile_select/i \    end' \
  -e '/local function get_profile_select/i \    return function(value, object, key)' \
  -e '/local function get_profile_select/i \      local uv' \
  -e '/local function get_profile_select/i \      local concatvalue = ""' \
  -e '/local function get_profile_select/i \      if not value then' \
  -e '/local function get_profile_select/i \        return nil, T"Invalid input."' \
  -e '/local function get_profile_select/i \      end' \
  -e '/local function get_profile_select/i \      if type(value) == "table" then' \
  -e '/local function get_profile_select/i \        uv = value' \
  -e '/local function get_profile_select/i \      else' \
  -e '/local function get_profile_select/i \        uv = { value }' \
  -e '/local function get_profile_select/i \      end' \
  -e '/local function get_profile_select/i \      for i,v in ipairs(uv) do' \
  -e '/local function get_profile_select/i \        if v ~= "" then' \
  -e '/local function get_profile_select/i \          if concatvalue ~= "" then' \
  -e '/local function get_profile_select/i \            concatvalue = concatvalue.." "' \
  -e '/local function get_profile_select/i \          end' \
  -e '/local function get_profile_select/i \          concatvalue = concatvalue..string.untaint(v)' \
  -e '/local function get_profile_select/i \          if not choices[v] then' \
  -e '/local function get_profile_select/i \            return nil, T"Invalid value."' \
  -e '/local function get_profile_select/i \          end' \
  -e '/local function get_profile_select/i \        end' \
  -e '/local function get_profile_select/i \      end' \
  -e '/local function get_profile_select/i \      object[key] = concatvalue' \
  -e '/local function get_profile_select/i \      return true' \
  -e '/local function get_profile_select/i \    end' \
  -e '/local function get_profile_select/i \end' \
  -e '/local function get_profile_select/i \\' \
  -e '/p.mapParams\["interface_enabled"\]/a \		if utils.radio_tech_map[device.leds.radio] == "LTE" then' \
  -e '/p.mapParams\["interface_enabled"\]/a \			p.mapParams["lte_bands"] = device.uci_path .. "lte_bands"' \
  -e '/p.mapParams\["interface_enabled"\]/a \			p.mapValid["lte_bands"] = validate_lte_bands(device)' \
  -e '/p.mapParams\["interface_enabled"\]/a \		end' \
  -e '/"Access Technology"/a \	 			if utils.radio_tech_map[page.device.leds.radio] == "LTE" then' \
  -e '/"Access Technology"/a \	 				local b, lte_bands, lte_bands_checked = nil, {}, {}' \
  -e '/"Access Technology"/a \	 				for b in string.gmatch(page.device.capabilities.supported_bands_lte, "%d+") do' \
  -e '/"Access Technology"/a \	 					lte_bands[#lte_bands+1] = { string.untaint(b), b }' \
  -e '/"Access Technology"/a \	 				end' \
  -e '/"Access Technology"/a \	 				if not page.content["lte_bands"] or page.content["lte_bands"] == "" then' \
  -e '/"Access Technology"/a \	 					for k,v in ipairs(lte_bands) do' \
  -e '/"Access Technology"/a \	 						lte_bands_checked[k] = string.untaint(v[1])' \
  -e '/"Access Technology"/a \	 					end' \
  -e '/"Access Technology"/a \	 				else' \
  -e '/"Access Technology"/a \	 					for b in string.gmatch(page.content["lte_bands"], "%d+") do' \
  -e '/"Access Technology"/a \	 						lte_bands_checked[#lte_bands_checked+1] = string.untaint(b)' \
  -e '/"Access Technology"/a \	 					end' \
  -e '/"Access Technology"/a \	 				end' \
  -e '/"Access Technology"/a \	 				tinsert(html, ui_helper.createCheckboxGroup(T"LTE Bands", "lte_bands", lte_bands, lte_bands_checked, {checkbox = { class="inline" }}, nil))' \
  -e '/"Access Technology"/a \	 			end' \
  -e '/^\s*get_device_info_section/a get_device_capabilities_section(page, html)' \
  -i /www/docroot/modals/lte-modal.lp

echo 085@$(date +%H:%M:%S): Add new Mobile tabs
sed \
  -e '/{"lte-doctor.lp", T"Diagnostics"},/a \	{"lte-autofailover.lp", T"Auto-Failover"},' \
  -e '/{"lte-doctor.lp", T"Diagnostics"},/a \	{"lte-operators.lp", T"Network Operators"},' \
  -i /www/snippets/tabs-mobiled.lp

echo 085@$(date +%H:%M:%S): Configure transformer for missing WanSensing settings
sed \
  -e 's/\("autofailover\)/\1", \1maxwait/' \
  -e '/timeout/ {n; :a; /timeout/! {N; ba;}; s/\("timeout"\)/\1, "fasttimeout"/;}' \
  -i /usr/share/transformer/mappings/uci/wansensing.map
SRV_transformer=$(( $SRV_transformer + 1 ))

if [ -z "$TITLE" ]
then
  echo 090@$(date +%H:%M:%S): Leaving browser tabs title unchanged
else
  echo 090@$(date +%H:%M:%S): Change the title in browser tabs to $TITLE
  for f in /www/docroot/gateway.lp /www/lua/hni_helper.lua /www/snippets/header.lp
  do
    if [ -f "$f" ]; then
      [ "$DEBUG" = "V" ] && echo "090@$(date +%H:%M:%S):  - $f"
      sed -e "s,title>.*</title,title>$TITLE</title," -i $f
    fi
  done
  sed -e "s,<title>');  ngx.print( T\"Change password\" ); ngx.print('</title>,<title>$TITLE - Change Password</title>," -i /www/docroot/password.lp
fi

echo "090@$(date +%H:%M:%S): Change 'Gateway' to '$VARIANT'"
sed -e "s/Gateway/$VARIANT/g" -i /www/cards/001_gateway.lp
sed -e "s/Gateway/$VARIANT/g" -i /www/cards/003_internet.lp
sed -e "s/Device/$VARIANT/g" -i /www/docroot/modals/ethernet-modal.lp

echo 091@$(date +%H:%M:%S): Fixing titles
sed -e "s/\(Modem\|Gateway\)/$VARIANT/g" -i /www/lua/telstra_helper.lua
sed -e '/local telstra_helper/,/local symbolv1/d' -e 's/symbolv1/"LAN"/' -i /www/cards/005_LAN.lp

echo 095@$(date +%H:%M:%S): Fix bug in relay setup card 
sed \
  -e '/getExactContent/a \ ' \
  -e '/getExactContent/a local server_addr = proxy.get\("uci.dhcp.relay.@relay.server_addr"\)' \
  -e 's/\(if proxy.get."uci.dhcp.relay.@relay.server_addr".\)\(.*\)\( then\)/if not server_addr or \(server_addr\2\)\3/' \
  -e 's/\r//' \
  -i /www/cards/018_relaysetup.lp

echo 095@$(date +%H:%M:%S): Only show xDSL Config card if WAN interface is DSL 
sed \
 -e '/local content =/a \      wan_ifname = "uci.network.interface.@wan.ifname",' \
 -e '/if session:hasAccess/i \local wan_ifname = content["wan_ifname"]' \
 -e 's/if session:hasAccess/if wan_ifname and (wan_ifname == "ptm0" or wan_ifname == "atmwan") and session:hasAccess/' \
 -i /www/cards/093_xdsl.lp

echo 095@$(date +%H:%M:%S): Add forceprefix to transformer mapping for network interface
sed \
  -e 's/"reqprefix", "noslaaconly"/"reqprefix", "forceprefix", "noslaaconly"/' \
  -i /usr/share/transformer/mappings/uci/network.map
SRV_transformer=$(( $SRV_transformer + 1 ))

echo 095@$(date +%H:%M:%S): Make Telstra bridged mode compatible with Ansuel network cards and modals
sed \
  -e "/uci.network.interface.@lan.ifname/i \        [\"uci.network.config.wan_mode\"] = 'bridge'," \
  -i /www/lua/bridgedmode_helper.lua

echo 095@$(date +%H:%M:%S): Check for fake WAN in bridged mode \(nqe bind errors fix\)
sed \
  -e 's/if (proxy.get("uci.network.interface.@wan.")) then/local ifname = proxy.get("uci.network.interface.@wan.ifname")\
    if ifname and ifname[1].value ~= "lo" then/' \
  -i /www/lua/bridgedmode_helper.lua

echo 095@$(date +%H:%M:%S): Allow reset to Routed Mode without RTFD
sed \
  -e '/configBridgedMode/i \function M.configRoutedMode()' \
  -e '/configBridgedMode/i \  local success = proxy.set({' \
  -e '/configBridgedMode/i \    ["uci.wansensing.global.enable"] = "1",' \
  -e '/configBridgedMode/i \    ["uci.network.interface.@lan.ifname"] = "eth0 eth1 eth2 eth3",' \
  -e '/configBridgedMode/i \    ["uci.dhcp.dhcp.@lan.ignore"] = "0",' \
  -e '/configBridgedMode/i \    ["uci.network.config.wan_mode"] = "dhcp",' \
  -e '/configBridgedMode/i \    ["uci.network.interface.@lan.gateway"] = "",' \
  -e '/configBridgedMode/i \  })' \
  -e '/configBridgedMode/i \  if success then' \
  -e '/configBridgedMode/i \    local landns = proxy.getPN("uci.network.interface.@lan.dns.", true)' \
  -e '/configBridgedMode/i \    if landns then' \
  -e '/configBridgedMode/i \      local dns' \
  -e '/configBridgedMode/i \      for _,dns in pairs(landns) do' \
  -e '/configBridgedMode/i \        proxy.del(dns.path)' \
  -e '/configBridgedMode/i \      end' \
  -e '/configBridgedMode/i \    end' \
  -e '/configBridgedMode/i \    local ifnames = {' \
  -e '/configBridgedMode/i \      ppp = {' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ppp.proto"] = "pppoe",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ppp.metric"] = "10",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ppp.username"] = "newdsluser@bigpond.com",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ppp.password"] = "new2dsl",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ppp.keepalive"] = "4,20",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ppp.iface6rd"] = "0",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ppp.graceful_restart"] = "1",' \
  -e '/configBridgedMode/i \      },' \
  -e '/configBridgedMode/i \      ipoe = {' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ipoe.proto"] = "dhcp",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ipoe.metric"] = "1",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ipoe.reqopts"] = "1 3 6 43 51 58 59",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ipoe.iface6rd"] = "0",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@ipoe.vendorid"] = "technicolor",' \
  -e '/configBridgedMode/i \      },' \
  -e '/configBridgedMode/i \      wan = {' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wan.auto"] = "0",' \
  -e '/configBridgedMode/i \      },' \
  -e '/configBridgedMode/i \      wan6 = {' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wan6.auto"] = "0",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wan6.proto"] = "dhcpv6",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wan6.reqopts"] = "23 17",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wan6.reqaddress"] = "force",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wan6.noslaaconly"] = "1",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wan6.iface_464xlat"] = "0",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wan6.forceprefix"] = "1",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wan6.soltimeout"] = "240",' \
  -e '/configBridgedMode/i \      },' \
  -e '/configBridgedMode/i \      wwan = {' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@wwan.enabled"] = "0"' \
  -e '/configBridgedMode/i \      },' \
  -e '/configBridgedMode/i \      Guest1 = {' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1.proto"] = "static",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1.ip6assign"] = "64",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1.ip6hint"] = "1",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1.netmask"] = "255.255.255.128",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1.ipaddr"] = "192.168.2.126",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1.ifname"] = "wl0_1",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1.force_link"] = "0",' \
  -e '/configBridgedMode/i \      },' \
  -e '/configBridgedMode/i \      Guest1_5GHz = {' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1_5GHz.proto"] = "static",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1_5GHz.ip6assign"] = "64",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1_5GHz.ip6hint"] = "2",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1_5GHz.netmask"] = "255.255.255.128",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1_5GHz.ipaddr"] = "192.168.2.254",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1_5GHz.ifname"] = "wl1_1",' \
  -e '/configBridgedMode/i \        ["uci.network.interface.@Guest1_5GHz.force_link"] = "0",' \
  -e '/configBridgedMode/i \      },' \
  -e '/configBridgedMode/i \    }' \
  -e '/configBridgedMode/i \    local ifname,config' \
  -e '/configBridgedMode/i \    for ifname,config in pairs(ifnames) do' \
  -e '/configBridgedMode/i \      if success and not proxy.get("uci.network.interface.@" .. ifname .. ".") then' \
  -e '/configBridgedMode/i \        proxy.add("uci.network.interface.", ifname)' \
  -e '/configBridgedMode/i \      end' \
  -e '/configBridgedMode/i \      success = success and proxy.set(config)' \
  -e '/configBridgedMode/i \    end' \
  -e '/configBridgedMode/i \    success = success and proxy.apply()' \
  -e '/configBridgedMode/i \  end' \
  -e '/configBridgedMode/i \  return success' \
  -e '/configBridgedMode/i \end' \
  -i /www/lua/bridgedmode_helper.lua

echo 100@$(date +%H:%M:%S): Fix card visibility check
sed \
  -e '/local access/,/end/d' \
  -e 's/access and/session:hasAccess(card.modal) or/' \
  -i /www/lua/cards.lua

echo 100@$(date +%H:%M:%S): Removing obsolete help links
for m in $(grep -l 'local help_link = ' /www/docroot/modals/*)
do
  sed -e 's/\(local help_link = \)\(.*\)/\1nil/' -i "$m"
done

echo 100@$(date +%H:%M:%S): Enable cards in Bridge Mode
# https://www.crc.id.au/hacking-the-technicolor-tg799vac-and-unlocking-features/#mozTocId685948
sed -e '/if info.bridged then/,/end/d' -i /www/lua/cards_limiter.lua

echo 102@$(date +%H:%M:%S): Show individual helper status on NAT Helpers card
sed \
  -e '/local enabled/,/^  end/d' \
  -e '/convertResultToObject/a \  local htmlLeft = {}' \
  -e '/convertResultToObject/a \  local htmlRight = {}' \
  -e '/convertResultToObject/a \  local attributes = {' \
  -e '/convertResultToObject/a \    span = { style = "display:inline-block;font-size:smaller;letter-spacing:-1px;vertical-align:unset;" },' \
  -e '/convertResultToObject/a \  }' \
  -e '/convertResultToObject/a \  for _, v in ipairs(helper_uci_content) do' \
  -e '/convertResultToObject/a \      if v.intf ~= "loopback" then' \
  -e '/convertResultToObject/a \          local state = v.enable' \
  -e '/convertResultToObject/a \          local text' \
  -e '/convertResultToObject/a \          if state ~= "0" then' \
  -e '/convertResultToObject/a \              text =" enabled<br>"' \
  -e '/convertResultToObject/a \              state = "1"' \
  -e '/convertResultToObject/a \          else' \
  -e '/convertResultToObject/a \              text = " disabled<br>"' \
  -e '/convertResultToObject/a \          end' \
  -e '/convertResultToObject/a \          if #htmlRight >= #htmlLeft then' \
  -e '/convertResultToObject/a \              htmlLeft[#htmlLeft+1] = ui_helper.createSimpleLight(state, string.upper(v.helper), attributes) .. text' \
  -e '/convertResultToObject/a \          else' \
  -e '/convertResultToObject/a \              htmlRight[#htmlRight+1] = ui_helper.createSimpleLight(state, string.upper(v.helper), attributes) .. text' \
  -e '/convertResultToObject/a \          end' \
  -e '/convertResultToObject/a \      end' \
  -e '/convertResultToObject/a \  end' \
  -e '/divtable/,/div>/d' \
  -e '/card_bg/a \            <div style="display:flex;flex-direction:row;">\\' \
  -e '/card_bg/a \              <div style="width:50%">\\' \
  -e "/card_bg/a \                ');" \
  -e '/card_bg/a \                ngx.print(htmlLeft);' \
  -e "/card_bg/a \                ngx.print('\\\\" \
  -e '/card_bg/a \              </div>\\' \
  -e '/card_bg/a \              <div style="width:50%">\\' \
  -e "/card_bg/a \                ');" \
  -e '/card_bg/a \                ngx.print(htmlRight);' \
  -e "/card_bg/a \                ngx.print('\\\\" \
  -e '/card_bg/a \              </div>\\' \
  -e '/card_bg/a \            </div>\\' \
  -i /www/cards/092_natalghelper.lp

echo 102@$(date +%H:%M:%S): Fix NAT ALG modal errors and include in Firewall tabs if card hidden
sed \
  -e '/^local ui_helper/i \local proxy = require("datamodel")' \
  -e 's/T"Enable"/T"Enabled"/' \
  -e 's/, readonly="true"/ /' \
  -e 's/unique/readonly/' \
  -e 's/string.upper/string.lower/' \
  -e '/attr = { input = { class="span1"/i \    readonly = true,' \
  -e '/^local hlp_attributes/i \table.insert(hlp_columns, table.remove(hlp_columns, 1))' \
  -e '/return true/i \  if object["enable"] == "" then' \
  -e '/return true/i \    object["enable"] = "1"' \
  -e '/return true/i \  end' \
  -e '/return true/i \  object["helper"] = string.lower(object["helper"])' \
  -e '/--Look for the enable set to nothing/,/^end/d' \
  -e '/^local UI_helper/i \local proxy = require("datamodel")' \
  -e "/modal-body/a \  ');" \
  -e '/modal-body/a \  local card_hidden = proxy.get("uci.web.card.@card_natalghelper.hide")' \
  -e '/modal-body/a \  if card_hidden and card_hidden[1] and card_hidden[1].value == "1" then' \
  -e '/modal-body/a \    local lp = require("web.lp")' \
  -e '/modal-body/a \    lp.setpath("/www/snippets/")' \
  -e '/modal-body/a \    lp.include("tabs-firewall.lp")' \
  -e '/modal-body/a \  end' \
  -e "/modal-body/a \  ngx.print('\\\\" \
  -e "/createTable/i \        ngx.print('<legend>');  ngx.print( T'NAT Helpers (ALG\\\\'s)' ); ngx.print('</legend>');" \
  -e "s/\"NAT Helpers (ALG's)\"/\"Firewall\"/" \
  -e 's/Dest Port/Destination Port/' \
  -i /www/docroot/modals/nat-alg-helper-modal.lp

echo 104@$(date +%H:%M:%S): Fix incorrect error detection in DDNS update when IPv6 address contains 401 or 500 
sed \
  -e '/local logFile/a\        local adrFile = "/var/log/ddns/" .. service .. ".ip"' \
  -e 's^"cat "^"sed -e \\"s/$(cat " .. adrFile ..")//\\" "^' \
  -i /usr/share/transformer/mappings/rpc/ddns.map

if [ -f ipv4-DNS-Servers ]
then
  echo 105@$(date +%H:%M:%S): Adding custom IPv4 DNS Servers
  sed -e 's/\r//g' ipv4-DNS-Servers | sort -r | while read -r host ip
  do 
    if [ -n "$ip" ]
    then 
      sed -e "/127.0.0.1/a\    {\"$ip\", T\"$host ($ip)\"}," -i /www/docroot/modals/ethernet-modal.lp
    fi
  done
fi

if [ -f ipv6-DNS-Servers ]
then
  echo 105@$(date +%H:%M:%S): Adding custom IPv6 DNS Servers
  sed -e 's/\r//g' ipv6-DNS-Servers | sort | while read -r host ip
  do 
    if [ -n "$ip" ]
    then 
      ipv6=$(echo $ip  | tr ':' '-')
      sed -e "/2001-4860-4860--8888/i\    {\"$ipv6\", T\"$host ($ip)\"}," -i /www/docroot/modals/ethernet-modal.lp
    fi
  done
fi

echo 105@$(date +%H:%M:%S): Allow custom DNS entries
sed \
  -e 's/"addnhosts", "bogusnxdomain"/"addnhosts", "address", "bogusnxdomain"/' \
  -i /usr/share/transformer/mappings/uci/dhcp.map
  SRV_transformer=$(( $SRV_transformer + 1 ))

if [ "$VERSION" != "18.1.c.0462" ]
then
  if [ "$(uci get dumaos.tr69.dumaos_enabled)" = '1' ]
  then
    echo 105@$(date +%H:%M:%S): Add DumaOS button
    sed \
      -e "/id=\"basicview\"/i\            html[#html + 1] = '<a href=\"desktop/index.html#com.netdumasoftware.desktop\" class=\"btn\" id=\"dumaos\">'" \
      -e "/id=\"basicview\"/i\            html[#html + 1] = T\"DumaOS\"" \
      -e "/id=\"basicview\"/i\            html[#html + 1] = '</a>'" \
      -e '/<div class="header span12">/a <script>if(window.self !== window.top){$("div.header").hide();}</script>\\' \
      -i /www/docroot/gateway.lp
  fi
fi

echo 105@$(date +%H:%M:%S): Fix bug in relay setup card 
sed \
 -e '/local function getRelayBackUpValues/i local server_addr = proxy.get\("uci.dhcp.relay.@relay.server_addr"\)' \
 -e '/local function getRelayBackUpValues/i \ ' \
 -e 's/\(if proxy.get."uci.dhcp.relay.@relay.server_addr".\[1\].value\)\( ==.*\)\( then\)/if not server_addr or \(server_addr\[1\].value\2\)\3/' \
 -e 's/\(if proxy.get."uci.dhcp.relay.@relay.server_addr".\[1\].value ~= ""\)\(.*\)\( then\)/if server_addr and server_addr\[1\].value ~= ""\2\3/' \
 -i /www/docroot/modals/relay-modal.lp

echo 107@$(date +%H:%M:%S): Adding transformer support for IPv6 ULA Prefix
sed \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \ ' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \-- uci.network.globals.' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \local network_globals = {' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \    config = config_network,' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \    section = "globals",' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \    type = "globals",' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \    options = {' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \        "ula_prefix",' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \    }' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \}' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \ ' \
 -e '/^uci_1to1.registerSimpleMap(network_config)/a \uci_1to1.registerSimpleMap(network_globals)' \
 -i /usr/share/transformer/mappings/uci/network.map

echo 107@$(date +%H:%M:%S): Adding IPv6 Prefix Size and ULA Prefix
sed \
 -e '/^local gVIES/a \local vIAS6 = gOV(post_helper.validateIPAndSubnet(6))' \
 -e '/^local function validateIPv6/i \local function validateULAPrefix(value, object, key)' \
 -e '/^local function validateIPv6/i \  local valid, msg = vIAS6(value, object, key)' \
 -e '/^local function validateIPv6/i \  if valid and value ~= "" and (string.sub(string.lower(value),1,2) ~= "fd" or string.sub(value,-3,-1) ~= "/48") then' \
 -e '/^local function validateIPv6/i \    return nil, "ULA Prefix must be within the prefix fd::/7, with a range of /48"' \
 -e '/^local function validateIPv6/i \  end' \
 -e '/^local function validateIPv6/i \  return valid, msg' \
 -e '/^local function validateIPv6/i \end' \
 -e '/slaacState = "uci/i \  ip6assign = "uci.network.interface.@" .. curintf .. ".ip6assign",' \
 -e '/slaacState = "uci/a \  ula_prefix = "uci.network.globals.ula_prefix",' \
 -e '/local number_attr/i \              local ula_attr = {' \
 -e '/local number_attr/i \                controls = {' \
 -e '/local number_attr/i \                  style = "width:220px",' \
 -e '/local number_attr/i \                },' \
 -e '/local number_attr/i \                group = {' \
 -e '/local number_attr/i \                  class = "monitor-localIPv6 monitor-1 monitor-hidden-localIPv6",' \
 -e '/local number_attr/i \                },' \
 -e '/local number_attr/i \                input = {' \
 -e '/local number_attr/i \                  style = "width:180px",' \
 -e '/local number_attr/i \                }' \
 -e '/local number_attr/i \              }' \
 -e '/"IPv6 State"/a \              if curintf == "lan" then' \
 -e "/\"IPv6 State\"/a \                ngx.print(ui_helper.createInputText(T\"IPv6 ULA Prefix<span class='icon-question-sign' title='IPv6 equivalent of IPv4 private addresses. Must start with fd followed by 40 random bits and a /48 range (e.g. fd12:3456:789a::/48)'></span>\", \"ula_prefix\", content[\"ula_prefix\"], ula_attr, helpmsg[\"ula_prefix\"]))" \
 -e '/"IPv6 State"/a \              end' \
 -e '/"IPv6 State"/a \              local ip6prefix = proxy.get("rpc.network.interface.@wan6.ip6prefix")' \
 -e '/"IPv6 State"/a \              if ip6prefix and ip6prefix[1].value ~= "" then' \
 -e "/\"IPv6 State\"/a \                ngx.print(ui_helper.createInputText(T\"IPv6 Prefix Size<span class='icon-question-sign' title='Delegate a prefix of the given length to this interface'></span>\", \"ip6assign\", content[\"ip6assign\"], number_attr, helpmsg[\"ip6assign\"]))" \
 -e '/"IPv6 State"/a \              end' \
 -e '/slaacState = gVIES(/a \    ip6assign = gOV(gVNIR(0,128)),' \
 -e '/slaacState = gVIES(/a \    ula_prefix = validateULAPrefix,' \
 -e '/^(function() {/i \function rand(max){return Math.floor(Math.random()*max);}\\' \
 -e '/^(function() {/i \function rand16() {return rand(2**16).toString(16);}\\' \
 -e '/^(function() {/a \  var gen_ula_span = document.createElement("SPAN");\\' \
 -e '/^(function() {/a \  gen_ula_span.setAttribute("id","random_ula_prefix");\\' \
 -e '/^(function() {/a \  gen_ula_span.setAttribute("class","btn icon-random");\\' \
 -e '/^(function() {/a \  gen_ula_span.setAttribute("style","padding:5px 3px 8px 3px;");\\' \
 -e '/^(function() {/a \  gen_ula_span.setAttribute("title","Click to generate a random ULA prefix");\\' \
 -e '/^(function() {/a \  $("#ula_prefix").after(gen_ula_span);\\' \
 -e '/^(function() {/a \  $("#random_ula_prefix").click(function(){var i=$("#ula_prefix");i.val((parseInt("fd00",16)+rand(2**8)).toString(16)+":"+rand16()+":"+rand16()+"::/48");var e=jQuery.Event("keydown");e.which=e.keyCode=13;i.trigger(e);});\\' \
 -i /www/docroot/modals/ethernet-modal.lp

 echo 110@$(date +%H:%M:%S): Enable various things that only the guest role was allowed to do or see
for f in $(ls /www/docroot/modals/gateway-modal.lp /www/docroot/modals/internet-modal.lp /www/docroot/modals/wireless-modal.lp /www/snippets/tabs-diagnostics.lp /www/snippets/tabs-voice.lp)
do
  [ "$DEBUG" = "V" ] && echo "110@$(date +%H:%M:%S): - Fixing $f"
  sed -e 's/\(if [^ ]*role[^=]*==[^"]*"\)\(guest\)\("\)/\1admin\3/g' -i $f
done
sed \
  -e 's/if role ~= "admin"/if role == "admin"/' \
  -e 's/if w\["provisioned"\] == "1"/if role == "admin" or w\["provisioned"\] == "1"/' \
  -i /www/docroot/modals/mmpbx-service-modal.lp

echo 110@$(date +%H:%M:%S): Enable various cards that the default user was not allowed to see
for f in $(grep -l -r "and not session:isdefaultuser" /www/cards)
do
  [ "$DEBUG" = "V" ] && echo "110@$(date +%H:%M:%S): - Fixing $f"
  sed -e 's/ and not session:isdefaultuser()//' -i $f
done

echo 115@$(date +%H:%M:%S): Fix missing values on rpc.network.firewall.userrule.
sed \
  -e '/dest_port = {/i \      dest_mac = {' \
  -e '/dest_port = {/i \        access = "readWrite",' \
  -e '/dest_port = {/i \        type = "string",' \
  -e '/dest_port = {/i \      },' \
  -e '/dest_port =  function(mapping, paramname, k/i \    dest_mac =  function(mapping, paramname, key)' \
  -e '/dest_port =  function(mapping, paramname, k/i \        return getFromUCI(key, paramname)' \
  -e '/dest_port =  function(mapping, paramname, k/i \    end,' \
  -e '/dest_port =  function(mapping, paramname, p/i \    dest_mac =  function(mapping, paramname, paramvalue, key)' \
  -e '/dest_port =  function(mapping, paramname, p/i \        setOnUCI(key, paramname, paramvalue)' \
  -e '/dest_port =  function(mapping, paramname, p/i \    end,' \
  -i /usr/share/transformer/mappings/rpc/network.firewall.userrule.map
SRV_transformer=$(( $SRV_transformer + 1 ))

echo 115@$(date +%H:%M:%S): Fix missing values on rpc.network.firewall.userrule_v6.
sed \
  -e '/src = {/i \      name = {' \
  -e '/src = {/i \        access = "readWrite",' \
  -e '/src = {/i \        type = "string",' \
  -e '/src = {/i \      },' \
  -e '/src =  function(mapping, paramname, k/i \    name =  function(mapping, paramname, key)' \
  -e '/src =  function(mapping, paramname, k/i \        return getFromUCI(key, paramname)' \
  -e '/src =  function(mapping, paramname, k/i \    end,' \
  -e '/src =  function(mapping, paramname, p/i \    name =  function(mapping, paramname, paramvalue, key)' \
  -e '/src =  function(mapping, paramname, p/i \        setOnUCI(key, paramname, paramvalue)' \
  -e '/src =  function(mapping, paramname, p/i \    end,' \
  -e '/dest_port = {/i \      dest_mac = {' \
  -e '/dest_port = {/i \        access = "readWrite",' \
  -e '/dest_port = {/i \        type = "string",' \
  -e '/dest_port = {/i \      },' \
  -e '/dest_port =  function(mapping, paramname, k/i \    dest_mac =  function(mapping, paramname, key)' \
  -e '/dest_port =  function(mapping, paramname, k/i \        return getFromUCI(key, paramname)' \
  -e '/dest_port =  function(mapping, paramname, k/i \    end,' \
  -e '/dest_port =  function(mapping, paramname, p/i \    dest_mac =  function(mapping, paramname, paramvalue, key)' \
  -e '/dest_port =  function(mapping, paramname, p/i \        setOnUCI(key, paramname, paramvalue)' \
  -e '/dest_port =  function(mapping, paramname, p/i \    end,' \
  -i /usr/share/transformer/mappings/rpc/network.firewall.userrule_v6.map
SRV_transformer=$(( $SRV_transformer + 1 ))

# Version 2021.02.22 set an incorrect value for synflood_rate, so have to fix it
synflood_rate="$(uci -q get firewall.@defaults[0].synflood_rate)" 
if [ -n "$synflood_rate" ]; then
  echo $synflood_rate | grep -q -E '^[0-9]+/s$'
  if [ $? = 1 ]; then
    [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Fixing configuration firewall.@defaults[0].synflood_rate"
    synflood_rate="$(echo $synflood_rate | grep -o -E '^[0-9]+')" 
    uci set firewall.@defaults[0].synflood_rate="$synflood_rate/s"
    SRV_firewall=$(( $SRV_firewall + 1 ))
  fi
fi
# Version 2021.02.22 allowed setting of tcp_syncookies but it is not enabled in kernel, so have to remove it
if [ -n "$(uci -q get firewall.@defaults[0].tcp_syncookies)" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Deleting configuration firewall.@defaults[0].tcp_syncookies"
  uci -q delete firewall.@defaults[0].tcp_syncookies
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi

echo 115@$(date +%H:%M:%S): Fix missing values on uci.firewall.include. and uci.firewall.ipset.
sed \
  -e 's/"type", "family"/"type", "path", "family"/' \
  -e 's/\("hashsize", "timeout",\)$/\1 "match",/' \
  -i /usr/share/transformer/mappings/uci/firewall.map

echo 115@$(date +%H:%M:%S): Add transformer mapping for uci.firewall.nat.
sed -n '/-- uci.firewall.redirect/,/MultiMap/p' /usr/share/transformer/mappings/uci/firewall.map | sed -e 's/redirect/nat/g' >> /usr/share/transformer/mappings/uci/firewall.map

echo 115@$(date +%H:%M:%S): Checking firewall configuration for DNS interception
if [ "$(uci -q get firewall.dns_xcptn)" != "ipset" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.dns_xcptn"
  uci set firewall.dns_xcptn='ipset'
  uci set firewall.dns_xcptn.name='dns_xcptn'
  uci set firewall.dns_xcptn.family='ipv4'
  uci set firewall.dns_xcptn.storage='hash'
  uci set firewall.dns_xcptn.match='ip'
  uci set firewall.dns_xcptn.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ ! -e /etc/firewall.ipset.dns_xcptn ]
then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating file /etc/firewall.ipset.dns_xcptn"
  echo -n > /etc/firewall.ipset.dns_xcptn
fi
if [ "$(uci -q get firewall.dns_int)" != "redirect" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.dns_int"
  uci set firewall.dns_int='redirect'
  uci set firewall.dns_int.name='Intercept-DNS'
  uci set firewall.dns_int.family='ipv4'
  uci set firewall.dns_int.src='lan'
  uci set firewall.dns_int.src_dport='53'
  uci set firewall.dns_int.proto='tcp udp'
  uci set firewall.dns_int.dest='wan'
  uci set firewall.dns_int.target='DNAT'
  uci set firewall.dns_int.ipset='!dns_xcptn src'
  uci set firewall.dns_int.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ "$(uci -q get firewall.dns_masq)" != "nat" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.dns_masq"
  uci set firewall.dns_masq='nat'
  uci set firewall.dns_masq.name='Masquerade-DNS'
  uci set firewall.dns_masq.family='ipv4'
  uci set firewall.dns_masq.src='lan'
  uci set firewall.dns_masq.dest_port='53'
  uci set firewall.dns_masq.proto='tcp udp'
  uci set firewall.dns_masq.target='MASQUERADE'
  uci set firewall.dns_masq.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ "$(uci -q get firewall.dot_fwd_xcptn)" != "rule" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.dot_fwd_xcptn"
  uci set firewall.dot_fwd_xcptn='rule'
  uci set firewall.dot_fwd_xcptn.name='Allow-DoT'
  uci set firewall.dot_fwd_xcptn.src='lan'
  uci set firewall.dot_fwd_xcptn.dest='wan'
  uci set firewall.dot_fwd_xcptn.dest_port='853'
  uci set firewall.dot_fwd_xcptn.proto='tcp udp'
  uci set firewall.dot_fwd_xcptn.target='ACCEPT'
  uci set firewall.dot_fwd_xcptn.family='ipv4'
  uci set firewall.dot_fwd_xcptn.ipset='dns_xcptn src'
  uci set firewall.dot_fwd_xcptn.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ "$(uci -q get firewall.dot_fwd)" != "rule" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.dot_fwd"
  uci set firewall.dot_fwd='rule'
  uci set firewall.dot_fwd.name='Deny-DoT'
  uci set firewall.dot_fwd.src='lan'
  uci set firewall.dot_fwd.dest='wan'
  uci set firewall.dot_fwd.dest_port='853'
  uci set firewall.dot_fwd.proto='tcp udp'
  uci set firewall.dot_fwd.target='REJECT'
  uci set firewall.dot_fwd.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ ! -e /etc/firewall.ipset.doh ]
then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating file /etc/firewall.ipset.doh"
  echo -n > /etc/firewall.ipset.doh
fi
if [ "$(uci -q get firewall.doh)" != "ipset" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.doh"
  uci set firewall.doh='ipset'
  uci set firewall.doh.name='doh'
  uci set firewall.doh.family='ipv4'
  uci set firewall.doh.storage='hash'
  uci set firewall.doh.match='ip'
  uci set firewall.doh.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ ! -e /etc/firewall.ipset.doh6 ]
then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating file /etc/firewall.ipset.doh6"
  echo -n > /etc/firewall.ipset.doh6
fi
if [ "$(uci -q get firewall.doh6)" != "ipset" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.doh"
  uci set firewall.doh6='ipset'
  uci set firewall.doh6.name='doh6'
  uci set firewall.doh6.family='ipv6'
  uci set firewall.doh6.storage='hash'
  uci set firewall.doh6.match='ip'
  uci set firewall.doh6.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ "$(uci -q get firewall.ipsets_restore)" != "include" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.ipsets_restore"
  uci set firewall.ipsets_restore='include'
  uci set firewall.ipsets_restore.type='script'
  uci set firewall.ipsets_restore.path='/usr/sbin/ipsets-restore'
  uci set firewall.ipsets_restore.reload='0'
  uci set firewall.ipsets_restore.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ ! -e /usr/sbin/ipsets-restore ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating script /usr/sbin/ipsets-restore"
  cat<<"END-RESTORE" > /usr/sbin/ipsets-restore
#!/bin/sh
for set in $(ipset -n list)
do
  if [ -f /etc/firewall.ipset.$set ]
  then
    ipset flush $set
    ipset -f /etc/firewall.ipset.$set restore
  fi
done
END-RESTORE
  chmod +x /usr/sbin/ipsets-restore
fi
if [ "$(uci -q get firewall.doh_fwd_xcptn)" != "rule" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.doh_fwd_xcptn"
  uci set firewall.doh_fwd_xcptn='rule'
  uci set firewall.doh_fwd_xcptn.name='Allow-DoH'
  uci set firewall.doh_fwd_xcptn.src='lan'
  uci set firewall.doh_fwd_xcptn.dest='wan'
  uci set firewall.doh_fwd_xcptn.dest_port='443'
  uci set firewall.doh_fwd_xcptn.proto='tcp udp'
  uci set firewall.doh_fwd_xcptn.family='ipv4'
  uci set firewall.doh_fwd_xcptn.ipset='dns_xcptn src'
  uci set firewall.doh_fwd_xcptn.target='ACCEPT'
  uci set firewall.doh_fwd_xcptn.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ "$(uci -q get firewall.doh_fwd)" != "rule" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.doh_fwd"
  uci set firewall.doh_fwd='rule'
  uci set firewall.doh_fwd.name='Deny-DoH'
  uci set firewall.doh_fwd.src='lan'
  uci set firewall.doh_fwd.dest='wan'
  uci set firewall.doh_fwd.dest_port='443'
  uci set firewall.doh_fwd.proto='tcp udp'
  uci set firewall.doh_fwd.family='ipv4'
  uci set firewall.doh_fwd.ipset='doh dest'
  uci set firewall.doh_fwd.target='REJECT'
  uci set firewall.doh_fwd.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ "$(uci -q get firewall.doh6_fwd)" != "rule" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.doh6_fwd"
  uci set firewall.doh6_fwd='rule'
  uci set firewall.doh6_fwd.name='Deny-v6-DoH'
  uci set firewall.doh6_fwd.src='lan'
  uci set firewall.doh6_fwd.dest='wan'
  uci set firewall.doh6_fwd.dest_port='443'
  uci set firewall.doh6_fwd.proto='tcp udp'
  uci set firewall.doh6_fwd.family='ipv6'
  uci set firewall.doh6_fwd.ipset='doh6 dest'
  uci set firewall.doh6_fwd.target='REJECT'
  uci set firewall.doh6_fwd.enabled='0'
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ ! -e /usr/sbin/doh-ipsets-maintain ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating script /usr/sbin/doh-ipsets-maintain"
  cat<<"END-MAINTAIN" > /usr/sbin/doh-ipsets-maintain
#!/bin/sh

if [ "$(uci -q get firewall.doh)" = "ipset" ]
then
  if [ -f /tmp/doh-ipv4.txt ]
  then
    rm /tmp/doh-ipv4.txt
  fi
  curl -s -k -L https://raw.githubusercontent.com/dibdot/DoH-IP-blocklists/master/doh-ipv4.txt > /tmp/doh-ipv4.txt
  if [ -f /tmp/doh-ipv4.txt ]
  then
    sed -e 's/\([^ ]*\) .*/add doh \1/' /tmp/doh-ipv4.txt > /etc/firewall.ipset.doh
    rm /tmp/doh-ipv4.txt
    ipset flush doh
    ipset -f /etc/firewall.ipset.doh restore
  fi
fi

if [ "$(uci -q get firewall.doh6)" = "ipset" ]
then
  if [ -f /tmp/doh-ipv6.txt ]
  then
    rm /tmp/doh-ipv6.txt
  fi
  curl -s -k -L https://raw.githubusercontent.com/dibdot/DoH-IP-blocklists/master/doh-ipv6.txt > /tmp/doh-ipv6.txt
  if [ -f /tmp/doh-ipv6.txt ]
  then
    sed -e 's/\([^ ]*\) .*/add doh6 \1/' /tmp/doh-ipv6.txt > /etc/firewall.ipset.doh6
    rm /tmp/doh-ipv6.txt
    ipset flush doh6
    ipset -f /etc/firewall.ipset.doh6 restore
  fi
fi
END-MAINTAIN
  chmod +x /usr/sbin/doh-ipsets-maintain
fi
if [ $(grep doh-ipsets-maintain /etc/crontabs/root | wc -l) -eq 0 ]; then
  mm=$(awk 'BEGIN{srand();print int(rand()*59);}')
  hh=$(awk 'BEGIN{srand();print int(rand()*2)+3;}')
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Adding /usr/sbin/doh-ipsets-maintain cron job for Sunday $hh:$mm"
  echo "#$mm $hh * * 6 /usr/sbin/doh-ipsets-maintain" >> /etc/crontabs/root
fi

[ $SRV_firewall -gt 0 ] && uci commit firewall
echo 120@$(date +%H:%M:%S): Fix default Telephony tab
# Default telephony tab is DECT!!! 
if [ "$VERSION" != "18.1.c.0462" ]
then
  sed \
    -e 's/if emission_state == "1"/if mmpbx_state == "1"/' \
    -e 's/modalPath = "\/modals\/mmpbx-dect-modal.lp"/modalPath = "\/modals\/mmpbx-info-modal.lp"/' \
    -i /www/cards/008_telephony.lp
else
  sed \
    -e 's/if mmpbx_state == "1" and (emission_state == "1" or emission_state == "0")/if mmpbx_state == "0"/' \
    -e '/modalPath = "\/modals\/mmpbx-dect-modal.lp"/,/elseif session:hasAccess("\/modals\/mmpbx-global-modal.lp") then/d' \
    -i /www/cards/008_telephony.lp
fi

echo 120@$(date +%H:%M:%S): Show the hidden Telephony tabs and fix default tab
sed \
  -e 's/\(--\)\( *{"mmpbx-service-modal.lp\)/  \2/' \
  -e '/mmpbx-contacts/d' \
  -e '/T"Service"/i \    {"mmpbx-inoutgoingmap-modal.lp", T"In\/Outgoing Map"},' \
  -e '/T"Service"/a \    {"mmpbx-codec-modal.lp", T"Codecs"},' \
  -e '/T"Service"/a \    {"mmpbx-dialplan-modal.lp", T"Dial Plans"},' \
  -e '/T"DECT"/a    \    {"mmpbx-contacts-modal.lp", T"Phone Book"},' \
  -i /www/snippets/tabs-voice.lp

echo 120@$(date +%H:%M:%S): Handle the Telephony card switch
sed \
  -e '/^local format/a \if ngx.var.request_method == "POST" then' \
  -e '/^local format/a \  local mmpbx_enable = ngx.req.get_post_args().mmpbx_enable' \
  -e '/^local format/a \  if mmpbx_enable then' \
  -e '/^local format/a \    proxy.set("uci.mmpbx.mmpbx.@global.enabled", mmpbx_enable:untaint())' \
  -e '/^local format/a \    proxy.apply()' \
  -e '/^local format/a \    ngx.exit(ngx.HTTP_NO_CONTENT)' \
  -e '/^local format/a \  end' \
  -e '/^local format/a \end' \
  -i /www/docroot/modals/mmpbx-info-modal.lp

echo 120@$(date +%H:%M:%S): Display Telephony tabs even when mmpbx disabled
sed \
 -e '/if mmpbx_state == "1" then/d' \
 -e '/elseif session:hasAccess/,/end/d' \
 -i /www/snippets/tabs-voice.lp

if [ $SIP_PWDS = y ]; then
  echo 120@$(date +%H:%M:%S): Always display decrypted SIP passwords
  sed \
  -e '/local mask_password/d' \
  -e '/if password == mask_password then/,/end/d' \
  -e '/v\[scns\["password"\]\] = mask_password/d' \
  -e '/ipairs(v)/a if sip_columns[j].name == "password" then' \
  -e '/ipairs(v)/a   w = proxy.get("rpc.gui.pwd.@" .. v[scns["profile"]] .. ".password")[1].value' \
  -e '/ipairs(v)/a   sip_columns[j].type = "text"' \
  -e '/ipairs(v)/a end' \
  -i /www/docroot/modals/mmpbx-profile-modal.lp
fi

if [ "$(uci -q get mmpbxbrcmfxsdev.@device[1])" != "device" ]
then
  echo 120@$(date +%H:%M:%S): Remove non-existing FXS2 device CODEC config
  sed -e '/fxs2/d' -e '/FXS2/d' -i /www/docroot/modals/mmpbx-codec-modal.lp
fi

echo 120@$(date +%H:%M:%S): Add missing insert option on dial plan entries
sed \
  -e 's/registerMultiMap/registerNamedMultiMap/' \
  -e 's/modify", "remove/modify", "insert", "remove/' \
  -i /usr/share/transformer/mappings/uci/mmpbx.map
SRV_transformer=$(( $SRV_transformer + 1 ))

echo 130@$(date +%H:%M:%S): Add missing icons on Diagnostics card and change default tab to Traffic Monitor
sed \
 -e 's^"Diagnostics", "modals/diagnostics-xdsl-modal.lp"^"Diagnostics", "modals/diagnostics-traffic-modal.lp"^' \
 -e 's^\(<td><div data-toggle="modal" data-remote="modals/diagnostics-xdsl\)^<td><div data-toggle="modal" data-remote="modals/diagnostics-traffic-modal.lp" data-id="diagnostics-traffic-modal"><img href="#" rel="tooltip" data-original-title="TRAFFIC" src="/img/light/Profit-01-WF.png" alt="traffic"></div></td>\\\n\1^' \
 -e 's^\(alt="ping/trace"></div></td>\)\(</tr>\\\)^\1\\\n <td><div data-toggle="modal" data-remote="modals/logviewer-modal.lp" data-id="logviewer-modal"><img href="#" rel="tooltip" data-original-title="LOGVIEWER" src="/img/light/log-viewer.png" alt="logviewer"></div></td>\2^' \
 -e 's^\(<td><div data-toggle="modal" data-remote="modals/diagnostics-connection\)^<td><div data-toggle="modal" data-remote="modals/log-connections-modal.lp" data-id="log-connections-modal"><img href="#" rel="tooltip" data-original-title="CONNECTIONS" src="/img/light/Data-Sync-WF.png" alt="connections"></div></td>\\\n\1^' \
 -e 's^\(alt="network"></div></td>\)\(</tr>\\\)^\1\\\n <td><div data-toggle="modal" data-remote="modals/diagnostics-tcpdump-modal.lp" data-id="diagnostics-tcpdump-modal"><img href="#" rel="tooltip" data-original-title="TCPDUMP" src="/img/light/tcp-dump.png" alt="tcpdump"></div></td>\2^' \
 -e 's|\(alt="\)\([^/"]*\)|class="diag-\2" \1\2|g' \
 -e 's| cellspacing="10%" cellpadding="10%" ||' \
 -i /www/cards/009_diagnostics.lp

echo 130@$(date +%H:%M:%S): Rename Diagnostics tabs and add Connections and Traffic Monitor tabs
sed \
 -e 's/"Connection"/"Connection Check"/' \
 -e 's/Log viewer/Log Viewer/' \
 -e 's/"Network"/"Ports"/' \
 -e 's/Tcpdump/TCP Dump/' \
 -e '/xdsl-modal/i       \    {"diagnostics-traffic-modal.lp", T"Traffic Monitor"},'  \
 -e '/connection-modal/a \    {"log-connections-modal.lp", T"Network Connections"},'  \
 -e '/string.len(ngx.var.args)/,/^end/d' \
 -e '/airiq/,/^[ ]*$/d' \
 -i /www/snippets/tabs-diagnostics.lp
sed \
 -e 's/tabs-management/tabs-diagnostics/' \
 -e 's/"Management"/"Diagnostics"/' \
 -i /www/docroot/modals/log-connections-modal.lp

echo 130@$(date +%H:%M:%S): Fix headings on Diagnostics tabs
for m in $(grep -L 'createHeader(T"Diagnostics"' /www/docroot/modals/diagnostics-*)
do 
    sed -e 's/\(createHeader(\)\([T]*\)\("Diagnostics\)\([^"]*\)\("\)/\1T\3\5/' -i $m
done

echo 135@$(date +%H:%M:%S): Add missing DHCP Relay configuration
sed \
 -e '/local format/a \local exists = proxy.get("uci.dhcp.relay.@relay.server_addr")' \
 -e '/local format/a \if not exists then' \
 -e '/local format/a \  proxy.add("uci.dhcp.relay.","relay")' \
 -e '/local format/a \  proxy.set("uci.dhcp.relay.@relay.server_addr","")' \
 -e '/local format/a \  proxy.set("uci.dhcp.relay.@relay.local_addr","")' \
 -e '/local format/a \  proxy.set("uci.dhcp.relay.@relay.interface","")' \
 -e '/local format/a \  proxy.apply();' \
 -e '/local format/a \end' \
 -i /www/docroot/modals/relay-modal.lp

echo 135@$(date +%H:%M:%S): Fix duplicate ids
sed -e 's/ id="relaysetupcard_relayip"//g' -i /www/cards/018_relaysetup.lp

echo 140@$(date +%H:%M:%S): Sort the device map hosts by name
sed \
 -e 's/loadTableData("rpc.hosts.host.", dcols)/loadTableData("rpc.hosts.host.", dcols, nil, "FriendlyName")/' \
 -i /www/snippets/networkmap.lp

echo 145@$(date +%H:%M:%S): Allow increase in WiFi output power to +6dBm
sed \
 -e '/{"-6", T"25%"}/a \    {"-5", T"&nbsp;-5 dBm"},' \
 -e '/{"-6", T"25%"}/a \    {"-4", T"&nbsp;-4 dBm"},' \
 -e '/{"-2", T"75%"}/a \    {"-1", T"&nbsp;-1 dBm"},' \
 -e '/{"0", T"100%"}/a \    {"+1", T"&nbsp;+1 dBm"},' \
 -e '/{"0", T"100%"}/a \    {"+2", T"&nbsp;+2 dBm"},' \
 -e '/{"0", T"100%"}/a \    {"+3", T"&nbsp;+3 dBm"},' \
 -e '/{"0", T"100%"}/a \    {"+4", T"&nbsp;+4 dBm"},' \
 -e '/{"0", T"100%"}/a \    {"+5", T"&nbsp;+5 dBm"},' \
 -e '/{"0", T"100%"}/a \    {"+6", T"&nbsp;+6 dBm"},' \
 -e 's/-6", T"25%/-6", T"\&nbsp;-6 dBm/' \
 -e 's/-3", T"50%/-3", T"\&nbsp;-3 dBm/' \
 -e 's/-2", T"75%/-2", T"\&nbsp;-2 dBm/' \
 -e 's/0", T"100%/0", T"\&nbsp;0 dBm/' \
 -e "s|Output Power|Adjust Output Power<span class='icon-question-sign' title='Increase or decrease radio output transmission power. Increasing transmission power is NOT recommended. It will cause more interference in neighboring channels, and reduce component lifetime by increasing heat generated.'></span>|" \
 -i /www/docroot/modals/wireless-modal.lp

echo 150@$(date +%H:%M:%S): Add cogs to card headers
for f in $(grep -l createCardHeaderNoIcon /www/cards/*)
do
  [ "$DEBUG" = "V" ] && echo "150@$(date +%H:%M:%S): - Updating $f"
  sed -e 's/createCardHeaderNoIcon/createCardHeader/' -i $f
done
if [ -f /www/cards/*_cwmpconf.lp ]; then
  echo 150@$(date +%H:%M:%S): Removing CWMP card switch
  sed -e 's/switchName, content\["cwmp_state"\], {input = {id = "cwmp_card_state"}}/nil, nil, nil/' -i /www/cards/090_cwmpconf.lp
fi

echo 150@$(date +%H:%M:%S): Fix mobile signal placement
sed \
 -e '/^<\/script>/i var div = document.querySelector("#mobiletab").querySelector(".header-title");\\' \
 -e '/^<\/script>/i var signal = document.querySelector("#signal-strength-indicator-small");\\' \
 -e '/^<\/script>/i div.parentNode.insertBefore(signal, div.nextSibling);\\' \
 -i $(find /www/cards -type f -name '*lte.lp')
sed \
 -e '$ a #signal-strength-indicator-small .absolute{float:right;margin-top:unset;margin-left:unset;height:unset;padding-left:10px;width:unset;position:relative;}' \
 -i /www/docroot/css/mobiled.css

echo 150@$(date +%H:%M:%S): Handle Wi-Fi switch
sed \
 -e '/Take the input/i \if ngx.var.request_method == "POST" then' \
 -e '/Take the input/i \  local radio_state = ngx.req.get_post_args().set_wifi_radio_state' \
 -e '/Take the input/i \  if radio_state then' \
 -e '/Take the input/i \     proxy.set("uci.wireless.wifi-device.@radio_2G.state", radio_state:untaint())' \
 -e '/Take the input/i \     proxy.set("uci.wireless.wifi-device.@radio_5G.state", radio_state:untaint())' \
 -e '/Take the input/i \     proxy.apply()' \
 -e '/Take the input/i \     ngx.exit(ngx.HTTP_NO_CONTENT)' \
 -e '/Take the input/i \  end' \
 -e '/Take the input/i \end' \
 -i /www/docroot/modals/wireless-modal.lp

echo 150@$(date +%H:%M:%S): Handle Telephony switch
 sed \
  -e '/local sipnet_options/i \if ngx.var.request_method == "POST" then' \
  -e '/local sipnet_options/i \  local mmpbx_enable = ngx.req.get_post_args().mmpbx_enable' \
  -e '/local sipnet_options/i \  if mmpbx_enable then' \
  -e '/local sipnet_options/i \    proxy.set("uci.mmpbx.mmpbx.@global.enabled", mmpbx_enable:untaint())' \
  -e '/local sipnet_options/i \    proxy.apply()' \
  -e '/local sipnet_options/i \    ngx.sleep(20)' \
  -e '/local sipnet_options/i \    ngx.exit(ngx.HTTP_NO_CONTENT)' \
  -e '/local sipnet_options/i \  end' \
  -e '/local sipnet_options/i \end' \
  -i /www/docroot/modals/mmpbx-info-modal.lp

echo 155@$(date +%H:%M:%S): Add Mounted USB Devices to Content Sharing card
sed \
  -e 's/\t/  /g' \
  -e 's/class = "span4"/style = "width:100%;"/' \
  -e '/dlna_name/d' \
  -e '/samba_name/d' \
  -e '/^local content /i\
local usb = {}\
local usbdev_data = proxy.getPN("sys.usb.device.", true)\
if usbdev_data then\
  local v\
  for _,v in ipairs(usbdev_data) do\
    local partitions = proxy.get(v.path .. "partitionOfEntries")\
    if partitions then\
      partitions = partitions[1].value\
      if partitions ~= "0" then\
        local partition = proxy.getPN(v.path .. "partition.", true)\
        usb[#usb+1] = {\
          product = proxy.get(v.path .. "product")[1].value,\
          size = proxy.get(partition[1].path .. "AvailableSpace")[1].value,\
        }\
      end\
    end\
  end\
end' \
  -e '/ngx.print(html)/i\
if #usb == 0 then\
  tinsert(html, ui_helper.createSimpleLight("0", T"No USB devices found", attributes))\
else\
  tinsert(html, ui_helper.createSimpleLight("1", format(N("%d USB Device found:","%d USB devices found:",#usb),#usb), attributes))\
  tinsert(html, T"<p class=\\"subinfos\\">")\
  local v\
  for _,v in pairs(usb) do\
    tinsert(html, format("<span class=\\"simple-desc\\"><i class=\\"icon-hdd status-icon\\"></i>&nbsp;%s [%s Free]</span>", v.product, v.size))\
  end\
  tinsert(html, T"</p>")\
end' \
  -i /www/cards/012_contentsharing.lp

echo 155@$(date +%H:%M:%S): Make Content Sharing Screen nicer
sed \
  -e 's/getValidateCheckboxSwitch()/validateBoolean/' \
  -e 's/<form class/<form id="content-sharing-modal" class/' \
  -e 's/createCheckboxSwitch/createSwitch/' \
  -e '/T"General status"/d' \
  -e '/^            tinsert(html, "<\/fieldset>")/d' \
  -e '/"File Server Enabled"/i\                tinsert(html, "<fieldset><legend>" .. T"File Server Status" .. "<\/legend>")' \
  -e '/"File Server descript/a\                tinsert(html, "<\/fieldset>")' \
  -e '/DLNA Enabled"/i\                tinsert(html, "<fieldset><legend>" .. T"DLNA Server Status" .. "<\/legend>")' \
  -e '/DLNA name"/a\                tinsert(html, "</fieldset>")' \
  -i /www/docroot/modals/contentsharing-modal.lp


echo 155@$(date +%H:%M:%S): Enable or disable SAMBA service when file and printer sharing completely enabled or disabled
sed \
  -e '/if samba_available and type/i\
    if samba_available and type(post_data["samba_filesharing"]) == "userdata" then\
      local fs = untaint(post_data["samba_filesharing"])\
      local ps = proxy.get("uci.printersharing.config.enabled")\
      local svc_status\
      if not ps then\
        svc_status = fs\
      else\
        ps = string.untaint(ps[1].value)\
        if ps == fs then\
          svc_status = ps\
        elseif (ps == "0" and fs == "1") or (ps == "1" and fs == "0") then\
          svc_status = "1"\
        end\
      end\
      if svc_status then\
        local svc = proxy.get("uci.samba.samba.enabled")\
        if svc and svc[1].value ~= svc_status then\
          proxy.set("uci.samba.samba.enabled", svc_status)\
          proxy.apply()\
        end\
      end\
    end' \
  -i /www/docroot/modals/contentsharing-modal.lp
sed \
  -e '/proxy.apply/i\
  local ps = string.untaint(postArgs.printersharing_enabled)\
  local fs = proxy.get("uci.samba.samba.filesharing")\
  local svc_status\
  if not fs then\
    svc_status = ps\
  else\
    fs = string.untaint(fs[1].value)\
    if ps == fs then\
      svc_status = ps\
    elseif (ps == "0" and fs == "1") or (ps == "1" and fs == "0") then\
      svc_status = "1"\
    end\
  end\
  if svc_status then\
    local svc = proxy.get("uci.samba.samba.enabled")\
    if svc and svc[1].value ~= svc_status then\
      proxy.set("uci.samba.samba.enabled", svc_status)\
    end\
  end' \
  -i /www/docroot/modals/printersharing-modal.lp

else # THEME_ONLY = y
  echo 160@$(date +%H:%M:%S): Restoring CSS files to apply theme change
  cp -p /rom/www/docroot/css/gw.css /rom/www/docroot/css/gw-telstra.css /rom/www/docroot/css/responsive.css /www/docroot/css/
fi # End of if [ THEME_ONLY = n ]   

echo 160@$(date +%H:%M:%S): Adding or updating card background icons
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf012;"/' -i $(ls /www/cards/*_lte.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf129;"/' -i $(ls /www/cards/*_gateway.lp)
sed -e 's/class="content"/class="content card_bg mirror" data-bg-text="\&#xf0c1;"/' -i $(ls /www/cards/*_broadband.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf0ac;"/' -i $(ls /www/cards/*_internet.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf09e;"/' -i $(ls /www/cards/*_wireless.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf015;"/' -i $(ls /www/cards/*_LAN.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf109;"/' -i $(ls /www/cards/*_Devices.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf0c2;"/' -i $(ls /www/cards/*_wanservices.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf132;"/' -i $(ls /www/cards/*_firewall.lp)
sed -e 's/class="content"/class="content card_bg mirror" data-bg-text="\&#xf095;"/' -i $(ls /www/cards/*_telephony.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf188;"/' -i $(ls /www/cards/*_diagnostics.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf0c0;"/' -i $(ls /www/cards/*_usermgr.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf115;"/' -i $(ls /www/cards/*_contentsharing.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf02f;"/' -i $(ls /www/cards/*_printersharing.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf023;"/' -i $(ls /www/cards/*_parental.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf074;"/' -i $(ls /www/cards/*_iproutes.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf017;"/' -i $(ls /www/cards/*_tod.lp)
sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf079;"/' -i $(ls /www/cards/*_relaysetup.lp)
sed -e 's/xf0ad/xf0b1;"/' -i $(ls /www/cards/*_xdsl.lp)
[ -f /www/cards/*_cwmpconf.lp ] && sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf019;"/' -i $(ls /www/cards/*_cwmpconf.lp)
[ -f /www/cards/*_natalghelper.lp ] && sed -e 's/xf0ad/xf0ec;"/' -i $(ls /www/cards/*_natalghelper.lp)
[ -f /www/cards/*_nfc.lp ] && sed -e 's/class="content"/class="content card_bg mirror" data-bg-text="\&#xf0b2;"/' -i $(ls /www/cards/*_nfc.lp)
[ -f /www/cards/*_fon.lp ] && sed -e 's/class="content"/class="content card_bg" data-bg-text="\&#xf143;"/' -i $(ls /www/cards/*_fon.lp)

echo 160@$(date +%H:%M:%S): Fix some responsive span widths
sed \
  -e 's/:56px/:80px/' \
  -e 's/:170px/:150px/' \
  -i /www/docroot/css/responsive.css

echo 160@$(date +%H:%M:%S): Add theme processing
for f in $(grep -l -r '</head>\\' /www 2>/dev/null | grep -v '\(airiq\|help\|landingpage\)')
do
  grep -q 'lp.include("../snippets/theme' $f
  if [ $? -eq 1 ]; then
    grep -q "/css/gw.css" $f
    if [ $? -eq 0 ]; then
      LP="advanced"
    else
      LP="basic"
    fi
    req=$(grep -n 'local lp = require("web.lp")' $f | cut -d: -f1)
    if [ -z "$req" ]; then
      [ "$DEBUG" = "V" ] && echo "160@$(date +%H:%M:%S): - Adding $LP theme style sheets to $f"
      sed -e "/<\/head>\\\/i '); local lp = require(\"web.lp\"); lp.include(\"../snippets/theme-$LP.lp\"); ngx.print('\\\\" -i $f
    else
      [ "$DEBUG" = "V" ] && echo "160@$(date +%H:%M:%S): - Adding $LP theme style sheets to $f"
      head=$(grep -n '</head>\\' $f | cut -d: -f1)
      if [ $head -lt $req ]; then
        sed -e "/<\/head>\\\/i '); local lp = require(\"web.lp\"); lp.include(\"../snippets/theme-$LP.lp\"); ngx.print('\\\\" -e "${req}d" -i $f
      else
        sed -e "/<\/head>\\\/i '); lp.include(\"../snippets/theme-$LP.lp\"); ngx.print('\\\\" -i $f
      fi
    fi
  fi
done

if [ $ACROSS -eq 5 ]; then
  echo 160@$(date +%H:%M:%S): Allowing 5 cards across on wide screens
  sed \
    -e 's/\(@media screen and (min-width:1200px)\){/\1 and (max-width:1499px){/g' \
    -e '$a\
@media screen and (min-width:1500px){.row{margin-left:-30px;*zoom:1;}.row:before,.row:after{display:table;content:"";line-height:0;} .row:after{clear:both;} [class*="span"]{float:left;min-height:1px;margin-left:30px;} .container,.navbar-static-top .container,.navbar-fixed-top .container,.navbar-fixed-bottom .container{width:1470px;} .span12{width:1470px;} .span11{width:1370px;} .span10{width:1270px;} .span9{width:1170px;} .span8{width:1070px;} .span7{width:970px;} .span6{width:870px;} .span5{width:650px;} .span4{width:550px;} .span3{width:570px;} .span2{width:170px;} .span1{width:370px;} .offset12{margin-left:1380px;} .offset11{margin-left:1280px;} .offset10{margin-left:1180px;} .offset9{margin-left:1080px;} .offset8{margin-left:980px;} .offset7{margin-left:880px;} .offset6{margin-left:780px;} .offset5{margin-left:680px;} .offset4{margin-left:580px;} .offset3{margin-left:480px;} .offset2{margin-left:380px;} .offset1{margin-left:280px;} .row-fluid{width:100%;*zoom:1;}.row-fluid:before,.row-fluid:after{display:table;content:"";line-height:0;} .row-fluid:after{clear:both;} .row-fluid [class*="span"]{display:block;width:100%;min-height:30px;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box;float:left;margin-left:2.564102564102564%;*margin-left:2.5109110747408616%;} .row-fluid [class*="span"]:first-child{margin-left:0;} .row-fluid .controls-row [class*="span"]+[class*="span"]{margin-left:2.564102564102564%;} .row-fluid .span12{width:100%;*width:99.94680851063829%;} .row-fluid .span11{width:91.45299145299145%;*width:91.39979996362975%;} .row-fluid .span10{width:82.90598290598291%;*width:82.8527914166212%;} .row-fluid .span9{width:74.35897435897436%;*width:74.30578286961266%;} .row-fluid .span8{width:65.81196581196582%;*width:65.75877432260411%;} .row-fluid .span7{width:57.26495726495726%;*width:57.21176577559556%;} .row-fluid .span6{width:48.717948717948715%;*width:48.664757228587014%;} .row-fluid .span5{width:40.17094017094017%;*width:40.11774868157847%;} .row-fluid .span4{width:31.623931623931625%;*width:31.570740134569924%;} .row-fluid .span3{width:23.076923076923077%;*width:23.023731587561375%;} .row-fluid .span2{width:14.52991452991453%;*width:14.476723040552828%;} .row-fluid .span1{width:5.982905982905983%;*width:5.929714493544281%;} .row-fluid .offset12{margin-left:105.12820512820512%;*margin-left:105.02182214948171%;} .row-fluid .offset12:first-child{margin-left:102.56410256410257%;*margin-left:102.45771958537915%;} .row-fluid .offset11{margin-left:96.58119658119658%;*margin-left:96.47481360247316%;} .row-fluid .offset11:first-child{margin-left:94.01709401709402%;*margin-left:93.91071103837061%;} .row-fluid .offset10{margin-left:88.03418803418803%;*margin-left:87.92780505546462%;} .row-fluid .offset10:first-child{margin-left:85.47008547008548%;*margin-left:85.36370249136206%;} .row-fluid .offset9{margin-left:79.48717948717949%;*margin-left:79.38079650845607%;} .row-fluid .offset9:first-child{margin-left:76.92307692307693%;*margin-left:76.81669394435352%;} .row-fluid .offset8{margin-left:70.94017094017094%;*margin-left:70.83378796144753%;} .row-fluid .offset8:first-child{margin-left:68.37606837606839%;*margin-left:68.26968539734497%;} .row-fluid .offset7{margin-left:62.393162393162385%;*margin-left:62.28677941443899%;} .row-fluid .offset7:first-child{margin-left:59.82905982905982%;*margin-left:59.72267685033642%;} .row-fluid .offset6{margin-left:53.84615384615384%;*margin-left:53.739770867430444%;} .row-fluid .offset6:first-child{margin-left:51.28205128205128%;*margin-left:51.175668303327875%;} .row-fluid .offset5{margin-left:45.299145299145295%;*margin-left:45.1927623204219%;} .row-fluid .offset5:first-child{margin-left:42.73504273504273%;*margin-left:42.62865975631933%;} .row-fluid .offset4{margin-left:36.75213675213675%;*margin-left:36.645753773413354%;} .row-fluid .offset4:first-child{margin-left:34.18803418803419%;*margin-left:34.081651209310785%;} .row-fluid .offset3{margin-left:28.205128205128204%;*margin-left:28.0987452264048%;} .row-fluid .offset3:first-child{margin-left:25.641025641025642%;*margin-left:25.53464266230224%;} .row-fluid .offset2{margin-left:19.65811965811966%;*margin-left:19.551736679396257%;} .row-fluid .offset2:first-child{margin-left:17.094017094017094%;*margin-left:16.98763411529369%;} .row-fluid .offset1{margin-left:11.11111111111111%;*margin-left:11.004728132387708%;} .row-fluid .offset1:first-child{margin-left:8.547008547008547%;*margin-left:8.440625568285142%;} input,textarea,.uneditable-input{margin-left:0;} .controls-row [class*="span"]+[class*="span"]{margin-left:30px;} input.span12,textarea.span12,.uneditable-input.span12{width:1156px;} input.span11,textarea.span11,.uneditable-input.span11{width:1056px;} input.span10,textarea.span10,.uneditable-input.span10{width:956px;} input.span9,textarea.span9,.uneditable-input.span9{width:856px;} input.span8,textarea.span8,.uneditable-input.span8{width:756px;} input.span7,textarea.span7,.uneditable-input.span7{width:656px;} input.span6,textarea.span6,.uneditable-input.span6{width:556px;} input.span5,textarea.span5,.uneditable-input.span5{width:456px;} input.span4,textarea.span4,.uneditable-input.span4{width:356px;} input.span3,textarea.span3,.uneditable-input.span3{width:256px;} input.span2,textarea.span2,.uneditable-input.span2{width:156px;} input.span1,textarea.span1,.uneditable-input.span1{width:56px;} .thumbnails{margin-left:-30px;} .thumbnails>li{margin-left:30px;} .row-fluid .thumbnails{margin-left:0;}}\
@media screen and (min-width:1500px){.modal{width:1470px;margin:-290px 0 0 -735px;} .tooLongTitle p{width:190px;} .smallcard:hover .tooLongTitle p{width:160px;} .simple-desc{margin-left:0px;}}\
@media screen and (min-width:1500px){.card-visibility-switch{float:left;width:20%;}}' \
    -i /www/docroot/css/responsive.css
fi

case "$CHART_CARDS" in 
  n)  echo 160@$(date +%H:%M:%S): Removing all chart cards; rm /www/cards/000_*.lp;;
  s)  echo 160@$(date +%H:%M:%S): Removing individual chart cards; rm $(ls /www/cards/000_*.lp | grep -v 000_Charts);;
  i)  echo 160@$(date +%H:%M:%S): Removing summary chart card; rm /www/cards/000_Charts.lp;;
esac

echo 165@$(date +%H:%M:%S): Deploy theme files
echo 'QlpoOTFBWSZTWZoq9kAAcC9/////////////////////////////////////////////4IePvgCo
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
cEij027HP/xdyRThQkJoq9kA' | base64 -d | bzcat | tar -xf - -C /

# Fix directory permissions after tar extracts
chmod +x /www /www/docroot /www/docroot/css /www/docroot/css/telstra /www/docroot/css/light /www/docroot/css/night /www/docroot/img /www/docroot/img/telstra /www/docroot/img/light /www/docroot/img/night /www/snippets

echo 165@$(date +%H:%M:%S): Persisting theme settings
echo -n "THEME=$THEME COLOR=$COLOR ICONS=$ICONS KEEPLP=$KEEPLP" > /etc/tch-gui-unhide.theme

sed \
  -e '$ a .card_bg:after{font-size:110px;bottom:-5px;z-index:-20;}' \
  -i /www/docroot/css/gw.css
if [ -f /www/docroot/landingpage.lp -a "$KEEPLP" = "n" ]
then
  echo 166@$(date +%H:%M:%S): Theming and de-branding landing page
  sed \
    -e "$ a body.landingpage #detailed_info_mobile>div:nth-child(5)>form>center>div{background-color:#f8f8f8;}" \
    -e "$ a body.landingpage #login_part_mobile>div:nth-child(2){display:none;}" \
    -e "$ a body.landingpage #icon_down{display:none !important;}" \
    -e "$ a body.landingpage #detailed_info_mobile{display:block !important;}" \
    -e "$ a body.landingpage #detailed_info_mobile>div:nth-child(1){display:none;}" \
    -e "$ a body.landingpage #detailed_info_mobile>div:nth-child(2){display:none;}" \
    -e "$ a body.landingpage #detailed_info_mobile>div:nth-child(3)>table>tbody>tr>td{padding:0px 5px;width:50%;}" \
    -e "$ a body.landingpage #detailed_info_mobile>div:nth-child(4){display:none;}" \
    -e "$ a body.landingpage #detailed_info_mobile>div:nth-child(6){display:none;}" \
    -e "$ a body.landingpage #detailed_info_mobile>div:nth-child(7){display:none;}" \
    -e "$ a body.landingpage #detailed_info_mobile>div:nth-child(8){display:none;}" \
    -e "$ a body.landingpage #footer_picture_mobile{display:none !important;}" \
    -i /www/docroot/css/gw-telstra.css
  sed \
    -e 's/.hidden-desktop{display:none/.hidden-desktop{display:inherit/' \
    -e 's/.visible-desktop{display:inherit/.visible-desktop{display:none/' \
    -i /www/docroot/css/responsive.css
  sed \
    -e "s,<title>');  ngx.print( T\"Login\" ); ngx.print('</title>,<title>$TITLE Login</title>," \
    -e 's,<img src="/img/TELSTRA_LOGO.png" style="width:57px;height:65px;">,<img class="lp-logo" style="width:240px;margin-bottom:60px;">,' \
    -e 's/Firmware Number/Firmware Version/' \
    -e 's/Modem Make Model/Model/' \
    -e 's/height:60%;min-height:400px;/height:30%;min-height:350px;width:100%;/' \
    -e 's/"-webkit-border-radius: 20px;-moz-border-radius: 20px;border-radius: 20px;width:50%;"/"display:block;margin-bottom:2px;width:220px;"/' \
    -e 's/"erroruserpass alert alert-error hide"/"erroruserpass alert alert-error hide" style="width:212px;margin:auto;padding:10px;"/' \
    -e 's/buttonborder linear-mobile" style="width:50%/linear-mobile" style="width:65px/' \
    -e "/<\/head>\\\/i '); local lp = require(\"web.lp\"); lp.include(\"../snippets/theme-basic.lp\"); ngx.print('\\\\" \
    -e '/uci.versioncusto.override.fwversion_override/a \  unhide_version = "rpc.gui.UnhideVersion",' \
    -e "/ngx.print( cui\[\"firmware_version\"\] )/a \         </tr>\\\\" \
    -e "/ngx.print( cui\[\"firmware_version\"\] )/a \         <tr>\\\\" \
    -e "/ngx.print( cui\[\"firmware_version\"\] )/a \         <td style=\"text-align:right;font-weight: 900;color:#808080;\">tch-gui-unhide</td>\\\\" \
    -e "/ngx.print( cui\[\"firmware_version\"\] )/a \         <td style=\"text-align:left;color:#808080;\">');  ngx.print( cui[\"unhide_version\"] ); ngx.print('</td>\\\\" \
    /rom/www/docroot/landingpage.lp>/www/docroot/landingpage.lp
  # Have to restart if changing landing page
  SRV_nginx=$(( $SRV_nginx + 1 ))
fi
if [ -f /www/snippets/tabs-home.lp ]
then
  echo 166@$(date +%H:%M:%S): "Removing 'Boost Your Wi-Fi' tab from basic view"
  sed -e '/^else/,/T"Boost Your Wi-Fi"/d' -i /www/snippets/tabs-home.lp
fi
echo 170@$(date +%H:%M:%S): Adding management tabs
for f in /www/docroot/modals/assistance-modal.lp /www/docroot/modals/usermgr-modal.lp
do
  [ "$DEBUG" = "V" ] && echo "170@$(date +%H:%M:%S): - Updating $f"
  sed \
    -e '/^if not bridged.isBridgedMode/i \  local lp = require("web.lp")' \
    -e '/^if not bridged.isBridgedMode/i \  lp.setpath("/www/snippets/")' \
    -e '/^if not bridged.isBridgedMode/i \  lp.include("tabs-management.lp")' \
    -e '/^if not bridged.isBridgedMode/,/^end/d' \
    -i $f
done

echo 175@$(date +%H:%M:%S): Fix Time of Day tabs
sed \
  -e 's/T"Time of day access control"/T"Device Access Control"/' \
  -e "/update\">\\\/a ');" \
  -e '/update">\\/a local lp = require("web.lp")' \
  -e '/update">\\/a lp.setpath("/www/snippets/")' \
  -e '/update">\\/a lp.include("tabs-tod.lp")' \
  -e "/update\">\\\/a ngx.print('\\\\" \
  -i /www/docroot/modals/tod-modal.lp

echo 175@$(date +%H:%M:%S): Restart Time of Day processing after updates applied 
sed -e 's|reload$|reload; /etc/init.d/tod restart;|' -i /usr/share/transformer/commitapply/uci_tod.ca
SRV_transformer=$(( $SRV_transformer + 1 ))

if [ -f /etc/init.d/cwmpd ]
then
  echo 180@$(date +%H:%M:%S): CWMP found - Leaving in GUI and removing switch from card
  sed -e 's/switchName, content.*,/nil, nil,/' -i /www/cards/090_cwmpconf.lp
else
  echo 180@$(date +%H:%M:%S): CWMP not found - Removing from GUI
  rm /www/cards/090_cwmpconf.lp
  rm /www/docroot/modals/cwmpconf-modal.lp
  uci -q delete web.cwmpconfmodal
  uci -q del_list web.ruleset_main.rules=cwmpconfmodal
  uci -q delete web.card_cwmpconf
fi

if [ $(uci show wireless | grep -E ssid=\'\(Fon\|Telstra\ Air\) | wc -l) -eq 0 ]
then
  echo 180@$(date +%H:%M:%S): Telstra Air and Fon SSIDs not found - Removing from GUI
  [ -f /www/cards/010_fon.lp ] && rm /www/cards/010_fon.lp
  [ -f /www/docroot/modals/fon-modal.lp ] && rm /www/docroot/modals/fon-modal.lp
  uci -q delete web.fon
  uci -q delete web.fonmodal
  uci -q del_list web.ruleset_main.rules=fon
  uci -q del_list web.ruleset_main.rules=fonmodal
  uci -q delete web.card_fon
else
  echo 180@$(date +%H:%M:%S): Telstra Air and Fon SSIDs FOUND - Leaving in GUI
fi

# Check all modals are enabled, except:
#  - diagnostics-airiq-modal.lp (requires Flash player)
#  - mmpbx-sipdevice-modal.lp (only required for firmware 17.2.0188-820-RA and earlier)
#  - mmpbx-statistics-modal.lp (only required for firmware 17.2.0188-820-RA and earlier)
#  - speedservice-modal.lp
#  - wireless-qrcode-modal.lp (fails with a nil password index error)
echo 180@$(date +%H:%M:%S): Checking modal visibility
for f in $(find /www/docroot/modals -type f | grep -vE \(diagnostics-airiq-modal.lp\|mmpbx-sipdevice-modal.lp\|mmpbx-statistics-modal.lp\|speedservice-modal.lp\|wireless-qrcode-modal.lp\) )
do
  MODAL=$(basename $f)
  uci show web | grep -q "/modals/$MODAL"
  if [ $? -eq 1 ]
  then
    CREATE_RULE=y
    RULE=$(basename $f .lp | sed -e 's/-//g')
  else
    CREATE_RULE=n
    RULE=$(uci show web | grep "/modals/$MODAL" | grep -m 1 -v card_ | cut -d. -f2)
  fi
  if [ $CREATE_RULE = y ]
  then
    echo "180@$(date +%H:%M:%S): - Enabling $MODAL"
    [ "$DEBUG" = "V" ] && echo "180@$(date +%H:%M:%S): - Creating Rule $RULE: target=/modals/$MODAL"
    uci add_list web.ruleset_main.rules=$RULE
    uci set web.$RULE=rule
    uci set web.$RULE.target=/modals/$MODAL
    uci set web.$RULE.normally_hidden='1'
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 4 ))
  elif [ "$(uci -q get web.$RULE.roles)" != "admin" ]
  then
    [ "$DEBUG" = "V" ] && echo "180@$(date +%H:%M:%S): - Fixing Rule $RULE: target=/modals/$MODAL Setting role to admin"
    uci -q delete web.$RULE.roles
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 2 ))
  fi
done

echo 180@$(date +%H:%M:%S): Checking ajax visibility
for f in $(find /www/docroot/ajax -type f | grep -vE 'internet.lua|wirelesscard.lua')
do
  AJAX=$(basename $f)
  uci show web | grep -q "/ajax/$AJAX"
  if [ $? -eq 1 ]
  then
    CREATE_RULE=y
    RULE="$(basename $f .lua | sed -e 's/-//g')ajax"
  else
    CREATE_RULE=n
    RULE=$(uci show web | grep "/ajax/$AJAX" | grep -m 1 -v card_ | cut -d. -f2)
  fi
  if [ $CREATE_RULE = y ]
  then
    [ "$DEBUG" = "V" ] && echo "180@$(date +%H:%M:%S): - Creating Rule $RULE: target=/ajax/$AJAX"
    echo "180@$(date +%H:%M:%S): - Enabling $AJAX"
    uci add_list web.ruleset_main.rules=$RULE
    uci set web.$RULE=rule
    uci set web.$RULE.target=/ajax/$AJAX
    uci set web.$RULE.normally_hidden='1'
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 4 ))
  elif [ "$(uci -q get web.$RULE.roles)" != "admin" ]
  then
    [ "$DEBUG" = "V" ] && echo "180@$(date +%H:%M:%S): - Fixing Rule $RULE: target=/ajax/$AJAX Setting role to admin"
    uci -q delete web.$RULE.roles
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 2 ))
  fi
done

echo 180@$(date +%H:%M:%S): Checking configured web rules exist
for c in $(uci show web | grep '^web\..*\.target=' | grep -vE 'dumaos|homepage')
do 
  f=/www/docroot$(echo "$c" | cut -d"'" -f2)
  if [ ! -f "$f" ]; then
    RULE=$(echo $c | cut -d. -f2)
    [ "$DEBUG" = "V" ] && echo "180@$(date +%H:%M:%S): - Deleting rule $RULE for missing target $f"
    uci -q delete web.$RULE
    uci -q del_list web.ruleset_main.rules=$RULE
    SRV_nginx=$(( $SRV_nginx + 2 ))
  fi
done

echo 180@$(date +%H:%M:%S): Processing any additional cards
for CARDFILE in $(find /www/cards/ -maxdepth 1 -type f | sort)
do
  CARD="$(basename $CARDFILE)"
  CARDRULE=$(uci show web | grep "^web\.card_.*${CARDFILE#*_}" | cut -d. -f2)
  if [ "$CARD" = "016_speedservice.lp" ]; then
    rm $CARDFILE
    if [ -n "$CARDRULE" ]; then
      [ "$DEBUG" = "V" ] && echo "180@$(date +%H:%M:%S): - Deleting rule $RULE for missing target $f"
      uci -q delete web.${CARDRULE}
    fi
    continue
  fi
  if [ -z "$CARDRULE" -o -z "$(uci -q get web.${CARDRULE}.modal)" ]; then
    CARDRULE="card_$(basename ${CARDFILE#*_} .lp)"
    MODAL=$(grep createCardHeader $CARDFILE | grep -o "modals/.*\.lp")
    if [ -z "$MODAL" ]; then
      MODAL=$(grep '\(modalPath\|modal_link\)' $CARDFILE | grep -m 1 -o "modals/.*\.lp")
    fi
    [ "$DEBUG" = "V" ] && echo "180@$(date +%H:%M:%S): - Card Rule $CARDRULE: card=$CARD hide=0"
    uci set web.${CARDRULE}=card
    uci set web.${CARDRULE}.card="$CARD"
    uci set web.${CARDRULE}.hide='0'
    if [ -n "$MODAL" ]; then
      uci set web.${CARDRULE}.modal="$(uci show web | grep $MODAL | grep -m 1 -v card_ | cut -d. -f2)"
    fi
    SRV_nginx=$(( $SRV_nginx + 1 ))
  fi
done

uci commit web

grep -q '^function M.getRandomKey' /usr/lib/lua/web/post_helper.lua
if [ $? -eq 1 ];then
  echo 190@$(date +%H:%M:%S): Add missing getRandomKey post_helper function
  sed -e '/^return M$/i\
--Generate random key for new rule\
--@return 16 digit random key.\
function M.getRandomKey()\
  local bytes\
  local key = ("%02X"):rep(16)\
  local fd = io.open("/dev/urandom", "r")\
  if fd then\
    bytes = fd:read(16)\
    fd:close()\
  end\
  return key:format(bytes:byte(1, 16))\
end\' -i /usr/lib/lua/web/post_helper.lua
fi

grep -q '^function M.validateStringIsIPv4' /usr/lib/lua/web/post_helper.lua
if [ $? -eq 1 ];then
  echo 190@$(date +%H:%M:%S): Add missing validateStringIsIPv4 post_helper function
  sed -e '/^return M$/i\
-- Validate the given IP address is a valid IPv4 address.\
-- @string value The IPv4 address.\
-- @return true given IP address is valid IPv4 address, nil+error message.\
function M.validateStringIsIPv4(ip)\
  local chunks = {ip:match("^(%d+)%.(%d+)%.(%d+)%.(%d+)$")}\
  if #chunks == 4 then\
    for _,v in pairs(chunks) do\
      if tonumber(v) > 255 then\
        return nil, "Invalid IPv4 address"\
      end\
    end\
    return true\
  end\
  return nil, "Invalid IPv4 address"\
end\' -i /usr/lib/lua/web/post_helper.lua
fi

grep -q '^function M.reservedIPValidation' /usr/lib/lua/web/post_helper.lua
if [ $? -eq 1 ];then
  echo 190@$(date +%H:%M:%S): Add missing reservedIPValidation post_helper function
  sed -e '/^return M$/i\
-- Validate the given IP address is not in the Reserved IP list.\
-- @string value The IPv4 address.\
-- @return true valid IP address not present in Reserved IP list, nil+error message.\
function M.reservedIPValidation(ip)\
  if inet.isValidIPv4(untaint(ip)) then\
    local reservedIPList = proxy.get("uci.dhcp.host.")\
    reservedIPList = content_helper.convertResultToObject("uci.dhcp.host.", reservedIPList) or {}\
    for _, v in ipairs(reservedIPList) do\
      if match(v.name, "^ReservedStatic") and v.mac == "" then\
        if ip == v.ip then\
          return nil, T"The IP is internally used for other services."\
        end\
      end\
    end\
    return true\
  end\
  return nil, T"Invalid input."\
end\' -i /usr/lib/lua/web/post_helper.lua
fi

grep -q '^function M.validateDMZ' /usr/lib/lua/web/post_helper.lua
if [ $? -eq 1 ];then
  echo 190@$(date +%H:%M:%S): Add missing validateDMZ post_helper function
  sed -e '/^return M$/i\
--- Validator that will check whether the given IP address is in Network Range.\
--- Validate the given IP address is not in the Reserved IP list.\
-- @return true or nil+error message\
function M.validateDMZ(value, object)\
  local network = {\
    gateway_ip = "uci.network.interface.@lan.ipaddr",\
    netmask = "uci.network.interface.@lan.netmask",\
  }\
  if object.DMZ_enable == "1" then\
    content_helper.getExactContent(network)\
    local isDestIP, errormsg = M.getValidateStringIsDeviceIPv4(network.gateway_ip, network.netmask)(value)\
    if not isDestIP then\
      return nil, errormsg\
    end\
    isDestIP, errormsg = M.reservedIPValidation(value)\
    if not isDestIP then\
      return nil, errormsg\
    end\
    isDestIP, errormsg = M.validateQTN(value)\
    if not isDestIP then\
      return nil, errormsg\
    end\
  end\
  return true\
end\' -i /usr/lib/lua/web/post_helper.lua
fi

grep -q '^function M.validateLXC' /usr/lib/lua/web/post_helper.lua
if [ $? -eq 1 ];then
  echo 190@$(date +%H:%M:%S): Add missing validateLXC post_helper function
  sed -e '/^return M$/i\
--- Validate the given IP/MAC is LXC''s IP/MAC\
-- @param value IP/MAC address\
-- @return true if the value is not an LXC''s IP/MAC Address\
-- @return nil+error message if the given input is LXC''s IP/MAC Address\
function M.validateLXC(value)\
  if not value then\
    return nil, "Invalid input"\
  end\
  local lxcMac = { mac = "uci.env.var.local_eth_mac_lxc" }\
  local lxcAvailable = content_helper.getExactContent(lxcMac)\
  if not lxcAvailable then\
    return true\
  end\
  if M.validateStringIsMAC(value) then\
    if lower(lxcMac.mac) == lower(value) then\
      return nil, format(T"Cannot assign, %s in use by system.", value)\
    end\
    return true\
  elseif inet.isValidIPv4(untaint(value)) then\
    local lxcIP = content_helper.getMatchedContent("sys.proc.net.arp.",{ hw_address = lower(lxcMac.mac)})\
    for _, v in ipairs(lxcIP) do\
      if v.ip_address == value then\
        return nil, format(T"Cannot assign, %s in use by system.", value)\
      end\
    end\
    return true\
  end\
  return nil, T"Invalid input."\
end\' -i /usr/lib/lua/web/post_helper.lua
fi

echo 195@$(date +%H:%M:%S): Sequencing cards
for RULE in $(uci show web | grep '=card' | cut -d= -f1)
do
  CARD=$(uci -q get ${RULE}.card)
  FILE=$(ls /www/cards/ | grep "..._${CARD#*_}")
  if [ -z "$FILE" ]
  then
    [ "$DEBUG" = "V" ] && echo "195@$(date +%H:%M:%S):  - Removing obsolete configuration $RULE"
    uci delete $RULE
    SRV_nginx=$(( $SRV_nginx + 1 ))
  elif [ "$CARD" != "$FILE" ]
  then
    [ "$DEBUG" = "V" ] && echo "195@$(date +%H:%M:%S):  - Renaming $FILE to $CARD"
    mv /www/cards/$FILE /www/cards/$CARD
  fi
done

echo 195@$(date +%H:%M:%S): Checking configured cards exist
for c in $(uci show web | grep '^web\.card_.*\.card=')
do 
  f=/www/cards/$(echo "$c" | cut -d"'" -f2)
  if [ ! -f "$f" ]; then
    CARDRULE=$(echo $c | cut -d. -f2)
    [ "$DEBUG" = "V" ] && echo "195@$(date +%H:%M:%S): - Deleting card configuration $CARDRULE for missing card $f"
    uci -q delete web.$CARDRULE
    SRV_nginx=$(( $SRV_nginx + 1 ))
  fi
done

uci commit web

if [ -z "$ALLCARDRULES" -a -f tch-gui-unhide-cards ]
then
  ./tch-gui-unhide-cards -s -a -q
fi

if [ $THEME_ONLY = n ]; then
  MKTING_VERSION=$(uci get version.@version[0].marketing_version)
  echo BLD@$(date +%H:%M:%S): Adding tch-gui-unhide version to copyright
  for l in $(grep -l -r 'current_year); ngx.print(' /www 2>/dev/null)
  do
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.09.27 for FW Version 18.1.c ($MKTING_VERSION)\]/" -i $l
  done
  echo BLD@$(date +%H:%M:%S): Auto-refreshing browser cache
  for l in $(grep -lrE "['\"][^'\"]+\.(cs|j)s['\"]" /www/cards /www/docroot /www/snippets | grep -v -E '.js$|.sh$|.json$' 2>/dev/null)
  do
    sed -e "s/\(\.css\)\(['\"]\)/\1?${MKTING_VERSION}_2021.09.27@17:58\2/g" -e "s/\(\.js\)\(['\"]\)/\1?${MKTING_VERSION}_2021.09.27@17:58\2/g" -i $l
  done
fi

apply_service_changes
echo 210@$(date +%H:%M:%S): Done!!!
echo
echo "++ TIP #1: Bookmark http://$(uci get network.lan.ipaddr)/gateway.lp"
echo "++         to bypass the Telstra Basic web GUI..."
echo
echo "++ TIP #2: Use the tch-gui-unhide-cards script to"
echo "++         change card order and visibility"
echo
if [ "$UPDATE_BTN" = n ]; then
  echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
  echo "++ WARNING! The Update Available notification is DISABLED! +"
  echo "++          You must check for updates manually.           +"
  echo "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
  echo
fi
