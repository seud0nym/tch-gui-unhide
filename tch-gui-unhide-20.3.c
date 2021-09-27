#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.09.27
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
echo 'QlpoOTFBWSZTWXOu/J0BjtB/////VWP///////////////8QXO2Yv6LUS5lWsaRpnDcYYps51PvH
uwHfOY2UXfX0p3Y91qvZvS1A10pRSozuOqFLruHJ9b6+4ARUKUCsFqi7AHpyqVaxX1pJ2+3a4oHo
ypQlQElL65d6r2vtp5Qqvr4FR1uzT1zvI71XcK0G6T3bg+z7bd0KHJd3e94veU+p9jQo6AHfAAAH
27gORaYBTiqu0hEzTJIHsx1r6wgFJAL2ype5uoQoCdgyLsagoAAdAAnoKwAeigAB5ADn26Hb5tdh
fdxcYifNtg6F7NejyveD0vNnuNt7soB7DKgEevt7npET3eq97d2+3ilRISq5Z3M6C9gAa+zHPPvd
7XoAaJKAUoCrW7vdxd2HnJN6OCos69h5x0z1QAAAAAoA+XPg2vQAOd3AFAoApQVuwOgAAbsAAAGz
Yb2Ac7Y9O56AAA4HkqJTznyPL6GgBIHoBoAenfA+6Ovuns+wlve56+j21vvq89u586G+946vZPuX
A92ba2+s4ahFVRQfRqVJJfQMUlT2AAGQKAAAJdYg00FAN92e++933dTvuHvt6l9s4+567W0em9zC
+9XOp99PevHtgu92utexiffe88+waume2BesOIB15rY77c9PQKfQQgoAJfeb3sd4PebZ974emlZU
ezp3nZ0A96w91ipBQe9bt7nQ+sl7Huzo33rHu9Znuw0qD7333vndY9KPQFet73hvegADzYKFGrYO
t7aJNbm8nSu8u6CegAaUBd3N7td887gHvvRd7AAPbAG9h0Xnt7156DuCt5Z72QXce9zmM33uqnoe
7O7N97zz2w6e+MOr3zc9u4c8ySnvrOfYdy3VaUdGbXM7njjdbT54zq97h3G9DL3u7zKztbeO7r2+
UUB2g56dcVj61renPqeuj6G06+dux5u9fS8r6etPW9h9B4PQvtkkKHfZ33uF7Dvm1vn3vb730po8
vu8vvn0oTur3V5fXvPu6fJ9febT0Kl3Z43m+877Bru5n2dPvpu53r1vffWAPHpr63zDb3KUdPnd2
w9feYffPXvCkvoOg5PWO97U96W1ttr3d1N21w+gHe3p25273vd6+nffZ3mvk3u97Pd1dctU8J669
9dz7vvPuJBx2rb7d27jt9e9t7rd3dTfbKDV31bfT7Nvhb7e88nqkJ9jUV9GlIuOrO+7pXfcEdcpc
N9ur3uO9jnn0+83q3ttvfe3bvvdN9uOS19e752L3zrvd4PrT4djQKAAyBIAAABiCqEgDT7W0troV
mhUjPAW4XswA+srt5b4cp5YHzt0+7l4vX0yD2+cOvNr7zoBtidtl99mH2y9p8D6AB93j5227BRRc
msrFoXt1G+e4910M97p5te++PL77vXfOt5eFvcHW7dEgyVNhmL1LRx4g5Oknfa4aq9ddz66vAXtv
tw+vvqNt1eDM659AAAA3zPr7vvHc2siTd92+ffL7iR9Penueds7zA9NHvt8re+8m1oG3vuuep7bv
KuN02XOHRpL60Dz23bc+3m4verzfQHR60Pskoyfc6Kw3yx5u40iIpUk97Y1wMeb59p466+611aTG
0ABml2VuKPZyuvro4PAlAhXoaiQHXiA9Dvu+DTPYd4+8FANZs6rlWvElXh5q93RwAb7AAd2i0ffM
9s3qzTfYzi63KZJBXb7t6NWtV9fd91vuez170jWvbpyM2Z5un3PQHoa8xne7PPE3vs9W2vX0LfWp
A3q7grt7577ntdzvr3K960+mW3n1jr1pTq7O83XhKat2d2tve73l9zm03Tb3fa+22eb327sIy001
ADR9btqje+j20l6ZVVEIAa7W6kUHWroFWnvcvW3nOn3Hyp57d97txvvfe+193Vz3bm9aOVrVpHc3
3zVX3d73Xeb41rVr7d4bsKUaiADQGjTuFvuaqAPQvdVzGWzN0x2rN1fdOdu7YOs0wA++331bp0fe
t99vivX1vXs7egA2ecL2b0nstung3dK71ccd83z1QNm9LGTtj619g699vnty59j59b773B1e3ifb
3aCuu9LH09fQuN15e77mvOHz7tbus1V996H17Y+V4AMuz2zeNM7Lvmd1Hb1z5aA7vnvDiEiQEAgI
AJoAEACaaAJhNNAATUMRoFPCT0RtQABoNAaAaA9QaGmjTRoEhCIIgmgiaaEwIBoRpPQRp6KntU8E
1P1I9R6am1PUHpD9U9T0QeoAyAAAAAAAAABI1JEJqNCMmQ0E9NJPACank1T8EEj08qeKManqaDI0
2KGR6JoGQGgZPUA0PUeo9RtR6jQZNNin5UEKJEQQgFHk0ZIqfsjUxMqe0m1TwFDGKemmSZqemhGm
h6Q0PSAAaAAAAAAAAABClCTIAExARgTQ0BNDQjTNERPJkPQhoEwobSPUAGgaAAAAAAAAAAqJIIQC
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
Q//4u5IpwoSDnXfk6A==' | base64 -d | bzcat | tar -xf - -C /

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

echo 080@$(date +%H:%M:%S): Fix Heading and Import/Export visibility on Gateway Card
sed \
  -e "s/createHeader(T(symbolv2)/createHeader(T\"$VARIANT\"/" \
  -e 's/(configuration and currentuser == "test") or not configuration/true/' \
  -i /www/docroot/modals/gateway-modal.lp

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

echo 085@$(date +%H:%M:%S): Add refreshInterval
sed \
  -e 's/\(setInterval\)/var refreshInterval = \1/' \
  -i $(find /www/cards -type f -name '*lte.lp')

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

echo 101@$(date +%H:%M:%S): Fix WoL configuration
sed \
  -e 's/io.open("\/lib\/functions\/firewall-wol.sh", "r") and //' \
  -e 's/uci.wol.config.enabled/uci.wol.proxy.@wan2lan.enable/' \
  -e 's/uci.wol.config/uci.wol.proxy.@wan2lan/' \
  -i /www/cards/007_wanservices.lp
sed \
  -e 's/io.open("\/lib\/functions\/firewall-wol.sh", "r") and //' \
  -e 's/uci.wol.config.enabled/uci.wol.proxy.@wan2lan.enable/' \
  -e 's/uci.wol.config.src_dport/uci.wol.proxy.@wan2lan.src_port/' \
  -e 's/uci.wol.config/uci.wol.proxy.@wan2lan/' \
  -i /www/docroot/modals/wanservices-modal.lp

echo 102@$(date +%H:%M:%S): Add description to firewall helpers mapping
sed -e 's/\("proto",\)$/\1 "description",/' -i /usr/share/transformer/mappings/uci/firewall_helpers.map
SRV_transformer=$(( $SRV_transformer + 1 ))

echo 102@$(date +%H:%M:%S): Check firewall helpers module availability
for m in $(uci show firewall_helpers | grep '\.module=')
do 
  module=$(echo $m | cut -d"'" -f2)
  lsmod | grep -q "^$module "
  if [ $? -eq 0 ]; then
    path="$(echo $m | cut -d. -f1-2)"
    name="$(uci -q get ${path}.name)"
    if [ -n "$name" ]; then
      sed -e "/local available/a\  available['$name'] = true" -i /www/cards/092_natalghelper.lp
      sed -e "/local available/a\available['$name'] = true" -i /www/docroot/modals/nat-alg-helper-modal.lp
    fi
  fi
done

echo 102@$(date +%H:%M:%S): Fix any incorrectly configured helpers
LAN_ZONE=$(uci show firewall | grep @zone | grep -m 1 "name='lan'" | cut -d. -f1-2)
WAN_ZONE=$(uci show firewall | grep @zone | grep -m 1 "wan='1'" | cut -d. -f1-2)
LAN_HELP=$(uci -qd"$IFS" get $LAN_ZONE.helper | sort | xargs)
WAN_HELP=$(uci -qd"$IFS" get $WAN_ZONE.helper | sort | xargs)
if [ "$LAN_HELP" != "$WAN_HELP" ]; then
  if [ -z "$WAN_HELP" ]; then
    if [ -n "$LAN_HELP" ]; then
      uci -q delete $WAN_ZONE.helper
      uci -q delete $LAN_ZONE.helper
    fi
  else
    uci -q delete $LAN_ZONE.helper
    for h in $WAN_HELP
    do
      uci add_list $LAN_ZONE.helper="$h"
    done
  fi
  uci commit firewall
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi

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
echo 116@$(date +%H:%M:%S): Add WAN NAT ALG helper count to Firewall card
sed \
  -e '/local enabled_count/,/<br>/d' \
  -e '/local alg_modal_link/a \              local zones = proxy.getPN("uci.firewall.zone.", true)' \
  -e '/local alg_modal_link/a \              for k,v in ipairs(zones) do' \
  -e '/local alg_modal_link/a \                local wan = proxy.get(v.path .. "wan")' \
  -e '/local alg_modal_link/a \                if wan and wan[1].value == "1" then' \
  -e '/local alg_modal_link/a \                  local helpers = proxy.get(v.path .. "helper.")' \
  -e '/local alg_modal_link/a \                  if helpers then' \
  -e '/local alg_modal_link/a \                    html[#html+1] = format(N("<strong %1$s>%2$d NAT ALG Helper</strong> enabled","<strong %1$s>%2$d NAT ALG Helpers</strong> enabled", #helpers), alg_modal_link, #helpers)' \
  -e '/local alg_modal_link/a \                    html[#html+1] = "<br>"' \
  -e '/local alg_modal_link/a \                  end' \
  -e '/local alg_modal_link/a \                end' \
  -e '/local alg_modal_link/a \              end' \
  -i /www/cards/008_firewall.lp

echo 116@$(date +%H:%M:%S): Fix firewall disabled setting
awk -e 'BEGIN{inoff=0;} /^  Off = /{inoff=1;} /waninput/{if (inoff==1){gsub(/DROP/,"ACCEPT")}} /defaultoutgoing/{inoff=0;} {print}' /usr/share/transformer/mappings/rpc/network.firewall.map>/tmp/network.firewall.map
mv /tmp/network.firewall.map /usr/share/transformer/mappings/rpc/network.firewall.map
chmod 644 /usr/share/transformer/mappings/rpc/network.firewall.map
SRV_transformer=$(( $SRV_transformer + 1 ))

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

echo 120@$(date +%H:%M:%S): Fix Telephony Service tab errors
sed \
 -e 's/{k, serviceNames\[k\]}/{k, k}/' \
 -e 's/post_helper.variantHasAccess(variantHelper, "showAdvanced", role)/false/' \
 -i /www/docroot/modals/mmpbx-service-modal.lp

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
 -e 's^"Diagnostics", "modals/diagnostics-xdsl-modal.lp"^"Diagnostics", "modals/diagnostics-graphs-modal.lp"^' \
 -e '/<\/table>/i       <td><div data-toggle="modal" data-remote="modals/diagnostics-graphs-modal.lp" data-id="diagnostics-graphs-modal"><img href="#" rel="tooltip" data-original-title="GRAPHS" src="/img/light/Bar-Charts-1-WF.png" alt="graphs"></div></td></tr>\\' \
 -i /www/cards/009_diagnostics.lp

echo 130@$(date +%H:%M:%S): Add Diagnostics Graphs tabs
sed \
 -e '/xdsl-modal/i       \    {"diagnostics-graphs-modal.lp", T"Graphs"},'  \
 -i /www/snippets/tabs-diagnostics.lp

echo 130@$(date +%H:%M:%S): Add missing chart library to Diagnostics Graphs
sed \
 -e '/modal-body/i html[#html + 1] = [[<script src="/js/chart-min.js" ></script>]]'  \
 -i /www/docroot/modals/diagnostics-graphs-modal.lp

echo 130@$(date +%H:%M:%S): Fix Diagnostics CPU chart
sed \
 -e 's/"sys.graph.cpu"/"sys.graph.cpu."/'  \
 -i /www/snippets/graph-cpu.lp

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
 -e 's/loadTableData("sys.hosts.host.", dcols)/loadTableData("sys.hosts.host.", dcols, nil, "FriendlyName")/' \
 -i /www/snippets/networkmap.lp

echo 140@$(date +%H:%M:%S): Fix path mapping
sed \
 -e 's/rpc.hosts.host./sys.hosts.host./' \
 -i /www/docroot/modals/device-modal.lp

echo 145@$(date +%H:%M:%S): Fix WiFi Nurse
sed \
  -e 's/rpc\.wireless\.ap\.@ap1/rpc.wireless.ap.@ap2/' \
  -e '/ipairs(ssid_list)/a\  if ssid_name_2G and ssid_name_5G and string.sub(ssid_name_5G,1,3) ~= "BH-" then\
    break\
  end' \
  -i /www/docroot/modals/wifi-nurse-modal.lp

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

echo 155@$(date +%H:%M:%S): Add File Sharing User Management
sed \
  -e '/-- usb Devices/a\
local function exec_nqcsctrl(params)\
  local cmd = io.popen("nqcsctrl "..params.." 2>&1")\
  if cmd then\
    local result = cmd:read("*a")\
    cmd:close()\
    if find(result, "succeeded") then\
      message_helper.pushMessage(T(result), "success")\
    else\
      message_helper.pushMessage(T(result), "error")\
    end\
  else\
    message_helper.pushMessage(T("Failed to execute command with parameters: "..params), "error")\
  end\
end\
local function get_nqe_users()\
  local allowedIdx = {}\
  local tbl = {}\
  local cmd = io.popen("nqcsctrl EU")\
  if cmd then\
    local line,name,desc = nil,"",""\
    for line in cmd:lines() do\
      local k,v = match(line, "User ([^:]*): (.*)")\
      if k and v then\
        if k == "name" then\
          name = v\
        elseif k == "description" then\
          desc = v\
        elseif k == "is Admin" then\
          if v == "Yes" then\
            tbl[#tbl+1]={name,desc,"**********","1"}\
          else\
            tbl[#tbl+1]={name,desc,"**********","0"}\
          end\
          allowedIdx[#allowedIdx+1] = { canEdit = false, canDelete = true, }\
          name,desc = "",""\
        end\
      end\
    end\
    cmd:close()\
  end\
  return tbl, allowedIdx\
end\
local nqe_user_columns = {\
  {\
     header = T"Name",\
     name = "name",\
     param = "name",\
     type = "text",\
  },\
  {\
     header = T"Description",\
     name = "description",\
     param = "description",\
     type = "text",\
     readonly = true,\
  },\
  {\
     header = T"Password",\
     name = "password",\
     param = "password",\
     type = "text",\
  },\
  {\
    header = "Is Administrator",\
    name = "admin",\
    param = "admin",\
    type = "switch",\
  },\
}\
local nqe_user_valid = {\
  name = vNES,\
  admin = gVCS,\
}\
local nqe_user_options = {\
  tableid = "nqe_user",\
  canAdd = true,\
  canEdit = false,\
  canDelete = true,\
  createMsg = T"Add a User",\
}\
local nqe_user_data\
local nqe_user_helpmsg = {}\
if ngx.var.request_method == "POST" then\
  local postargs = ngx.req.get_post_args()\
  if postargs.tableid == "nqe_user" then\
    if postargs.action == "TABLE-NEW" then\
      nqe_user_options.editing = -1\
    elseif postargs.action == "TABLE-DELETE" then\
      local current_index = tonumber(postargs.index) or -1\
      if current_index > -1 then\
        nqe_user_data = get_nqe_users()\
        if nqe_user_data[current_index] and nqe_user_data[current_index][1] ~= "" then\
          exec_nqcsctrl("-U \\""..nqe_user_data[current_index][1].."\\"")\
        end\
      end\
    else\
      local name = postargs.name\
      local password = postargs.password\
      local admin = postargs.admin\
      local valid, err = vNES(name)\
      if valid then\
        if password == "**********" then\
          password = ""\
        end\
        valid, err = vNES(password)\
        if valid then\
          valid, err = gVCS(admin)\
          if valid then\
            if postargs.action == "TABLE-ADD" then\
              if admin == "1" then\
                admin = "A"\
              else\
                admin = ""\
              end\
              exec_nqcsctrl(format("+U %s \\"\\" \\"\\" %s %s",name,password,admin))\
            end\
          else\
            nqe_user_helpmsg["admin"] = err\
          end\
        else\
          nqe_user_helpmsg["password"] = err\
        end\
      else\
        nqe_user_helpmsg["name"] = err\
      end\
    end\
  end\
end\
nqe_user_data, nqe_user_idx = get_nqe_users()\
local ngx = ngx\
local session = ngx.ctx.session\
local tablesessionindexes = nqe_user_options.tableid .. ".allowedindexes"\
session:store(tablesessionindexes, nqe_user_idx)' \
  -e '/"File Server descript/a\
                tinsert(html, format("<label class=\\"control-label\\">%s</label><div class=\\"controls\\">", T"Users: ")) \
                tinsert(html, ui_helper.createAlertBlock(T("You will need to create at least one user before you can access the Share."), { alert = { class = \"alert-info\" }, })) \
                tinsert(html, ui_helper.createTable(nqe_user_columns, nqe_user_data, nqe_user_options, nil, nqe_user_helpmsg)) \
                tinsert(html, "<\/div>")' \
  -i /www/docroot/modals/contentsharing-modal.lp

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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.09.27 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
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
