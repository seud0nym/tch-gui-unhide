#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 18.1.c - Release 2021.11.07
RELEASE='2021.11.07@15:34'
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
SRV_dnsmasq=0
SRV_dropbear=0
SRV_firewall=0
SRV_network=0
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
  [ $SRV_dnsmasq -gt 0 ] && /etc/init.d/dnsmasq restart
  [ $SRV_dropbear -gt 0 ] && /etc/init.d/dropbear restart
  [ $SRV_system -gt 0 ] && /etc/init.d/system reload
  [ $SRV_nginx -gt 0 ] && /etc/init.d/nginx restart
  [ $SRV_firewall -gt 0 ] && /etc/init.d/firewall reload 2> /dev/null
  [ $SRV_network -gt 0 ] && /etc/init.d/network reload > /dev/null 2>&1

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
XTRAS=""
UPDATE_XTRAS=n

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
  echo " -x <feature>     : Download the specified tch-gui-unhide-xtra.<feature> script."
  echo "                     Specify -x<feature> multiple times to download multiple scripts"
  echo "                     NOTE: This does NOT download or install the feature pre-requisites!"
  echo "                           You must do that BEFORE running this script!"
  echo " -X               : Download the latest version of any existing extra feature scripts."
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

while getopts :a:c:d:f:h:i:l:p:rt:uv:x:yC:TVWX-: option
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
   x) XTRAS="${XTRAS} ${OPTARG}";;
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
   X) UPDATE_XTRAS=y;;
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
if [ -n "$XTRAS" ]; then
  echo "030@$(date +%H:%M:%S): These extra feature scripts will be downloaded and applied (if the pre-requisites are met):"
  for x in $XTRAS; do
  echo "030@$(date +%H:%M:%S):  - tch-gui-unhide-xtra.$x"
  done
fi
if [ "$UPDATE_XTRAS" = y ]; then
  echo "030@$(date +%H:%M:%S): Any existing extra feature scripts will be updated to the latest version"
else
  echo "030@$(date +%H:%M:%S): Any existing extra feature scripts will NOT be updated to the latest version"
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
echo 'QlpoOTFBWSZTWedNvkUBu53/////VWP///////////////8UXK0Yv6CQQplesaRElLV4Y4Ueer13
nffc6PuQ9ZUXnqohXKgobYOneezipe++33vlSCvdzkbZb13ryqnzt1KO2RR4vcN7YlV297vNGve8
45Aeb771V0KhX2fcK1OmlFXs9x7e9vfF7T73Wm3vqBU13Z2PevOvprsenHszXZUywnhbQNKXze+9
9vvgfSj6AaKBe8+lSCqOfY707c7vm2zj1vtwdHeWjJu+93p9O2jvvbwH16fAlaJtpGxLesOHZaaL
dxx1kAYAoCR6AB8A93A+PrWzbABLxaOi+T3nrD07mvfY5KPPdevW33Pm93vO7XMctng1evfa+7zo
e721dtXn3t3ho1cq777ne2N9y758X233nz6NCzN2p7tffd9fee55ae3HR2fc2R776t3nm32kVSbB
ssp7rdtod67ffMdT53zy7ffb7x9V6o775Rb1na+Pn3vLnvAANXYKa32A6Tt6b3x6d3vQAN664A+g
+qUMUi1vthd9nAFH32AADoAB8X3gUB9xPffN7m98AFAD74fWTUCee8GslR61QrTNMW3WTvs3WAtg
58F7o+9u+Pa++Cbu59MfQr52e2u+b6fe6+e8uhq82jN33h958drGL289r6456ruH0Zzu6yAbe4DQ
OQUVaxpRfd3YwK00bbF00vlrZj5tKpUh0DIH307hrCtercdBDWAhKAUBQBIzLBvfeXvvqjGgvZp2
LxeO8UoAorS9t09w6De98pqdxNk3fMK7rur6+u++b53Yb3qHbfduTW+7vbDy3tgezvPHaaEjQGPW
1js7ge9npvai7LaiwygCkqbYoUANNBL77A+x3LnXweCqFezC7fBOzdAdckzzb6vKH3ja99733vi+
+D56TKkexk12OGu11dbVaN6cRUrsvd3085shIVk3lL7s31vr59brm3Q6iuLBK66Lte+t6r2gNpWh
0e8bkwr72vL18D2+4o97e3H3uIPrTTrZZvd933j6Ra320k2b7uOlVpveD77e+voFs21VB9hp01RS
QrbaBWofbrd6+48cfdl333vfeL5t4cuPnuen07s26HQdGNgZKolSgQUQAKJ3z3nrvpe94kQibOYe
5ob3HfT19kAAAB0A3HePV29jQvfPfF8OgoFVQkEqoFCL1j2BtdfY9tbd985C89PoGvuWxvjnHx2e
PJ3Opa3sPZ7fb3oLe6es9Ltq+waHuzOZ7tuY943Q0++3OZ9ffZ6+tW319F9uS94+vr499PveNb7G
+wPa+ePvXzy8vvbtZ4uvo5Xsb20bZ6Gvvb5J3n3blUAXV7T1C+gZCfZ6feed16rgtvd25w9987ej
fYc9W99lBPufADWsgB948+G+dXdczrZh1u3FIE98zh6O2en2WtPnvvvtd9fEgzrzz74vt9p4+o++
97fb6008vY3ttOHvu47232976yvPN9vm62Pvd7O+p13xtN3dzH07XHrKx9xgDuBizXOHG7O63d2j
p3bh11ndXDdnHZVUUAAKtaynQ3BRK1i7G7WW1RXNYHTQ6b6z1ZvO6blxld7201Pa1WPcn3e4b1l3
bvfPvbqbrbVffTukw4AOfcOHbXr7zz0e99932s+4trZ2fXp0AvZzFt1gHbruKXXTCQqKoUp9l9xv
PNt3uvvKOezMdRGdoqH23XnbVBnH1y7W2W2tWL3WV013Z2xnNsR1s02j4enh2NjGxgoAEmg2mVTR
iVEqgtDBxunAbfc9bnu7ms1S9u2cdGPdwznZeYkLz3PvhqXna+nti3QXszVAMt6eenGlLJQYaV72
Hd57s5XucaO4buMB3Vm3B49L3a6ZLprK9Zndzdgu88jbRXsdcb17Q7dwAAMxUFmYsnzvd4DpffPd
42Ujb5bq3brs6g13Djtod2UQgy05aS7nz2vaB5ArWgNrYb0bprA9BbdXG60MUT7j3qteZ13T3b1r
Y9U5U+urbNqbNCNXd9vvvugrnr76p589iy7wKKpQF7DXvZ62oC6am2q2aMbnOnU7x9nqH2vo+nit
7veCgDtdIfbuvO7nbEitnfdt49XW8zN1D29u5z22acwBUWxvPbudn2y7NTx0z6N27rW2bylUd5ur
otYN45bs6m2a53AW6HKiIU9sqKhVU2571wHMu7fX3j5nPex7sb6axtLAAendqs97F23gW7vW5s9t
XTDgVKroyqkUrYaXw+9vQ921QLqe3QPer63sdPbDveYToyt5A6CgAb21vdvN5OOcSrqLQ51M6ABo
BqgB07vO9dszNHN95725He7PvXnQG0qtrNYVrayqNuu59udDUbYNV69G7GlNTzVsYLdnOmoKtSub
qPe81PZMZHdsM63tVcXXt3WmW91abKHbSWz1rze62VS50M+mU71pjS3VlCmtVQOl2d9Um67w9re9
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
vv/9P/9/j/2/+Ho/R9f+7q/2/+//j6v4fq/s/b/v/9X/b/5v/J/8H4z+//R/u/97/0/i/yf7en/j
38Mcu7/J7v29n9/+P+r/s/7v939/+r/p/6/9/0Y/3//v/H/2f7//P//vj7sK/93+Pf/9/+7/1/3/
3/7//uD/xqfd97Hq0VGUUQ/5f8Df/Wp/0IgzR+plD/w9kaiDEs/+dSj/3Bi/ZuMfh/36qx3aBwBB
/3LMgCT/6BxGqwEEko8AT+R5DjEhTh2IJ6MjexIM8SNZXXp/w5JLOBHAdSjhFs19gktmrhy+xB1A
3Ni7mU7b0GjuqDi2ihOi2ixRF//W4ma4/pkX3BnWiWpHBbaigEOG244ybf+rZeanM1/4R1QN/4Hd
s6DnoeoaWUqbTdUtjW0jiqqFtqGoM5MYULxyZAyVGjoW4oaFQ2EE16TAWGmS5AzXQ4TTr/9Ttr+3
D/ofuIYh/yQqA8n9/NSNT/poZCUIB/1gdo/9eAkJdNKClFPOAb6q2UYEb/5ziaYob/M9mQbxv5rF
c8KXs54Dg14AvHkW/BC0gwf/fPHq0H+IPJ6PMJHIWjpUJdYU0hhXZUj/teImWqXKGIMYqI7dpSu2
or/9CMn/GPp/5f9e5V6lWu/d/482u/mwCCTUFXHC6665RVVVVVXDhnYJmS/+IrIAhctjkFsNC+qw
3Ip8HWZO+douxS3AwxYFvsBERxLriKLcltzlu0HjHfSZWiXKy4rv2yRn+9Tpc3vyHa88ZGbHTY7K
hJJJbw4VVnowOIeLht6eQ/+1/2j/d3imB7JcOY9E9nT/7b9UIShMuzHqxhu5/1ZB0ajGRfXXjFFE
5d17Fxr3Z0hq9F9m+jQKL6hyPQZbSbd379OmMP1l1RyXaE15M89oEyKncM6Eg5RC5d06cMDuSivo
fA3S8JuybmzuoGTbzlYpcVuBvqXk89f+zTN98bbnr4uCqOulJDb6yyVSxrOCi9W4laD0eMr2xs0x
aalbvlBPXCAw8yoPlF/3+PFdTsrI7CRw+oKe5eGh6anRtJ+p7AXVo31YBtdzFgCUJ+C0XUxf/n+L
JlEGQqFMUssY/6txvxtuEm94ZjeUt7hgH/9h+CO//iX4on+Db8Pk7ts8r3A8WPuFOeOLnC4SteXz
GPN0jG/qhxtgC97DqxlV0chAK7OmwqaUi4kKqhP/tomVSIwSEAu/6LHVJSqw2SSR2InRraK6gGNa
Aeg9RkKmQgQ3/zScfZsD4Hg9nkkhIVu+ITQjdES85qY9n699d6FrPz0+z7fTbbzrONmRepz0xPKa
hYwZ4sKU6JYXFKIjajuse5oXwSZOb5O99AYtHdk7MCcpJOO93JMpTBEWhdbuNvKpG/ZiOlj/P/5+
dyhYUuca1oK6qrOyHVe74VYKtLs4OWwpOLvlsunbUPVS1ay3bbE2TpCdRQXaXXzhaqRFnMpKEsMf
Zb/15VY88pYzjGMZzlLjjh3fes5zlLOc6tCOSvKb7bjv0jl8vvMdMxR08sYjUQdJlyJziLF4Kvno
9a4dsGtG91fZZHguV0rcL8T/s43bklMvGszNLnAE4jM58qchg3XdA8yeyL+YhI/DfY8NdebZkH2v
4gCVfd9U5ioWJjW3OoD2XqweMIG9PCxRfBZoZ9+SdWCf5m9QieiyupUPaKlzharldROnWtipKDQ6
aUVjrnss9faInmMYy8qldcHb0xFc1Nt+ghkdP+m8J70S9CsM3wiV739JaSRfYyMeTgYEGFKwkIbt
Q5UjwIXxkuSR0IRDjRxyDkHLNf0N0r9D64LGoYAoYCWOIjqXy5h6bgCVSrXgK1SLpzIed42MYzJF
zCVrYmaVVWeu0pCC910AnWgnQwIwMi4WNlp14PA1dMp6bQPi0LLt0qFUBnXBCJJJBIgm1OhYF2zF
XZFKM2D8USMToLGcKbdbjW4GioeoID5kE/uH0fWdfP5l6sjUYRx1ldZ2GZ4T4hqrGVVJc3hngokH
uN6rLO8b6a36ADBs05Ts1oPO0DtIhYQSEQFDNz8VhVG//uvLI4AEMwN5bL5NKJ30c0F8edOs86rN
eR5986PfQdWQcGkMiWYZjxaeBvNXb9kNGCYY0yYZddgGQXwpu5W4JQSqq4IRHXscYhyaKf4sBASj
aaQw5zEi6S6cd98CMJw6oHlEBXECeeeJo5i/uquPbwctkvfTMyCDOAwtUKq1X4Hfx8fHmd307dux
GyYczm1ipoTc+U7eJYDSa0GUGWEDTLYzu7q+E3TTA1L1VWZdq1iIaFExL9nCgdyV4lTIop664MSM
0ha8VmzRHcsw04PUsmtV+JsR7eh6HrYGhewA4DFJIqoKsai0j0Bax2vrrXdhzE7REuDczLxRSgkL
QoQzrs3r5QC6p4sydHEktVc/eBjYict7BKw7tda5Ht6Xuhwby15Ds0Yq8nToSMSXjz2cUMtedDSN
SraPs780OFZGoZd8jHMiQKdUxFft/Ord3bR/jDuEYJOqR73R7nJPegoUQ5blFupmVVkueFDRQ5+G
CQJCEJCUcrQ2+LYQPOht9K8ezBzq7T/+EOlQx2VTVVLmYj2Ch0vzUqK8dJotpjhQogdw4okkk0Oh
uvLd1Pby91QCZLdrEkAhKbB0rO1hHUSM+CdU5GbLI+rpb3wWQ4m2fIX187lDtSAeE/zunHcFv6uS
2iLu/N2gZ1qGUSGLxtiqCdzi3XVGlwQg1imss+bQ8QDxuQzE3icQgZc3/yVy4ptbZhcgl7LmhoJ6
UPJSckT6GEA8AQ5kUEA54d/b291Sx77q+UMhFtgMjswqp2wxzjLLVeHiHsAVD/0f7JUN4dYoJaha
pqtjNvXZhz07LVd2hFjlZQlrFIDu5OyhR1Wa0VXhTGTMfzazbUf9qN3ee/YEA0lXqb/IcywHyYTB
SCRAEEVEAVBAPVxb+GnKc3F8SjGAmT7vpf0iXlTwT2c89kKuZt5jp+uK1+n1h4ua4uC6uyVFrx+o
S+v6/rgheznnPPHKhHP7oHSShx6qV4+vpxRa2d/hOnycBDYTvEiBGOzOUAl9Xf3IhmCZ/DrN2lyQ
kKiptPC6c7gJnTzfCo/MojuiqDod2wTw5b95DnyTgc/n42Jc27S/pIrrvJ8snQJ5BGemhs+sJwg5
Q6LXEz5REzLu8kC4JseXTZeOhupwadjSDADB1+nqk2VyPYg4OOQf9nsrFY4GjGkoGslJNSB0uJws
BYg6TJ2xXpQHQKuUHUQ235cuorMEuWww5cebhwmb49e61s7c8TBlu6JYgxsh5gPhzLuc3mb9qQ3J
vnMbDaVXNyJn13HJQeWZMG6XhzGksZwjCB/apgiqWMBtGhldqAg9fft+o+seUN7y93yqrAzjIGy7
XBaFGxiwOfXfzTFyoePJT5DgCr9+Btt2QdaeerCKWqIEsVhK2E4RRpcMTrDqPQviBnTgtd9qOlas
sV6X+FSu3aBVPzRuu+rp4lQXCy/1KT7nlKOXuZEZIhiBB8bmW52ZbAzZbnzDCOiJhUEAkLQ5dCKd
m6r/+rc0DBIMm7agZRVY9mcMiGZR4QgYXuNyOL53h7PHjIddSDHJBknrYM2ZStmleBRDiIcelW2I
Yq7j/67qhjdtz2xk5QMbuoyD9+tkXIp6KjBgr7p8vo/C4YQJOIdkyd2LMMIiIIJqA7wMfQud+3hx
foqnYpsb2Oep3aJyp7FyVyouGhHdDl9CMYa9YJ80zzDP4+4R5GK3u1bzPnetm0yatsw319+dW3Ih
dVZf9WiTuVSJSbqi1sMzQHQAZYhhUHUi5z2HZqBgCiAIGSbTBLl6UC+xZYSMd6IpBgG1G/iJbEkk
GRbbtp3BEnHa6n1H5aA7zpXduFn1fVI2polHMyJ2nUfJ8k8Pp7vKcI3JxqvcY9ZcZuno3tRlogf4
/FcK1ArUENH6vtileZn6JHxVTIk5tKiRTngfWeOngt5trJ5ElFUK4WsWV4Ul5TBO8V6kKlZfp5Kv
JxFCEdk9oI+ztsaplXBqpdblJ+eB6Dyv4COmMYk463MzG1+ii3mKzR0kwyiZWerfa9/J292Wym0O
yG9oIdt2P6Lfeb7LV8pEax483+JI9GHiHm9FUdt4sW244MZanDFDd15K8p2Q/OOkkO0N3GbvhMLn
P41577UYNdFVLhG1x553+91rI7KZ5XOlZVo3VTOhcOUUqhY0C+pLISKS6avPGBjo+rDVRSwZtL4O
pBLweHqjEtZUMJE+p4xlq88VGPHERdZ9le1F7d2WhcIpBFVqK548XFulceuY0V3wP4Rary4vVo3j
aMjV7/3Eh+f5u3Tr7HH4jc9U9T77hOKWKZDcoGi89rCoKEvxdbzZnaw9ZrWL1neehJehOY058ANt
3x8GMB4ZvwbO/RLsBPaYsDQZAVd4hNjO3x5KTc6Dz96rAQsEQ6VzQ6YQmS5Qsud069yp6iXJ+U5/
G7Fq4xly/dp6/Pj2fLr08EQ4WK4+QvNIt7JuyxfCLkWCywghvZmZUO8MWXhEzjjKWgo4iHNAeYQG
ybQZ4cwefLsQmtcibaIUumaScy1f05nWtY5o+aEmF9ZXkS31dAb6f2xvCSYSry4z4f536Z08LI9q
RQ6Yb4hWKm2460zAwEIpSxgcQEUuFO/1IF5sTYksw7s1kDGHwhdZ7GRtNmSbIFaOdwoaaPhYDr9v
lAGeeWyNeM6jcFIKiKRwjEP+ffO9byvtrXUgyVg25QVrse70xjD3wXnJD1PGv2iXyfCA2JBreh7i
JZy668NfB5mj1hG7FGEYs8oUMaOaEB1p+R0dnTRXiy5s44qwi4mjFs9pGpFCN1Z6Lb5zES3YEdEI
RN1YiJlWHm5VD5N5ycK6o5l5cTdnQPxw46eCEQIEkxeT24+AGnZm8jvjO2La6ykQTGCDKThK9FVg
WfNmU/F92+iQXRAoyPKTEJMnMJKAx0raZkxIOCGoWKoySb1oZJpNvUoBDG6gUxUokYIWqGgkIWQl
WWBIIJQlZkAhlSGIaAlYGObrOXjDx9BsNxsDViFns84qCnoVt65WdW7ssnOxATPiHlRUsRLY1vuU
6Fd3Frv7JOjJUvtOnsJnXXLYvDD8x0Lw2lKGD/7jvY4FiB8FQTBUEc3otw9fmvgrquFdMoEcu9h0
lj0NmYl0232qWA/qiOWlzuaf67WUk5a5wxtH0fHWMZ19fGrhpvw8OAdsuY1MlPl94/7Tp5OupvXe
OtepA+CneRNfh3EDp7d1Hr29MhdT9xOsGm+kPw4zjqKw5HWxWlKlZBTO34/YxqnSs1VmPDvaZCKe
kJQCWZN0VJ6CXct42Fp6wVPf7+vyzybx9c91UshpxtRYN2HcWOyIYiSJYhem5GAKCiTVEsLpjZpf
Y0Lzja4YKaSEvQrHhwznTMR3QkClJJdwJNJ7hqpJK2V/C7usPowebajowMYAMsegZPlvf4CuBRtM
yb9vHIyhyz+tQ8aKbgAOSuxQ4j+DnsDp3NZB6efgqzM9DmgIIANmXqoZ4IdotJl6F/NDmmINnzMU
GNtdjQ/Tr8ubPoGZqyuWDcE+5Am8cXLaAnw1Nv/s6Hm05nuMNqphfOG/lB+DDJHkcKkpE1HGvLmV
YHWVD8CLKvmam77r+jCTiXHuQVAQ0R2wjjC6HFlQXqUF1zfpd6krsXyTsjPMZdq0dVVYSgPNImIa
3pwsTRWEB04JZRFvztv3qm0mZ2bpp03HNHmwmgj1ewBQ9vTLUjQB13omWPiCGSKqKhXNdXjipUXX
R6xBE6wrSu61792wQR1drrt54FTD3a5W47aPBz33exwtJQhCZveDueDyd3eR6s4OFc5/m/TKWGdq
F47ZiIvLBfzJncxS1UPAnn3TJvXgZgWV8h8rxs2MfJkfW1cewSVEzMIwqkw6GpC/BLTHaiCNFmK6
oYwJseEtwrhVnLpYTJPpsP2GOg3HRLsHjp9LQchvnS9wcins8fFV19s7O8A8gojtggdB4rdfDer1
9B20HR180qs80x4LSpRs8woeLtLsjUGu4sWAk9BsfEnfr97NsYcWiuGvlbjcraeFAlu83U6w7eee
52hpIPJEpevaA9Tu98cReedasrtGY2yh2NwpbRGr78xzxqrcC1DYiqKjb0EQ4ZYWtqbK4mFcxVz2
KmIYXbUEaRYg0mxQv844cMoC3EgUQvS5S9sFuZb1aMkFVTbsjWt2ui9Goz6cq2bYBXOdt0FI44Le
vfk9QSS5FqqYegsDbkl4HsuVKjB0RO0dRbUQS0Vc1NmabuFRJ8myzrKLWoLBFzW1R9/6hvoxvxjj
jbxNYuInq83jOcrMLI2F7mAOeibpgBNbB6t+jn5JseTIyE7LGPMNDCRHbqRjc8uRFOHNDMwjRES8
WgPhdZtthCcboZ26qzQ2hYCoMmc5VRrXOfWXxi8LGMYtPi8MwzEekjGv6TsfvYdGHL694Xxz8MFZ
EdXr54vB1cydJtlSGkdxOCNrO3JTaY+s8N6gT50+MQQv3vDEJJIhIZBbznfg3nbUmB9/jjlrj/7a
NEgTPnF7TgR52Jx8wfCsX+i4vHtRhK6d6TfJvdZgnUD4f80+8G4un4FxcRhjyR0br7q7C9U66DEc
63v3p/FPZwftazy55SXA+Du1KDauW+trTn2fds2Uw/TvkIrheuidDaY4HDj2WXdplUL2NnPcxBB9
CInpiAkQlmmihcTw6V5ykXAITUKkMGrjjlaEmMnRy45VUfzYhEjhkp/Piy7Y4A6440eiBt/qYri5
4+rbYEVIqmspcrQIIZhrmAhaXoCImjXiPPKhdGChoHQZgSLQHvZNhCdkD3W4Afx/3voGZPnQ4B1J
EAe0vIdWF77OGIbIUgtJef1h7Hv8tVUsvrXovlVTu37dlaVpE6Xo3Qx1s0kudWlBRrqemC08xP4P
pjo5xQ9ES/uBCLEQh2nd1L06FGje5cv5UgdYricJ7CD/iT1b0QkVb0TgCXcFrdJicNUxynzDcg9R
k55LzZC8cxctOjIo2SEjGDB2FCzSIPoy/DJx7dXumG5zmtnwqthFcGor0bqtjF2r1gElc+JxCZ4K
cwn5uA5xedwiv2KGzys63dXcNWMUFS0Dbv2a7ugivTxkY9ob8BcEyV1e3aZzQ3uk0A++ldoat/V3
HMFaheuD8oNRu4X2wVut8f4b5wsZbjJaPjj2LPS7QdMRNpmXQ9x6VIsDsL20WTIRFEdaSShR7jrS
5+zL5zv46U9hHPGsk6+B0qjOcw0iMistmYMjHZqgiPTyqPIy6i8vMHAqqtxxNJP658i3yJ2zjLrp
qKr1qCskZ65M4JL08LByYJoiIsnUCIfh3MeCa3Pncaz5aXnGRFc7eVorXmRPR85fKPI7O2xfeBnm
Zb1UFzT1szgJwBMZW3r2rvWrla+GUcK67dfxe4EDaKiB+kh1bp5HgIPNuoseaFXuezQ9h74E+R8h
HsukkgVlh9/lAcLRP8gtRegkEXIXjieMH7TYiqHNX6U6OqxMf4ph01bHrwaFTK6yZz4KLX/Ut5tE
JXiT0P9n/6f0v7/30Pi9JSIfVhdqpsA+iBXwSnv8PjB2kT1kt/Gqv5CWKj2RaDAnvOY6MdHfxhb8
zRVT0KxysYP1fsc5vRxOUOpIc/TQ8T2gK+bt7Fa/FN5QPS6nJv4p/IlStL8+ByEWAwhjG1mVdy+L
3LnDGuvzoqJAvtTg3K4K0suJRnQ/TAW5fiNiBjYzavBW6c/y3nuEXzV3/y8gScqdH19eMKw+4nw5
8jTTGiqT3N2bIz4rn7qvx9x1y5m6bBz6gRk2+e9f4kPeYIpFGYGMuOleuyzpAIJXI+x7uLz+I4z0
VVVRVVVVTVVXaHb5TLvwBP9bqv8ZcH/aKPiXfuBPrBICEBUIERjm/TYh9gCaupLHVvSbK2mAoI8v
WiB3GE+K+ev6/9qWQP4wiiamh1XGqAiIRTVAA7BjIlhm0Yh6Zdr9nZihETll/SWRfuww5j9aGy0S
RMJFr1UUZhu5hRKAOeD02QT8ZwbnUaFgyBDS8ty8YPQfe4CQ2gD6ti9qbRDcBuOB8XEunlMniR4B
QQ2TVNYF3eO9TWIK5nYgVFzIFdEJguXK4VAPCKr8RNvkcrQDsNOpiB5jVTm6FSjhmRVEVFFU1FE0
Q1UZVlRRVRUSU1ScvNE9houxj/3CnZHcUN35RLd1bYuYFCpQ0QZBN4ghqiWnQXAG+VozJQEu40Wb
UDYKqLtogE7Q060CjsyJZHTTRTykJb470X4yQXkloe3dKOSX6aNeSqaeZOswoc+BDxnw+VRbKA9I
65Q7iut2QwZAg5C8ndZbIc5XT5S5pJkmFJlcm024UulbfDrA+CHVCB1UZrlpHxneIZbZJIExM7Er
3MnqNCA8ViozKiBrBAgKLoU2JR3nT1l11VUziMRCInPqIgfZ9Pi9P0fxZ+115/nmt61l1mxHBqwX
G1Gm3d0K8ePicro3dznWeFFUPKf4usTbkYMjCQxBDKax/uMGRqYMZkHEBpfRWgyyH9JKf2R6aYnc
B3pnqQL6zbilLyHIE79jH+a8Uaf0YFLpK/XszWXWRoy1v/vZri9qvlgop4pOXMV0vtlOyyHWTnld
SBoO5FMqmfw7h85TL925W/gw+yjnB0OC3LRKm5qEhUnRkv3teiyFEFFc2mA4QXEciKIiIiIiIiIi
IiOZb8uhxj0BGkNAizGcz/WCaVsMZw47v1Do59geiZIj8TeOUAYbzTjnhpnHq5J8LdQjA6EhISMf
xlg5Rjzb0vKWt88Vq16cV40QcFFYD/tZZ9ondBj5GxX8EQIkncJ5z+28YRMeWXHyOzC6W4rwmW7J
zFTVDI2uqlph/pzqC7chblmzZ2x/HITxfTyrjh24YPxEBkbLpCEw4hzllbQE3ECEI3czl4ctl9pm
EWcKSg/+MoFYgQcYMICApfyRu0p6nyMAKeGUNE8U/VhpQphDghsPUZBh7j1Gehs68GzpyWaoyVJs
oqDA0HsKIIJYydDQihxA4WOcSOYN0FiEIgbVhjqzyWGQ2jZaIH4IEc6eRMyI7C1R5niHifJOhvQc
MjXAaycsxaaucy60I6VEHHJlMZwsE122FSYMZeUJXBTZTcaqGKswmITQz6zIxJVEHNNieKbEFJJs
bwbjMZODIGeGlsZcZolRUBSxvWZMp6uxtC+XQcCDuN96iWLE8zg4CfvNthV1G8TdBGQKJxFYnm47
0GdwYTOxZyvac/xqn1ek3j2+Pv6rcFABLcAdgIhgcCI6y9XJfRfHCTWgg/SPlUOGBNnU4kQcZNg6
Msg8xiGXeZkJhK1EDkPZaUOLkUAEhdgc8pzzsdMKvr4LB1IxkQC7v/Z/4q5eN//D+rlHnuGlX1Wf
zS/p8mMa9mz/2W8W/tprltX+7Ta3kzspY36HjZCmfDCd63+26v92GvVu/nfSW8/ZAhjaUpqXVw00
t6p01xxqtXlZDDPnS+9m/L0+F990jp3/w2cpweg2nJuB+r/d/nv138no7f6+b+7X9v+91/4P8f5P
6+H5zySi/EQAI/R8hH9L5k+f8+DMM0ufrn/Uqr+DH6d0ZZVTD8Iw8tEPp4iSP3ecweucc/mNoFPH
4t5GH3/jPavGLt5wibCmgvScdVS61KR5uRzzGSaGynR5kOJnrtu7IXzpHOKmUENwHRqWd0QMkgiX
yLFTdP6DptspunFxOnaAsybZm4ziALXlqITdF+mOIZnlxZ7dW6H8cjf2uxXSTlJL268c0yLHV0VR
4//Pp7EZNDYOWNsM4u3CeEw18Nw2ZeYjpGCbpjMNgJFbs17aApsSGlmcTkbyeAMghkM6vYalFOjS
ga8sxQgnVxHRrHBDVmMa1pGBaKJgZpTUaQlw9vMkdeOG7kSntMymXD24eUykEPwOYpg7qEJa43BK
H5DrrZWX2kMwum0sBpucFn+dB2amTpc0M/r9pjHVThUVTPRI9Q5Hwpph5qeYNqZctm8Ho/uodmk0
XetYkr1XOnAdS45IuU3+KAJn2/54JSydEawFx/UQQpkOjArTarJNu45hBj1OD+BqK+w/3cu14mv7
FP+WoKKUO81a+iq9mDF01S/XVHRDDpcPCKQx3dzJy5Uuc/N6TaTBymfeH44w/IJpOHlD6wI2dLwD
CIT20KcYQXeI9RNfvOSii7G948kBAjCSlrMFB8CiDoephgzCFKcBhgphSFMODgxYcEECYSYx3bd2
twNuyMMZ4v7wZ5duVVVVVVVYHlCD0/kOMz2pu3VVVVVVVP0Ox6GO+FFfoEke/4Jvt87adkfM97Oc
N0g2S9VK+CtFIUIdIUjNu+X0/MR6PUExHWDFCQYREvNu2FhFS724YfFXiImavDjkRw3iI+3A59+b
KjZH8kZJqQ+KQNYKqgHU6l/rv2hxgjv+s/ae64f+OM8UUQquUFFFAg6B/jZpgHgLQHwZbUi/azGs
ZPnQMGmyErT9xIZCUN8f82ibOSjkiUItVWy5QHuTBT6IT7u3A+KD1wn7EgejUSiSEIMSQN05zsm2
GC/kq391zs4rX3RTIzXQVEr62jTsYqAUEkOYHYYmwAKjf+3J2Wq8fqgBXU3ipJSfSfAgEAdK0Nqy
3c0c7YZ0dKDHezCqg6C+CmES0esmqanZCA68K6oEv3SchtUge3TVbftweOt9QRcccTPWc+Un6G0+
Oz7hJIT5UGDSonMZKIiEubEIsRXxOBKc1aP7vIfyj+DXiG0xLgzgJinE8SJmcQgPJFWHqAgTA9xB
2KnmbxAj4sCSHNt66bwHU/b7HpHGXHXZDpRy1TcPYUOJFSFL3TFU8PiIZ8MGMlTWlatdUXp4xBLV
+0+DaXkPvDZd2IQGCH5UJp9n1/fKSX5m096fEK9nYieCqofrussVVWAVqYRxPcbTU5TEv1z3UnZ6
tuVrhXmQ0CSSJED0Vzwhzx5Bs122UqrHTPq6xdj7KhbekY+C4gFwwhZbnAXgDCVhd8F0OJiAIQkz
ZToxEdTkpZAPNHJlnC/5UdQdGT8TBsWvFkHGoOxDHNGRCqiSHEo4sblh0Fxgqqq/uNOj/k2OQvuQ
sCgOF58dZY4tL08BPHL/LPRu4ifCE3YT3m2aNBYxTyaRAO+RMCE1lXEmRCLGNylnp17D6WP86oFM
yqp+R/FkYIhoKhfzhVHgmmxbt2CHD5v5T32wuXMvvAVI9/Zo5SSUNEpeAwM4e/vb/FwDxY+SXA+N
yUfXRi5IazHT1G5E042qUyUHU6N1PRuUwKhSoBgKi3oaD7V0sNrL2GJPcMbbgYMFEW1gzTiivZK7
Cxo6NEFyK46tAgQaTbNJJUVxclujsusDwLwC5ZSphgMQyAwgC+zPp0Y0DoyUR5NpkGunW0U/oL0y
Z8aqQ7uzeGgWmim5XGaIO2OmPcbN8GR1uRZ3BFx2CzzVjXvQ8/UNMSQKKIZqqYjOTKGM6me7gbyr
RalCyO1y/r29x1ACkTgSHJ+cnIYWBiOorjrTbGAmp4bIbh4lUDwKQp/9TIJnmHGYbusGsA4jx3Tp
lFlC5PSw8AMhszRlikWm752hc5eEQVwdpwruyycJoGzJ1DzqjbwvLT77dkrkOUcobYgsdfcnjp3H
O4DTyhU9z3XmpsxgC82LxRQtoD0FNHDjMIipUZDud/NNKHUi9EcI2qXZV5dCD1y5oTiGt9SHAsF6
82cNp0bRrzdjANoNA4gr0HQsLBU4taLd/RitAYP8O2s2kIqF4VYyEhZfJlCYhO/L9Qg8kZPJwkTb
VqARbMM2sEiDQqix/BptHzvZppvM8isUUUznQAc3glAkwpj2fx9shlDSHcczhJyATIhnRt17a4PI
KOl0Kj87CMOQ6ECf+IOoSnDOxezdCSnB7CSlpLEciP0M01MRAqOn41+0ovDuJ07u4vkxn5ARNO4p
Petmj3lZzrPHz/p+Gtjod49oqpgj+FUCNzeMvmHU5qQgqDBBmVRW+W+ns9xI5cIxpCvZZ3SoZ02c
7s8qGtipVEoojyH8xjwFO6JSvtosCEYmGEARIETVM0QhEkkFAOY2JQ0GSIUUEhMJE0sVLLFBNUBB
JNJMJKkJUESAQwlDMRMyESQRQ1EUkmGZBBJBNIlEuI0J3QJuNljVZAGDSQ33GbCwgsh9Q/iXVPxE
INiAAZA25YxmBJ6amKeEpkeEW/0IH7x8Ql8Vlmq8M3O8gnwMn2BxSSJncfehWle9DJJjDoEISd3+
szk9vsXj7KenKu/ywh7nApApJUnisFdIyWDyjOMDeWOwS3NugTKHyih9P+UpTu+uAPjO32wdE1lc
qN3Kvxgzt9ZWI+6oODh32kM7oI3XazlsAoaESpbdhWXzmsPvztECpUBY/eyOJSLIhEsKi84EftAD
+b6z8kCn1p91u79HEF8gfbGLm4y36a94yrx99F8zL7UGYVPOhEYRzzKtrcfJVfzWKdb2uD1sZApu
9LGERhryaqsBlpBUGVWYPR739WsZQT3s7sUY0zyk2veXrOVm+JQxPGYwh0Ho2rIl1mJ6CkQxYUzw
WAc0Q4kBRQ0BPaSHHOk0HJhDUydB8hXgUuws4NSPXbYbTfIfihiSQgdEHuhw7ILQkHvK8o2wjSFl
z3IpC30OCxYm4skF1wheGhbDQqTALE8xaT5dMXgxsKCXomHfzT0J9l0Q/y8L7ey3eerZvy93DRwS
PDbyNxzNIuU8IQy5coNjwgU8QoyNRwYMj2vkbG2GVw7nZterOOcP2fnOfLrjktzEB/OIZFTAzINo
1ylIO7jVisow0hIvq0fvJzJimV5PZjcEoIfaxCAMH9+nA0n8EwYeYOrudkEPkg7dUsEHOM4CTYwx
QVRbCJi088DaQzFvLitxFDMYsvLwiOeUwNJDnNDhI45kvv4kLPLVSY75gM246iVBmN9LDRC4+5qJ
VrmbAuY0JlZoWwhAzFGQHTK5OXqjxg7j15UCtx/N6JcwIQgbnIhxAt+e03MN39T1O8hzk0CHEeP4
cHULBDdXG9M+V0RYtGCBAgQjZ4gKirM0YEN2o3vXk8w7y/lf9XDHf0Lba4Dk8PidUSMgQhB1QEoK
CRHT5GusheklhCR8GBHyYh+7jiwRGQmEMQGQYEJMPPOIqEUSMwJzgkJICfWQUXMygdAmAa8Gi8m4
HAvMRBIlYbQEgTNtwllE7lRxTcwMFdS8L2GJYOt3QAqqoiLYUN7b5hYIKEiQDI1cNMbrZgklZSYF
qjFRoMiaFjAgXCSCTB6nOMQbN+I8aMFDzDL9fvOI9P2CdeTPKhJLXeWmUkvUthCCqq60MSpYwlOl
2ftU9p0D+RfTPdvlqiY9vKAh5exbw9B6AP0JzROFQngIHBRVQANoxuN5qR4jhg7y9yAB5iA3mwbH
SePywhCGuPt+H4v21mOF1KeUCJ/hjN3ejrCEIf1hFoxb+0P7Wn/lZV+g/B2bxA/wCbRC6ddzV3W9
IE1BUE+sFBHpM8ANLvFejf64d3eSvdbvTQoU3U/OyZl5qIxVx6Hyd4Ph9Ya1x9TfaFKEVMiCp0jJ
ThjaYXxhPojQRCuYp9hQvS/Hr/pmfLthb57H4Rv272Rtbaeq4Kiaw1PgP3SNWTejmnrcuq+utfJ6
eeGBZ/Nz+7QrNTQXUYyOkkOZjOLEL1IozSJArHqhVtXQZ1lqZVIpJdzn4Nl5vlaf4sBjsMjenYmR
sW2l9qtEUu3F517TaAjuvHdM9RWMKIgMYB2PZ5aMc+c6/lTfCqhm0CLAc4AbMNOSpXb9hGmCClSG
2lt9gfTUsI641TgFYlMHoOytqOXl5E0KBWYGKULrccL8bbEopi1FM97ZC5IUKHdiSDje45rquU0X
iQluu/5JChqwh9eY/PHQCDzNDUzddeDhcZlbk6aYbWSuz4X6mEenZYg7MXEwUrIoTmRsWVL3LyP+
LCSQvpLB87L0L1tFKDhIurLygYFQVBQgDHAUU6V+o1T6IHFFwdstNdGzMKK7hpemLD6V51TJ160o
tdhWlQy99B5RZFSm+yhtOgvcCAX1DDgc03koiQRSJJgciHiiQPw/K41aEDwoZy+XmW7MdwmZZHkQ
IdYPGfkw3ngbPl8jv7ut9epETJQVREDEqMggYnIY8Z1mDM0VOia4A8XfSeSj5kHYI3R/q8QbXd0u
V8HMPEdl4hV4nLlQwMBBeIjAoEpwi4pJkJR6PWcTKpN7+6KXRrFLRyCBUqS9X9yuAElRDdFfeZVt
hO4v8fohpkWs0XI9EQvA4nEsHfhisoKs7/scIFZ9zB8xewmaoX+T2aX4Ytlvox1BY87SxQokxZoo
5mgnB4vR+4uwUVNhUSDiDDXGBYEhQcsGBhd4TRB0FJISLeuxClb9GW3oPuKbs7gtK7hHCSc1FEUF
BQwwXCOcK9hytMqEhE1C/LBCguIT9ksHsZpR3OqCbcGQRwxS1piiSL/S6CImahBQCuZiTEgF9V1T
LY3tMwKiYF89ENImkEd8R2322Tmn74F9akVca61o7wT1ragTaE6QvL5ExL5mNoJWShbe8DvqTcIF
2quNy7nQR0YoMh04NZx1wgkf5LPRk4abnOakIqiIALs3lZdkRCwLGpro8qz3q8oLJ79XbKHAhqvv
lx/edAG8+1gGioS1ihLgGNJmYJwcNQ4jbA2JXl5E7YA/QZCG0HQM61247dWzLkxiahvJZ/f/nxPs
6sCEITJAkwdQA5zz3cMFl9wsrwDSM3Xpzeu52Va4EihCNnMwcgaD1xJrYdwwyTg4IWGvZizOFwoR
ep0d0vAvd0yv6oSeU7PoOO42dkvVHvFASWiJOBBPsQUGLHuV4wU5oJ0nIOY3nIuZhcMk6x5z4SsR
AEFNxh02RRDvEVAPQa40GBCk1qvxfjFy+jdSRmzQ5OjvGSvHmsuXLYcampxriefT1aqUIu3bPwIW
gXH6XJKuVLEkiEIQhEREREREREREacVobraT8DgcNbY+fQ/H54DquS+IyiKapXT92YV8lgnkIcCL
xNZCNvLpiykUxVlm45Ppgcih0Glgz0GRd1wFMIHvOTApsi/eGEoQR2FsgQZ2GMZgxQa6kIcjFBjB
kGkoNJTDg6HC6kOChwccBFpkGGurIdKX0eS3oULwdAsJEQiOTsFpNbm2Gh+H000zoODHMYRGBdEp
itcIlQ9F2yLiFHnB6R85tybOzS+Z4LGINaVgooKfnSZCyepOEdHKA8MMFkhJZ1X1ffrU51k8eXSm
6CNBqAuf0I6On9w+44FoRHN/Hh+eTBktoMaKblP+PPI3hqwc9OUKRnXGG2dJLC/OUpC+MJdlMux0
54K4Ez9dMzD1il2TQIARi74gFBEANDfQppeZ6+N72ixRZ2Qpd1J+B1IuiEum2ge4HGfAbHTIc0HQ
yMOmYONxAHHMVYURM7jC5E62fFm2UzSrzzFYOYxbo46641wzPnZw5pBAcOvopPUMN5/d84Z1IOnH
r3TP4ixs8ZE/FDi8/SvGRKEZyetRcCnGl0glUn4WPZRGOw6eIpsEULjI6B9APPKu4eY0IOwjrIMC
XoMMCNxw+Zh0acOjTy/Zzy8qiqqqqqvl8nNSSSSSSSSSSSyHr28YqX9vux0L65mNnMpJzjUr4OMg
arx2vLEaZjurj3wWfcenr0ZfumNTU2mwwuhNDMaVbT1fDSGyUfipEP+mEihSg/ehDWeEAG+fhGQU
PiPhYg5IU+ODpOkzbnSaOvcYAeKe6EOOBiR6jsz65eSfLHGcXJa8veG7Q7t9uDiGWKIKGBDWsFiB
Vo5RGx3TtndmeTOdW1ObdVQqb1NT7BTpwLgxBUwMDsGHHMQUUQZSIoosxmRJgzDI5AzQwJjm7p3r
grnqUYymIRBQj496cQ8hzBDzHgGu7xqRLBCpJKxpDloHGJUNNvjXI6bNuUD4+C1Kh3kTM9IhAjzx
No9Q66hkl5hn8/QcLa3NwHSpEMy0GsQe0y0ZQvDAeEb8ofBVcGkQOXmolGYf2/v2zDNZq8o0h0zo
zdlhq3CKhouId3sfF0Ht/KsZcelh+pJpBZh8Xr9M22Kq9dT/EMNdOO+NxuLTAjvVdRMrEvwoXXZL
F0ZiG1EQAk0IZmcEImtrwkyqmIqJiXMUZGDlkZ6Y4UbC6Fa3Wz5zT4DFR5IsQVOpkwL+ucwjewxC
kecxaOvfgPl6Oc0AiXKabaI2PMZjzIByLxfPpv03bt/T8+45dx5oIb0uAn1DBQFIVBRYDB3A8Uqd
BxkaJo8/pVDaCjAQksDDKtTSEyhAsMjCEqPlQYfOEY+p9Z2aZ8u9A8JYrTRCDzntsoFlE29HfyHm
gyAbYJjdy67zKzz1LmvY5hDolE3WIi8YE84i7+7FLkWESIb5+0cumbS4TebmEV8hki4ODLZGJaRw
ke04DBAlV0PeWxEkYBy9Zmqic+4qDGlMFt5mVpIG7AIZnAxmurZV7cmRxujK5euJzmdBeBEP9MQp
JgRslxZc8is68aMdQ0dtuIln408nSR9XGG8gfV6zetipeJhohR65BeTLeEwzdzZpRPQOhhtI3nvi
fDMwtonFJHpjYKV0zaAyE8om3JQO8nbb84dzZ8vYlRjDto3xtBXdiQ2WCIc1HSBHdgITjazMxJTr
VJfwLJzac5VM3zV6IISGIAVFREgTvF43ki6ibCyTg4IY7JpZuBxyie5R/DuestjPl6rfDgqBCDNj
gNR0BDhBYg0QOIVYILBxpMFlsGSZhx2FnX8Ds6MEAuAeghDcG8ywkzTMgUIkIO4rvBCJQEpxGwVP
J4wcS4TE7Xqz0Gi93Ap4kGMPKdKHhV8hswKl4pfcNZAgk2GAonQu+/cJtXYpcWk60IohSk7QJkIb
JmWbDTU2uyLorJuZkyxxhNoShGbcHqTzR2iIokxRRRRSAw/tQdIC1NgV9Dw3JtfT4nvMAvRTejVe
XdAgu6V8JvYytvvZbgkFZQ4Cg44DExjyIwUbFsEw7bHJNB7SCehywaaRGTkgLC2YqnC+rEfPS9Lu
aJP388ZSDQrtwgheioiSGRhCkzHcRkSRtGOpTicknXAJzYZSgoQe+xN6xJFww6amfTaUrnnUijKi
wFZFyVktLB7+Re6rGlJVPc+SDIiRDeChxCZUDETgXRNbcodB5j8R93V935to8uKbwq98nB4n3N9l
Yzn4RioVVVVmCsooTeBCTa2Yntdjl++/8/2/6/9tzMIfeHE2n1/ZL2xf+Dxl9Rn48LO7bUP+Ff3D
/4EHYcsg0BWWJ3BAA5TrUyRUdm5ogiEw5/0BMJqBPq8JRC/mHU+RcCguAmAYAoD7b2Js40HIvZOM
zBpyOEXFbFEiTTZsxRxDreLxSjCmMnFxlrLh4UZ/48lWuvh/9bzP1LTaRuW+sWxCQe7hyl4wa3eU
23MJJoYcxBHXn6MZpzbhrIG0UQ8gfHDw+IZ78QZUjpksP8MbcsmGepmH9PVcOBH4Y/9IHnW4a5a8
S4wSKHwk3R2MZ6MHSonr5MhT1phVnE6V1UP5FAmqdanpFPxBy7tydefCK1lIKdKI90fmKiR+pY1B
zTVVQFUREX7N8dtvezM3ua5pN72o3zNU1TVjb7t5wkvkNP25DFYsM+bBu8vHz8SqMVu4QssnY04x
I9TBGBzt3G1eBvc5PhFU+wRUOJslznzpXXOi0pSlKUqqKqVO7u03cnpibtdu3R4i7C/kQlUVQhrG
t/vXdKCzgnBRl4dgbF7TqyzOOVwp+L1sf4Y1jyKL4Zwye3URTUGT3aA9sHuU4mIiHl9P8H4f3v8H
0+r1MD61Wta1jKznOc5zVVGOkpWzMfQyWE8MN+XVs6n1uJ1P/h2/99U7Y4VX7ajeZb94CXqbxDf2
8DS86i/DK8bbLHKLu7tk2TYti2LYNU1T6Tj61rWtaxlZznOc51obxs2xrYHd2M9A5M82xs6Tsw5V
7MpgZZBlB4Qd3doNi2LYNg2DVNUrNUq61rWtXhYxjGMa1q+MNoQzNlM1s1iM5KNdu99GawnUt2Ub
oLXffdfEllLa2eTtjimOI91XMSSYkUvqGyF2YGVo2WeqVwsROvKm/KuRGmHn1bp6czOB29cOGXp4
ULzo6HdhaQhszswFbzdV/UY621DO7XqdNxHzL+PXYvgIUzlVPBwNwFCHBCEe0RoDUjWCNDgOUIEI
HBBs7GShpJHJESIoKP2CEoSCJsMBzMUkRKAw4jniaDjkxS0cwNmTkRZY4ZEECESOYOgdDRJYiBqP
5xyAoOampMmSJFQmBk5UYjEghTVyYKERCQwxoWF7Orrw0UIc+vfy5dPb4EIikIwn3nbGZnCqZ1Wq
nGXke8J7lakxqqiLvozH3fzH6/t+ry9j+/+PZ80IL2dxQ4Jys48+car4nSqyjsgBWKqKrhZLt8u2
xaKjXnmxh1mJjYeNEyLIUa9fFr49RVQMMyrvh0G2MVlsdXIF8yPt6TLurccjh4mhLcO5skkJRIye
KOotr5QSCyG2MRmx0Mwi2Ob1KVR1zm+k6ta6zI6jzd8yu1NlNAfWJyWkoOmBfg56fU8AhWKuY2bJ
dZBOmc+VWJLN2aDv0iv0a2GVZFd/XhLl0cvUdKF/nzKbMe2Vu5WT27qE8QvEv9E+Z8vYFbdsl2vq
0sRpZCZPhH39K7+0rkNGgs6dpquvaYXTs6Sb9UubTSpQJJSZnMuqelGpchMJyqiu7p3VXDvUvmnJ
MdtNMlOxxEFwtRqK6IMGZeLXKx707f7PcOeju/Zstq/LokEebV7KDyDcodzeZF21bdPPyiJ5y5mY
zL99S5mzLWk8MBRbWgy2WQ1gbK7HwdWJFaWPqyS0siZXXTpLyw/k7y+n7Zd/N2KvYoMRHlx30Ybg
N8cTWk7y/dvu9cXSH8w6zlCOxbqAUWbXD1m0JOKDt0pdL8t1HFud19arb32fnFl+jhCon29eJki+
1ifVnS7thJSupVFP8X7BVBx8EukE7ugJKqkJoRIOTHs79/PtapEQrTjoWFoyiqKp4S7S41SvZNO/
PO80NvHCy3v68x/0RcOcN4+AvDBhxx7L8Pm7HIU3GFtevJ+MN2GBXQu3T5PgQ1t0KqXVwxhUSh2k
VE1EiogoVdelyZvaMzFsK17PZtobdj1J6MHWFceGmOyCJgoIpJB3oyJIfrJt1a9D+G2Tr2YcLKpM
1s4NFtZd+PhUlYURiuFUqHVb1VMzG2PwaZtst59bDshmkDeSWKqYHRpVGDG6zWqEoT0+3CitXszS
VG2YUtMtcELBTCbqTaWEDoWZE6cmsUWknY4X8NiTI5WUo9ZeVEaZj399dl19AqFIdgz6RN6ldLNm
xaipSdjqZ5Z11z0r68Orb0ZlV1bMrpgb+znAaGGIm0rVMMML8Mai41QLCw47JHX8JdWT7tdl+O/A
jduv5Wkm4b4dHlPKmhtJIG4HEOgLSu2z7oae7yeOwobOihST4VeKXt8nzvFuec8cHueVYtVb1187
7IcXSlRbOUYbm1DS+yFzzs1hR7USMGuR6ykYBO4539monDjA6r63NXTJzZ8cxyJsVdmO3Y726v5+
G/ipwUzZhmMLlHYTS7EdBYWdZu7Y8i5SSxd/Tlz2R51z3vKz6gdul43vp2jRL/C9LwLenJGU7NoX
rr1eyZ5PQ+inZIY2Xonr9UW115wPp6TTQ5p4Jj8bbYHnhuo8CkwxaqDbPY5ASPVH8tSId5HCANCE
iRglqohZrMBPvBHJCppAqPIcXQG/022tdqeQPHVBmkoAF1iqPV4tfEq5V/bl+/qct1w6Obr3GJAI
WcjqiVVd3X12IcrM9t9J9dsTq+g+U16b+NJ24Q+W7kYqIaacOuTy6elpy5+Dqx5uDoRlB8vIk2ZJ
uyCnJU1Z+DNLnSdVRuL4wVWh1SEhTycI/MBxvqjybLaxBP/cq52ekgh4K+AViLQNbLP37XmeaeXd
6IwRd6t69+jpcPAwDnkE5YOiD5VtLqkdlT0xtgD9UiZb4w2KFeghdik7h+2QTpJPFc7pYHhB5Y3Q
cVnyYEhHHGEaQ72R0BnquqXZLz2Mqb5NUh3kFFH8Gdm3UboHc3ri4QHykOpQPISdRSpUkRynrmzs
PInlucOVJfx1zxMt6VHAEPhqIFi/bE9GYgz6j00Iav7y9viq8/CSSSXq43CBVXwW+RIW/TPn4b/H
G/SuwUZULkRQxZlXJRiTHVJiaL1yYnqhuQRYQl0i4bfLSSaqZrPbB9u1uxRptPeFmTbPsM5TTNhs
FNXCLwV2dzZHaSMe+jusM8qpUbI6Q6zG/bPS5nyh/k7Si0ei5Vr8HdkqTJPzxMyjon43Eef9ePFe
/TH3pvNSbTYTdGJGToBMSsLtIZbmpMmcGMrL62+iovmfDdi60l0caqtpdtr3rkKVquOcC+EaV4wq
g0IzldTKMnmY5RH6dbMliYaTfTOT2VSZ7o7hpwE5LtVMamFVhSPDdtWqNi48lk9ioSUM8rOJEy4c
XZN5daYWH+GXvyiIyjUcc+yOt9anjnEMOmmx1MvGUexHDc9YLhb90kS5oTt58R1QteUMdVren6/r
Qlq2dV13EJuy0iFK6/DVTxLtRLkx1nCbZfeHJFahN1Tps9I6SZVpuPZ0nfHVMuaJhHMsmpcS/eo6
jpJqgxGNIsFpY7+uUewvlJKlDmcFwaCNbPTbVUhs8L9nCMjTflnCeBJrBaqEV6oVrOLpgwNiqMlB
CA/bEg6T5TcsWhh3OUsj9XHfKr7pw3nLkLhR5pnPzRrh5VrKhCRL9tSHjrfpqmOV+G8fteTzyOKB
mPgovpzzP4KOrjmB8JxCFlF5Z852S9207Sikfk038/RjmiBt27fg/14G7APwFEJHLusseXBcez+M
9cO7zZae7+qvYyoHYPB8fz+POR8gqQ/sP5ioP+7/uYiiXKcQZhOgTmolH+H74DhAsFX/Oqk3whSM
HUn9giZYhu6aMFhlLH+seyvsJ2PqQS+ng8hykJDEII9cdYrYiFQa1V5oPZwr6ztwF4XA6ksdRxPN
X0eyufBpIRdfTQXW7FEhEAp1sJY/nAKenr6e+3Va3SQJ1BF80GoJCOn2fl3/1Jxa4B5655Y0Vqff
EMF98Rswx/DMJQXufGWSvnSHXrmYlGdlXJt3303hcbVEzHutK9GZG7Xzg4RcNHE649PsgmCTCLj/
HAUi0KCDp0e7mpeBIE9kuzG5mIrdNLQ6+oOtJ5UH9+KH5vs7PJGS/TmVvkTor8fa1w+36ey7uGGK
+ubndcmLfkrzTX6/A3kVfr9k/RzUc0kMrfXheLhKeIwxc7+Fws4qfZ9LW/YhNRX23UeErCMIG+xq
+VUMtkaxT+o8/1+QTxKMjMeTpvKjw3phD56CVig9VfxYCKqOno2DLy/N4Jw7q8NTRqp1f4KkOiMb
rE/kz/oKTgxIed9sRNPf7A7PZs2/WS+IN7gGooMEknOG46tQjWhmpAPyFqmgMG7NPy2LVuDa25Wv
kCBNdf9fH+CInhcbrCkqtBx2+z73+Hzf8fhTq78yqAk+LJMb+zjW9bR6lKCDC0LLSBcDm6ExEMdc
DKIyNohCqDcc5lD7iqvCgkjK2eyqkDY7y/z8x7/h8FPJVyuY2IbX9PV7paZCLjQc6GTPfv9+Fe6s
rpwxVwsURURYIqwWK3ZDi/FzAY1p/+jRn9PByGN5PsvFwnORWRtQvV3iQRgdlLEYiO4qCcjsvzdT
gOJw4cOA5ft21VV2OiPDqIwLX6IF6b1ENPBoa3VItdS4NDxh5pUe6HBPDmzluXrSxyCmalfADw47
ZF8pbB3BsxZBKdQoVKqpqRQ7cQ9PqPv2/vv8pzaVWKGlp6JKKwlwxJB+sIiBhjTYLQxp0JXSDWu3
Pi7jjs8xjgeYwwvHxAcQNA0X4N2zy725XxQHDi7v8PMivwkVP8mgZYjRMtEixVIUEUNBBJVSMVFJ
TBM1BVNRElM1IUlBiKX87iO2uRhg1ADSxI5lERQUMNMETTBEkJMQYoRA5mIVTQIRkUFCxIUoESFA
pYA5PxLcN0S2wbMDLNxCgMNwmFKRDIMqqYiSDMwhpSywQmBIqGJKoYICCRImCkAqEzEyVhKlWZCI
5hgW4hlUBbiIOMSSNC6Y1YqmQBmCYMQFCUrEi0ExjZCSRBIMStCWYIEYOJQkGYlI5BSkQtC0IGZh
ElCSGTSOJ7EmQ7mGMgBklBkIGM2YZAFCUhEqkw5lZQJVRIwSMkKrShQqJSrBFAhEgFKUAEEgD7Eq
fxkppyaCsWYYZURtxAE4Sin2QoflJ6fIYL40MaWIYJApkZaqgRwxMTcI00Daj+klAxalEIlVjSAp
oo+kmiLGSzAYC5zA1iIcsJck2HYAReiATCQpBSh6ZASIXrdtdwxWlRtxAKDJT3kRyVVaRVPMn54F
T1qEDneAGhiRWCrByIS4toAD6GWRtuIw5g4Q5KxLRG5zcU5Jpwwdl5FIg6WTLbC84c0KUF5w4Giu
yRMocSIIiEcH0CE2KCHlHQZhmGo9QdyCbCVbiOS/8KAy2QAD2z03xeJe7zIqdhCgOyqi5AZNABQr
jFAZAiFAJkPINgGlHYVyQFzGGDARPEjyH3lFB5Ar48YgcJVT2kOpQoThG+VHgbDAHhAAAHNCAoe0
1/A32KboGnkhrEV++bc1k85T3xKUqVRYH8/Z8/9Hm/P6oL/kn/TGHp4JmcGTsZVvyMVTN8BEq234
3fKME3OlZYcBfUgOAm4zBmRyAqWnty43bdzYVKmZQBxCxUBVh0957s+cSX9nDv6PSDDzcY+lIQ9k
xexFoOcw3/qnj/5f6tT6c0NR2U7HWXXZDCHTkWUSFgzoLMtAo7/iuGXfhUNc/Jz0BnEIYe0ylABf
PFaFsWyPTt/Tvv1d5tW03DDUnXD90f3S6baFMgfjAj52pWktsJmgowDVikkWsdVwfTszOi+VnOSG
lkUqFieT+VOWGkoEe5BiBUgkQ9dDBfbF6W2Md6l0GVCcRPug1mlOf7vK3jxKYeWEemPEXyXhkuCo
leclCtbn8TLe/5XQPAgGefVZ85NwsP3Hnsg24NSmwjCccMXHyH9759h6Nmp5Ped+z33Ztw9VXo2+
vs6l/PVNbN2PwUwuh/g9MJxkaVM/XKNUCDWTKRkk47Wv6mgbNlBycLt/eQznO+0ZGZaPW0Xd7/OQ
w0wHpZlTDfbO5muVyiQdxRl2Afq90imrW72O1tHxXRR+OF5tPS3VdMdefahxMSukJF+A1GoHWuUP
F01zd3UeR+E3LIon68PehvbJsTE9FLwSAlxdjk1BEv3eHaSQok0r2QnabSW60l7MNzw580OGwI0A
txrqPIqSSEH3ZkUddLWnX3FQd5xqO49/ijVqLRN+y2CWPwMOGKa8lXNC26N5BS5brk1jJe6fK/HM
uo9a2p+17lOSIJKyDYEWjwRFAHWX7dxY0PbmQkP51985a32KN/zZSeH9PdcSdPSnXhQ939y04uLM
Fv9FS19UNM87y5cX42BA2gdbTP+5whEztNG04I3F/ZT/2T53iy3KQKPamyjgJarYFUObJzhqroUD
qHeQdejsbAtCfXi5Ghlrza/7Gu3/W71PCSiqaEqkoImSiakSamGkgZImiKSCZuWDJNVVBVI1XIDG
KopmiqgJJAqPWIj7TdYrkPAw1WSAkogliZilYpFQWaAGCulyiwZVRDgdP0vx4+X+3+i6/fLeU3QG
cl8Z8lMvVUhH1bqp8v0wzEKS+e/11EV9fRCZvNYHqx1+WVfC66m6lbTlDsv999Vnt22/CMtN3GN5
f7/VD8l0d9TVBwjzIb7Zvjv3spF+bv4xLKUpP5/fw393cq2btbO4hxyvjjFMVxhPn0G76Lq5b2rX
Tfq9GtnQ6tq395M187J5c9/Tpp52fux9OLnClpBhaSXsNfAupXPd9e/JurZhpY1l8NKiHcbaeGNe
vusGOvZhfv4KsuLCNdsh34cehtY02UvnJd3IV5Vb66tvHjXOvllxa7A5uOzdv9WtUsp7dOi2+bWr
xJujyil7nr6WSHVniUOW/De+Uy85WFbV7oF0+KyhKIRR0EYGMzykIeiD1T2Xlet/ZfLn7d+3lPYq
U87aHXo47KssTTMitXRvdeENldKaS84j4ZdYzM848c9ejvm9A+DDmPhPGfsuofph0l/cV8GKbONV
I9WU8tI3Pi+jZlfyF3X43RWwdbLtlsRuLj4TOV5O2G3ExabZBsME1TiVevlUl+mOfGzCN1nkh5Xv
fYbs8LqyMaN5H57KrdDlv36WhS8hNIcItuWxtr7qte+pJaV17NlnHhIx6N1lF0jToqLTS69L73w4
IGkr5iEpwahRYmOUoUXq+eGUSzYlir/bCFWNt61iiKDF6xTA301c2nGS0qrr53HF7Ylld+dcrJuq
UuZnY3TjCCRbfVfeXTtLV4I3sc/aVHbHuseqc9iJhzCxEmU6JFPDzS1t2YVbGW7bWRvVK3ulbxfL
bbPIrMl13bclhZfUr22YXNCTzz3M1wI4XHDjtropTrOT2rx565TvrHlVRiDqgsixdSNVHVmoaQv2
wi3L0c6ZXabmNl3CD2mC7sjF9sWPRZTM/k5fGr9OCLKvvx6YXD+XXTClyeealod5Z1scZ4P3s61r
ylBT2rRTGXTMXKKrSN2zZtxx4Q5vSbbG2KNlcTk49HLWENzGuNgSL93Mrd7wsRYJNUKZ1SOp4rZ2
GdQ+HKV++d2tW6u2emzGy6elcKatKdipgWoauiWLVK174zqwyaOCxkXNrUmFss3wnE7IvpUbYEJ1
+SMyOvR2fL4Nv429eMTnLlniXI/B/nFruSZ2eV8bK+vk2ERoj09IjWCO8zEdbLXT3O1qEY3rq93P
qeMXXKswMKuZuzXY2NuNamEbnQtjUq1Y5ldjiE56z0wsGpHs71pGz064tV386++XFJbOK2Wqut9n
X3dNLd++yytpdbZT4x3zpyhnNuNe+Nbd+BrdVdW3Nrm5xYwZVJ0uaJe8IIsJrusLIV1rY9z2WXlL
675569rV9Ose7lvaJpfXWuphkiYYnW6vRYQgomby2ZzrbdXTSpRYLO40aeqXvt3Td7ltHN/Lj179
cnTT/Y8w+OHdOuCYoSIXkhzpovJN9c1l7vTkrnmsnGPPdBwTftu439HXEez49Pb3xfTbBi71nrFX
XGkQjHfSFS50zjjFdczfxONS/Wu2/XSeiRnp7ltcIwYe8xzWfZOsNNmzPl4edkxmph5hCOBFmyUV
I1Zwk9hJ4WOHzbsl5KefvMKoW5PoX890HXCfpBiGfXyxjHh9O+OHxe4q2d52+tvczLxry5/L2efz
u8VtqSum64i5jCSTxbsuhC7vtcjpOnbXVrWZyawqu45wMMUXM9RlSxM26HDYWRi4XFUti5V8qPVc
eYZ6raoG9aLxqv0Vp3ubqbS2QjsbHi4whZC9pcbdIWXRqaFa0KlL5S0ZAlFbclG9NTLIotrElO/v
VW+rPWXGq/D2KmPpvq8DGnkIFmGa7G6JpLqxllZc01Za8+KuQrqk+Zmt3YWVwodtzuHGLcrb7i6z
HqRCLAnjV4qqoqkEy2Y4XQuNuDdOn0x6Y+/0v9V2jIly7oS+C9dyO8y3KXnYXrKaNCV86ospsVKK
slsax47PN0Wf205HhXV0TutKZ/e3rq+Gy70wx32RqtpjhGGrbJVbXGXpRNZljHTN5NBiUPm4wLVq
WrbYxRT1RyeiU6G1hC7Gsao8bGnkSGdbWzrlKCYXUgURnt7xgcV2mkpOM8aQ2MNGEXSeWesQeZ0n
cRnjjOK9W5+4N33PNvHOq1Wun7Jwz34NdpIKBYbJGu2npnEPhHO1ZUjsdmPwgo+7pZ0oZcOaHOTp
xDSmnzdsq9D8LGvj67rPV3izfTV8dVkyWRxvrQqgRrBdQjrZB9+mydZjXhhUIHgqPCReki1ZrJc6
2ZhrXgVXWrZWdxzby8cSt9FadjOHZVFpQcaqr7rPHpqAcRN/bXQRLVK0HS0II9AiO3vnZ3xh37KT
b2QQbp6lIdndq2h5zN4jFNfy+chkrSIEkXrVDVT9nhcuVWD6EX0ey0Ta6TxxX4Xwi2TXpoXYFKSw
7xva4KtYTBhm3c337x64RZD8wPUD6B7+7/ciqaWhcVgVVAe1mXmpdUNGNa2e+urjw9ZGFldudcN/
TXfO7p7KsKU9Xq692Ow3qsMC7bvkmOsjth7e6zP8R5qt22xOvuhEqWUetniYcsobNttlpg6Zxyt5
u1125CKl9z93fxfHvwOUNcMo635w9dlXr3000pA6DnsD3r58UkpXldJMZM9oqNYoi0jg4vA0Zd6z
4UqpXEK1qWF00hD39BZTa3qbdPYtpevtfN2Tfhg8RZqQz69uUTrRPkqqC1iecw8gObC/ucNsgtAz
NsQMVirUOOCLZ7DRVQJvmu2jNh+qD+WP17gdS0THwg8xpTwwa3TfT0PRJfQh/n27+W05D1J0qDCE
3+6CfWHBKCARVAw52bIjYbwiyJb+CTMXlJo2uq5aGl2eYIGTBpnBIuxDRUhZZrLM9HkAHmQDSCaB
WLq1GoFkhDVT71+/nLhh3Fib8PJr7zxMUmhtcMzMGdpTBylHB9NEZceiNmbOZrA0lRobc3IenlKt
G2kh+cQksaF2IgnaKNHHYcx8mQZJH1iB5lJsghMR1FFKVKt00+JIMFuNmabhMVA1URVJA77H5wI2
14ELAbxhxaIHUNFhdYMYHmB6oPIhcBcsbQQMA5DpUi6OeC8CAOYdSmYGY3dElEepgUHQdOQlaYoi
qm5TRATw8LqqNlBtq+yDFSwu9v2uZ312ZBgoJiIPm7L3nu+DKHnx0XMvOKN4YXe5Ne8quvfTbOpR
xY5PzTGMmSK0SoBJnJMQpdkmlp2aBkRr0T18DNMzQIAoaUzMTwQ5bvjI4RxDKZgiKq99zTneHLA5
zE5cbjOYOhfIM7vPA0g6YlKLwkVHCtXdVELIjgYLeJTM3SZBu84Xtqb3NTrlzobgPG38Gq0ay1tM
Y4i0VMIjAdjdD2zlJE/OA6kujIT33ol7krjs4TAQHQmYpRkTIU4ZmBGWHiwNjuKct3eamHCzMxsZ
apoSnFJMI6KmgkMslyTCJYwg9b3vHpsyoIwxZUeEOfgbKd4Qvs7M7/TLv0t3rVPLXJxbKEooxQib
v7v8aDI2fJY3UimAhC9tqrFIDHSkOvfA2tR/A+77/N3Ojb3wOikDdwfzJds2bNtipGn5qa/4I43T
8eS/Q28F2h343PQQar+7t7+dm+7tuyt+nt9tpL8Khd6ES7lodGKDwKrqT2OOfEX0EyUYFRBVZr18
fTExxq18uJ513LT9/g5X/BU/sRO9E8/327HyN+l+s7+2eju7NvPMZ3liTzKS7YLBAiwOu75p7I8D
pDtPGzAi1d70sW6ohKVYQNfk5lZSWbvBc1lWbHRfP1lydDYlZRTFi4m9a3SIHeXlVVebVNZXYYCn
53gV1m3htt44yNhnim6JjfJ71oTeRHGTWztGXMug5ZsrB7yyvSFvTw1hKE7beGbmN5atWbt2M+MS
M7jElOD+bbZrQlP+3HJ7hVLrnYv2RlVldtpzveUjbpTzzlcYvmLAuxWJgqVkrUKscQmYtTHfWGMJ
2X3avhJsMJ45qQ0uso5XnMLaoVIuHSZzE10msb+lonmq3XloftKtpU2UHJ6rdeeEub6Uzs43+w/q
T2CqogKigKKKkwyEShTSNKJQtIxTfVt/iN5dJEI/T+PO4NU0AEkIARyr5h1ovH46tY2RXYTYkBMQ
RgY/2v2XD/OCo3A0xBxyddBdpJgRpIIuRQ0I/l+yB+dgzPak08koB2Haf4TqsLPt/D5y/4mqgpFb
v4LA/ugY+uN5IWh4k/7M+rGtqOHVCW/WmRzcBAhAXBi/5XMIJIg7b4T8Dsf6P2h5lP8JWcZr5RKy
o1f2EE/uIL+0yogUjzFnegagNoKYDX+H1fzHRrwMf5ldidg9qRMfrjvDUiCioiqYKYWgiKJkqIJi
iKISSqaopIgqmJoooImmp8n+fG+AHNKbrE8gz/mhDktfQO7RvqrSDINV9aMZFZa9aqJmDE/a0AJx
Q/w/lozH6k1453WB3hE2ggpvDTaxWrIfz1YS/zrrNaCTTeXJuDyeSwS5X8UDr8quwD1u/iWTPQjC
D86pX2FvyJZ/Q0FFvvwBISEwS1wuJ6rf30WLf6j/e5Z/pirIAUISDVKYD+nZpX7EZVrlW8dgRH+I
aTaWtRUrsce48YNf7fn28IB8dF5Ia8l3/pPFLS9gCWw/w/1Mb9jmW12DldGS29ZIlJAkNm0GMDqS
JnUNQRtcOejSmKzc1qW/WXn2ejoDG9CPXNCOmHhtIHIYnjiQgQgUUFi3xaBq86pX1/k/4/Ze9Dju
5fu/rOwwaHMU9A2ePRRyS2JHmESZDOgNp/fDlF37gLchvHS/ydw8DgNFRUFsGNSLTKPDwZJIiiKr
68yiiqoiKoi6gN4HrwB/czQmHzfAdUAPD7fyv3KCfMzCkEw+5F/ptr8Ckf5/4XjqBqfcHiwf1Cx1
+KoADuyOw4O0eyDnvRPqAn/gDr6jtP29PcrMQRUKKON4/O4GX2hX8PA5/YeuNpP6ebORF60w40g9
5uDQg1AHY5yqfEju115+iMKKirN3dQ+GCh5jp9HZejvjJRR8eyIs+PEpaqgz9/AmdNJUxaSNt8Iy
k2GQSMPAPU20PcLWImZm1aahwttMiEYSRXJIE0+btAqsLaw58fuZV6lBXBgPgv2IRVvioH9B+s/W
ZXkk/T9f2fMbpHtKoK3Jm/R8lPNX8cqvp0yWUv/xoGVPuJ0YyD3oKH0BOPxkUZSCiq1joef9Tfr9
7aCsGQHUqIykE5ok3UkxP4XCPP8PrnWjh9KG4+fvkaoGoeXG5DH6M/VCzgjmsygfw/XvYVUCRShM
TKQxiYIU4eB/D2mFME6OuEDMF6DozyCx4yl4G6bAf2QPjQNHvSJiBmKqdq3p8uMjX2iWboU4AXB7
EWDAyC+xVVZ+6tfsl6n6WER+fochtJTg0Xp5fr7ONyXIYz+JyMxdu4bGop9SWMCYPqhEkhPzDcC2
7aRkjCgK3MtdXiUNofB6H8Z5S7wUsHJM28DmMbPIXM7Fzbv+r9OriEAjCEiyRIRkkCEve9VXdy3G
2BW8T7GCfKYDG44AVvO7V0kYnpOd17qpaiKqY6TMwiMckkUGKSWxug2yBBkkbbYxtskjG3JG5JJP
cyVzwuPjA0FIxvd28bbNURc73arMyKqqqiKtyMyqrDDGqiOzDCIqqqoipOsNlO7kSJJJKFsyaboj
oxbOIR87WR27SzpQX7sT6jI+ZqZJwoq8hnse+8sqAWgQiOcUuuAzg/amwLFjTmzEmZfiMaPdogQj
IK7HQMAxYxeVPaLJiGmjr/aF9GL6vr9S+w9adZAatR6Kr4CFi8+cos/uGMCJTYj5J/J032DdVIP0
/kPRd8u3O+k0+S/z3mybLvPtyt+2IGjs2OUN0U3SETcQhFI/HgPY3+RS6HvT58u4mxdva6pf1ZBT
FIEhTHwA8jFKboSBx3Aa6I9ojccr3Tg6F4aNNNyET9rUWkJkMH9/1kZPxgy+oY5t0/Jp/cLlBBDE
LQlKERwjqPseMGIYmIcNhbE55xxT9R+qCIIiqaZJe7YsQy7ET6su2yha1tn4wDken9fhgc+4APZF
Pi9pbfo+a3mV93EP4P4YjA8/3izM5eD6MxzV+6c3j2p+P9YcDt3+UfqQoClKAggPX59h6aPrZeQq
tEc960khIbIe9bv8AGz/TiIeI/AYgOFoPxQpQWafgRM21aU7+FHEvzH5zXd0U/GucIE0qOFvjdlY
iXMGR7mBU3Z4FQqAUH1hN4WyA+Gc78h9ZB5ALtUx+Sk/rgtaA8eyoXwXBCtDBjH50cSvkr7vNGP7
ngmQXJzhi+CeXaU6ZhINKlmjSEH/dSB3xLdLu5+iqrUeI56aGgEsFL0UWkIk4Lmr4ch+H4Hlfh5Q
jHcySdU9Zk03JmQAsmZGaE0KEAWW2f7HHRphvU4N88KMfUASDB+z/r8o7cjR1eBIWLwiOo+v4/w+
r3vzdqFV+g+arS1Uf0Ef6DicyX7+PlG/4MJsIqspeoVh1yBLGYCfqP2n7iw4J/An6fmz1U4kVN34
8ov+3J4Lr+HGuekPkpf2s0l7ddz/Ng0TS68QFyR5VnyEHG65XvE7U/iEqA/An6Oo5JYb0GEa4DrT
XETrQ0AlLqaSCHV+BDnCaD39NGdH9OfOOH3Oa0vZu08hs3w68B0YKharlscxtGHVzQkWGNu9DuHi
BA2AbSdE/2UgcQb1GxSA2IwXFja8l5J6R3Aujq0BcyZoXmRqBorgfftL2AySFoSEgwmfBD95t4jA
PATe8u9UPtD89gv6D8yD/D8YJ/wGCofzEGDY5+hx71Qo5Yg0NuoOSSQPE2biJP2TbPRczPzn32D7
rtLaXMhCowZJGcHcbiGbmNgyyYHI/nOaBZLwtV6S8r60u/1zMAPtUkAmKVhpEA2h96uHKAP58vR9
jmSbdw6AZMIaDWsRtFFnOnobxwcGct+jOVVQdO/k4bUENIgSEgOsAZj7eaPC1fpMZpy+J5k+ru6H
IAffIyPpPozPsH9Qj++5v/j7ynaL+RzOaDdOPztZISkKVsNWH94fkq30ZQsOfd9ElhcAvyyiBCOw
DXQr6CHEbrjA8YLOfr2CUSmB/AenDVncaeUNNm7M+dSyiIlK4PtNxREx6BRUaSwiX5dYSWSySRuV
JspbbbUBLY4SW2uoKSkkY7bUhF/ftHbwHsMKQC7GkivqmVSowGVRh0Dd7r3z2Wv+1s4c4pDQ+Iky
Gk+IwLg+cIO8DaMFH1Mw3gPVbr8yYif1EUf8EEXdxZdkPF3NzrhvMDaZfIeg/CeH6P6a2H0/rxMq
p2uzXqzCiLHUsZzOSSVm5WCe35GXXU3fM7R8mPyp7QK+9ciwYBaG/DZy3Y6pJGQwLYGBxvTY8u0M
v3fmPISQkhJCSIkhJCSIkhJCSEkJISQkhJCSEkJIpIpIpISQkhJCqo5jN+JVweQP0qnoAsfP7q2p
IQhkYAPEXuQhIHc+SzZuPtT8b5hyymRoV3Hf8wffGFUnJ+o4Adz9GA9L/UL39dv6rcp/pf+cwkxP
6ohnGTa3yF4H5CVM1PIAgZoO59DghZ0/tc8APqQ/ifacTj5HjfMHpkoTryjDYB9DqYHrDA/ER6i+
+Ce/ZX8Pr5g5gOfe8x7Xk56JIgKiqhoCKiioqiLTD7LPc4VmJjmNbL6iFgIGfaQ5XD+zH0XPwn57
70zGqY6IJ+a7dBzcjcF4iZ8EYgKDBzrUvURz+oX7vcdTCt0esDARIR83NSvp+Mwd//KYfhm2s9a9
xHCnu1KUyGH834eYfN+/6rfwBl/pMfL4B2e722LGmjn3l0t8MLCedAvZsWUqlpTG9LqvYZU+c+pZ
VGg/cmfJAqKjs8+tVj5DuhOoC3I1dwCd6Bn4C+tA+o1TQCHlNgUe0JX4579tfiQ7s8zI/o4FSfk+
wqBIctqGwDzw2OkpSfn3im7YJxiHirL0+ICg9LwG5hLfqmA7ZdiUkrvMH4EB8zcTnMgjr9ZkQ8D2
JKAEjfHDskg44P1+6QmCpjpgJ2xgNAmZN0Bt8JDhY7eAb3zIHkXP+hr+XvTkp6TvAdwBvwN/4vcY
Yb+aHpvaQPT260WdBxkERJV14mTVNV9+cXvwoDhaG95w4PuIjV8Z3jTAQUjEPphkRUd10ZsnfOR6
dYWaYVERFFU9eT28WOaeEh/H97J5KAoE/F6Ffjyk9HZYCshh5g7PEO7v3IHnpIhs3oi8Q+yHRx/W
HQ/IBUKEm5sWkjdf4qSVYBLvD5dnzqyrIJJL8aOKJQ+bmPCHYinV+LVzPIIQpr5tygahEdwH84Qj
WecmVG4xoHrZJ4c+yKX+RX++/684X8+ctMhAg277ocdMDl4FIMYFmB1hQmZmLENSAp+4UA/TJ2By
a67mi/CJzCWBBrL7+VFoT98DibwcwMGWtOQQCRkR91i15eRIfuH0Phkwxs+aSTMMDfNoxS+px6H6
hMUj3PkP2TQX7BNQTUOlGBA7AR/TsA7oRmxf1/d6fvA+T3vxj+TmD9UUfDv5YO5XkfzwXk49A0QP
VXqJY56Klyr2ooInj3e3uKM1T81t7mMP4H0e0/J94WPi/oPlmFFC69/gLThPB/okPAEpJHPpgfh+
ig/xrPynSEYmwrr6B+KziSsjXZCF9En5mefTNP5OGlFERsOT+59mNrm7wojCIqqtMMKi+M9X5/k8
nZz/i+NLv3nnsIQLcc2K72Fz2d1AuoGv5wNPwyOYtW/ZU0p6IgdYkocNENKJ0gaEhQVj7EYICFaH
bt8ixoOvRiHTIOaKR3K+cXo6HoKm9VD4/oPrKsfFLXtavjvaX/UMOouigH+Q/gUWzFA7d87ixc3t
amDyRVlk23HFTQyTT7NWm2+bJLh9SfXoU7XItplPxj+NSEbY83xGb8rDJmn54amsOAHeF7m1ETeD
pstUjTHH0o5BR08JmEo7KSic48hg8ASYwwMJrdBjINNqDK3wn7wufR27kmSY92alEa/f+ljDD0M9
3G5mNxrlyWEobqMZhnZ4j+6TeC0QwnXCE7Ln7unJnmpHxPl+AsicQ60PEG8Yw/QjlRTrKkcnSHvW
HwWLwPqecMFJOn7CDCAOTFax2ZT61PrHvRMv0fplWLUKtfS69bbhIztKSo3Z5iyTHYKn4wMdMt8u
WpdkQ95z+uGZZkAonf9R07NEyPHoP16yEkFum7dnuPIqCXIdGGaGOaCUVzo0DQBw7/lkaZqiMali
g8zNTYdW3Xo6gXNSWCL1nrcJD2r9CS4e1/AP39w5RLYyoIQX4HqMPpaF+5uaqfwiGtMIL8jVb05s
rIT7G9v/LA55P8ZwjitZrhfTMX4HEL1IdnpzU4sbUbaaXTimPohtpVn0jJNjYfHM9O883csEBruL
4k8TrNJnANR9XB5mZMW2Ys3buKPkxjHIx3ykrRSFvjbzJE6mStW4lPBqYqUQg0bbiY1GXmFyNt3n
teQZmi047ENlVyl22W8+MJwOS/gEWzYNEo7jyqWAKLGOwkyG35xy+gRMkEwghEtgdgRx8LewG/cZ
JmQnV9YWzEzIBIlIHiL/SWl70Uu6tU07NicLUOifebeKt1luLGnhoAwc6UkBMwnJ/QbO3FA73YPO
KgpBDzrE/H5kIh4JCSkhKbDAxgaEMh48wQ/U2QEGzFwgllfZafJg6SvYKcSCSHyjkcRBhKEQesYU
e148O6XZfnON6/v9od3TyajyvQ2eTzfj+txAx8A00ofmQsxb1zHsKtsPOfK838Huj22IROYoK6ar
Adac0T5tV5iuHdVW4EYHe/0bQmP1AuodZDGbhBpi/16oVtAoP1KWQKDIy9tPIP0lw+CT/fQdK69e
kAYQqlAUBSJHxk9rUXEkH9BwIXwyBG2AbKVTUsLAWqXuFD5jU9j+RH5/55+biP+GW6uPdfHAR4/U
w9kRB3TxVYko/a2/zzs0D2q4KiIeVEExEK1wYlN+xgkDjae1wAWbSO+qjIQzdgdRcuE1bi2LQciL
tgXHZFyLHzBmYQNSrjpD66tZ2qz9Mfu0cSaA0pXzY0taBIe33n06NPxwkT+FtjzkExnsbGf0ecx/
0SauX11rh/EzLy/dxh5o8+i+hJHr6fd8Pxi+C6p8y70RfqP62+fr5SMWvPp+0IXp7QI+kFBYBxRC
fZkaAfIfOfsPhQPp937/rNeHACH51YPeKnNT/SOKW/kojmROXK1ukSTF5YzOkI5HS3sugxMgLDFz
/eUbomVAac6hfoDsvoeBUkZhOa4UsLjwIWNgmOO4sBwgqZg/WPmKHQ0126/Lg+lRJnyAH2Ie1Pp+
T2oHtYyEkIQPdQ4DCROv0+onrQ0yDJDln+9NjJyGiqA98cP1ih/OR/SJ6FTkjpgw3uK6AiBUBBA2
Vn2PJwTmgt5NGHPK5CG9NPkFSUshDLEtMBSVERRNXODyLsHV504+MOHAjTHI5btiiqiooqzZzRvA
413Gn48E9noIGfjz4o+U7i/Xi/HwQW0xFsR/qH8rL+X2QLmtmRMbYDAwJGIPlslacNy0ZXI1hSYO
KDPwmjsGldLKFaKOPs8Z9zH+/lNNsbbI+6OmnaSu0T0cZJ0EpmCQdJKuSjNFCZNFWIxodZRWGBOD
DBNFDt/YOS+uIlDNmwg4L7LicDr92Q+9iygoOWvvIMHjycy6YjuE7QaR63Yp1uwDZNPGouaBIjYP
jPfiyu750Mk7ef8E8yAwXf83iHb7weI0PxRPQgHf0rqid/VbdqEUFBH2iBGjqCHSpr2t8vKBpmnc
Fqfj89LId7zBsDCfDMIIXKSAHEdxSYQD5pQBHeFaglnim1g/rRT2H+SJY2NNn5vlP3g/WvgVfyP9
ZB2yzLaaekuP0mu7f6FpXMHMHbbLHI4ODmBiS0QA5aU1EKiAyLu1Sah8jQZ/fgry1JJ7xC/yZ6+5
j35v76dfjKvYiKGKia3s8XmSUQhIUOaO/Tuk7p+OctN2OhDhxVuL/f6tdtuTV0ogVoCFl55PziPs
M027PiA5zcYdy9X+44UCW+AvYJMQxJniEO3BD1ub6B1el3Yv7vPcUGHYcbK993BxESSFYcSSEsGv
6GaJ/P49eJb8i2GY235fEqjG1AkG21xAOvvLUnOHMLXLMCkMd1FG64tksyDIkJzSmEeU6d0OSf0B
xq6RCa5Jv56k2AbLFodLaXw+Z9iueIQN+/kM8W0FPXCVVJQKFLE0lKFKnuIA/UBIf1ff5F6g9AHy
jyl6uTlVZfQYc/c0bvW8S5KYUQIEpQxHGDXhA+73XNnkhWfx6PWFdmyj1woktVRkCYphMkICsa0O
K9X/OYIsxgdff/FKzqM+mWraf6rGQHKP2RsP1fYhXMp4+df1E4XCR3NULGIMHy2q289w289Acfwf
fdXQCIezuEwErBRRN2umvDnwHDcPI+0tYZQQ8uauhvUdweAw5uCVi04Hj9oL1UlgvzmZugGANH8B
/O2XYoZAc4tzoMbENpBdu2VybNglqf+xY2gWQ7IE6oiKLw/CPY/TJR7swgqJYJaEgqLMMqKQgiio
D2fGZyk5RoCIcSf4R/ohf8Vy5Rd+1M8iGpRD4iENoLs3BJVGp2tNHI/hzQy+TXbwt9XzPTXDewnB
/mzj92+g/2/uY3yq0urY4uTxjY3P1LgtDZrohYI56df/DT7S7naH5WtCdZ0NOSrkECNRp/YpEGQo
tS09DW7gPY2Ozralijbezarl9C/30Zkeg27rIfAweLTRoWAgwbKmNgTFrsyFII0FS0PMeX93C9vo
+GiB/e1e9uxx6coci1f5EMB+Agafk+r/XVDV+f6tPDXYmRvmfyUgXTEBaNulZTmNNtJmJaNiIP5S
dkhra2pL+w2DOLF+jlME+X7NAGKIr6IHdalxxp1sdFy57NPr3IF3OJ1QOE259Z2HaQohD676plX7
fstyl5izRdvY+vpeBBhGDzwhHih2bA6d7yOqi86fxH1AaJ51dUPrVPyQIb+J9h35q+k6kJnYS7WN
ogAgUfxcY/vt/YymcSRggE6mDbPrhzfYLzDzqOxIJnEtg5gnAh/SpPX1IbX51yD937+5R6Uz4Fk6
/bdNtuS74whITtlFNehInAy53cQRKhGlTAojcGgaJDyhWBxSycQ71TAH7GCpD9MO/QPcBzgblPzC
UCQHzJDcaELvvPpk/SSiT7rWKk7rv3B2JUUxTJLLR+RkY/KZ+/dNcO5+qGHmQm5KG32k3GE7W34I
GY4iIUv5S+W9+0oS3jaK2FVIRkWwJBCje65FXDmXlTDm4gXlNpVVEcDUKQrttOks4nDjaR2D00hp
t3O8/EH3GDYopwd2Rc56BkSIKErYcshjDGzAyx7dx8AdgY2eS6MG+omG6LljAs+CEndO7uLJq3O3
9Lt/C6f4x1HYuH73l+6uqxTLrmtXG2L8CxWE6L19Gx884Vpv+z/D/r37/8VzUslkltuK2yRySOeD
Z7uPEKt73CEmUjsJBzyXSXP9X/lUDs9PTjEpqqkqtdgbFpYbw/Uhv3kPBMZBDyEJRCNAQtfBuH9e
LLr4DWiXgdYEOwHyn9X0OLhI8Mz78tm+jQbfCnD444cy3HivzJIT8jwZAjG0j2eECDETndDMHEm4
8MmZsdDd6zrcubFyiMxyKGqqqL+ANNdaxzKrQuU9MR6DOIN3w6dhHbZM91yesTq12OGwBaQKHRdG
n2ymCR8vo/FfYYfSilZAX6Gc98vWSkfgIUEX6FGRh41R+DHxcdxkGNq55TxJOnEZg9DY0hRLSaGD
FEXw1v0FnIf3tGN4iP2zxEsEiwQUxMxBWGZLEWOZBkmBRLRENFNJShE1QsEtLQU04wYTFVjGJP7S
D+ctAIIBiKXZmBNLjgWYESxFBEzTDCZOURJQlAymGAYE+EuE0pE1FJJAwQFCQSFpmURTJTMMTMDm
4axsGU0XAxMiCYYZKwlMhokkaChqdM0wxNlpadJAxKoIYigGnhGYYJGAY0ES1M5OFRRBNEFJAFK0
hLLTNJVEsSEkFUkUTz+kxN0wyGwzIIJKYkiCcnBjk5EQlUEhFRrJlT7JOwbA+3r7HOM09E4U1UkM
5jmZEFQYGBdnP1b838/aZsNH72D+tU3KOegiiLL8m+vfvNK0mKF5Wma5mISrVFz4oAfPxEVURUlV
VbDiwMA28geDuUOPzkk1mKiZIm7MhamWoO/7KDG8CE9RkLzg6CqISMe8P43NXA/ptkSBJDcHTrkf
B2ltwF3nGxx9/5envyNe2tcMKYa5Aouo58D9Ko/rB4t59iL8IHN39IGxeoP8w/V+rZhcBgKWgoOk
8YdnUdTfyXoF1N4VXJ4zt7QIdSr11278gaE+NC3sSNySc9/drpvNmS5VmXJbmZP1h/qeP6/0/vy9
1v24EGsYsfOHjChd8JEWQcExIQYyRwij56fuT6un8EUOoYjk5UFgxlSZ5m9ACyMbJAcUI3FC/wrM
8IGgvCPsT5D+dNfjZQyQ6kjThyquoYT72fINUfVbKdVwhUMQo224vxs9RjX7LFWF+3FAMOEIGiQ3
7Ofnvsl0JUAxXyJYikBv/YOhjUaRRobzL560lEdem9Cthj2JIEkmGYsWPwP0j2KQ/K7VX3t1KTCG
jHhJ0Ajc1hwOGfHP8nIkb02IVRWnsLnNZjhAjpaoPhKhtUcrWrp2FyymAJA9PV5aEfRl7bH0wP6T
fhPo4fV6pTRGQ+g9xH8QH7V9xrtC8RFt8f8LE+e+R88drAnPm5n8vwmMG4Cg+ZPK7eqascuxDTGW
1E3wTftDzg19fmv87ieYqS2UY9tHRh8IZQf1fo++ScJGlmRLTNNLDCGGEzJmRGZAx40ls6cfpf5e
hzqtfjHhg2OSLIQvl5YFLbOa39R8BRaM+YTJSxYh9Pvfx2vF5mDToR/pi230c7TGFbpfv8YzxnkB
5W8f2F3IrIKLDHPdYtAk0SLUJE0JH7fjNe/UfAH6xjwsT5kjtAPaOmWaGSIOQZhSnSUU2g0EF9f8
/7X8lsX1W5UwD5OPqP13ACMVar4/vvhAl8OD53IRtD+EQ7eoMOTBbBpqm/FDvBHse26x62UVHvNi
GSQxfHvvjzqTrUKIgFxgXNDfA3mMSxxCr3+Kv1XWekTBczoRhjjJ6X3nyo+j3Fg8qVsleU9IP3Bk
eSK8r81wiPhhhNBUTQyMSH0uGZEUqpKO853nI3uflX47UtVQz3e/cNti+cKeg+oK2OCURT4+0uwj
CP3deSSJD/VQ2hp3giegBwmRTU1FEch182c6PkqqqqqzN4QOoHQQY9/K9LqzAomfllISJfcfkY03
/VaC6MDIQbSf7CJQyV0yCoSRogwrY3YMnaqck2yHliRPN65tybptjQxkrDwa6lcjTtPB5PMZTZ5f
Qk09nAFK539v07Pikmsk4w7r3vdqSSSeWfk5nuRu+/oS5Q/SJP8gi+hdoqZ1gW/lhg+K0GMXVXFy
sPnaWWDP5TRsXkkkmtj3bz3ccepHrflpOgHCZ/JBMCLj0F23QH38R2ehIDzy0b958ZGV8i4fKQUP
edgVp/2ah9l/+7KO3P7pLAQPyXoC0qIYumZZpuwoo12VVf1H8jtdska1Hlg099LeH9uDF5oGK/a0
WJJGfbw5/653pX67BVc9oobvl8ke0VmZVVhGRWZlVXwT/XDT4dJZkU1U5FFVXRRifAgNK5MywQ4V
FIUlEVFNN9kcIIgiMDTo+3CiyzLCKszKoiqiKqsMIxwyyiqq6zJ+B48efczry7hYDmoWjafp6qQz
U53+nd+S/DY7GEdKQ3nEuTAVMTTRRCc9VU5FUUBQHjoYbwRl0RpzqS8Gc7j+iTwVBso8UcImowjG
7CgXzY+cvY8OlZzx2FJqO0jqXKYOWEKCGb0sKDlRcyAydlH5/APPudlVbuAaeKLkwzMKoKqjLLXn
IiZBgSsbbO14dThlbnkOb9nyCot8JhMk5MWMEzJNVHZhhBWZFFZuX1mGzRcjOfXLpyaa+wf/ME7H
suFBxy3u4wGsJwoafugZTJuEpDFanxiKj24yimXCiKQFv/WYwS5JcgFc8PcFkpSML1JI4sw9LlA/
EPvoPVzcQfEJe3R0hm7t3XkAerr4Bt+OD5CKv1j/mPFNR9vOWnk/ZgIQaz7dMurQ6vI0O3ewnUvc
yxHKpMkVIbGK/sLSJVuOcDOIAwfEYhq7MFMQcFg3VHCgS1v3geRvMV7usrpd1RyOoe0g1NgGCGED
EBQ0BbHAHTbOxwKowjAr0Ho92qdVUdZE2hPJE5RtHlWmVX/7tY4eusS1HVHUhDvHejz957XhpYSE
VaMyG3IZiarpt2I1gtR3/VL3BsBsJn/Owck5J1B6QHrGzXQ4nYXwEOAgrjDsWiUdLrEVS3A7FtTv
4fBBjjMAqDBWqHlfi5/k1S+gbtBlMixwdHKB1/n93zjrhXjpUOMnoEIQdkzb5cZA3nnHgueQ7iS6
W6+tTK54AIV5JSq+TKB5JIQgwb+WXjmMjfWVVejcoD7EKQaYmS5Imhux2mri2PgCgQUw20QgvvqZ
6MHLdASQyp31mW6KFC0h6ZDzlqmTRy8Kx5dJCLBxMk1p9jbNJkISEJfPONA6fh3UUKoJW9HYggFs
2aTITIEhakBGdwQJVbeC8HgIPA4QTWr4hOnoDrqYKjifVHV0c6qmtTTdK/gD+XKT+k/XzRH2+Td3
s+//Z21+L3a/41P69BhKI57DMIo6sXSotxwzDI9bMwyN/nOjQ9JXqqlOqpOU3ISEiUMQkM/weD6v
ivKangfEcXw8za+la2/zo/K4N4/3s1PSyq5RaCK/DVR8fq3+RA4lxW8khu/14sQ4nZh/TzOQWy0J
yYEIkICfRgQsSEVF58kKPAXdCoyG2aFPLJqKDWPWeiERZsD/9Z54v8Lpl6dCMKUuRfPiYEhuXZl/
uwn16Pr6B8fpzUyleK7eMcFzXzM0ee63i9TaH0zs14YOrMM22HoCsi87FXKKxafSXClCfp1R5mLV
WVAw7ZNtg2Sbc8ScBL29u3+hTtAah3Q4kl0xhlKVdR9q/r6AVLKzPgJOQoZHEN+7JCTJcodSQO6Q
mcAcyzSSkhvTuWc4O68+/9vlRqdOL3cSovl58+PSKh3ffWDtu5lj71LCcGTD4K9i9nsJKynY+nWw
5vz8sLPKpfQmHXYqM1XthwhHA3z+vZheheqAX8RLry7TGDY4O4SYZTR+eJtglp/cdNd4nbYyU9VA
rSJi6D5sVJgEqax0fxmyLyZoqitNmUj+h+8k+jc14shKye/3THr6SFpCOF+fjvrjeRyz2xt5M6nG
9i3s9F0fILyrGtFFhd9M6uvr3YzOYrq8jjjczUk6Sg7ju5geTiL/L2B/1fEFj572Suc1OFtgQmY4
1/ZBjU0DV1zANo64iFCH6j7UebwkKIJIEqs7zZSEmilpqioGtE7PwhjtvWY1kZhYDwDEwpO+aHAj
gUVNQSnIuODl42bGCSh2TfX6g16kpfmH+0Hqe0derdhgexYzMRMzFUzBFZGAVxeoQ+Mge3uHRo9J
ElJk4EkExNBKEQF4DSDwGBTgWBER5tDchhqCdZwLQxMXZfwlqcYyCSrmFmaTl3mQwwooGchAfg4q
zo0R+AQIuG22NcSBzrdhqLyI5nqa2WBRELERGFowCKcJQSOjTrLba2LYKzliOvw/os8QnrdfD02Y
Yoi986vUeR8KiS0Q2k9hhNJyRUbKsZaPnwIbTXL3wbk5Q22x8mniJHIHFukaTRAqiIqR0wceTDhR
HCjm7YE1wlX/UKuHOaAaQdV4AwF4CRRJE0UxQRvBtDWFlgKaZCqQiQogoaWguueAl6ZIncx0I2g3
MCYqOZ1oRORsZbL31lUxRVLDfMMHJKYJLv5ncZicOBiO0WkZFhYTKQSFVFUVFAESek6QuG4iQmmK
hkDMNNIVOyYWg9RkJRzKeRSpSMQs2ZFlBSxQSkU0zBAaHRJpbYVZBRWgQRBUkMHb2vjD5A8/d1Ox
5ukbb35SPMOmPCxICaIhFBBbE9CViqSHqrxDQqS03c07Kd8bb9hEgGAVkSXkaJ5O5yKSVW5its3r
CBP4HsPQqDBuWde9EHE+uPqFNlez7I1dNocwUOOcLguZQlin2KXzkg2J8eguyHhc+bmh6x3QIQlg
sakKOsu7fafh/y/y/L/f/x/TyPbEZPXHWbBy2VnrNXK/bk4tf7FSdGKHdYzoy9zEWm1MzS1Kic4q
1A9ZT0nm36c2cgdjMYE+DJswBp73lZ4KH02k+InBUs50z01+81yN0UIrNZ1njnnnbXlbrUvjWtbr
R8X6nc7nW0JMGJoqQPnbYRAFJvzzSq4ujOtjCqCnzQLMb3jajIJs/pPWCQTeeurK/DHG0HV50DTQ
sOGIFGvhEY1x7Ic9JGRnE88zs9cNRBvIB6jMQg6NKI12gcS0jESGUEOUEhkgBrZ6en0hYTEh+OZm
7ZhTlg63v4HIVaa6sUTQ0N+EJ1igv4/4J3mHYwKILIjVMi81UQ43snJ/HYM7jzw4bys21BFLUDYd
HsT760TNdoPT4vRb4QD6mfPxXE19BJ1lIj+Jj5uhrAEREiKXqYqrjL3xRn5CoW228J+l9/f5Cojg
PS+0U2ZLcmE55icgGYpkJ06ExnXcRjQYsnvdfLCeHhxi8bBHOQX5mB0TBHI0sYektQHB0nZhlyhc
661gQq+9LhZ81OSok1EK1Rq6VQQHe8VnoFeZ8a482dxkVTJi2bQUtbbGOLRm7kjdDGF2etpLgwSf
hWqnrPPCxleHhE8ROeKHlmCIdnbSeBN96HlvPOb+5DSH2NcGZ8Bgwu4UDwFEOGQ1Xia2cLBk2I/1
0p0+zn3oR5tC+DPoZWBWT6ICLYaZ62Ad2kb75YuFQALRtBerWjCwA7pWfnPN5HAdfVQOu180XZG4
JXR4uXI6uln3Hl1OqqENkHoh5uLF0rvNl3sg0LoNnlh4SOedVZBnpVaIicVyUzVJuORJJyMO6jEB
e28nWQUiLeuzKIg5DDK+FFrzzrtpXjgADBdGpSsacatpJpX1bUX+eRrwnktvksdFyNiA6uqjtqt6
4G+Uxf7DtOk8nzHo/Akz7b9iqKCp7jurSpkzjtDGhd+ahjJ6saO9tfyMMaJSR/hQcE8MqERgD5bz
4/41BYARLi8X2VJjCAxuZR5M/GHfl3+HD4ir6nmCECEIQtsz913Rl2CbyFaZ2VCkPFqZD6hLmD5g
cN7gk5EF4REku6LFvrcmiELC7mHTBHM5ZQxqG5KkPAa96Ru1+8SCgw2TJxorUGzIZDOMD8rCs6Ii
52wN2wi/wG4LKgk8CHCSZKHMFFg7ITCD8xDsAI5Wb/Rg94dBFiNCEOe8RwehQG8GC0qEhhiylxu7
YFSzLy8Ufio8BRRTA84xea8Y/TZZ3Fp3EjqxVu7x2c7NDc+/vBBptQGCoz60HAhCORxyzwI9DgkH
BySA2yLECkjUmUJgxQmW0N9ZX7McfPVjNiYWj2IG4YzdpQzelMiQYChUuQpkRNpcTFChdMrrLbbI
zlJ9k7SsgKVaLOr1u2i3XmwyOJmXkTMUIhgRKWXZ79swiWFpePBhqbL3y2VGy8GMzMtOSEnHKrce
sswvqZpbb0rVE6lVUG/LzGNo1S/Txrkm21XcVbbmO3hjdoQpIIKIVMwWEEIvSu546CmReYlLInMM
7EdtEaH0voyHT6xzHZlsww0hJfNzFzOyTAxZ3IoomCII+LfLDHQvrksxBGxm7pmZoVChfNNNCGUS
UiUZmDebDJNJyWt0sU8/HFTYfT2PJ+f4nkgomDndR6liUYnrbpZZEZ7zr7eO36hNRk/d2A4L8FSM
HZxuVB1NdKp0Ja6foZma3u9kKfYfb7im3be7z/mqdW7HZSmI4mDwoq9nob5nCF40nVDBS1R38N8E
ugztXCoZyFe6fYv3zMUqudVvxbvXxXQviY7FMM2Jq0mMqndKE2qsqeiptwgYLLCO2diLI2770nFs
iRUlTF0yuMOpR83rUh6fSiExFQyT+yQmZ0ImwfyD0QjB1NE5+PSCX4VEYd6cKPpRmiRnjPxuGZV4
4Zkj6elzsjqHWeLF8yTwJPQyocEja0usDul3CbWp2eKkIJ+KwE67V4fOU/Sf6G2yTXLPvyj52h6T
1N4dmWUHHZOa6OQ61GlBlj52g3VmZRIQFidMm52txsavwXsmy1krrBRdKvwuJChLG6opPJ2L0Pgu
o/o/mTU0CgDSKMCAgDC03IWH2ckVGw0tuqR7Kht4TB0ZBlKYKP6xRyCOJ+3+H79P1/cwCr/WqfhX
+z9lJQrEuPHg2v87+5eJFGGH6uDixHp5PZFIKop74zi1h3z/WX54pou9dRTt7fC0fCcjw/Mv5/1H
8P2nOYSv15ihEC6VKICM+mv6PLabKikbVhETVURF0z1zisNNNYoGxZVl17F6jr+LRo4jMzIzLFWv
vrSu9XvviXxwvvvtLsHZf73v2p8yfmAgP6E8Cp+aXWfrTxH7d54M7g6A+b27f2GoHk8YeRP8X+M9
v1YG5TTQ/Rxbn0EU0sUR9fGp8/8O3p/qYiKj2cAOVXIF+w+4RHIJoBDCma/7QYQA+f3yWQoryU7s
hOoX7ZdsB/WG5EP3oOFvBDjy/EyAZIgYiHIYEtOb1gbB7A97p8OocngmieN3vS5hmHQByzegNU2K
9HRWK/h4/19fhUjQP2A8mgRSfqry6PGjoXEM08R5V2IfY5CPiotNElaVVbNg0HuoNp7tL5FIbgs5
B8kEIEUIEA+LYhgROQaAaUK8LpNY0cvERrH+TROJpD5ZSZz3mCn25wXOVmKOOJUMzNfCbDwVieOM
GM4MYaMgkteXZcDd/TfC83SbdHXkmaht4uUkuD0baEkEMExE1MlBURSRAKi5eiZil11hWaHnTy53
iIUKIeYPu/xmGCiqLQsN53PVFHJ2T+geU4gNF7iaKN3FqNEQU0lC/HMSmkM9sNzChyd3P4/w1XsH
g7+LqzIjAJxzMDApCoihmoeRHm0Zh1BP+z0nxtt9vRI+K3ve7beglmZn8q126oPKybk0hd/riF8o
6PXsGSQenmAaQDwAarE0+ojzZwmJe/17wun7WYfP8/W5ekJeINRWJ3/YV85cXKZCwjBYYILBNJsc
NkjmwQ3Imc5JOcCLNkEmG5OSAgQgRocTJmplCEJq2P38UcMxg9vbj2+sE0TM1JirrVdtdttuCR3B
1r2eR4FVdzcszNZdV3NKAc85ltqXQouM6HDYy2y12T4xyNuJ2UO0vEw7qLO45lmCC2bWsVo76bWM
YxLpFumc89ya8fOd41uFEREROYiq+GToFmElyaREcTAqJiZTi5dKrp6Xq/HGudmDzQ5w1Hc9SiQR
7EWdBw6lDnkCGwP2JLL4LPM1dmXNE9zqSEiPCEnBFkIZ+u4YM9g9mCGNNA4MdUNUKNDmgmB2S5Va
WklJcaOPCgMPEMw3J3hwKXUdE/w/ZJ9RD+mSiRlEKMC4D1JyD2EBk451dFlFncacmGabvnzCpUYU
wa/6wPK5gd3hoGYnec6uDUKXgeGHsYPHxT5QfeFaSYnMDAr6uh3knEg6gwKqZrTExOlPTUMI0+AZ
nacWwcwByfU5Rt6DWQfFldihG3CjJbbayultQ0vAPivh8ngL6zw80Nti+cREGKJ5FKFDzhSnhvsu
hkdzE4QeEpQIIjMF8RppCOKxjAjN3NYV4ksGy7eQY6jh/wiBZoy6c9fNHqeXHub2NhtfFXBOBp63
vO73rWeViB3e2E/HEQuz8KIOMcuOJJXzy07rsV7qnmNiRxvZlZzxTu9s8QLwK5zeUlUUw690kI54
3jMxqb0UUuvT09O2G7aGSomrrI4zbym3CKuIOM0N4mW2ZIEku/GqnCMdfEWmZspjOM0TeHYz/SyY
wV+7ZpCEPgPJvXm3EebwhITjrqjzPEC2nMG0HoUgC63lqiIjq4txW0xeZHzBph+6/txJUlbsTpSV
XI5QZe51PJA56vW70SQlWtxqrVaeh5ghZzf4Jnk7CEO5NFVwkRTlIEaSEOPnJm5NEkGS3mGR0sUj
x2Ud1lWZEXAuO8OA2jTb4gqxjqhNyV6tGVSMIce3MWUj54bLMgx6ZBxEVREERSSUOORFMZg2DjgR
kjOktj6ONWSMG1DiUY+kOHW0NDpZO5hHQdb4GFDUAUPF6GiaSonbK+iQ7p3og+Q8B6JqA+YgogXm
d5R5zLx53viCdJDikQiIlFbYJMcKQOPRg6RFGiNB8/LPFnlNNijRwaMH2ied+yrv4EYliWN3EaWU
ay6Wd4LXiGK6jUO82EAiYcO0Mx4lk604idib3gEsGxyP4fz/u/tf25LWflz+mtmM3m8y17uhm5C4
a3JldKqdVTvEItVT36ndgSTeDp5y0JvoPedgwJiXxHUYaG0did5vxIsohayOsNubq4j1+IId6Sqn
EOcJwN7aNtAz3wGmAaaUpFioU/K+bePRefPPExP4u3MxyN2hDueHjPpGAyUwMU+r2Lo/RlYNtnlU
INz0RYh+QfjOGu0KCo5jIxIyMwwpRI1AZJ188VZkwwwcg5CS0oyjMoxtL09nUz0YeHWUy5ledOlR
MjajeYSWQRHw7O9DhFVt4gpNN2jo3k7zXJKxG3DgHrTbY4ljZvg2Gl/JyGB5JoQbBh5tL11w+RZm
YSrDiMyvWWXVMBBmEUucPSfKF+GzKEK9zwMByPStAdsZIhuYJgwnEN6HwIcZJObqIpCPricFd2O7
Q+lxmNWEQ8wq2vQ4Qg6dW2gCv4dO2YiJiVIxlMUqZSmp2c8eEuc7jVOCZCSEJpXFzi5CHHXpD+qS
fM176+clVAhjJl5Uk+HxSSdgbTw4vHidaGVWbxsVJLa917vXJVUQxjJxG0aAudJRg6SjgWPhN7Lp
m3hGECI0e5/IYdiZTK2ahSmZg9KYXVI22MmZiKZqq2Zj3qHIoe8dM31hhgRlXJoh44pK1zXAO15y
aRdgpsimgQRy/LxzGyY8zDy2qwkcCMTemlB18CQcAlcUDxu4OgegogeMQuFbbZJrbdTXiq6WBvUN
KDulFse2ZiJIJisHTV2bIrhT1EeB4m2v8gydpGOR+3axJd13R9PVmeEsLnDZ8tfbg4GISZIfqPmX
PY8nC7XYek5IPaI4nr1VxWZeBEc7jlYEDpJ6eHHFb12dxZRSSVO4jjLnFO1dHgTdHexNjE4xULPP
Mbpyum8RVc5gicYeQlISOXS6QZcZ2cMvaCI0+nEzfZPYD3D+X10mq7CKFWRErsN4wm5EE26rnVwz
0pO+4nOcZ1FTlCp0gkERYu/r06eNOhIZltsJISS/bZ13o9h7umVj856slHG5au4q8b5jaOoGchrr
tunWb22pzZvU5Ou9i6ddnRinB2ojkS6dek9VyRRIKywkmImYiXFE4rFyXYo6dcsmQx1AzkVpJCg5
Ba+p7RVU0xeg3qJ8wPB+/uFj6B50QoSe0+mDbw25ZWj0GjTE+2cePtz7nPnbrs1wikkkJGvR46u4
hJLKws5ec51WMPY8RQdcT09CqtTb5WSWFwUPPsT+FDBeAJzsQ6WOSRpo3pcI8qVgmiJQL6g4b17b
anWXdCcR0ZDpJGdefkco45OJSSSwo24224Mjjb/V5dL5AO3tUbEgIGlToFEREaBr4PK1ERFdA6hr
3ibl5t/4VRkXY2HHUrkqQEYtEDDQOfCJpbfXSDBZBAIBxwenRweYg2RMUcF8WNExFVVB60nY4SBH
0ntGajhFGmw9qw8SEIQkjkSGiOOSSeh09nu1drghrWqxdRd0eTDp1sca6KHIHIchyQxyQVTSQURn
UOACXgi+hlhbS12Q94HNELnltttvaKqgqqdQCE95ruV0iwPCKeMOcreNuFg3xk2tl2oHjQu59EJH
AW2pGG7X0SsbSV7hMkm4HZe5D07zBiOPBztieqMLTicBYVzla0VJbOYAy+GgSfhRwYLxJt8ttkJA
mccllXaxEGiJq4OHXw27Xo8jtp/Ss0N+IqezDHwYH23ke3MiCSDHOrsHSSHIGku3xn0O5ihsm1VO
7uU2Rz9xiQSYVQnO8czSGYi/fiyBGj0tgIzVAGKJqbkMBEO7kwFdiqGCCg4loQGBOuw2SBRVFA1M
T4GgeY3nXXMjoT1X6ciqqZh7IyaQgxOEhIEaUZPnpSjGMYxjrU6siaKrlkcs59X6PxdJvth5H0Hh
fvRRaEHDpQ9GxI+nFF2MWsyjMMMxsA0uAOiyeCGjDyWxo9TiUdPsURrsERRMF1Cv3RSQjQqkLQoL
6T1WzPRx8WV2EbUbhSwdttsdhYBwFaWEhGJsgxNoNe2H2oRtzx9XkiwuLSYuSycgyfEBuON6EOIT
xLH0t9jAONtuOUkkkvfPz46l36yJEREkzLojMwzMOHSdCYOIdZQxMVjyGU3EXkkYnEmsR7548nD5
e0REVTVVEaoMYfkujn3y7eyjtjmPam8vJ8mLm8Vyul946YfbvudLWcWkJcrJt5pc8jy3Suk9Ol6l
+phuoT2LE9t5BuRMI7eNiZ3OJsN3TcZZmcmGTBljPSThRDFFXvWL0JUJAye9UOIOg7LsIx5w3Gx/
Z0cRfuYPgIDpKJIsIBChCmLeAZ7pIjTvCNigNVidRXAOhS5qo2DcByYEgeH6Nm3Tly21jH93yfMJ
BVDyiBuw8tlof8qf6/OaGfTToyG7xoTk7IBlqLDzl5Hvl0gFyn9wPqpn/HDYiYgUE2IhNYgQtFEw
z4/3fs5JsQH2927WOpo04LxEcYcug4kTzHp/QcFcpbHSI0LOF33vxmvD4A3vpNz/I/X7/Y6gIdox
gDZ1kQEaxAwC9jD/CpgY1lcjQ32447YDZ/TsOCiqAG+SKMgIcTm43P8P9Vdlisw28L3TEtkddwgl
iOblu6K0nUoiGfSM0ImChnMIqUk30p/x0zVx3Z5xtDGeVW0hsXKsp07z+q+NCp1rxDoZr/X2aFmr
lW4JAOJQd0BRsF4423230u354BopdTbHkKaIDYqVQezLUwPHjDePVzgZoUlJ6QUkw0tU7Mw4D5qC
GouQiCC5zKxXbobmugdK9mhmbIEenhmWFDcRQ2EVDuLGVkA0hJ4+BxyGyC8Tsr/mpffMfKDy0sNu
GEPhycbNlOk2enTitM2Tg2U4w08jmHhswpkJ63WRbkSSz/HUQUVByB0gjdf29R3p6wMw0IHBNBHl
yk98+sqJvLsLI7ogNuGkT37xUkN1l5g9DRWnBsOzgagnHh7z3iMutCGcR8B9HDg42930qZNDEsks
ga2xAwKsHJySvlMcVIkQVEhDdMPO3ben2PhGGpjBjOYzgnvl6WowNHpiGIpPp3YRMRhEDuVi0iBU
cekbuS2lDeWOejfhE2ZuRpZrcycLNktOveb7F5iGmSdsHBAPl+kQVQmUOgF+gX2wEwELpchdlhE7
zeMnb6StZCaVKanonpvXp86decvw5eSzDtok1Jtltngd4toD6X8ul0kGSHeWtjbAkUaHvsUR2+iH
i2w2d4g5XXLWNz96XcUbyEIT6rMkDGuHWA2OkG5JZFWrXBHGYWQGPImyzMwXIC6wMhb8+PJDM10q
mNRm4ojTrfmhTiLW/H2Hr7nrHqfIvrdpEelsbjTT4GyPW7BoQPtF/QIlC+NhnpJKlUoplAANmyxa
8nhQWkje1LzxTtiZ91IapuoS98tQcvkdBhs1amxR7NDLQC1+wORJAQ8MBB5BN7YPlLU/EHNgUsCd
wn+MlTQ0zG0ldgdT6BCxESMAJJJUFJFBFCTCwySBETMMxEsso1EQMEBKwlDRNMRQhO/Q168zMWPn
L5+KUQxQtMQzCTNQqyJGNwmmGGg0MuFtKYLT5WG6gggTS2E28ysAdzCLI4QeMsYEfhLkbjg0o202
sakQbYeDHvvrQusMIDuLXHuaQ4doJwZCNThGWpaWiEECcJBDwGnOHEOgMhRxgqkCu+qTTF44aHvY
RqJgOcICqaEYSAYSqqooQpoFIqUqJQkiIhiimIiiiGWphpppqWpiYpaAKIhCGSKkEpiRiEpJqApE
KoUJlZhChKUKImJMFA4QnUoJc2vQWU8iBilAOt+heb7prCqkoKiyRNLUEI/CBwIoKSZT8uzD5BSd
kJEBJKceBxw0fQg/aoMQibENheBfWoJ3wIdkgGJNItFCAJhhhMLkjBBg4I8ouaNV8aofwUhAr+/r
NEUlJQQRMgBQ2HLtADtHxQEHBO002QJhRFV8UvC/4GZhLygbo/zwFITAEhKSMTCFnCQ4h1xeiCEh
ih50zNVClFNTKUtC0jVUtBMBTMv9Lqc+nDeYtDtYKRiDLi6T+XxEgV81qHVuED4YWG3Vb5Y9pByY
H/CsWZDLYI0mnr1lr/b0cjmsghSPj1Do08f/UYsEjOYpn/muAdw1Mk1EwTphYQY8+XM3i4CilCT/
IYVbFVkZT7ZUZf6zF4RUVw71FHX3XaLELZBHu1o/xVGGTapsi21GFZ3EUv5pSiw6PFUiEGxttjPP
7Tk1Dsfcfw5+b11MSNCFLUS0RSkyE0l5z8BkxoRQEkChBApvVpT9doqhwlfu4cpPQQYi915ZpBqk
juf7B179TELXDv0xNm3B+NDvI87BU0RNUDSTRRJNQiS3Z+g8fsP9Tqhh/MM4/k/Qe2mlzoIyKnLZ
/WTy4zjiTi25hnMdcnKK4YBgGAFhW/J+DQ7oGWRxQ4D/GDBHLgwYepB9djQj2KJ5YdqSHeqimpAM
ByimA+4bj3ZYjRiusCP6kMAyoqolgqappaSIApgigppCIoKqWKmOlNsE7UEfIG45vGcRgz4RQkW5
7eQwB1lGhbFQYaAX7D+w+RG05hF5R9ww/OsVMLGwOT+K5FOX/FiZMRRRaFFgTWZmOBjFWGBj4+9D
6nQfG/y4SFU0BI9UDzXolyoe6wUWPZCexzhtMWOMox/LI4EUSNAMqWrTFnuddghsbIoQQqjeCcpx
lHOEez4jE8pMRMeKTpMFjYOEsa7W5E2kCEqIVAKBkEghEwxEorcnBNOX7DyAap/E7E1vSiT8AFz/
XUZewgyYkDYIEf0ek3fPBqkqRNDQ+jMoAckVMjdwYl0ZQlI/2owbg4go+gQhohHjAEDk/llIgoGk
A7jvA+YUPoB0T1ESBTSl9UUfTYebTQj+4y0zJDCRV0aeBFpjTNMxo5REfSB4gj4HcAhQHCdEDlfp
CP/J8V5NCwfJawSdpVUuOW8sqswMNXZ7HbrB6WRfYQJpBwC5zFQfEqjye5/ESoKdyGQAuSGQDSIn
PIGICpkKoXpYoKUIqHrAq9SqchEe4BV8SoIeE4R3qAHUqCFKeJRHxc50qeqBDhzAC2xYNyc6M6pD
gsGZFDoyolFLohGwYCVzdA4VK+rzOqhgwthsYdcwsQMFiCLjUpFuwcXtse3PIQNCvYCpOTDM9Xgz
fY+4NDs9x6LTmAVnAdUGICNvYdERMSB8MiODQlSgPO7fILWoE03UJcrQKrkFtSLU3ujVJMVAQFUX
vMQPxjAAPjiqngEPF6Tue9PGPsA/EPln+h/wdP+5lqnyCF6sfxBtcENjBUVQaSeydmBjMOKIcwQ9
LgbvibgeGwNTxqeaEghGAUVElMERUNCpMNEzUTRTEFJFSUFAUIyyFBBAVS0okwjBCECwkyDIQBMg
MyBEESwxCUhEQMRBJFDBAynBdqqHkFOHQGvqhIAxn0kir+WloPJUqQgxRD+qmQyx8IhraiA9GwIa
dfs+QdgvZAdHR1cOAcYAj+KH0haGYKRmipgUCWtbZMw9gQ2pADZH66K6edo1HYpPSVPTvt6pa9iW
liyLYH5B+A1Sam9LIG0gFicz02HLlfqN6UnwCYERLFulBR3BzG4RQ74RWSAAoFISIClKRIQUIhKq
poCUKVGpJBooQlJQCEZoGlSIoJIRrxF1YrsIBQP3oV/w/oqqqKqqqqqqqqqqqwAHgjCMEgPGHR2/
xQoREEJSChSIwkqFCNACRAUK0KBSA0o1MlIL2zEOSxCAUwsjSwSlNIMlSqUBEqM3RJhA0AUClCTF
JCTCRMglA0ImEBS0mQ0IYY4oshKYQkREBEQqypzCfcIKKElaQgqY0Bdn7EUI/o/tX9cHB4qCe6HX
2RgB/GfwKWrsforjSzBYHV1WH7XeGj+XdME7UW480Hmsd8XoDw9fg07PCHfn/Lm+HrbypORU1TAV
IwEEyyTE8Iw0nA/bmSbOUEkXlk1NwMA8eePFlCKaE8FjUAVRiiQmNEASE1VJMjMlFBBKJEIUK4gI
e5IJkQgDeQ0nEEfIqeQkU78F4HVQ0egNQOV5cAMCAMho4bkMKnHDARqKYmiimqapkAgIWigqikqK
qQZkWokCImGCJgAJJQGqqqqKKKKYmqKKooooqmiqqmqKhaqiKgoEKoGmghhQ9U/lQlvVdZfuhpCR
Hg+ph0B3Hm5n7TiwRfRk6YtGTDOg7bAKGwKUKh/KEI5IAbcgUOXYFDsKeslwIEIl2GGJ+YZ96RtC
BxR/2hodHB6z+4kigsI5mYjk/gsP0bB9B0HA3Lv6Pe08TpLBEcROZYFAqZgTGJilSEYBiymBWBBg
mSZjBATD2GGFUqHUHgS/JAZD6eiUHy2SzsLnUcOmL4RA5oQkHNz1KCiL7ok/ptcvU+OJUrRTTbHr
geKVtF5AHgeQ7OnVN0VSSMh2GBJPizANAEO5FEwfJcwdcyCokqDDnOKdkDgZCPAQeBJqPAy53vsn
o1BwLZyJDdxtV7hFW0h1VexCGN3+jqG/Fs42xSMio+GLlrXJAPLwg5vcxMQNAb4eJYLIq4Lq0KBz
/PSpgRw76KKhuLfXS7C9p/CeqBtek6AThGQET6NxbbnOU3SwPDS52o2R6CKo+gNgZiaEJKzAWqPc
Hi0P9gEG6H08E4G4hzeAqh70g6ME9Mqq8UqIZICPtLQhAdhQnj1i9foxP383nHSfEC+EQlDcBXTV
07O9pDgfamLwgeg8uLmxGnBPT/CMBMAAFMSkIUhIBMKNABKylUipFTMRERLMRENDISVJNJCMyMIU
MpB+V9ncB7Zf8h+8cgdvjqqrFD4u5T+ig9Wfy7YYN9PTE1uuEag9AUBAJlQsH1aonTwZhENOAo4i
MfQGHz9UDyQ06gYQ4nriYHzkKJkpoNE4JuGIHd7OC94J+iSmqChGgmFPiD3DBAMrQFPYpufQ9JtK
YqmGKZqaCY4ecjLGPyJQUDE+XD9ReYA8no4w7A8r4HOw7GEDlPUQ7UAPniEhg/VcDbmeInALk6Xn
t2L7ic4B41Xq70glCCUGkoFyHjgdCiCokCKpZqiSJGhEJojHMKWiIKCCJoUgSGJYgmGEIEIXDTQ0
SZKCAuPmqIJqKKWiJpVmaIJE+Y8/Jvui7IuqGLHVoiVqimB+WBEvLcOUlDgSPhUmEuwxwT9IfER9
Uwj9MGbhhAZmLjNBsYoaflE0wSrjjTGOKwCYJKkQpiEARKQBgMhiwK2yVDmVBKVE4c2s/UKNvLbI
uzROLqWR7uJIhJGMiSO2DgVNIUJSlmAGEGfmLt0AeIeZ2gInTERqKCCYggp9CdT58qcerzWiGwd7
AkzQkUUvM8CD7hP2YaHRJsdhi/Y6J26FCGIQSEBQCdAhG7FyBs3ikgbKCRn4c/x4AMErtLWoOzMo
MI5460epw52X9V1BUgo8+txAcR944EGKZjWYmYmEn4gsN2gYmypEiEwb2Yn8EuPmOiPTj6ngweAG
l5FE8ktAMQIUqw0y0qqUIIlItIoUIRCSNEBBAIB0BIa21IIe5CYowhFEkgQgQKyDFJCr2g/cSgJw
kEDBs3LbMSsjKnMAwE6pXAhcjCIJts0saCaYiu811BJhRS/udVGzIVoRa10GRoZ7MRU6PtbvLRsZ
bZqXGom0oKJsY5CRssor0cM1wzUjUiYJKilrhVRLPyDJmaMpqDHmsbqYEqqGmxqSDgzDIEJIkJka
ozDKycADz3RQYgc4OQXMisoyZvZsYJKqPBk4dbTlFOx5JtM8BGLEd9eDhkjzwYHemJETJ6gDPYQq
YsEKeWcY0xGIA9FjSRYRIYRJFIVSZVDwGIWLhouIIUImQGFpXUcyJQ4BUioxAAhTwCQjtcFOwpae
5TFJXw5uImBoEh0GKhhIQdsGKnESCGC0oo4cOsNm9Cdx2JCR9lFRiKDw9vPZzw3a+DMHJ6DQ1PZP
2wxREFUsAEITzh9598CQQKylEI0qlNC0BVLMKFKlFCJAhRERSVQzBQpElLEIFKMSFTNAJSoPQCLy
lEoEIQIpBKgYAmofDDIR7UeiU4EAYF6Htecw9gjrZ+IeIYjiv+QN0YgCAjd+/2YWAaap57TNds6N
IrCh9vMaQ9knCJQI0NY01QyLIIsiFNTcyryvQLIGmiyGiirKEgmzCEka+TnR0QkdG0xE5mZGFNjZ
zm8ifKwPYeRzGn2cTTi9o+cSZPKuJnE0rMyiXvNzSBkOgbHzIeVSCJ1LjUP2mHqeq+C9wlHg+qB1
JQhQhSGoO4OSIc8EZDCXB8cS0FfntQfT+cyIbBpL+g1GxBQKQArwHPryhrRVfYxGDQba4pBaDZA0
MRWT90WN3hCPC/niYED13hqK0QSPzs1Aw2U0DMIIMD6CT6l8fZ1gIhuNzvPSI+1VUPi7kK+J2isH
I5coyGAnvhMhyWXrUAkaaajRFTEEwk5vqrJtYrpwAfqGXkOKFyQxZA/3sDXzoRDT+G5TOCuP9sME
yhjv7zfub5kyWX1P4idPd3MoSKNfew6w16O0yzA2mw7w9BCRUkJFBPuIh242nh6EDCE9xHSE0toe
c0Ao2QIGwJMlAyJEIcmqQCQkRwDAxGWBwXk0gDgysDpVB959rAdpg7BQ1AhOn4Eb/wEGJGNkxRRI
Uoq20xRKSyNY8mJ0ozMfrrQaJqfujx7xsHWORtpjHlbg7KDCLzLWVlYcIUCxCgtJgFIEhBmMaVkM
Ijt7N2YDATIwpnhOAYJA7QpQWK4EBOZIZk9cTE0IIhYDVAoB4nMD9ZsYoiJHAxBtgmUyMSCbT7kP
5T6/mB/rfuuad8j/Nm7x5+rutRlCfyy+RS79P9QVVno7sfAdtHypi5SWXIGcQNNzVaUfZggdv+OP
1Hk4bPkhJJMhJwQX0/t7N7yibd9jBB96qqocF3YjoSLwIlQLwcYed+zLAt8xxO6g5BLFrUSxRrz8
8MTTWILGDonaF61sSAbFlhv2R1TU2jP/Jkaabfvt167Arsu9Q/zNSEIQoXiHHSyCGOJ+ksgmgRFO
zkUogZbA5odHT1BzRfWh/l/h/V1dREU9uWX4A7PFprrWFlhZZYEFWZYsWOnLjZKqfuhci98KhUHx
j/0DBi9/Vprs8bynidnWpGtRaJnLp69E2pUEEMncb0DQE/xaPyjwPc4wjLCfZ8fZ2RTbfvBoIH7Q
yfwkI422Qg44MQtf2n7K2+CcLA6dUfkNpIF5NIfwhX5NmcPneYQXUQ3LFF396+/HYZiXiNfOYvJh
CpEhnnmC8hoZ0CJwJdAmY4p95Ofy4ofyx+X58Eo14aaO+4jB1tVHlImtBACMPl+pGvmFBFBRYCn4
ZFPSdHjFDQ620+cwebmDsk2zEUUPwbRAibjI4QImN2kCo6TBoDZlgrcAIEIoD8YCoUBpl2ouYpNo
pweKFHcovxbY0fWnewiR2faEJONbjYLt0MGXBOHmpQ0SFdcPCyd2p/orEZBiiREtMsoESMBBAQlK
CSMg3eH3z+KA9U5KGByJzBTweVO0Q7ZFN0gAZKMEJQgRNKtAPzIEyKQJGBmAmAKVNshFZqTalcSB
EyAER+sI0D7d1BOLWDg+9Sxza0UGPccTA6DuNQYMzQgQHKLBYsY90MHO5b/F0qm9Prv4jxdIib2D
ImQQH6+DhBwbakI2oeq9uHzqiT6kj+BQxgy5evg9RORz7EWhbdrPHn10hPFEPIAPcNMgo/NOEVRI
sUQkUSNRAWZk8lQ4FqU00RMo/mPp16Q0CcFc6/jjCdIogeWRfMP9/Wd/EIARMZFgN9UwGK7kO/5A
IEIkMDHR9Ah6oKRTDuAete5+A/IJ/vf4X9XuR17hXxldwP7N9eI7cHHtKhEuBg8CEGEU0dPBed4B
0kn4gnMPOV4qiMiChqiN6e6JNeVzveGaUXqeh+kN++iqm3pLDXTR6J6ENUc33Itz247wOK34JR7B
RslUlUHG4kTVL/m0kg7IXoB3BJdE6jadRVRUmkahk+viENB3951HX16Gha2NSBCPwgqcdujp8tnR
n/gaDSQ+VEE+GiJW0+r1xPpKHUlCBfiXB2QrSwVjBRhZCqdb5fD3+rf8ltPKptsRjmp0qdaQBSwP
6374sMf73zKmDFMQCqYbaJ4Q4PzBM+0SFhlTSwEIYQhCYURA2jY7GVXQGxgnO8cCNHX9WY+K1Rh3
1aDsvQy3LeNGsIxtSSDUkorS6fmhhw6U54xqqXrWGsSQ0BAah6ZDihNAIyG2Pp9Olsa15uI0ibbu
EfvSElrJIw1g4/KjDk5ygpmaRkzNejr9Ljn3KqyfyvtZPflD+Ud4Zq0lwovx/Q3+owZQcgKTsahW
mPWtHktxApA9P2+5Mvv1zUNgp5uKNBbsMLkT+rzG4TWOhRWrTaVFepxsMDwUaaDCuzI4jR/Dumxc
S51OGoABbbputJgkJOegyHv6kKUVsxmzR4eHJrYeaIXw7TSMVIl0JaC8uS0BMf0c6W4WN8FwDgBp
EskUGEI/3H9T/GjyeLpytpWaiqhi4/4+XULMbdEzXfWjJKbAbplsGLTZw6NgMY2basiA4eDRgdtd
A39Z8YiJQhYjF4lJtFKRoOxTOI7dpQE7aThf7GLd8JDT7M+sPDuwZoTIHdUPrWGBtYEMKMIhwSBj
EImEcNxcK0mLUOk0YZ4/uGH7F935wYyDCMrCEIRRkEr909eDwTqOCocTCICJSCCyQhEETEESp588
qsO03arTii/3U9I0QEEJULEAwEseMjAhJIZJK0AgyosRA8XTvRDgzvIA6h+UpapLPzQkJ9lbwzBo
R9fjPbZEnH92guzS3s3UkPe6kbeM3uITu94kt+gnu1ueVKQuq9aQx0GXjfEa2N8ImD5U+Grz6Li3
CBlsyoRvFMWLi5D/kNhSDR+n7qlE5bYW2+QtnKf2vkfQYFF+5wF6ALvDR6MBjR5HugYRlF9Q/n+b
8giaRCtOxRhJ07Tr8bBNRBVNURBd55jy9O4zYfj1cUSP7d4qe2F7x6jrNFU2kBQviPVbuSCi+JP3
R2DsNx7P0EL+UgFYMTFwbBxlSEzGTCzTq6ufl2nvzIyLAyaVKIqcizBKGIPrzCKojbGgJocMSMTH
E3UM3EzDSHYixycHDWdtgLIxjYjEKyMmjIwjEq3NM3IDdnCiclmsTJhcKIc9A3a4w8sMwMTK3NdA
mXCaCJxzcA3NMNDZoaDQtokYImCiqJqjCLByYKQmSSgoCRCFIhgAWAJWV2cisgzdwl0wysDdcCtJ
hhwYXMB01MCGikpKFiA2BwMwVwzVzUZYP8sfg6UOQlIB1JP+h1OkTgBgHcc+FCwapA0acigTch3Q
A1Oo1vAdATRTMHcQWCw3DAhIKZKL4AgF9P0HJ9CLYa/pYdJF4Qg5NJxDOStdUZ0GDkbCEMIpRpHJ
VzMSzFCmgF7DnEVpqmCKGZaIkWWIhKG1xwYtDXh1cRZ6lOmmTUv3XzG80EDVmWjDKIowg5duzZyc
uEKRALpKCQ0hDDIkpLA0ETItAwRBC0BRCMMhChSxCo8kDs7OzTZEP1J3YeEghJRNgj1OHonzhAdT
6WSQ51v7nEK9Rx68N4AEkByTyUkONUB4s3aGqBTXFV35mkQELa6hpSJ7MMS4HPH6aLaI6Qzw4rZn
8eIPY8D/PpkcRG41eFv6w6wUdNsUHkxkxQxkydt9PoGBQ7dIvFOjZ9y/TSpD6f0RvgKdkvrwq2Gx
0CZ67nH0nsRPjYGGVJ8Z6NUuzzDM5IWiFqExOQISdNue1rDuBBOBlopQAqCwHiehNQ4PcaHkNhnk
ffBiRGKwAVA3yctkBdnQ7wwBkxLaClg5+gz4EORNHYnkTD2WMYckIjcMHCcDXEzId1zdmrdK1tYy
xXdc2MWwmVTMsZAisAIgKwjtIlCkSglSFHtjLJU4kUBoGuEuIwDhm5kgDIjAzkEFGGA0xESThLEQ
SoJiRFlrUaWDTSRLTExAhRSSQCYF6NEXR8O3t+fUYbkdMm9Z7HGD8J/f+OTrUVgwVOUJWEnaDrFS
TS4h8lXLTN3gmSZwOSvPFZcUeQ7Q2kfRDXktRiJYJ4fD+7xkucN3AaOG0TfkveabZtNyAGoByyip
7SomhJFU+IhFd4Wl5giGQMoFHhiYfSXZBQeIbQKAUOSYkB9q4TtMiBJiGhPAkJLZlq+fLVMrRyM9
yv4ch1PCbPZAqR7JcNM4QjADHMKX01VbOLRTC8u33KpoD0b8DDeqvtJIgDmEd3tugLkt+GSXLmB8
UlKdw9d9cm3TZNfqJ7ZTVjxAfIBP5ElX0g+vkvQh+pJv7V6dnDfa/+gRObLpRpaYnS62c91ahEDo
NNHmEcx2Hg+yECDFYwtEKIGibANqc/0cjR8mSmRmbhdSoPEoC5XPrJP2p+vGJF6g9uZUSo8IAIIN
xJrAGyE9BbxQ2dSlAMBw9VgeuPEZUE4IVUhtpbdLXqi4DzofJaxA5xOngYAaPOf5A70SZVQ3Gwgo
oKBJpm9+rLIwfdFpU9DrpTMB/ZPcq9ruJymZ+eJWg1+zOlIQAgpxNGlsBmZjSTlGQyNDMsuYdtZs
EhtGBV7eqxmOqTY2OmtEBspRm/1SM93wIeTNIxxZCWWet4K2KClzKIqzAyiXxw/yZv739Zj0R6fd
h7ex66xMZXsZIRtR8s8VUhgoWwYCI4bTcKJA4L5jCWtrIylkgKxZicOMkmpLIWYeES0+jrLEUfD6
3k2rZmjYmiUY/JDRp6cUwoqfh7UDA8HPtTRMqSmFqfPjA2EKCIzDDb+vTE5+jNIp2qiKjJDwjIVm
KNtTdjPeOLThJcxHsYujyRaY+JGmXB9yQocgNnJPBGiVVVRS0SDEBc5oa0RVRFdRhL0PXNKKDgY5
dYG1pyN5lVi5AfJI8XbKAfWCB4MVe8HJKMnqMzXajMiDDaxMrMC3dDcw/Dip3UEVEhVVRSFSSbhh
/OYHpPRyLMMwiN+BZFVNJElNuZRRXohianWCcg1hYsqMJKfYKCPuO+tDIvVB9wNDCCAuxxAaZO48
qijUjmyaS19bWCIwyN9Oa6h1tnJQNiQcPIscwfaAJ7nbwoPD+KDg8AMchkgoJqssvY+AWIebhB/g
zDuThLpGz5wuOw5/u5X3bnQ5vqxoMwjHMyBwrDIabFQMSBWyMDFkFwpgTFgfEEQPept3Z+aPHqKx
OjpkQzveFEIWKbHHMztYnoybWTeJ6dbfVto2jIMho4oLROCr8aHzsvsJhUAeuSNBw7fosgXFUb0m
xAjaRz+cwKMyy6c1OrO43EH0QDJConqevS/QC0PsGAM9oOpPSDZIl33MbBLSu8qSBxOCdO0H40AK
PYm408jrAqpfcRkq6ooyd59m96SXv64HqNAxiaBjbMwKKkANKqtKOFygN+8DfIifHBF3EP6YT4QT
IhRoIwoOwIcgIgAaAMh4ChIAnxJAQwSQST5Zl+PNRdMdMXEgzAChR1UNWF2UclEJ0MMlDYUcldCQ
UsAkNqyCssk1SFNRwUBMJXCFSkEDJUBCNSG0wcgNnTMAXUZZgIAWIJSUzMyetwNN8BwRegkCilaC
IiikiiPMObTWYCHDuTAyWC4gAN3wFBaBaamj+MF3ItL6eZHsjnqt7dsUwxHwnwh7oPAxhp+6rKC+
ZNsE9s6hRokXjBoyw+ssYGxFgYUUsVHYHASnrg16Iu8+7nguNFe6hnahIHyETPsurzAdrrt7eRIS
XZiNBkBQRmAPJ/GcDeXD3m74ZbN/BV42EKREyAT8UomQmkAAhugyQGCpSkEPCVRyU6kByFWk5KBl
1CZAmQhkJSClAeJhAfva4bJEpQSmJES82Dug7SEyDWUDftxUDZDkiUIbAL6G4sQ42wAayHUAbp5h
V9bDH1kQaZw0vnNmuXv++bdtRE20JtiIk+mZB6ke4a5eMUpPfpr29sBR3mMxhq03bGcWtadBIusy
w0BMBFDKVOQPFKYEiYTXAxIJJhICp7ijOsU13DyTy7biUhGEBymFcaSK0wYWESi4svBSVtnLM6oj
Qi6ubU2R5UthINmmMxMlHdKlHRaviyvG+CTh7C9OQOCho2rWwGJhgMhxAIGkyInBKYR2QHj3iFqu
I3nHi+vXnUujQ2GJe91rWhEmyMZp5bZoayUET1Lw4KT5OsBt438JBj7MiXgMXayT7x98icOU4u6y
4VWDVMkUkMhdGVay0FgSlNYBbCEMZFxLwsJRTjRdnDNMWE78Q2x5zOeSBLDsmij3Ical0xa85gXW
5U1oRJ1I+GiZeg4+dc7lAyJlKENLPGKZ1JTnKkodoUaSGhMYtuHFlEQ4L1A5gHvh0Q+nh8PfWGCn
UJQ0JEnGNeGPBogaCgHgsIxAPPGFyszFxmJIo7gx9vbya8fFhhu3OaJ7Wc1RbIThrlSFYNgNsgMG
EUw6MNTZie5O9tOvO9Kenaa72Rx1uNJmEsK4DpHpDjLaR2MBtAJpAyDwa4gGwclwvAWEscPGCbuY
Q55ynIaApB7gyDmYe8GSr8y/Ecw7g8T4l3vMJeXiEYk6nIoKAoXGy5ND6sDyQ4R7QX+BLVzTJ8dE
UhDi71jYsmVvNEAWDGxsQxjGZA8lyTqoD5E9dY82MDsgXKjqDkGyfgg/2p668Rr3ebbah0nIMl9f
Q0qi5EN1gaYr5KGTirTDTYyalfbZi+E9Yxduoi8+mt6KnDfRwhonC5OFsjTSXMN3TT175pBNcl7t
3GknTDuk1vAkGTuyKnbQAcYr25eHf1c2r7B6CKgM4baQ0fr9xSu32s66VSMIg1tgw84oKJDXlAnp
rV1djvLlpgHsOm0brIJENYqZMb28LKYyoy56/t8QNK/wIDBh2Mxx3FZkZU4rJZXJKYdRdGJdjSiw
VEs4u+AXL8dWkeHz8DrEJ5Yqu4cJB3Pn7ZBcwopyFyBsAjYzbYM0702iaNsiIwLAxgwi5gGRDsZi
Y/32P6G9xIg8n6sihXiWg7OeeGu/8izOMGjXeJgt4cs8f9Yzhu/rt0bHWbdd98t6awbEjtpFIwIk
NXGufSWN99ttmxzMFNAUMBCqKUWdEp+r8hnyM2NhdJELBgQkXoECKpQyVR0SJIqRtQ6TF0hMfIGu
OuvDjR5pf4H4ueUYVlZ+KokKF6LFU6pwalAR1eskEHVNsx9ls82FfvKKPMENSw0sBjBeQJ8lh5x+
GE+c2oQbkZJEPrgUVNN0R6cwxDYdP3XVtyA8lWRV4rXeX1c2EiE+/tElIIIsdbIocs9msfyjNxik
Z8ELEccYtSr650VCs3uja0NXUXzaYqr75t6+FtXVNptKJRQvLf5ZaQs7YOmYYPZCwhSpS6kUG706
hF6KDIJvK2MRT1/EnH/FD7/Z5jqOBrxKKYFVc+GZlkPFz2dfqej6yQw9RkBkIMGeh0IfSkfWLxxZ
4dgvR1Gs1DJrU4C8LdS3rUZ1uvsS0lesUGPxEg8x4cA0IiF7/GYj8UoZIdICS40JfP4HFSPdydLD
pLa20qGp3fRkau9O/S7vdEN8NRhJMIEjONQWtCA9iQ0kkvtJAGIaAXY4jjm90CiWz2iwAqaSwUpA
G5SCME4HJ2mC+RFoLgUNLkGoGW0mc90+cdqiZpHUXVSw0KRcPUHUO0C9iiGOE4h7JHJAuIbWzekF
2CQYlBbT4WkjYMKRQBiEquA2BgGggHAMFSwEKJQA2YLpgFMxxlRGoVDODJRK615gus6wg8LNZdjD
1qQPE9oFBknFNowQ4RC69grcFPjPF1ZCHkEVBThEhxenV4CTv1B12+HkYldw5ZGElzBY7khiPDdH
REaxyQjTozQkaNSpzMAyTANsrikBiQ/pdhjQU8Yf1MJg9h50XQjuoSrEDwd/Wg+Y7jURBdxtxREv
cRtCfV4kpQPYevYKcOLfiP0hEV+cgxIII81hNQEVT9EZ9MPHAaBqHWGK8GXlVg8kpPWGA1lkZGZG
RN6RlQMExcHDAlcxR8QUg/4DsMXYEoegYm6JmhNLoNDFk8AcQZIFgGUA/LAosOZZ3AZCjph/Fgkr
L6bGJdckKYkTezQhT9IZz7ZfJPrrE1lx1jttlJghN2RzU27AxcGC0qEF0/DekhjUK23GXPC9GPXj
PTMOiYK/R53wpxkSJWBYvCQsh1q4HwtGhq7JUPkiIxMM7yDskkb7biygLBhEHRt6xeqzqexcIC1a
fR9KWdEFWyxteYkB6TOiWQEeXkEifLEaQxLibAnxDmAAffXqT6Mp7SjXLU/Fd54tVmNKDjWMiYai
mX3lSO5Ir6R4DqYlayJXxJRsG8N13MBOSLChBLJCEMSBAEkAkEMQAQQpFBILJKxARBIBjYSkVZyr
F0UT5rH3HG1268If4+T1UlQ0nV2HR3HzB1TQ6S/UahlMnlE7a2/uAfHZIwGA70II6jwwiZ2hMkMG
j1xQ8FSnrHCCxrrHCNAPJzPFFDeKz2+AUrmHO2HwL4nqlAsMqD9JKBtJD/Xjpv00AypoTSFcIUiR
gg1GMY4KLAgGSYBGj0AP38HqP4OsNBxGDrgCqPOSmiMGkANtlGCz4nwdXTLY5j59RYcsieM0zUQx
AeMMiYmJiYohiAzDImJiYnjRHlhEmQEkCCIJCKIiGGGAkKaiCkkhCCkKS70zmxMokcCAghwikLMz
MWjISv4CZw3bLMisqTMLMqSsMUTAiQgxMTEkgmSiKiAcJxSRqg1FJXY0Al0sjWUMTAJM0wwq2TEc
iJTZc1waQDUhHTDTx485iD+QPALFCsfHHvcPe/Xq9Gug5hMXyadWjeoOKlD5QYLz+j1/jGB52nnk
GrqzymL1ze1laKqvSlrGYUtQr1w6S4wgHm+bdpb5s0OMQkQkQJEJmJCjQiuLV+pCJt9/usNjhzoV
EgkZtMB2KoZAEAEv4vYFdMOa9BUPCTZGbDBjWaYvnYyX/RB1UqBcxGSythmZET70UWhMQ3viUHpM
HyAUL4CPkrAGDbITPn8iggfmIJvX03S30wtjXR0iyVXYtonFMwDUtNURLHmgbIC38hwvpo+yLbzt
AkYDEkYDxwd2+ogzn76bto+rb0s+kS9umUz0qd1+83YLAYTD1f9H81TvMUvjwpQGRUdyfkMzrOHU
m/nd1StTprTiYNjj12GPFn76kghgRL4GtHzkMExXtHgoHoCxvhkx6HWgC5BvwKd4dDj0+zmJ69P0
yqMwwzl2jktJf0OBAwNAxBswD3SruCDKdxwP5hDPY1jDzaGNfN6ElgvC5j5sY4SSRvz14eRzEw5e
Lx4qqzS/JhhEh5POb0rx/vHgVUDvbGXZOsDxA8BPN4JICroRtk9XKtliF6D+XqAaNI/r8+Vwkgmv
k9/veZlMpCKY7RuBvnuddx+JuXJxbJxPxWz+HnbUfFAzKYz5iEFtP1/omDTGmjf1VvvuGLsEOiYL
b3UrtJxlXRhCrSk4IVzhjkPNk2We5tVqt6XyHAuSZfgji7fb2RaAT7no23A9qjXqqDkhIZKw0edd
xyDg+riFyUeoLVKop76tA593DVhILcDF6lTEl1HAQisQDYRhJXVAwjN/NLSsSh2yDE/zX6qo9j7S
5ia8qZmgPYFw7XmSwHGjnjzdiSOYbIXTHSUNVSSqQ1IpiKqiSSGWqEgm6jF+eDh44NrW6o1MUFGD
ijOQhDUISFZMUL26htCD2cJTfRfLm9NhFFcZVNdWBYLw7yV+oUOMgJAgnb2JCc4WEOlQAv9Wz9SC
pwteXqbkFSUXchIj6zgmwY4gIMoAXCAdXiDK8jzJ5uFcRgNYKUnU+RKBzws6F7IJcMvI/vyDEGUJ
rZ1gnyxPYN6HzOkxMEQaPd98piD579DfZgSghsFcEZSmwOMLAl1q+pCuwhIQcSixgoGoSZTjE3po
tdt8Dinya5lohdEcSiMh646IDkiLfmZiG7hcMBWKYZXajIZisTLQKbIZiVFCbED5b8YHV4TlWIff
rMfRiucBUtDiTGd7jL4NVPbeeOjV1Ye6JfVFJDMtUZKSiOBSxdFPaAmBETZEENIKkh1EEBz0rbOd
ADr3JbU+j5b18+Arnw2kxYQHG8CDE2D1Dj4hJOsCq8eWGUeZcK+R5CHrTYXIpmKP31FL1hyLUhlc
lJmQefZVQMg8aimILBS3GP1mhVFFIW4WmYXDuJcbPrXRVdwuJjpKRPjinki48Ei7SwewR+QI07jo
fAF/ASghB6sEOU7A2qbT3EhlVQVVUUUUFvMQ8SO5VXq5n3pqG6xdwRjEgZncRyg4XZ4qiX0Fkr6t
PL4YNkkblRYH1S1cIkHRi0hDF4jAWNgzCfEOENdsJPTxo4dadxeZLhIyqeGmxVE/e5v34sAH5zz+
3lM5Gn54KKaaaaZptNryyDJCEkYz7RnUMm9ERYQJaVFhAg4MJCBBtwYSECXjDEkw8masGEhAkIP4
u1HALzEYI2ewn2P3mu6aaO9RUN8I8DaREdr3Ow7Q7o24ZQtKU0qGJXKT6eXA5IPklOm64eUeofrO
hPsokIooIRoLynYnQdJ+QgqmikYgOx0FYQkR4R6v4PtMDrh06SxOZJPyA9pxsz6fUPxzn7+HvjTD
2ysG9PPsykEH7mhL2MRm+XTfJAS+7jU4D00wXw0mTlbgdTsNJkPmDZ8Zwh2bvrXamje9XV0rxGGy
uVJlR7YN07G2BPCTBpj/DgmcMwpZmT09tNh9rIofFjHmMixzIGYQju9tF5kYBpAXmDZo9OIGRQHi
BxgacTOu7WIs6SFGm0ZCA08iUOkDFqIGYNsZDXhDyEpu2fHyYu6HfBrcAjF0dGMaXZhywp2mobwM
kWXht0WcNZrTt16PVfjY0dmks3Bvgc85thW0agPebzUPHtDjxOEuHxhEXxyhWYytKthCASBJhCuD
o45zecYVCaS7lzjd9ZK2xodqkkiYHZziDEU9tlUYIFtM2Y1YRxyPj8mpm/cIrRszkc30KOIw7/5c
VjgYwmbQzdUNIhug55D7BtNorUweRzSVQ9rDWOth1rTHNsuR2a2TQhxJYCQSSiU0kOeDm8k4JREO
bNOJC5dstkZNjUROCBLGEw4RT75ZJ0xzvcskTV0gwatDsCBJMydwWIMIPPeDmYm8sMk8iR9aTNpO
IxKT4COhtlHE0wchdgYlJIoZIAIAu4V6lDqQOmrCAwx8aa1HYjChKrShYc3SvhOKkCpELDzkCpj4
lIeEXByGzfIlpCVGBdsyG7L6nTpM9Yci+dQE8DWRHIPSdqiGd1cs4Ftu8CSBmlchkNwEs3BrSgz0
i4I3Cy8iMYfut01wYFjOdiGkFtZRk0VBDzTa304LmtHm62xSC2yao0Lh4tdc6aKzGsUIRkYRjaGN
4bH0zXlFB7YGjyDMuIRBrpSrbzlpKZFrN3ydqiUfVoOMkPA3vv5HBzvlNl4yZyQcQ02gRuZJYWmp
06RgUuSnNK2hsUGacIWFDUIXHQfabssItHXI/VmGeUJcoeQVu+O3WHV8IzDkTuDMGIIMdIbKYzvn
ebhnfM3UEmJVPmrK84nJNkTYyefHDYfT0yJYy0HDjtaNVmlpcTS8E40aePe1HpOVmhrYw4pHs4OF
iuQjZJHItBOwzvTxR33IXmcUzg0FNSId6Zm87FDw4ClCJFttOW76oj9KaWhi1cIPDyY0RswZzxTQ
ZVMctgg6NAZKkeQvaM3FkEMEnB5db71UJh5Rsyaos6GLNxkrHPURm8xTuPl9uPpVrvFG4ikBT6MO
da8RBiXBGO/ML9z8X20PblTz27EDOzsLGOrSUt63st67JqVJ3uDnRmk3Thk6nXkM1xd2ZYGAxqlZ
jxB1IcVChvoVqrFSMJyDMNptXLLFhbJtGsG6Fgdd99UpFwOwGuHG6bRofOGrm6MqpSdVkZyTnEar
HUqCmpsCC2azoD3TDW+nbTaa0ZdxomByHMiZ4cpLai/PPIuSeIOnL8Od8554LFrF8hXXJkcOLE/D
uO22TNe38wHeHfCNDDDzBidVV0uhbMHDHLeKxzmDiYzZ/qpm3HBldayHDcHI5acXgJXLhLgjssGd
a6oxk7S64ZloR5ubiYRuXvnUXgUOofWTbSVqWa1TuQhJugKbHC+1ybHu+L0zofLEuqM4d4bC6Jic
ZjIsJqaE7hUTuZK0tMuGnOcVgGGUJqTHUe4fbWJLpI/NM2UbENtNoWoswyfjbVRfOTLShyKgzrcP
I4/GzbbeqfCKHpULCNxy2PhyyIeBA7WUTwcaLcqVyZMWsIU1q3Mdy0xOhx1SEjjy6hy1lRennL73
1oKxjprUy4dOZtT59LEuiGzzNoOq09pm3y4VjDxrM9YPGemu0QZYNMD9s9ZL2JivOLj6YZExXtEE
pQZmeT0zjk5DKdjUmYZ927+RmZJvghIplpNkfiCiTiihGhETjh4qShMnU1mB3kxI9D3ScHFYnEIf
NzSxRJbXUNhWsDufQnQJ03EQp1m80lSCFzLkZ4Y205MSatO3mLpxlu3GFM8hxT8KkHVukERT3h0V
qopQjOKMS1rRI7CE+zhpIezGTSky0Tdo4zobMxnT2EC89CvtHmHLouXCPbnc9wmsJQcmnjamcZO2
cZscKO2dZJIXtxxPLXL8uOeAySd5TI9t0VayeOs8LMMXFG7p4Y8ipSpRZio4O22oM5QPZYkIzGYL
IdIinFzLa01W5MJzjdUEaiGOhZiZaEiRxE6aJhwspAohHWBShOW2cFDEVyBBgxhTkp4bWcv1ERqd
GayyMmGMuDqw2JjFvnfwlwlsdBkXG8eE4hMerpE3hlN1WlUgOwW4pHd09TIalwQicalt1Sg2hwzM
bmLdOsZnLzTl7yIjgxkw3CpVLiXRwmAiAu0sSpieZeTPXnjq51HM4WPOBjDtjDj2QKCdmJdp4cOO
uzeBGGhEHCuiMQauqwlHto7HQ4cOGs0HQ1cwgHimv4xUvw0GocnlHK5p9ILlQ9w0n9tFkaZxdyzp
AtnZcFcQBo6vGMaPPF5FO4y8NcMosaOrRdNk1URivEo9tDGEDVhXjU0SclrbvDUpmqUY2YpliWzI
PcbOAdy7WhJ4RAQwdznZU0UzBiKnlLySuNBlYjnU0Sz9NatmMx/RXX957/9Dt3p79X+QTieo5dmu
vGpTzP4i9vZ5dMl853Wz2Too0g0wExiE2A/nPlkWUvgYx4QM2mTGQW00BlOG8MHDIPHDVQhHa1Op
MYlhAObvgtGyoYsa1ug2FFvQIILgZvib3EhCCPp34aXV6dsQ3Wr1iXVNIbKbNGm8VfuyFGaIthCZ
qxEpSJvlrBu9uEYk6cZeuXqsxAPE2kh09vOyyB2kLQeSNMUhxMWCIFnOBzpJ24yVjdGp3vXslWd2
QYNdDIjv38c/WKbyYbVUOYtuFe6IDabOGp06cF0mNs78bIqbGkvHUHsnS+PFWLuFtiURRjNhIHCK
oh8Lm91hsy54WdPpckDtsnCho2bUq4bM2JJYg3Y2CHVyrBQCMQ2hYajLXLBh0iemCp/i7QVuOt8E
3+OMW5hQRBDEQ/qclUPXetdmMYOs92yaTbKwdNUzaVs2u8rrh84wYqrMOFkAqhti0rQU3cUNDA7d
FjY2sWxTUk63v0ub2jbbNdoRifSBmOCdYo05ouENV3GaZyc6jzoYhEB11pt7zpScOcPneFX0ZOje
ptopuLs6wrCuNpnTF0uPl46mszeXAsmnliA4ChXqRZuydoMYMylDnpsktemDfRF1q6NDbXskTMCn
83+CIGZtpbZtrpbZ0fVFGIaBbUfg6/zKGMNXAEpNjF6ePS2a0/3DOCfpBwYDp5OcLhB08bjDuwc7
5qiSV0iZmE0K1B5VW3K8n7kGotIzmCkoqIh6fEyKRGPEwDTY7h1ljkhaHIXvnuGGVN/Ht7ShsQaE
wlfHW/EhmphJCHHe+MwN6Y9nmd6raPRcLwgWr+Z1oL3AGK6IVBQkQCZ6WRHFylTSVHOgrgzOWjtj
U21WkJG9qQh9n7OrF5rs5Eebo368eXlJte5+hMrOXNzcZswXlpVqKvg7txzvAexRqQgxtaoj1SV9
pT4Fi2hcPB48GkpvJgjDjhAX5OGwQlUQBIyEk34965r4vYSccaDTTYxsYZGxIJvdoUkVxJiLntw7
BmrblCY4FjEzq8Q9GqVtECUMhISjlBvNNjVEzDDQhTO4NBYiSGgdLQphSLDpCEwKse2zSMrlZwMg
d0kDsDCMRSFByy0jbYYsSISLxu93WDFLI0JJmqeWChoS6djuPmShweQ5zUBFBBUgnp5kTO1dHB/G
/bg0aZw9TPsOgelBjuHcOwCYuufVvEycHaOInBWa7w9xHO8lM5ZyuYE78HiA5Tk4tjT9kNbJsGIO
mtGmi/TNvjPhHwjb3+z2109/IeIVWaJQiWn6TbbSGMkiR43fTumQt85xwaYRtcep212KYoiBhQLc
b50MnzmYUzKUL8MM6KsSkGMEEuhBzixNPTnjmc77a0MdpOkSHdOgSEITJJuGh2xFoznouKVaM946
l3YdAho6N0IamqGcyxr+nkkavcDbp3R5w3wx0jz4HoIIw1pz5jGRzEnPmlbNo4t9JKZnE+apEqyX
kSd6Q6ikpOEjOCVETnILQ7xZ1J/PfQsRMc2NrpArREsiuMO+95rsybIhecVCtqHTRbxLqJLIFvDW
IwFPf6a2PI1tke9konIQYDGm+jyOv5ukMIQoPLCWPkjO/S64dWRAgMY6fQ7vDNA1+p9atHwfHsJ9
y2imJRS+UMrzVSWSCsgoy7yxHoq9sUtt66uN6YxftsvrGObVK9ytdM3mP6dcXb+6W463XI6Kz2gf
1GPv8T1N+vTp9fQ0K2EPGEz1FWgHrZ9prPhx86vOj8ZZpOD36x6qU1MHsQ52NNVYu+uGuCcNy7DS
lt1/pToBiYPbIzcyzb08ZkFF9JZ29vkdc03Eqb1y33kJSl0MlnRwinSCkdi3hthXutraGyOKwSER
ZZTksOTZvToywOEZmKQdq9HTKtSGCnGpERJk2DaxUyOcNcSuxTnzGEWjz4wh2iW+wqPrXKbYyS2z
vOWq5cDfwRSblxLVRIVK7HRCIpsnOa2kckgMTpQ/TfSzWNGEfUD/GbFt5gtNBLhPuUKJSzZx2wue
1RQWpCPFblzQiBydq1a3clzpzkjpxS9MQ7PqbdbwFsr3qt73ogaInPbOnXg6UUUVbaOXTJ+IJvYw
xWbM9LG0zC6+dEh6Mw2uVUg5u3TIcwaRXTyYyk5Z5JspkeXbRvPOGpZm7gcS0djDfZJEvXKVl4WG
NaRcqW1+Ks52kS9HlkEyNDTe1capzJtzvJ6Ta4HjLHtk3JucfT7T58+/WAeBfQ36dflInZoIc6pk
QQhulSmqMj1ofq/B+r9zf45IVTR4/x3wzI1WVdQzMYKnMGOvYRirqjaRGH8oLopUK7K17ir0cimk
AuBZ2A4D1I40M7Ocgsj8C6MxpA6ArduOBydBzJI4SQQcEg4iRByQOew2frKJEckg5BgcHHECEF8Z
snqBDb1xDdF9KBv4EduN+NP2bc8OgqV9nbqdMKhvjjfBXDikVuq09FpcxIgummqd5mJpkdetRPXx
iiY3wU26OEklPSIAozhDgRD2IHueMOaqWy093HBGxxtw+cbvOy7Q9PTjPgzepDuoEUi72QRjozwL
z3dHiBjSpKhRFFVBUq6bu57Fxzxktj68Gn1xwc7dGqPNcbCG8qGF1mKrJikA1RnB1Ru47k8M+UCW
FiW32FW4UA3Ld4X2LpTJFHYEMqZk5Im0FRCg3CxcwsPqCFPAmmNENBqc3xtbqoQgjBoW01GOA/Yd
Or6wj8CRMESJ2MJ5YqiQVf7/b8d70mNmI4NuJjioPJy3IZWEB0xUJWh6zEa+cQgwT+c59S/Ly9Yh
HqKBdsnEDD9WHDqqDhUn9v0VSm1EDmYF9HesK3poLfmkmeLTRB5i5FhSCc+HANqF2cptw9P7+db5
KKSIjY7zDamCSOZwloNJSqaGimiiKhD6eTe+nNVcSNq3osoAZKY8uLOQVBS/gleenNSDrNNcHkQZ
htzlJCjb4m/ZlhAkt2ZaPw4XIOCKAIoKQ9ywQiIKlgewgReqCAI8ApoKYlGkxTJjwMQDkSj83ToP
buiuzTyEOIanSdK6h/mI6Z4jgXRUmlVUGQOipE0qz4DQHU66o2JgWBqrZLAWXUQugWESxSmxM9Bs
YH0hBE2IQdYSR1SgmTQGT3DB2hdw4CYBlfGF7PYaAMQqcAjieyYAJw6LEMkxUJZUIZT2AnsJdFiQ
H9HsYA+hQQnpImjpFiGFTGyNr7GAXSkU1DphsRwmHO7cSouY5lGRpUJJQFajQZKiQoF3lboBmxQQ
Tj6Po+NXggOJxxUewDrFUPWEGOlBgMPK+AcDiQGPH1BNKGPU4SHYVENUULMRRVRE1URhy7MPAYkF
i7khg+HweB9mluBvWBqKwxrQatxWZBaFA+bq6Z20euPd4LKbFjqQtXxECY79hyqeKIiIZ0Boy2Wi
Y1RBJRtSfUox82F0DlBUTxER8aBiB4POC9jvD0C+2EBOuCdPWMQ9qsuOe0Q47zcNbt2Qgp45YPNw
KMjKubhqVW3UbQfbZVVHbN+h2E+k+krdAdHXMl+69bxmCchOEhTBEMHbOyfMW+BLhUmLsjf8vQ6B
3fzVBRB4kDGzJIzG6ySB0A8mkwTEsMPLjvLWYxwwhcICyjpBVUeEJjyQdRwl+IM1qb4OCFNGiCIQ
YaaaoQYEFyVCIsZhyjA0G+CrbR8Oeanh0oniNmwrzatClMRmYFWGYXEkjBvOGEs28TRLQzFNByOi
asRJtWBL9RPD19aqPA+A9jyjrUUidh6ZgY8HPPWJYzgRFOWOA0mjot8h2MvPF7NYss3iNmqGSKYY
Gbi0w6eklThDNcMZK2gwnQZkWrSs0xQ2kKdhUH+WIZv3khgI+tjLMZo4h0PbddNk1iDbgFZg6hzI
SnUSFOhImuHOkF1qAxcYRst65CgbaE0l5Gzn/8ewkj3B+NEaBCOzR5z1YVvwMoElkACABD/BVYjI
Uh+M32LCAG8CAIfHO4gDbIH85Cu4FenpUzjLqpWe8C4hQzDSsyQhTA/YHIoaGg9uL/QTTSjD8/sg
SIf7Z5yIJCGAWgEzO7JwEIQvvBnQcOYAQTPXEj9ygCXIOwOojQeyECI1NUedAJRTmDCSlA/T1ZWh
ASpt7EJN7H0nZx/uHiCP4z8gGjCfvg/qfiTuE5j+iuV8AESeeJqT+K7f0BVi5OhXpCSAEYD2A7C5
Q862rtkD/RPHAMSCEYycgzqDTN2LMr433fMPV0kJkYt9CIDXY09gkYDao1WTSDHMSIpAkgg7wUM7
xXoPXqjE74uE6EIdDcuzTAOj0QjBUOPbigY6PmduBp41jpNUYmnvHCkKAYgSK7xy2BxhjzioeQgm
IgNgyyDyGYORql5cAF4HDoqsG0SgxLot3tsXxEI3i+3ZA+Lgj8XdFPXm/enX1mKuh8dpTtOODCjX
8WgtAGazwYhsuQEhEkJCcwpOuc0OQOkm3Rd+26/whzrbsYMSaKgodjHw7K7EOOC46ul586uWgwa4
6Iuzprt0ynD00luFpOzLFnS6FGAwp0WwmfTOp1nMc8nNEC2zJC0bcseDgVrNqHgGWkjQ84bc1xaz
WzdI5TeYTXEDLqzjQYRB3w4s28Y3522uWzyYOR2a229GsxQsIeMIghxtmp11yw5kyQ4ZU1zmyrk3
NNvGTiqxoG0GtodNJ5Derx3iRXcfFOjrj72aYo+XWvDCXUK13kUYRp7nLB8kRw0tMjQZY2HWY8YY
0WTKkU7taQPQwIwjZkkUjYPo1HjFOHNzAp0KxsfPDIYDaOjEzKV9JGRjGQkjEQYuBkGDGeEhia8G
ZcKdiqlDTbAlZAVYuuRKvP6u97MWmB1ZvSiDnDmiUYzUS5zm18Zck073QuzyahnvYRBS+kOQ+O8D
Yw+3F7J6H4Lj3FVFF4sPrTOjZNZVQfTyqBhqBthrCIdvDJG8ned1YtrVlZZyRjHo8K0GQEMmWFhl
EZgJXLmcZHNwmqSIqmqIrFMyq15MLyd+m9eVLw+dmEZbG2gQzdMBA8QCQO0UC3LMMw50F0KBrUSV
/Xo41nBJYSQZWXljocBsFosHoTUhCaGQGwHI0QwbxmgFyxUF9KGgXU2gZB0Zpdd5UhgS2wpX93r1
FdWVxTNql934dD9h7dBjpPScYhhywTIShM3kwSyxE5jO5cR8WMjlyE2uQdaRZFDxT7NSB5D+ZYS0
PYE1EOZObqOk6QU4w5TUEDOc7Tq27KiSCCKJpoK8JwNQ7JE/pu8CA5LyBOphsh/qkA2enqDr7Mw7
nZgf6nMoeIDzgElURFUIwEUq/eHzxKIOMfxYbjgaQFVVFKuhOB04dvsf9OB3+kOI6lS3O9boNQcR
IVeCEpSMCBLA1RJDAwEItJELEgWAH9v1cQ4kzCMTsAAOZ/sZKGkVwQfJFKciifOMCRMSFA0IyTMF
XseB4dT+deu9E+4IxE2GgUXyCWETKMgRkZGD+IEucRM/AobGp5jg3HQhsOCTyJxE1AAgqNjaHmTt
j8/fYkNKNTbEoaHLIpia3MCagyQMLKqchKyHGMsIXItLApdBMswC3gXA3B4kUd/E6jebAsEtRUkh
kDhbjkzpb4dEmBWd5EMEE/w8yIQ6FzQ1ADKTCSzEuFkBwzJmSg/DZSc001lE3DqD+WwoNwTkQRJv
e6MjHgwQ6A6MAhAg4JDgyrrCUPqEHBOsOzfYPO3vdgdhmGFpU4OHKHyCoOalQxBLIEE/BA6QAJQZ
ViWJZ6OI9JR0oJ5DliM1T6klkk4JQAnKi8xCErSkFBIca6cRoq6A9p1PafKDmi83gqe0esyBBDvi
IoSAAKyAIMJAsUyiLngSviLKhyACKkTMxPnU/b0RKr9X3VckCgaBSRaCQRfxIH4lDH8w/nRP7v77
IusADUM/Qn71MREQVQVWBxzf6MNeNzDlCmhhAnJbfClY5SBSxEWQjDcTIOFtFkqmurKRDs1ukpbX
/iLaSZIEfM+kCppekPMjJ3GjgaE/ToDpIOQA98D8wQJ/kkA+bpzUEXx7CfUREnpULLQWGBR0DXb+
iO0qfbp/Obj+05QfWnsAShpSIYk5ZghDGVGgirjU7IDIDtV+g8VP6S4ofx6FV6ooiie+KLyGCRUW
0LUFJ8C/oT1kUbPyX91Db0Z1pSacNzSkIk5HJt3AxoGjosp+S8AyUiDLPKByDowgDihogmDLhQqy
4EM1bM/3zTUZGD+cQNSukqAzjpMoniQuGYUWKyS6UG07zmoSE75FkNP6oMICjb+vPtKICN3dIlgN
poIMwYgDIP97p/L3XXTOYEVmEYfBk6QKMEpJU/wkQekUNjO1ibUJK0SSMYxQk7SjHrCUg5DrQbGw
2TCcyczKAycArYcgIXYnGVKFpALJEFdSkRUwBsFSr/DRBhNMRaY4/fMyt8X056tgZcLICD6Z2zZr
k+aRiRiAQzS5eqje75+0G0R7O6fPqjjM2GoYBzh8gB8gnXOGdrFEQYQutlN4h+BPr64ApRBIsoCN
2d4g7jkdkELQh/HCmeMXIAZUTuVX+TnAZFYAYmhOEDn/Q/H0muCvrYI5kUDKM8Sw5GPaw31Tn1cn
Fyg8qFKmwBSg7IuSBkOSLoB8CRDIBKe1hTZFDAZF2PzQGSIcqQyTMxApyAckKOChQCTQnGPvS8GF
Fte/60olGvbhD5YZVDMd7GQh1GIoGNjqXtIlntzMXBAhBN8RMsGKaGk+D6mtap1zoLyEyCAKMHBG
gzIuCLbvNw5njcwt7jiPsI0EaPrAL0H/GLIhk6CbVCCcdBdqn3kEGEGlUppAClFiRmUu/U/QJ7I/
cS/EtENUK/ScYkwkQdCJs1D9ZtPX1PbyHKYnpOI6oV8uk1pBnpFCvDWJ20jSaq8J3xcNCKYWSb4I
a0cGhaJc/GQjD1ZquOJjBQ03eg+WByHBd8IzQI9IN4Mqne4pQ0GOBguyjiSOEdEOhueu88JwDoIC
uEik0SLoSZCkBGwZDDAYkYTMiuMCYSXaylGZlIO5M4R4SE0HrhpzicWX0Y6BnXi9igmMIJS+OiNV
1EAOgIAAkBYRZIDB7F0ejj8DgB+uE/2yl7zCCEOUDK2hl8pClTrRdSz1u11U5wDZqUJX5yb3mXoP
No+gkNkeg4jk7TmBC6MBcokoIXI3570AHAB7TibIBsrxH3qdZvDaQO9exNqFKRCtHMf5TlOl+i5b
lN6m5eATQB7+4n1SBRYBxNywHSbAgDBXlQhTDDEQb8C+Jgc/Co8EOxMzGgmKuIXLiBaKBbG4CAo5
wWEnpLIMkqGqhgkGgGJUBJpi/T6fxOST3VnLivj/LrG2jcNOUrmw+Hz4t8dvA2hFMNeKRGfisbb6
yMI12kMHH2G9CEkuowP6ULFw+djm2LZLhnhOUZMPZgkRncYCSXRpjNgb5e2ZkudAbJmE6oWoa1FN
E2zSoDQcKA2OEHjJPHTSiSd4M0cAjA5h7kVO03iRM0EEuJk7nFNiAQsqViq/her2lrYoeM6rVuK0
K5dRyw3QwdJsjZOG6cWlHuTmVlwdiERzIw7DHJApBzM8SAkSqhwie+9Sk0bEY6ZWMRxExeKT4c2K
EjKFAOSkm4Q6FgW3xozZaEUUBRfNwacwaQOLAyaEdF4I5DDh6djkcJKggOi3T33zd9uB3ZSXQ0zw
A2ghqQF1RXEwUFsUthhLAQQIlAIyJSOcBTGqA1ciQ1JDhIdEByCNx054IdU6JehjdarhTUnWLqPG
oygmdmdf781u5xvoo8NjRDkyMOKEk0lIppccqgy77TNpYvMKQiewhwxVD0lcLiaRQfRrR3e6bt91
LhMga7LhoLsWsq0pc5SaSXRDQAkPJ0BLqGLviQVMgPgMCJ0oy8eYHggDv3N0VOEDZgdWkUIrtLpi
GgGCodMptplJxxOvUjs+328V64VW+TVUSsS8ZajsnpVotDOu7W02a0qGHYmIQpCexquJwTHgwIUi
EiJY5koFKAsvBa5AhFAyA3O43I88qz6gSdm4a7wUPBJwD1O319K5GRmYZWWZYJZGFllE1YwH+2cw
RgMXcMEXRgu6TAqASzrN4ztlXXzSvlDrypdNPT2xdxJ0OnZ4MHh0RnSWXA0SJgQowZsMI8rA0jDe
iGiYA1cNRhYxVSJFzo1HULsMhC5LLhe1N0nhfNDwe51CJXDBKiRPB7nyIe24nFHJB2onFUriusU6
LSCGwpaIBbeCBAKjyZzfjqWiJYYTmb7WxYTiBa8N2nGGrwCSHEOA40Qcm1Lo6APIKRpTEQJ+1OnK
YKCoiKIpqmmgJKKqgJWKkkpXvI7Q/2lSEzCHyB6ST0rAYcUGP2/hwXR9kOBte8OZOc1NRltWLMJY
IEz7MGY/01RNR10Q5/Yfx2ML94k1GnDdPazNbhDeFFiQ2DzG9h5Y9sPCr2BapoOtxP0E5/mBzI50
BuzIEFQZmLEBZXP3904VlgX56PPBI4HTdgxgx4NhDim2ZrjMybUkOcusTAzHEkaB1MTy1IFJSUCU
yQlUCULMSyhdg+U+9ol8oL1LBhhFpEQYgE4GGAhhkZjADJoCami4bgH7NOYkUgUGJAdKHA8qh2KH
8asqUwEQxAyUFKMMEEqU1MNIEQBMgwQKTIEBBJKBsEE8w7n1LMJEFEjH1Y0voOswY9dyeTN0h686
MBlGViqc15HfYbNUYlAmshiIQh21obdJgKZDAUbmi97zlqdOwDs7NaRpyC6ZmqRjCHWl0OAzEDrO
lWuh9WZWGKTVW2XphFRcHNsamoLGlTaJV43fBrZ3p2zZBXWt3T28nHXVCTUr5N6jbw5GLNIgWXdU
yRFnNTJTHWDzKTFNhuZbYoVmbH7AKDjawxnI+Un4xnCptYuZxkX+wlI6owoqdTtMhjsm4WkVT7Yb
jBurPW2BcGIMzIFRW8pm6gMMjZ4dMe+Z1azEzUOFxkQ4S9pHm0bGrup2QdIILKSmaK54IS5yDqsu
sDRLcO5zXe8my9siiW1toDghkGAw2ARAjY2XYF01+F3RINXGuEMxMznCcvDGqMr8OtM1OHeAnC6O
KNYNU7sEeKMmLlnVlMumlNnZSGny0O2EZzGKRVagOLqCJIgaRMnaWocaqI+x0ZuuQzI1qYdF6dDQ
coSjqzy1Fjm5Vu6jB1er8edbaFEEYqoVDtDNsYfLmDUElrJYNxxjBo0mRNsxouQm9BGjaGRocIYd
LosAWGBuHfgkYySQsKmbbsScoHvbPtk0cRTUba/vee3FON9aTLhbjNm51yslxNWBHNWwlmwrwtnY
BuwFVg644YJUJmaus2Q2YmMUdLl8EbQyMA306OsZCAUagLKMY8RjyKEIgiwaSm0kaC3GKOUEOK6g
SW4WIxgbMDSKEMusCtyNio4LDYJgQglN7hIU+jxHyWVfGaiZnxtwSEFkDs0DoAflqKUgh5AskopB
sMFeMQcJEvCRcPdQp6UtS3YohsISXyZQCthkw2FGVVDgZmM5OK5hgzBjiYVQNmJlQUhV24uJLumY
GomJKTbmBBkGFmoGMiG5rhhVjpjMauBkHogfroZVAgRLQ4FFEPzRckWCFXPrH0j4HRZTN8VOY7Bl
nI2dycH+Xco1hYF4D26ualpkKcJgipDWAgGKSJXAdQDligp5ANTaBode8Q3lK65AmFLLRtsqaqsV
1OzrZRwL7pA5j5GhhjTqZFBpIxF+sw4lEVRVARFEQcEzTCIoiA/k5nLM/NamhFDRRGYeUwXdcsMp
jxUYEFBFteDTvnNrDNt2gwoacf5jQxWogI5izWLk0SEKkQ0KQQQJSSQSxEKJKEDCEDITMMQzCzJJ
VCQTJMTCvGMY3EiIKIwgKIoigqZEkyTIlgokKKGMRwDII7CxIIoBlhgieY4UwZDkzoZiGinZmAkh
BMGBAA5JaS+BsKYTqEyJIBEhGaKkXiQYTAEASMgBBBHYUMEGIYwwwCnkkwYCSRkSUgJZSqqiUIeI
YqOIBRLmKYKh005gi4YmAEQoRVSri4OCBCSEYGIiYCRMSpCGZEVYIboQGDQmDjgTGBKYyKRBgZgk
SY9fYKBExJIdWyDqDrkoQpd+wQ1QsfF8Simx3gAGjDJX5SKWzAlKvah4g/3UkdKSqGBYe9Pt91JJ
Jb4qSvnqjRvQfi85VsDhrLmDAtFqTCpoRKCG4FD0bdX3nh3Pje8g0f4utAIkgCDAJNmIhhhRSgDs
DvTdLp52MgwIGZcorXMkzHJKGcMzN+I6QB7lFlyCGVTgSy6gA55iRppICpSIXcqAcCRBVoRHUgPn
A/iOIU5RJppJogEViJYKBob1bzil+MQ2QqOBvNA8uN5jKMjEBg9ipyL7KhjIb6QoCIdp3ZT0eMqv
Dl83TmiaG9OEYQUllmKaqqoZmgQZYaZmImEQmqIiGgIIqWoSYGlkhA5npOU2qBsBOGokj0QgYpEA
RJoShioOxBEPDtSB9Qol8alGW4kihSXNFRdkVf2EUV0GAivoYCHAOcPrpOc6QbtFj/TzXUCw5qxc
RPohpFwCT3mM+N9h75KPWVZtTRfFonmuIH7YFc6QiRfoIXiJERyWJCQGQgMcwiCkJUoUHAlyQzMp
UiKAYDGj70DRDBlFNUQ3zort/PFB77ZMcx79OwfjR/SwHgwNJgwqBEonkFjCVTjJRB1BDrDvU8CC
iJrX+793XqvpPi0PFv6FT1QUY/oq63LJUB01ZNicmT6z7CYOUvR5EZQqhKBFm9He1IEprAtKndnb
osD7vylqqu220IK/YYSGLIlMFA8ypCjMOHXxgVg2uwtwKZBQaDg2xoSEH9YY62jQqRCpZr7o2f2y
idTkBkKjEIFAtUKlABkh1ImyNBWQOBRCLQUAQSD7nr3qoHcI9QqyQrQkyEQopEiGQuQrRSpEBMMw
NCVEAOSgGEDCRQUmRmQ5EYRiRpwzMO92eMoRAyQClKIyRCR8iQwg7Y9zo79uGQ43vNhV7iUYKH/N
heMl0nIm8uiH/FghKg3HApQpTkBjEnnA9SXSR0kscEsw9p5AvJQrbI5kX4Qk04RkG7AvAuQhFdIb
N0N/DBZMBxDRm+6gj+4rL9SP+NEs3CMosnMR1EQx/or2yIQbd9rQYzvsCX5az11aRwLJDIwOPRII
fugjPciRdUqWOKfhQP8SiGKoomwq0OXr+K/jaEkOXMcJ7s6JzEE9nh/T1h5A8h5zhzIne7ZeaDx3
kkNIo0jM2Jx6zRzFCPOEDuIeKFGZs12Uf3+3+fabHkwWiPYNYcQP4Tmo7NgTgPv8I0aIgZsxaxMM
MhiWjMRMSppzDIJiKQxqMmqSqaJQgmEmRaVACZWlYCoHj+z7+QJAdwc8XahYMQEfpiKBzHPSD5Ma
mQOR0S7cKMYHrVVDdvdPJsHcf61o50y5jxl2iQ43r7BpHYc3QYHBUPuSMhASJ0BtoelpC7Lk4K7u
6kkA7jJKDjY9xkHHBghHka/8JmCt370wuGAmUEDCGC20SSTMpEkITs/RFDBAh+VTu7COsik0wYP5
OCInTTTKPdCGKjK6j42HxxHQmHB9iv0BBJLDFQUkpAyzNTIlRM0iQEkQUPaAA3UU74Hsh4wvsLIX
6SlXtKhEsLjPoFtVhUU4xqbRWep3TP7cXCW/yXUn2nAL+bhmDI+kicnJSlZFY4MpvZpDA2BSmoc4
UrGABw+T9pY2/j5lpqtJZOc0h1lBQ9mnQuKS0DgehiHCHv4kGIwaYAYN3OO6D05BOOQ/hSENLIHI
8RSGwUahklJgz5hPyJuQPqUHofG8u41EOU8VFFUFFFVMUCSjQQSoBjhEKQW4ZAcwUG8DICY1p4wo
MIHVAEF46jY2v5XA2HmhDlDmGIQpEuIwVMh2wLtjk08I/ZfeD5+cGs/FA9DWQgZEiD3DFyqrAzIj
D5OPU3ibh3cpD0BgHwmlEGQRckRdptD/FzAuTmIeScCc8NNBI/SHoWR2cAGlCupyfhlhhhlOWVlj
hEWr9fo/m1X8En1M5EE34k7y3rTDmmZb/qvRmS6sWXC2V5cw48arveQn44SkMaGjozufF31kFXic
jIyORj3Z0FBpsetwqukqU3TLg0VyRtJs8WrvFU9bzWajY2284pcaieQhWETa3KwszMVZY4mxRTdr
FoZGBjCsSMawaNhq35Ef7HmI/Z9Vs1FjSxin+BQFp3UCtXJGq5kAj0zAG287XWRjbCJuIiqZnITo
OIRG1aVROMG64IHeqhXSVWWfzfcgRVQ4aXNccAjAB/hQf1IEciDBeInLdmJ1GeGh7VjPAyCd1VVV
VYGEQ7RcnIDlYuH4i27EuEnTmYMbNBQbORBhhMZGBGWEZZUlZGEZJMUp1H1bg5YWV9PNodpAcssj
pgjXLMBYQJfJVg2Z+QbUHV8qjx5EOIGQ0FAckzRD+IFlHEgd4HO8LBkCbhd9nlyr4TlSMqk2TmE1
5fCcFxzBT8ES2nNRUNpCdGzuVG+YxBuXsEmHtD7P2CISxURYm0gaghQRBCqWI/wKLujSjQkkMQiE
RQB8R/wkiBIMii6ZXOsh0X2XIJyO3gGWg8Evlz6SmAUUE/oZJ+8FvzxQdZxm0J5HNKxCizA5I8gC
YObgmGURZjZmBBKRB9s7ribLjtOHJAPyQ986d++7YqnYQJg8SY9Q/UpoqHBGIClOjwjEwXQcJpgk
ph+xpqqRwhMbqlIVoIxCLZGqMRP8OOCjpL0AdUVzBOEDsc4c5uxUsmvHJFTcMNFqy5bhsBc3HSIp
G5rKUaxpsiV/3CJGGTGkGYo1SRwIwnMHJXI/qnesfEDjmd8XO1JrJo2TYdhg6wpP7bkHMxEyEczm
mW0U5OU5myWay9abG1jYo8GFhCQy1ptpioQZvWCxZLJLbLG0Kg/sQhZJCVwbW3MGIGwIyEhk5o2u
bE3jJyB9p+kO6+waPAfy7HBwcNBfEaVopWhu5BwcHCHyw0XSl3QLNYT9GmfLoDA7HhhyO6qSikre
nNjSIskrPxubBglkF5wuFhYWRWFeFtHYsLCwO0DaDsNz8EvoBfJp7Pt81xfUiIo3s3QPJD99oREn
bZLQdkYQBTF1OXPvA/MgAfXr5fn7u7c/GMn70mYZlUErlGfe0EMiA4swyBz14L+8KUun2aILutpW
QTZiHgxTrc4QnI/2C6WYBkoKuMTCb5MTqwsANEIhoocUrbbGwALf7uBhB1cWHx4cE0YgKjohOu4M
5460LvE8WkUUKUo00Hd0SMCLGNnVoFPtWWiJoRw1rYNflyLF9iDQBmBdBoTuiaEBYsEsYaNihAHt
BwukpDXWcdzMLcQGITYjgwj4dpAY5CE/qIcMRWeDWhrxPrXIaB69YlRP1Qr3OgQBfgxDZD3GUomq
fGImQfH2zbjB8tfm7g0qdDgD6T2npJ1kmkVbD8xlP5pz/QQ7mljbXI7SCqYAF5MqZubehxmuPLlY
tdpbxoxG9Jigfww7MQdD/I4Yh7ZhZmUUqQwEnUKYOJK+OmTkINIFoXy2zoKIekLbWG/qrohqaNrE
MVdE+HaCWGT3aHw1rs3GaVnqYQaCKoS0kQ2hVWRHm5/0X1/PNOZzoEM3P5BBqihiBunKTwDM07Ev
1c9ybE3COcBEDqkQmhhLtZzN/vPcMQYV4ej07ee6ANrRooJFYDVaPNLYd0w68LlvxJ4dWoj4ctDB
YblELgkNybYqeKXIJN1xDEdRn0DY9NaYd+vgyB7MgFhB5BTWTkmIgHOjiBqB0YOAxZuo0qBscJ6H
IV0IhrCmBJMgcQQ0PugUOEJBBVAHUFDxH6CL3+DMhCKgYqEz171DDeYU8Ppdh7Phl/DsmCS6Gh3U
VfCOlQ/eGTlMpMgdCeq702HZ1ygA9VujDGNehbnEeOBoZJh4YMDg4Aw8CRM6S4PjU1uVhxWIDDHw
h8T1IPyPTwTQuVNjUXy+7KL5iqqqqr1BpsDDU4b96d6bIKECkU/TKpEB0LyxO2JqaKCQiAgn+ps4
OYRMaTYmZiKmBUGUmRjhDmYFJlkhRRjmy6smBZhmLhIZFgVJgR/CrGMvCTGExkejRwgOsM5jpJBa
kLjOExCakigTJgGJMiZmCJowpjLDqh6L5D0QQDk09BbA5uZDcfURUw0In4wHiqcj3Yh9zhnFcYqC
mCdhTJgQKIAQqkqG8pTAGeShuGNFIlB3WPZitwLCPsDXdB/P2QSAFICESgMQYic4Q487A7SMYWKi
qoCiZAwYBwlWkLGxKInEgEO23XAMqsfIWhsVGBgGRDU964uyqXbgBpMBIjzsM6igJJGSqqGirjgB
Pe71CmGZnOGhsNFCUNXTx0A1gqhMCBZmmIYJYLEho6zBp0JmO3okdF0YlBkJDMT30xfEUhE0N+Qe
L5w/8OCISKaBfm2a3CMOALcjsAkQ6Q2idRs1aCl8bYPSQBgoqGNFzG2wSMxGdQygaFqhGxlg2Lw6
qCJqCogNDuPw4oB4Ew80gciXFVVwDtN5MJMKsA0EAXcpqB6ggT7xNNCnqjg8gJgnL2iGbO7MscyI
kkrnQV5SfnGSUimkJgrqJNY+hDCHpPMRyGh2uI8Ot6uo8fEd/HFW8+g/A2PxCrCjaHA4nEsdGQ7T
UpihQzcmDJ7g7nmcHB/oIIEQIRfCHbANAbESMPLKCQeMZJvE28TQNTebixbgMS6Auv6gnwSQhDoV
+k+lPq+tD1PdjMNCQkJ3u+WTapKSQOoL5lGVu9yF4mISEx5naNbfbm4d4w8Zpv9incSlI3XJQmCr
QJQQpJSEmB8Hr4X7wjDvhM5XDDkMMHtXcqIHQJD6ke+eVOXeQDC+2UoApWgKpEpWmikKKAxwBDYe
hAD0U7aj/1VRCZhcNyoiJud5wAdXZ8wcWHP0XN5a/dDMqihCJxoIoL/VPhuXEkRuUGP6Z2UDSUXr
hgpslAVyTrM2DJcf0QQRgSFwhKAOWgkvdkbzTWaiZ3DA0jF0LKjMIwiMmzvTNosTC3MoJNgcybMn
+SHm4cMMWyxIGAiDcTHL+D+Ho5jxDagqGSAg5Irq+OC+LN0iLIjwyIGbQEQggokKLWDlGIBhgkxS
AkQHMgnA2BxkyySYAgtqGkXNjKNwMFaUtjBiUIqIih6cTfbmhu6A6QRNRKJETFSmh7k7UXuYhSET
rLa4hSPO8Uz9uHcazG+nSiyEmQ2yP0kOikRjANjFwNFNMcJ6pOiA+ueQhHj6MF0KYmU5WwMDGYXF
EgIAgD2h+HeG5iRO4hrCpIJulJmGQd1xygmc1gtQvvpTwt6XmFyNYDzKhyA0B+o86bliINRD1hOU
UA3JrNCiVZAyPopLt3KwmxgGH8Op7o229zLZTfVAfjvKQiinh03iZ/t6y7SVlk5nl9nKOOENo4JR
oFGgTaDccEowAbAc7RUEBSwMpBIImUq/g62Dl258nseT9JOwbgVgOEyr1BIG+EDYk0lbpctSLnwD
5e0uSu+DT1ZJS+J8SKe0ANP4Ph5tMh6YAR7j5pWacZgNyEBpsGyuRwkZt4tSJKSpo2xwlyQmsxR2
PqhyX0inKJ79fQToDwdOiSOjiSjH4DXUJB9Ja/pjvmD2GCPuYYyjAxIm45QhiGZSiH7ESCBw59Ef
59A+MNo7x60m4yCazAAhgBwzkL1FuTeQCNAlMBFAJNIHGYDvBOIf2sUUwSwwjTRQtPfFOycNEeGq
cCQAhkRqmgoWkFi2AeJU4wePi4L9NEDygUgblPWBy8vQHIHuEB/aNSqJ0+z8dFxH5z5/Z4zM+M6R
/ZctS5isY0kzQ0LQZZJQGEiYzEtCsRhA4StKlDQUg0NGZgGI0AhL5Szn52N5vJFYFhT3nvaBACH5
XZ4ujCgOoi0Yhohias1bCSVU22wX91rZxA1vdMYcbm9yf7UAia4a209xJSTlpa43UqmHLOGA2Fmt
YG2jG2NrRoGsw4JpUjKS0jaN8qbRNuhDwnAsURcj8dVPIckXzECSTKJATBKskgSzR5AgOmBA2Jls
NkpCxFQ+46H5DKgtVJdUNNTcJyeaTFBAg4sX6pRGkWgATIc/Y7GiwigtsygRHOBID11txUIBsZoe
QIwBiIEUYeNiR9yYjGRi2zEQLSRG5iUD+OuqOdUJrg6bPVHKqC6MeUyEyzMMYJcDafmffBRBJEgy
SooD+D/ZzBkopSLbLfjuqgQaEGzTIY5os/VKfWLSEBwve4LR7O5Tq9YdflhBniSEagNSHCq5QuQ4
F4m2XcfIQbv4YPBOLI1zMykNxAbucJMzK3spahIoKpEUAroA4LhiV8uoxHwh0Z56UGSeMIrxTnak
SDc0Llg5mJIojRBEQxKiCfM6a98bCTI08AmvusAVcDkETUHKLHabR7/pdQMDEQlhiI0ChATnX3Jp
2HSBwB3nuUegwwLRAdAQ6rYihk2QbQFAn8VymGyA8IBwx2twD7gez3s/2jiDDESDH+SDIGqpBtsS
qv9hb0zIO4rpPFqvNFduDxk9MfNeidITAuBgdT1ibDtNSLNstLTkmTkGw49oDIIxzCxvANUDIiZF
0wpvYQgIIdELYdAP7GwgRsv+mVj0YioNIebFBENic6AhxpPgiyA8uV7NLBG0kGQB1SHkOQE4hDtA
HWU4kEQnJLYNAMx8HQFD3CdO0MJSn5H+XQ2PVVruvC0nE1hs4MmyFdGcQzhUraNiYa1G0PSwWgrz
chLDbz8OJrem40oO2DqYGcxMPHgtHepzqO4MJOu3OnpzGm2zTbBsiqsKq4R8a21ul2tEyt2rErc3
so4aahhBtNUXieVmYcg29E4jfDeauFZhMzFmQcWjT1qmijIziRaCsY04GYZS5fJAeKi+RfR9oeYx
uethlUoWRXMQ7juRLvX0K5X7Qe8aTIDBkh3JRUh31/eOQOckkHXwpPZF836lG2w0eBBFt/HgjEhl
X9ymipwR7hESrk5WE7jIWz/pQORkRMrEC4+z1oe8UkEUNqJn28BDo2+nc6lKNz93TMbTZIuCQpRS
QSg4xtcHBghtf3uv6vh6z1PDGYPIQip/rmIqmsp2ybO2JOtOD3/KmbLBVh1BFs1y+bfkzqVL+VcE
f81lbG02/2fvq3uWMpJM8Ecvx3CKSbb3sI25DEFBpr59OGnphGeC2TUOlgw4cTcNv09nvptJC2NS
x6yBsIbDtkhMEZnjRzYc0yUxmXJmPHZ5mEYdjzZim5CajGQJlHNyeTf2+qOiMGdkw6lJ5JZA/m7a
Vhsoq06sfBvHT07eWdbzmHQj3DkHUTQTJUFESROdHr78Q6ijzHfmw8W3WZyPFOXdwibT0WMQ5W0W
/j5Mfzg0+gUj3gDCDC66JUngztkqH8kBL0evkL+RYMGs4448rXjPuEhe5y68Xlgh6T1l/E1pSQe0
gf0WHJxM7NnaQHnSKSLV4tsBj/kzfYA1plTbZ8xLrEveJFFheLiSLzZUkMYIhrye+PHr4glJCQSG
kIFPDvZ5qvjmekZ79cdesx1FZGHd3d0tR7YlN0GFbLozbx3kde9GcSjtJ618YB1yZyx5aGcVfJdV
0xXE9WZmG6+JqQAAz1b6JAcduvAU9ZlnKS2jFWPZTEMEBNeWO8eV3CHfzSTJGm25TIpA5CjjhhiI
8ObqcjE6wM2jrFDG7OmfIh5NMaTQmBjPSeammGJwuKgSS7yUwKWaajSKw1KuMNujHb4muUzR8oCQ
r3xuA5g/rZL1ZmkJfruT0Rr0b9H56G1jSZZ6MDPSwUZPkZWKg3CDRCOETCRg5Go3IMUgb3AuRZYS
UbTGM8CzJhWOwichEN8EjTswkiCicemzaMyxMMyp+2209c00cJCTTKbITOkJYMkjachGxtsbisJz
MQyOR1LyOUFFDMyVcls7haEnOo+RgHEgFI0JSQ2m3Brq3rwjwYbbKkIS5fFuH0DfP0SQfNhrNd7L
I/b/3R1vPySRA5CD6cb8m0zjJd3MoPDJ2Vblg6nBpqDz7PJqi3I98UTw+REDknpQiyAZgoAyUvWD
EEgBKSTpgAAfxp7fzfIfZAHsn8/4hhyoGNIIzAAaQVQIlMAhwEek5w14hxTtVP6iHQVP22Y2/gpA
zLK4IJaqAg3oMJCyH5mm+HA7ODnwSQPDKHwiqiiImOjskoiZqCEhp9pXEiXgUmYBiyYBiN2vUKn+
KKENgMDiagYbiy4SPCFV0DXGDMUNYDWQWJFNf809vFOAMQKRCSQSxBXxHtRhvwgOSH0MFIBFCxSU
Hq4EwpHYWAREJQrCQBUzAk6vrTtI5Pw+sE0VONZ/BEAVGEYSQ3x6KPlAJRhd4HaccW+EOcUEwBhQ
AILHUZV+dTgMQhqCIJq1s/qy+Efqrf5jqv9+0NNkf5/9Xk2BsSSAuLMRlIIoOA7omnyCYyufPBCR
o/r0eJMdZZUtbvfkZ8DS3ESYp8ep040cHabohKYQm1q0OPDIsD1YVLs62gMS7NAilSDQmCLJ1LOl
7kZRlwijWgxjRrvJhoOma6kWJs4kmuGs3qPck1wcfbDPpwYOLuHEA39z87VBKGhySnsQbOo7NRXW
slAy3Qctq0QM1sH62+QkLr+ceb0ep8DhP7xUjEWhiJlMSQtVRM0JE5gSacD5oGKfUfLlOkEwlCUE
UutKipwdgv+cIEFDwAgOsRFkUtNuxmuktkoAfqz/xcg5aEOszOoz28of1R0cwHI/XTNQtFMwlSVU
RAUzBUVFFDQUTJFKUlCQUUzBLFEFIRMMkkE0JEREUREEEszRUwQEEk0LSUsFREXOb/FeQN7t6avC
1UP0/KBh5Q8aexbQIsCddD2RUBoA5i4dERsv+S9F7AKVemoKNn+NKxCPUJ/aJDQlKlHOet9XZfvG
nq7feFAv+h+h6TISWE//FiLk1Fj6bM1SjF4KqENjDQQJdnDz8j2Pe3wKR2kCgN0+r8/rEaNnD2cD
ckeMr9uPClY3JkpXGmSSHjFCs7MLCEGhwxlWsk0u0zGczvYPafFN161mN4NtktbUgnZDTMbyWPe9
78OCts2RjWMRvFBWMyn5sg3eddcxonBmMxI1u0ImmhtN3r14wzRGMrp/WQN6IKXwh1p8XVSJwTEv
qSTj+uoJocNChOPah4hnCOyOm9m2oNTYpvjLDe49b2FCm9LGRjhWZxomlhM1rWDNA5NUypmqXURA
vEzFJxYTtu6w5nHEDs+HxkAnQ5hS0NB4l97d8PhYreh3gV0rQW455M5joO297nOYULGVVZO6lzFM
XaY0bvO84JOXAWAaYslHM1SSSQ6d6yi6pppyKgSHSiek0VS5J0B9Nxt5OaXiGmcZujKOpw1l5izL
naq2DvBxLsFwxDZGgIiEfCDl5Ml3aW0k5FU/+/uzSLVrWZmygmHfqpukS2tO2DktrKPdDC5NUs10
uDQ3rdItNzBg32zjJrUsN2xNyrONXntN3iaTkG8wcGoDMdr43rXMc0UyU2PDVeKwJvUy1QlK39jm
pNNSQ8Ja401wrNdOm9UUaIwg4s6W7Fw95VubyLW5bM1xm9RRzOKcWZvWzaYxtOLcfGFYx74hAq1i
wLIg9iYcPMQ2jT4vC1B7ZNTm5dzJcieFijEOmhU/BhXQiIm62qaXfBiYG5tXe5BPnOpMsg9Djwke
+YFXVjPSIi74ZoK9zeYGzgmBrTbOMWKRrcG3OJU2RltqU9fb/CL+78Zx+//Sz+WIibYmPrCMbG7I
Wz/jZjK1jhrtKxjG2nuImpt4hlMbMpJFGN1jddTrI0twj+MUYJCStxrDs8KcUoSYSFkOqNGXdKaw
mabCQPe4x8pyGDgr7kvPzhJA+IMPu4Kgw27bFovZFqdsB+MPWe8xcwupFLrYPkTZj7BPkPlED5iB
mIhXpOlV1BiSDE3QEbX+nuGOv4frns0lpfDJQNQLwv8quhYUrhDXjLpPZawllB7IGkl6piGwRLEB
EA9RRU5PZ9YeTPPE8UBBqKvjJqTYvQ1l/yfqvHZtsY3EtAckMJOYIMlgOtHk+gKIh6jPToHZoYbM
TP0GcEowkYzVdlE/aaQJ/ZCUADxcVyB2HtPym3lOY/BRUahn0ZHVqROGaNa4ZpkoUbf2/8THqOjp
CyizbcKImK2sNRrrrnDhxUMgGgiAKLp2f96GGkSRAVEROhmBStVSUTQRbGidRFVVTMNFRURFRERV
RcF0ISCoswwgpiia4EFZJStDNmYYg6inKpipioqYg0gXeaDXpifUJyHSAm4AaCj844CaenxmdQ+z
TDRsuwiaanXuUf3MUDuf3dZ16er1PWCf6osggQNuKESFGSAXcYDGKGYMoiGAr+UeCELEoSPIqDT5
SEiJebTNMlKyGIEpaRCAhGiAgkggI1UDAcATYd53uihqa8U1w9kF2uhEP4ku0XSGJiVPOiEQxEUS
sQAUKwSo1YdnQFPcUIsdz09UPTCvP3diZoJmrviAxE/RFCtABSjJCFIUNCQxBAQQMSJQEUTTS0VT
SMjEhKEFFTQDdxi4MC4ehEwHRIIkoIgEiCGPtQGH+NfI/vr5Q5w5wOL0xBMJEUhFKsjDS/WzqEe4
VSV6uR96Fu0gdsCgpohYmFCwF0Vg4wTqZIzKU1mM4AiZARQRMKp+SQMgy3YsFrZgMRBJuhxlHyH5
9qaP2pKKdhqTEQlMVBMRVQpSGTyMh7igyKpCCd6wNLhhRtp3SBgYPZPXWHdZgFKFKdDxQwVPh+OX
FsDcXqS5i5fZ8ksporVumsJhi18011JD4jC3NpmyF7rYo6XF+lnoLqBzAYTYgM8xo2sNAw3Qf5Th
gvZtgDrtaJuXpgoYIRpFCCRMR53fBGTZhD9QXm/mZP4mQsW3NrAdMKRSKDJu2DHihkzHR+AFAY0D
f55FTfaV79tj0Y62vThTZSbvRPRsBcLpc4aNQWMdaYtFsCGSi+OYxVGmBHZIvvtEoZRRCfAeZhoW
czOHEo+L3CryTzgP74SMR6QphtQYlAEE0mxsgmx9Av33caBiJIIkhl6XHEmTUcBcVGmgRpqhpgjy
ah92YI0SQaYQqZcIAqoiSgKQ9oBnH29XQMwspPYjm3BNmwwLZfPYho+1MOfeA9QuHbHb7I6Q0Q6o
XXzTz+YZ6ZmiQGp83IaaKbEZmqC0NajQxQocM/alVDUMYxEsXSST4VZi/5RKtFY0qh4qqy8OPj9k
k2JsW5z8NYLMJC4UznMPxd9IrkU1utNcjwyM9H5XG2T5znh8u3FtStD0mZzvttsrtn/FWSe0uspj
GEoj4YRaEcDC5IET+AxJfXaxvoiJUBNaZ5s8vAhBrSZbS18OyEO75E0kFI9DKzJDij54t41jo9iM
V0+NPXhOzOErJAFUCyfA6PczN68vZOsEw43msIJqckJQY9OvYwYCOFoNBqOJ7T3FdoB1vORA8IPP
iqCIgpw8D7YIeWCgAnz2dPDG040KjmKmIAOlh5XkTRUR1cWqTq50XKNisnjbcjr5oYB7elOSdfzn
gEbixgrYis0NDPfJWBwQ47wzN91DsBTapEXqkbNJVfJvexojXkJ3hAQCoZ8cu40PPNANnlnt+Dvc
jwWPgIRbIXhPg999jcebCX1vBTPl2iTtZvq9e57hD20yp3krUAOcCI8jITadGB3KbSLkDg9R3wo5
wqD8bU6A9xD4n7HGaCvTRR7wncFDySxwwJaLvI4x+7DS4DxJJIkkAYZpY8oIco6SQyYQwdu7loDy
yEMUhsnK+Lc5kDBwChKlKc5ffrRqS5njBThM6QyZwTaqHa4qEUCBXptYzS9kuUICpQtOImJLISEU
NCSlUIuYOJBhIYEj3gpjQQAjFC0C7JJ0DCfrYeSBwkQYjgmCEMQ6yHqMzLKoGQBTREHfu3HK9Rdx
Ry8LhvJnMURDhfyBu6Zz0MDO789JVUfZykxBCBYMSO2z3A51rfsw9DD0gSEMhDRRkYHwpFWmxoZz
MZdMRPDdIwgfyi0EA2actl2VXMEDjxU0VSxs5qOUTmE6HsH6ooCkPPwDbNF4BsPWGkJyTvmTTEhE
+3U2plU+1T9gn8JyZImJooKEqqBAlJSSYJCJFoKpohhYikIkoappIiKFYSJqCoiJhqhBoZlEiQaA
RKBEpSZRSWEoSgCJKopn0sZMxMkJEDQFAVMiUJQpMRAKRCCgFKlNDE1USUkkU0NVRVCRJFSAUQFI
RDAFRCNFESUlVQBMkqVLTARCQRCLTSBUVJLArAQskHrIOUJDIEALBFQxDENKMISFKSUCzFEkgdko
jgUUyisQo0qoTMzBAiUB0dqHHEE19vSpvRvzIFX5GvfYMda/RY/kUS+l4ai4a3B4qs8uZFFWmGWn
7xim0jrlqjJDrgyErTaIwbPzL+H4Tz40LzTPJ6LPADwq4uZ7Zh0xkovryaeVyi/FgkENjuwYwRMN
LEMJSr5vt+ckhGTpro4444+BeR7Tw9ljTbGLZtr2AYVmJi9KePkkYGKIbS6+a2r3eWHMwEiBWm2F
6LYICIXip8QdRtOqF3N0jT0rw3WOuY21HPdbfISBRYT0TLATM6zMueUi4jguwjSbnPQ33DlEOdBH
M4gcyi2EvmGJo8LX8GSLN8IyJ2u663ofM4aRsQzQrC0tB0gRJrboyFmaCIiMTKwXhYzeRY7KQJCM
nhvq4444449gcgkelNHNk8RM+Wc/YEDv1WkPVHEE89VBESLvoyBsn5w5YS2SQKT+SYDfsHcTUHMe
IsAJcOGAGGmHhmQ9s5JKSbt2FEmQV8twNILAjFMyqjkjeS8tFWR8OnPUg52D3syewePgnaSfBOB5
2IOgHQCLokWhecgmOgwaQ17xgbMih4MNzZDQ0w4MDhDBi/apzoL1n8/QB6YyRKB88H8r6BoK94vt
gPpnCEVGigsNRX8bNCbPvOuCYVDP4SgKIRZhNdgDv++NM0jFAQIIQmXu5c58D3nePygCBoBqxDQH
QD4ozwwPkTQwYihCClmWCIGFUn5DMWiCUZJaph/GeU00TRlRwkEcAgUFmRJ8p6zVU6IA2pAlSUa/
2T+y8+Atexk4yaFCnVRt6Zcg5/2UE10sfpvh/UGz4qv+Rxv+qBe827toRTZFZRiNjijA4B0gHjfR
RIUIRCyQoUSQzIFTDMVStAsQkRB+Q+s9IwPtT3nvTi/E23Z5f1QFQJFEn3gxfhIhIKB7U4AFyPJ6
fXbkMAU5HrT8gQQ9Z0Hrq9XbQH7iRMENFZjk17Zsd1DMzASoqocwyBq/v4GAQjPHLJN0CO8aQ7xT
NCsxKWnQUcjuQNgmYn6ILhHBczDQI8CzkAbubtvwPdKgzreWUWlasXzppLrcOboFpINaNTa/sPAp
rBsIEyWTYp0pAlzUiJ2yGMTdGJZJxMDKIrRz+8frDmYcemD1BJHqSOuKczGyvm6XBKM/ghgSpb5h
oT5QDSRlOJ0UGTQ6FBAfA6XI1NvyY5AkD/qlum06tpLEMQdLv3jtUrmJZHcYxzJo8a2p9PnD0oKO
/nSJFOZ7sTQhqWWuYGELk7DuPDEPWp9CYO0LaNCMcUZOhICYdoJ736Ci4x50TeKW5J3nQhsQNJRS
AhDyD+QgImIKKUCgCdPjRDVEEwD2PMbHnbMNxAJS3K2exaIqi9waiFydVRqOOs0tIBAFW0VOEziL
GIBBzKBogjzGINXDAYbX1ngJjzhtIVTkhCkTyjfaCu2Iw6RA8h7n+MkaNCqsIhCZKGqRpaxjCKJS
gIwDIxxphI06WSyzeCp8ve8/yUWoPk5sfmr+qH9kbkcmZXL4uHbkXrWv5v5H3RJ5AB+0CIIGYj4o
iIE7n8g/mc8zBfGm4UkPEUPHZjhJqnGkmJIz8iOghoj51idyHhL4SvJH13n6Mxj9hgfqOeA6MHta
ZOh4aLK8RVUYv4/5fR5m2mKJqHsqI6cjRBh5bhOnGDhcJHZyfWOoY6xPa4PUtmqaaMY27XQoOiE0
U+ylMFgzkLK5HltSTGH952ClZqQMAkaJFpMrXRpJBkPiVJUo9Ma1jqzJ8rCJ16uR/LCIPmizl8av
Bwb5FVwu6Tv8VSLnQN8BJlQoZ3D5oHUq4oRZtMBRROxxQmCCEU8dkVRp7bcBTV/sfMaCd+tiQfdG
hEQm3KnF6q2vmw4HEA1EmzbyLhyRHXxUkiQ4H3jkbQOwG0FRfmzDSxnxkxPGD0xt+UCPw8acI7dF
ULsAYERf4WVUN3ACRkfUYcSTx6/46joPBoXdnZIZ+10+3oeDG4uDggOaPgMOuK0I0QHYiJ7Sgfg5
KaIwIIWFTRLWUM4y9Ja72/rwgZMRbvCL6SJ3Eelg/Pc/Dz0amptxUyWiUKYhMwCwTKZQggKyhm5m
HjFMtZ4X0OJkEdAqY0/uywmZMMIBgc9ku6LcPTM0P1J4AIJzgc6AuD7TU0Qe17A26CTXg/PEwUk6
wGKfAPcjqO0ecvIAYBmgG24Gu1CKUgGEaJ/Jg/tEfixw1/JKUwsJ3GhvA0OHP5+h6FCY9cGtWkVS
0dEhklHHB50wwLT4oN37jbtyAhi214LkaWyh+IiGMgGbzN6q2UivixY9rlMjrS1tETKpc6ObmITC
8vRvK9HcB3KLvZ4DvXYOqlEpA2QzOYaJrJkgkx6SCYw2dQ74cRxXjCJIHnHAlF0gOntZCDAR/YMd
EArQKsEoARAjEJ0EiZKqxIihQsQqPcKue+fGHZEE8FY4JQgUDkqGSgrgEgDkLEAU0Iesp3eEjhHJ
ED0JFHZAShWkKUoUJkUKmEKFEoighAoFKEezDTFTGJXbJm960GmYyDSBsKyy6aEjrrTplUThACFA
PIAwkR9WRgDQ1kgCecEEUbZcymPFAAP3nqLKkGCJr4hUBtiSM1tlVh4DNLGN4wIeJcLHkfkZWzH0
dYD3qxaMIBnXHgBs+s4VeUKaAsDnmax/T+3Pyfv4ejZn2nZxGlEQBA80ckaBMtx45QdURoVYEcTT
HBiFOJHUAF6a1pi1OcBW8AE0GGtRIMC8Q0jTCBL5mLM1u9ghzo30b4Xtq4KEaNKyhzyacUjSI9Ly
8IImqxlK8QGS6fVpoOn0w3Mj8LJgScmGbj/bLN+nmbHyjn9ovLzJ4lIyv1dDVWi8T4z6t18M2pEs
6Hc08KNa/PQ84aoyC/PbAiGme/YBzOd77zd08St2m1iAQDSlBKOYBdlij1PC1hU4zwlODbAfLRTs
bw8Tk79NcTuczXIR7ZsasSkiCTh1IgwoEvZbwUFoII1DmldOmkCe/nxZYjAhYatfjTVesHLjj0Tp
75KxybZ2b2ItMdWMjv2Z68tDawFc3bVJC6MpoSkXdapIU3EoJtLgkAbo04bQIah6zv061M/ozLVK
nN4w8egevWdxaDq+A+R8YC2IbErEyg6iEbLIoIDyEc2WNeaFn6NmdhWUW9XBCvN14g77ysjvhwsA
nPqUCFFUsghFopQIl7PtVicZvMo7Rtaww/u6M9mgjBfJ7T379wLfRSxROcCh2vr6+3S4DTfMUYPy
JGSWqiQ1AbbL8s+HdytsdMrHhcZ3EQIUnu69FbZIS+04GKn9Vn64dYZISwOYEce08keTEe0nOkXw
NC0TG/pGcIyGaBqDGEaLJgymhwqkbY8YOfL7bLjK04+Avu+TSoPitgZk6/FbxnOcfER5S01BdtBr
M8UpI7fHTeZGvUq5sXZpdT6IivhmMrUemRgORDZgVDJDzsIEFoYpVJA5ebea+bPW+/CamCS5WrDp
Vhg+sMrFm1EgP2kAtBU6kwR6EIOwWDhHEnRcxjUeKGihoAXBbiHAhwnwPMI4A8TUwGDoMTHgvBDQ
08YGZmSEH5RFRsa49QXlHj0o4A6FttZTTx0BnpCXiB2G94cHp4DBh+rBXr1DhhoxB4ZFwOxAlWKA
IHiUUFC6Yi6Aeph+hC4FFxHFNCuCxQxg3UOI1CLD2tCm99gdqeCkPXD00EPGUES0CmxWSn1Y+lS0
n1EZLbnMmrPBkNizu1QPwBkaFlck1p6QF+P5iKvYptdetRPlfskkn69vea72ayVaNve1awwnx84L
/cRKJvL8rGVmqfYafJfAJ9QAZRfabzsmw1zA03dsyISEIRRHBOk/wBgY+512Yf1HHpJOvHdlVIQk
2js6UL+xcJIp2mEnSWGOY5Hev3Io95W2biet2084JzZgW2GKrQKmKObV6opqfyH2oHuD+q9w+8PM
D5VvkFAPOyASgyTCAoTCIESRQVBQsjEJCHMh9Sz+5t7+0JinYTF1Tg+KpzSyuR60JZkOu5zNk8Jz
J7k5FItDGKg2O6fkw2axrYMdgdZvHn4dHPht4gk7coyalyMTLFkMJRYhyNPH1BzjoafrRHTI15aH
rIYyEdH4mjALh+HijTFCqgT0BiGRgsiQBAwCn9UaqB60ZXCQ2I4GqgDgi+iFd1lHwsNGyZpIiDP5
gzTeH17x5nVh+4cWIYQYb/HmyYG2tYgiM10KLYzTEzKUjRWmwbCsRWQc6wziPRCoag41qGaZBpBG
FhRbuaBhhDFFgiWRtLBpQaG1kRllm7EBk6WobSrijEOCjhEnNkVzIJukibEG2Fa6SJ6E+nlx8dWU
FOVExmGA75xDzuU4wORj519OKB0k0+JMxGDDhkBoGPmZPDML0T17Kuk69fTw7CNcvBC5wxTqBwVA
bKGpTGjbUjcDpLRttLYuGiM2t6zM0y7QNhpmVBBjdLCg22YjSvETaDWOBialg2Dbw06XguzqMdgj
HMIhkTXK9Bci1nDBcuuoZhdQ1ZnbYkKYAoMDkhaYdmlMw4D25bS4GNQYQqZp0PogZDhCK0zEWUwn
oSWsLZCLE9IarCsqZpjiMNZo1Y8U3yM86IJR6FJcEZxzN0OtOYYG5OmWoI0aaIDerTGViiTEYmla
HQDoOgmPRfJhAcel0McPJyJ1e0DCmZFuZjTBULJYabpgwXwZ0XoZAxNRAUtDIEFBUFXOk0BcU4Lx
8ko+mtAYGWVBAOEIQEKyGKYySrBgYwmGCEidOwBe1Nwgb7wPHCknD01KSuuduKO8zOyhrRpBUtCd
zD5hmAjeqhtDMxMQGLPieg4g8w7tBR7YApCCJLiHPBQ60MVJlEpHJQsIMFxwMVycKkNUe+GobIg5
KFZUAxKZCYQLwhV0qEpoyTJiTLCTSdpkA2BoGhkZ1YMIV0KlNdE0NcF/Dnt7v3S0A/BUPgSjP3w/
YqedJIohsDX1fi5H9ZlTNBgb4Kj0AH0rBLKQwAQCkimaNQPlDmAA7H1IRUsSEgQh3SGAMMBAbTwP
3Y9iJpwiLrxJjYGuBoGed8QKeLeZJjMpgQRzIICWEfuhINAObuUeHenjI+MgVAGqoCogO1AX8wAw
pzBsAOoJD170wClmAYpI67tbCVhAKyEQiNtg0KxhkHggxP05jJ0C4A7zmivAhCGGCEgjjR5gkENw
GoehOIeLWIVkmJUpaGhIoEkbA+JDU6g6ooS0KOmvMQcHs+DlAtEkdq6CaiRB8MK9DAeQTyIzm7Zg
bFOwOgUfUpTASFsNQ5Bb1fAQUkAIvUFwAMkUUPyxMkMZDGLW40Dxw6ZzUUj54fJ0AFxyu9zQ/p1r
aODhs7mFP6r7/43/CaeEpEpgKC4FqrlwgDwQOqYgiSJQ26BBeUTI4qtI1PNV4UWqC2j5RPs2AUY4
5FalgdKdZlPhL0/iVDQy2h0FIYkvDqSVJDFSg0BJK+wrHuD3zSOAh4/NEPaAVgMT4OcPRXmPKeu3
rZkFiGANyh7kEwS8BnU7BgZ9IjojdH9sPc6KfpxVYBAzi58c3UiYtBUkbV4TWHU0/WFR7f8XvBC6
19o/WdaimhPaQVkJL4jMRKGJijZMr8pmRNQTEsJRebAK4hxcKAyE9PJgcug0sVDpDxWWbDSlLENA
aAckRTU3h1rsHMDMxKBcwEILoQ0GXQEkwQ34Qqe4mJEQ4KJ5ZU4dBglArBXc5COExIDgYn8DoDon
lk9MCQwMmJEw1w7SEMAjIAgNs4gnIEj8Fed5Hl4Pnk6Zd4ToZRUHlCK+jJW75lXL0RpNB+QptmhY
lMZ7Evf+18FUfV8HsoVp9lA/42o5MLRKxJUhcilSP2szMX+PBazT08shrLYohSGH5t0y6U5cM3qV
4RvIOKWwIVM1anYGWwcoEHGNDOWa/1mauo5GCfHDVArHkaeDUakIjUg8JFBg8hUNyDiOD/X517u2
IgSJSZEOppTYu8ObuulWBgY543NTqzJx2OsMqlozKMWSJFdartI6z/jShRpJttjyQOkjyfFoG221
NEFQdXttW7WhdO7zvp6mluJNbjZHbFG5I1Wuxg4SOZE+u1gccSbxs4x8CiWTm1hpo/z3TDJkUk4I
03C86r0lmUjHkHrUo2aihQcIgOoE68c0e7sgoCg9pyKU8Z6Z3cTkqczAKQDIFMl2XIUacJebYHeB
cOciSkA5B1ImwG5RWCNuRG3RjOSTbHCJi12tBjBvnk1vQtCZU2KhoXBrtsRBm8kasY2Q25mxvVNW
3ruselpmmnXjVYqw2PA7GrTe8MUlqW3rNJzWPIxtotcGZBRixps3N5O45wKQw9ArQ3kY2ScOBYwr
GWljhWm92ax8XWtAMXZRoIbfOy1VlhNGDQRiciCRrly7GxjZUkXdJTBh2hECph1bkzhMUwzK9XZJ
WY2DSVoXIA5td6Am+3OrlwgmRsgm1GNj23ApcYTHlJgZbjDlFnEu4x88bx8VhaamMxYVYswQZcMO
Hpucbvvm4ceoiXvcgbhiMbZsTIxmNzJrWKiuRaeXe8eozW3WOpDx2QuzrpOEF11OpISKU5G5hRcw
5HM9NdjqHZ5LDDlEIVSjjefXrnmrpwvaPMXMCyAy6tszAirdnAkPonDJFkU2ZCleskZ1441j46MV
aENR4uS7UERHWAeZ5uUPI2yTLLJvBhvW3vNsidMb3aPNVujaJJCkhzqgUxEG0mJrmTIZvejMyBUo
4uUQRTmVqq4qNg9DIm02MMsG+MDLGmyRQbGmmYQiCTHfhxHzmt6DswDnrDM3UwCFTzAuQFHVk+bJ
E7vSJKtFmNiBGQbQaKiZjk0jmwYtN8zqyecciqWzdc2sYqqko2MvW4affe9OBy0Qeolukb08YF9z
V6l/tkhQcaP4v7pv3Kng6eXFGdIxCJYmBI+KvQbPfj/x/G1DDsMMoZlQCwvyoNBcgPmFGtfMr+A0
H0Q9h5ERiMQK8kTYp6kQwKaBcnRHSwY8jcsleDbGgfv+htUE1IsUQtAvv9e+/hi5NQ0Rf6XCxkvy
0YDI9XxqQjfNT6lGrbwuQhAtBjm+9ckBL+4AEKIA5gr9EGBmUUDor8p9CRERUBRIywHWQDuJSXZc
7PGB6UwbZYnLdm0v5igJG0kH744ooRAUghM+g+LZ4jz4gUFmKaFHJE4c4BodAfASb0SFABfoA/yQ
uCAtAS/Gvq2P9uDQncLm/u5lPgzohIIJeokm42mmYZGYsSyGjgk5kIicxXIQoxXQNtGWT70lNKYS
lqCaViAoRJlBKFWhChUkgipYqFYAhQoCARIAAg63VT7IPqAETnEoQ4dx3EOJW1VE4kdAUNxuVKgG
8CO0bCDoIGutC0iRCGwh2naApxAop6aIWlK9UmTSKKQoSFKBQAIR6T9yROB4+ZAR4z3RbkZc2pmA
3J0HBDcah6h1zCqqUKMYCEUgvWwTf52Kf3Z8jNKKRd3L+3Kt94Dau+hvWGBt43lWgmEjLKDJmIQC
0KXCIQgSDVgM0LlnOw47zTdifQU+vVisPBmPxqHS6sN0GmZstjpqswgInEJMPRcgvcp134uwDS9B
ZE2eekFZEENpBTTG9451hWzQNtqaQmhgfUAQFFDjoiEE1Lm5ykNMbY4QWumcrVARrFdRYHGm6qDE
dBdJxE53gdA5OXo5BkXSc3USrHoYxGJkM6GD5io+D9tGcKXw7KrfRNNZC4ua4OhKdyrBAEAFgbtc
LDgs4aLD2u7R6ba4dOG0giuhy53oUzkyUkZ/L0/y5aSPk7qkkkWuVKRiLlv7IfluuTgGZIZp0jnU
9ddCGvasIZmC7JKAQSI7sCSykgkhGxv2Nr6gB66ANDU+Kjku3Sa7t9tY7zBj9uuQsJwop2C/6Afl
7DLeAH3pHJBMw8RBHqUEcvGDds3OjRFBiudmU8JzTLRiPdqbr69CJeuKDIgR/PPEGLZAOCIDuYXH
yhpAKHIIQNSmgwC2IJXOOlxPIODCftyTPIumUA3DFLiwHCRDAYGLSsPG0JgMINyzQN+IfBt3FAUQ
j24YkHwiloNIxgfyfvGuvZrpObtPzXi+cTg8yimqP89cB077HVZqQ9SwTA1aIJ3sCXC5LP+wPGGv
8TwbF4GNveio5k87rzBrnlcLJHgxuhyZ4G2SDGD8KAEPLfPlRNw+JULBQblIOAo++1F13C4QDn8n
PcS4pmYy48wrmTBIEARQfN1Rn4iNlhGEjRRzTDSFOiKqjj/S4ZaeiA4cSfUnaip0IHSbU5zz4LuB
9QTxHd2Q00kSMiSSMBJ9vq7ebQxiyGrMQkJwiyABsA1TcRpTtUOIh2gqDcfXB977nw5U1gyKD6TS
QTahmmQRBkCZCOQGUSUXwxNeWcnHLB5DQRDu4uEd9vjToknoiSZUwDkr9cc40677jfWASpp+P8o8
D4JUNChRSHvM4eL79Dx6+rNMwm0SpEoDcKBYiYSlmC9sUalvIUN6mpcMMpXQEvDxQEzxCpUNCgli
j4yeTTclkNYjIHBc2k83XT5YQRUNop7bbw6zSIp5oIngspnqLwZmiSKomICpiJqhpKIgiIyM/Fgc
NVwZZkIogFeSqOEgkwBVIBkAUYEiQQJmYxEMBQo1RBA5mREQQSFJFkjhFE5xtjcKSJkg2c3TdSKo
JJG2aMJYKKSlIqKiiijcHCE2MISJCnLYNigoUlgoRKQoiAIlKWmBkgYKCYBpNtjYVqGgimJSWaiK
hgiQiaWiKamghiohoc03lxJCliaApapiqpkIaESgopoGlprbJKGCEggClLMVMkKgsgwqAKGYYVKi
I5lhiBYBihhiYzHE0IsaW4Y2GBlwwBM4BDjxxMEpZIByGAxYXWZTMHENTFNXTSw2IpTBsUZacvTT
wC4OEI/c8D0nQIUvIahgOwNES5CZ+RzAyVDqGCWgPlg4vMGItsBTb35iGqMsEqEm0F41IOygJEhC
E+YISCGDh7Ce80Xu/jKjeHYeQ/JDlTWTiUSnXrykTrnhzZsO6M09WigcKJJFOxFkQ7+zw2Hkz5om
g1t5FO9Id4E5ILnQgRBL8Vbo4KUOddUDqYH0xJF9Rmp6RQWoDgG8igg+BgTEALwfBjPsYNddYwLT
g/bToyNDgAEYBZ8KIPrHwD8OXBCNqBu0aUhYBxEkAlcCxwAXpvFrrKYhIxkBiJtq5lkGceto1Mc0
5ZVmFUTIpaM0NhZvc52z+wrpFwdagWCIGsADjJ3vyIQJAdYf6PG98B/hCB0iJ75I3hx92G42BxYo
nQqEnJ8uKPmOeSiojkQDUt8vCxZ2USDwYFnJETaJ6ISEO1xgJcBAvfaTn5+mKIH1e3ssg9NuFd+J
2THvvlpvZ+UO+dLmAYvRsCSBMn8ZHheuUqEhPxHMDupXsg4PYCpzz/6GesvvRVVJyqi9TB8h4D8f
w4QVJCduHufNB8SURRFU1VVRVVRVVTVFVSFFFVVFFURVVRFETRVMQFRNPXON6OhrX8Q/wGQpKaCM
rhR8IhgsVAmYDHBFuwcJiMT4fPshCNw0MjKzwPRSHFgmkHr87BRYbAijvEQ3lKG8IzAySWKiGJj1
kDYjnj7+BZjZgdslirgIzXgbWHTNSK6jHEdP17Sc/N2ip5kVA6R0j1PYj1EX4AsNEsBNEsRDJFJB
Es/EZ67u5NS0ih8RiHnPrw5BdyQAbGaBQBeKnmgh66oEdUH8vusAW8D4pJaV0/LKtCrWBLhB7Ghu
Hj717UQT+DpH81HQjDAAwdAhu4YOjmJT8IElTtNE3mqmrA8JDBhwjIMsVhGZLMDLFiWM5cRAxIBj
Rs37PEfv9SSQYNDkjGnG4o7KNzMwM2+4c/fjkccWWM4Rpoo3QahGEZhkZph5AXH1MKGzHbROBh5S
PJGdIg4YRsavhN42+x9ZH1PhP2kkEBB4z8HoM/X7zOUI4GXd1aD06AxntlyPkqMIQi+rGk6oXhOS
EQMiBiPftPILn2B5E3H3F2QZLkG5U+RTyeaAZBaCHzUVZlOZlhDzSGEisXFAhkFACJsp+fcpQ9kh
FDkoKbLkYQtIdwKuwP65QE4SpyROQA+A3mrkIYyRClK0BQK0KGuIfEstiIETlTMoVNBE7oKNEQP0
A8oYbQ1oAVrC7ImkTAo800g+8+0JjS0M2H04vDWaWQJKiRJUZ1kFKdXGXRE+4cFQsDtPLYlA0wHp
KLkGRoqQ8DXaB1MXD46avImTY4ComEYoA5pQRAY8q9c4d/NA20gMwijaezwCIWtEOlOlOOO11w1B
a1kVahI0DBDPzIPnvPHO3giYyeLJwujA2MKBBjhbHXBhSgRDdKRDyJtJajBVKlDrguG1FJhw5c4H
nrOFZOXoYPXORRtUxhKnLseurdOuSacstJGm2yYYjEyoH4qpR6097oZ1Zqi4dJzDcyTSTEkMsmP0
+cHYGk2kg1IBDMw3oGfyy8REImKJAOTAxpeajjCUsBMssU0TI2IRpog00hRIK82iOJbo5Zasuhim
POsCgSO1lkaRkkMHhwcGiWqYJJphK2Di1oROEzBqwYEWwpmBfHemVFjcAoZJYMFBMBpFLoC4NYVZ
56zNGsCDgJIZE2mdXWBY4x6cBnGXcNF0mSAZCQ+YcA1C60ymk6nGkk3AmpoZQYxoGPTI0Dr0cTSD
WIKFIQIgQ29cDe8NsmtORtW7mNdXSFpzK8NnhswZwkAbZEijIBGkgDKKIOuGg7WQcd5plQkgEgQG
2BubpjZmJVBBjmBbhgsTKsiSjCMOen+7xqybmxgurxNHlAINgukgT4RKPicaxL/HESin8l3H2rYi
5BxM9uQAxtaajFWlCAM4KclmdaloZchSue3g0b1ynLLzd3OZkXLEHhuRGZDG3xDDRhRjIqe3MxRk
OSntpnXroFRnl2ht50pWw6yCneC060YS0kinl36nPSzoM8RFkJ7MCdE9JUh0FgdOOJi1ZoFz14qY
DRoyaY1WzgETGYSWl0i+/56h/A+CtJej74a28sxIYrEqGOKJsPxX3e2d/vQfXxwI8K43WAOMS18d
10I2xJ4qpIpghCfIkHApigJWFVa0DkJlAVEI6JgYo4YGLQ2xHR9+9Ped6Zisek6aHdpIkipSMHkC
VIkZ/BOLAMaIlIII/TN2Iz+3PlUKHcIiUID5/ud6ggUqrR7uHJrB8vPNkHtAwecCt3CnTAZ3ijOH
jR8JH7NL6FgRckOHXHptHg16bLZK+tsk/1vEDtRh2ymuooGTGanH6V9GeCM4lyFjdQ0syzEyVOat
kMxVVEsrVDrEu4YbEIRHwomcQ761UD055p5PqjfOuXDyRxGkzpqDAgD+9/QK54V2OJ0zoWX209l0
v5XMTEdrUjVNq4fSzBCtPvxvoXlc7iVqFQpOakNibrkK2GnuOttYbzB6uq3areDMjIsLchDAExUe
Tl+BuymDtDea6E0g20mRkJMIEy6oSJqCqCJCnJyopDs6zft3x2YQbhowUuJeBx5ez0bTjrUXfQwD
75f3KcOcLoASPpGXeKXTo/TjPlnUHEbFLuowJMZHUvStEBJrDWcN5HO/PPrPAm5vDCPLoeFooBzz
EMOmQgq9TJgwOcl2lDBQmt3yhoZAHkbiqMREIgu2WgmdGg4GBGzW62Sd8DZCwMPxYrKXGDDbvjRI
EDlO3fh5D/1FSQiPEAeHiBaYNPk1FDe09ZkuyISSStVruxquNJtcHWcsvHtpzz8eu1ny7KPQQwoO
zc56171D316NRA7APjKlSxMjMV+yxjp15MyGpDgIudwbQbaRB9fCJVcdUF0z8TN0O/f2RpdEJCTa
KbkGGOnIU4gfcjMHeqSLhY9BzFKWQ+BDrAky19jih5VXyvqS7ldNhnLYdjVucerRAkM7ynEWxDpO
HbtM3D7mV6YDXijGXxlJDYoEzA7pOzNLSjCXkarrdGJD6Hykdl8VxL8ju/fLM2N4LDs8zmhN6vSh
VgOFDWyHYEmbRhxseE0e6+mAwFEtuJQbER+Pv3h4eEe7ua2tyDSYuIQ6NE6CIJDALGtoUEH70JpA
4SUvewMYezvBq7DgYMZhsgDClGMhGkcIxGT+LyHDQDu6oIeEK+umUpHnEOHeC2WZjJjmOPV+JS+8
fbmQ9DjjBsYrthXNonVHcXxS1LBlTilWFbpVqwl2pNJItceK4uU3BAVuEPZzT5dCifLMsY22nxEB
Kaeu3ClopWckkoyW7aU26dFxc3DQ9O+E4ax864NTKNPiCo1tx7sV0WkHMldY1hDTMaCsbkwrjkbs
hINv5C4PrDK9V8OpR+GgebpDUEoDjINjQztuKpmM63pQ02ukN3rdtCp8ecFjE8aAjYxcMOPFCEVp
B2LWRe6Os344g55320ykutljY6TzgX3Ya/EjuIVE0JtNJZ+malhuGgO9DroYWD2lJhGYD4ksj0iz
AEBCQd1VEgUBQp5+RdUM7oFDgQKbAOPPDYWupkA+gpcCEhg8A4gTwNRxSnQDHAZ0E+0DHQWffyG0
TRPOJG5xkdpTE6RwsnZTEJQjolxImCqWlISRZECMDZsPgMGB5/Sdholjm6szcUd2ChIhZgf6FPEp
DpI+CfY61t/EQxww3VNSclLganxjFU8RZlUCOGAP6UkdQCRzCTmuEfgqiqw4kENpgpLClyqKKwUw
GQIeLgBhpiLGAi4O9JBdHRMEDrXFw9RLiFKBPIxGUlJHJRQwgHtSQxQgDZAwAhiQjITttQbSdbkI
Rjk+VHt7GkBmhxIkILd5MdFQ+DEIkpH+jBXISlaRaBGJYliSGFKEapAlgKCiIGgpYJUpmEKIhWAJ
EoGCFSkillSoCCgoIloUN7FO3Kb8oS5hisThH7f6b+S8JukO6X8dlfNptx1VUWKnFJEqP6gxmzDP
p9P1QhszDPE65s9AXCDIMT7bWNZ9jLRSmP725JJIUYY0hl/kMwbNZfDDJCb2VVPci2xGjRDhi0G9
1BUNI2wmZUtNA2Ead2wgxhJJ3eyTJTNUkkkklSRSHRfdIMwG2RtB/qTPtnycDvimPXyiqSSS1HGO
HZEO5BicYxHdkzXkd0umponC6morMQ5BQu5AHcEDQmZwOSAxhJLlspN5fpLjxPU6HjgOn27XguM5
TyfTbohlU1I+zfmoLAYYaO0DTrNDrDJqptsM3vMcxlGwtf7pjdzcOvY3NDIalOmMXtxwcgxrHAbh
lgmNsTpAJbSMppuDWChqA2EZcbcEhn6szDSt1jGM3lZ/AMbwqZbRiZowjIWBE6pL+4l2nA+pjPTl
tECPRpmByFMypaSE6QT3r2o0uyHFUjgk+GnUaVGzUflA+SrnmYZm3QkcYHOOc3apr8hrSyLbIjs6
PgNTO/J1WouaRL2cStXr09ySk4o+xex3YcZ0wD544SORAefbDbQcKj4k22DqeiehiBWFlQ8Mfm6f
pV19oR3neM3hcOBZqOglHYelEoXUO673UdwO5D2m0IfoRbp6sDJnGSnA5yVDf1P6VU9u4+DgWBgX
bjzUwq1zKAg8AYkEUoFKnQfwh2pqEdCYmGB2HQo/skYhdFjGSkiRPJoHABFTrAe5EVeSCpjCFAoh
QFIIHJByRWhUR2EBeoUDeQAxhi3DEzZcd3DQdg4sZjwZGIZMkItNDWKYQg4XOuIdQiPU0qBSpsIp
vDjGgRZKZCn+KEDA3GgTIVOrIeuHeg5U8syNxTdehYeGmJrXM0NMhmS3F0nSYLc3RzWIkIIsBJQN
gp3pMMq2BApQOI9QdEgWGGqTTCsoa0kYsiGMWmkWRVpINNMiTKwwDCBBY4mIdMHSRwVSJDQxYJZp
f3LwYThmrP6Jd6MfAmhAYGjs1UHqDIGHaMZKnEdfWRpqlopKECgopGkQkCBoUIoZCISKlSJamBBw
LIIiQpKZplgmohkWEGEgmSWpCWCChwrtzrFddIdgFegCASVNVXOaWRcxcMwHDiRmJQ1V9oYmRBDB
ElEpUMlNJExSkBRQVMEpBBUKzSpExVKlAEwESkEhQC0qgREKEKdJ0GHT5VAHE4Se8rTGgAzhYGGE
MuqStA63RXaSjM1LkAessfifcfAwSHhmol4GgQAX2MAeLs2aQlRAlAQQpp9vjmzDM/dOCBBAUISR
+6MDMcz23NcoU+QJAmgiQJE439tPphcwBI2tKbMI8y3UzHwj6oB2ww4eaFOGbcyIP0J+O7EH5raA
XAd9ibuTWS9JrqlKiUCBvSERDWdDeFxyMElw/LOfnT5skkLVXpm5ZZ+qNE1rGcGu4zkvnuEYEI1O
zbI0or+WJGZO0FXyyNDaQ2g4EPuaiv4ojYY0dafz9oGG9gnxnRKABeixAPageI0FdkilKJBCP3QC
dABQhKCFJEqxALDKghgQiQQIImKo0oQijigIYpgmW87jDQIlCY9iZDiVUEEORjURBIWPnIQeSogX
HIROcFirwk4gyloLfFbqweOrDZr90G74e9+f+g11BAyiO1gMJBhJpDYLVKlQAtF+yAgbImedrCY1
rN0YIy63xgYLT3HqxWxtFaVgJsbJBEG+zLkU3RjFUVtyI1ZkwbocFHp4/3J4c9/fhxPS8/0aoUZA
bbgvKYU2UyAyMkKSkcnIiioiSkaRpKIqoKUIozEwJTs+GIqNJHVQwZWb3OMiu2IZhphF2NccCOKR
BExI8GtvjZrmhDUeMl0JaaOGkrLISpJoq6P+aeh3V15za89GG6+DRURDaHWBz32zpq8NkHmGYsdc
sCJh1ju83vcX20UPkzSBsDSvUj98DxSpEpXreph8yD8ziGmmkhs85ufu7UX+TqYPzrntrXtRGTmY
U+8/ohuy7NAfZBQIxAJ8EYeSRUuGAARIpQMxSOOEGIQLlQZ5njRIhiYKpCJqX0B6jCEqJfP852aB
6czY6ITCYgwwMwhMjsTf8Si32oJfpOPgn9yaZiGoQqIkBJSBHUikglCp4RBJMcDFTsohSYiKQ0gy
QMICkbqaJlMLHEfVB026UV/tkSBQkICTyV4FwPcgEHADyI9g4Gh9H5IfaZ+wddpjXvve5VUez7s0
xALpj44x1NBD8kTLchwRTQ9MNUsee227ts9+jmWEe2LIDcgKtRchoUCJXMxQPH+/VW/eP3E1eJP3
se4lsmjJOJXMg+SjcECEuwA+gz0hmlzwI1QfvAJ8Unei0h2CInmSUhiJIhGBBle0NTympgdIHq+u
ujGiMzrXFKSnmZSkpZWYJHHHMmCpKKIokEhWIgs5uZKc1IKC+uaZ0swLtqBU9tIYyNOQRabHGAUM
nWcc4amVuZhmzSUVaWNARLRFtlFLDFNJVUKRNBTmGUVmGSqVFFbORSUFFOi4BhCQYGYEchdk0qoI
jFeac48glKpgHICjCBDAgsMUMhiMFJYgEyyJCTJGlIJYk5GFollYv/AGROujowYnuHCQ7gyudLEC
TDgOBguMEpCKGBiYA1RQSkgpASKHDoNFaVDioaCPtAYg9PpgK3r2YD6nkUHegJCQB7ASScDV083F
E08hgsLyShIhpCJiUiAYogdaD1IEDqGqqh6zwnZ0T1TCMR9Xrug3WapqSQbitYMJuI4wRZBEOwaa
gGEWMZJkDyMZDjhnKIV5uLshZhEvNMs3QxJevTpLRyEkikwTMEGEFJkUQOSB1EaOUIywGSOISBFI
kWOODXCixFCoIZgiMcylglAiaGHIMSqBVYCZDWRoSE6xLDNgBjBytuhHSQsJE5VGhxCaNZiVVIIF
WIFKQU5xzjxxmaWHcJlieTG5CCxorTo4ECADEijoLiQLAbErELCQsrDVKMMM4KkpKVFksRETKRJV
MOFA6S5FFFKAB+qTAJXolRAwElQWGVA4y9KfzqyhiMBxCQ4KC6YCpDx4Yw6igIZkKHhwxYJQ/wYu
gSkUWi2QjEyhUhFAUJEwJQSSIISshNthMsSwJJAEMISpCRvFTTakCihaWilKFImgKCYEGxR+5JHJ
MIrWZGtCCZpmJ5QwXUmoo0AQBEA0pQSErggDj4IdYpUyO4aKckclTAgSLIoKUioQ3CQAY+x+HlOb
8Ge3Tz7cCOj3m0sEyRNi/oGRbdqP5zjFZA2byObwwZQjbW1rGGMJ0P/L9U+7smTIIaXCZBwHHCAC
ckBQMgEBQbF9n5AH3RHqAzudDTl37Ds5YmGBi2fdY8kkXioBpBGj7uw3HWXC0mz8txHt+Ye5F/Xq
Du9tm4eAsgkHyKPSGQCdw9cHTgcdiago1WDBxiIf8tFow6H2IAp8fmP71XJ59fOjzase+Nbcue2U
zr21UzxtvuMF5V7TxKfgxRGBFQdgP2wBv8eAPcqby3TftPwYPSY1FMFA7tBpBqJcPj0O8bkj9veK
/+ymomM45dQOjgvvaKVuSQCsL16Wf6IDOv79F2Tzggf7PgLnMR+ftlQHfiJYMjFIRho1TrAvHLQ4
nK6+6BwiBgTLo2ccgVMt+VKWhvIDUFLJFqBpikMurjYD+3jXr7YLyDx6YqS9RY0Zh6ZROiUmVAhB
GiVCRMiA9GKRK8R393RVHPWy9+gjteCBhjia4oG0QsRKt2tnOODFSxSklBDBEgU0TamjEk3+I0hd
+L99r92zbeTXfcYVNbuhNSy8n16ai8q8uXaOU2Q8RWp05CQlGtzKUmXdv93hopap256Ns95EjJFG
V4hMF53feOT3mNmHry6OOdVESncU0oSLVIo8Rx1KeWFPyEkR+ZoI50YqqkToNkU6QmInHqoM7Hl0
/QeZmCtyEG2251KUz0cb1D5ePY7+/NAxlh2tL0vOH6HXPb9i4zQ9qpMIGyxWGHVN0qNZ8JkeBjss
3hbrrkXeO85xHMlHh2s5Nw1ISCwD/N5cFq28k7a5+iA48m8+0z2nTx9eutKcUppM9ZOIM/dJ1hOH
uq0g+HXkEdxium1CG5Kum3WzYgyECERWIHeP+dZHzp2zjFYi8gTcpwvMXVx333wdecdE2qqGypQ4
sk3YUUUPKB8Xzz8hUuGbZPm5qI0lH1yVgmwddvo60Y9tV+xrdmmK5BRnD3Vt1HDSrVGGM4axjYY4
02BwNUrOr2T2GTPLV/h2f59Y0FoB7WPsQh7J/gf1C5oGwbhAo+pnUlWE/n/3H2JPUJV9Q6A7iJ4B
iMiqKgoIegL8eLbu3crx3NJRebR3Md5VQcm5w3Fl2RWRurn6vhaBMm7EuMWkGJ6l0VQ7AVIH+bBD
I/ClfhQn5TJoQWk84wUhSAf2kdXVdA38j+LzQ6mw7DMoUIhOT+OsaP6v5OKDY2gDZZhSViINEEFF
LhWmm+2DtHpPsNJOhBfLzdKdko0IQSACAxKlAqVREtLEQwRBTREyUxSas6d9CBmmMCCwCdaAfQQQ
O9ADIkIi/cJp6B/R97uRZ4yFLYkNiSw3M/PqkGgh5DhHAE04hJIrCBxYE5Cp1omCvaAH4yBkkEEk
gEoREwBhz+jCFxwcT2x2U0xwJMwNzG3KV/3xDkFDQTFSTEUKfpMELqcLo4w65QRrgmOEGBBobmhk
qOJ08oR0QE35YZKmpC6wOrzHmDQXbRMFERFE8HDhW+5cOBkgvtWRJAJCKuDzCOwR2AdYdD2hgNFJ
RM1LLBEUEERTKQJJDC+rPwflHnQ1Ps7KTb/F+vH1gFzQ/XNvpfUwPhKobr0Ih40TzEHpgLUJiZjM
ARCBDKI+cIV6k8Uq0vUGRSFlDMTrQ/0P2hAj5AppGgCQF2fBP9tE/KG4kfqE81RLNETFUVQUCwoQ
lBTSNRKQnqP0AQMCHF8Svt+OUpaB5b71PgIIc6jkAfQNgP6SDIxxcWIZhlTAJckgIANCMIjmOaw+
GD2fG9Rc3L8ti0B0HzGx0MI0/ZVVVVoP+L/X0/39gpuN2evk+QdjW8CikIerDT0FyHIKTpAXqaD3
iXDpU7kkShHgHyHAPvQhLJ4fU4qj8bAaGhAQESIUq0I0CUBSRJMKURCkhIFI0K0JQMBQ/eBGsyBL
8VXHBOQIxACHsg71EJUBE0poYlIuL5mxENEAwxQIHQYpwWDE+uEIJShoSJAIkQaCYJYCwFv15Eqs
qf94/T5zcfiL9IOxQ0BCG2gHfDEq0jECRTKHqRju8AfojhGsocU90qLJKs/czjNMEPZbWHlkNPn9
9hZEEOSMDWLuSK6VUAio7FkphgJWAGqP7CAyaQNIyGl9IEyFWYAxIy0JDbWVXCWGBpQMGQggULcV
NvDAeYlMApIHmIcV+LEOKKEDWULssHSEcJT2BO3ETlScvMB5mn5pxLnlVIHUgWloiRCkKAAD04uC
KmgSgqGSzUCYQD3C8PXB6lA9riP90kTwkIbMAQZZESMnds5AZB7o4hinifB55rBlSLvIAUhBA7jM
sPOXEVPx78/7ILXkKIINcsunExdj8kudDBK0FIkHogPFQSOVx7pRD4FA00FZrZSiMGxjczKzNAkD
133gg7TNQTQyVMZ+Q4YI7OECL9hIL7EaMjzfhf3tr6SV6+RY9kNDZtKKqmpUpgOcTaQQOCqZeQpT
rL0hnCQsQEp+sT5C2RFTnD6Or/WHH/g4HLZE/aH+mJw0qHwNbn1LQYBNCaQWyxUyyGOnvrfJ33EM
RyMaJX5fEwBy3bLHL2mAp8SkAHMyKxIhRBEpIhF1JCYwDgg8cCgSmgvOiCWDiPFRSKhPqgnTAyTG
gUwgcCJhGgqkmChoWIRJgIZKUpH1nAtf76V37jXUxdsJQoFIQQih1SigYyh1IgYyEGcExqkw3Cp7
VmZmlSmSFIIaigJJGEpagqT1ReCHUCpBtkaBKEaBJVCBGlQWkFGiilUVkIJPUfWgiYU64PGCIIhG
QgWGSZpCWIhqgApSlpA9UFD00dQHnUeouJh5xPlIgcH/cGZiXAPsPEI+NAgbNQKJgDlNXjEKCAIk
BPBePAEkrBAHQICfyNlFNBbQxgBAoayv1gp9hEEEIHEX0TyyU0n8XeNEa4uWTMfCQ95HWHqeknqN
h2TqR0KbIxIrIyIJyDNwdhpf5eYm1mROJUxwHAiRbWIUsVD4D1+wpu2zAKFDPwGQ+fYbMhTFSQFJ
FQfwcj9Vmhm6+yRJefARTzYMHw5BsQMtJJYCPttqfr/xvPynffg8cdYGiiOsyaM4GIkB7e8GeFiW
AlwVj9Gp6QknUGqM/zyJjDyYW8ns82GCaCCKoL9nw0wzA2iob+VJTKZdz1V/4EdKwQ228HStMTTU
muuYPFMDsF2ZgFB/fkIgAz0wM4YpEDSKcgKUcX1JyW0mJMaSgZlkIghCSBiYhGCQfPWCQdB6GnhG
xpMbS767LxhodY2mUi6+kzJnAFXlyFRyNOGV5CHd1r6n4MvF4jq90TYTy3LChhIv8ECYEEkkSAUn
FN/N50zx4NzhYPUHgfEv5CTt/RyJ1/mlBTEY5mSERHc4HUYlNAycJwAOYxkgRNAH8sIkPoxYhlck
BtJfklpIPtwwCSI1xwIsfTA0YJlC75rqUVBASQPt+E0NGYeA+n8GJ4vaE2uQhIBwhm972to2zxRA
Y+AkSTFuYZEjSSmmI4Rx5ur+lLAmOgwyKAhKAgB4da8ApMhTv6rrGImFB1iW1oCRSWowqroQRxwq
5i8kJ1uIByoBz4BiJkhebAeQ7uLAVVAFQ4momlCmYREwxRxWcmCxmHXmppACQiBAOkMHgjzfjPIV
DtyNOBFRCTI7T4G0+A7YRoBOROASR7zuAKoEooIIGlpGkaqkpIhiWqGmhAoiRqkppmKVpppIhKAK
oKHdzdLxzgabm0GRl6ACYIa2mAMkgM6VQFoPQEWoJZ8idA8nbbB9b1qHagn40EhOkABDimQUNp96
iCIMAATtYASIklmADnE6SA5/APDDGgyxyqorBnC8Hr9AxicpGlpAVQYQYQ4NEfq0zWXJYxxmGqkg
mEiCIIgoJgaE3cgIJdG2M0crMwpywWKQMA4WhqQBAmgSDhQmAeRxdWBDjLSTLBAkwiFRRUgez7IU
FUkwUqrwTuhLqHFH6jA0B07/JzA9n/b2VWy0DcMQsJxAIgOF8wR7igpY6hP7/3wZ9g2fnyZDIMbU
eT/ZzYFkUD1G+Iw5HYgHL0H7QZdGgIZH2nl8fXpID5SU4EMBBIHjBgKAZZKKEomUkJX8r6CnRocf
HcH6CSI4gkkoShOBIH3OLuE6qivogOpndchgd6ARKGCpqZCAJ9ua0bOFKon6JRCZXJUJKKNxxIl/
mLJT8yGDjJMcIfGqIctntXMVjgr75iGgaF3DvKDbhixFBRLFBBNfBOy4kgnEW+D5EU/P7u48lT8U
VRTVNU1R8iISca+thA3iEj3ByIgGMgminUd3eiHJIUhTRVDQFTUlVQdSCfi/0VFMyRC4Oou83cT3
xoAaPvgIL2RSHznIPx/JIR8Rm+DSyXMQTKkoIJUGPkGcgutPKKeJaElkCimKoZKQ1AFOV4ByxQHM
eW9IRbnB5plXMMT0Cf4k/vAUkoUvgN6qEriZvhPHBWpEDocyTuHbbQ+xXvqgkE+rmlafUmeD6dhS
RESjMtKNDQe5X2ESrERhAP78D7TFFwwEH5D57np9BcgbXnAOUJAQDrQ6Id/0VsLFUQuEYS335dBr
mmWGCVS1Q0UsBBESCVRqhSn48k5wgH4joKML0bHbt3c7EYeH3I/INdJAnxgQB4a/qauEMWswdc00
9wsm5sPyo1w2kmokJOOk2G1RJEDhDzxnDUMCF4avZxRoqZ/Y4Q4LjASuCyjhxR90nb8m6CUhCysI
MKyMgXFUkgDE84b6gccWDt8Pr6zXdBMTJURIwbjeRwI4BA6Pgie4ZIkSqCZA0RRB6vPuPPx/wesj
dmfq01/kLZfs3abDL+XPwHFRBRRBE0Iwbdc/L3ySJfj+4i5dv05QQiw+qXGzl2EgsYtsqMluuLf3
uJsxhupRsn1W24hczfnopxBPVCM3eMy9G4gjKc90kDmdYULrMI3wwNreP3QAhxqlBg2JMQNhxzV4
iJ+FdpHloWM06YJG6jXKQToLdBhdHpiW/ekmGnYvcddkPLrrqE55qPfKcyTiFrbSVqEzoiHweEeQ
zVNI0jybWhz6MONq+kbTSIiTiAY4mdeL81BZJDpUrmkszkYuIhmbmxzSgpBPBzHqbXBOzhhcvN66
8UMmMwiwFznKhGQJ8zw4wLwzcgVVqJx6A4WXjwms9ajhdRfp4O2a1yJbW4C2nWdvTky9/ScnxB7+
p6OnuzdkPCBoJ71EwlIoOS44njcYCrkFbHTONG8Go2qx7tDskHDbDUMzolC8iGfAf54B2bKb8Qr3
EHPOND24hk3R4n2RTQYVDgj0romoXverGduliRS6TMexWqy6XugtoWr6u5Re0badgum1zzOfgIPA
aEQ2bQVJOU8N9um0+WkfLwK0afgwzwm95nXReUTsJPgavFoOCOi5z17PaF3M47Mgl1SAl3E39UsE
JNAM7CPemCjZzYS43aAMAEINzFsYjrbexmZiW+NgVrZifbr+3pwwwcTzgDyiYyMVLgW7PrI2DFix
kbVGAi85xlScNljkMTsiHRGVmNRU1hnz19/uAciOXnU+X8fy56ZYyTITu6brvVzQmbKJl3Voqb/M
ZvXTnWNB0Q02WX/oRuLzmwjkQOluUkbDc55JLiWDnDJsCWETfqJmIGjdaUUoan95gHIAwRIrdiqc
R1q8J4bN5ng8hzahYQ0Ig73eXTMCL+cljc5PEyMCU91EIjwYkCXCPi6Hqpp+jS+QOnkPBgsHRvYJ
6ruCd0kVB77iJ4XvwkPYK2KQgHAdhbBcIxobDiiuxDXW6qMczAyMIYigsmCyliKR4SSLAg2M1AzC
zmvnDU0Xu+w1/ItghBIJZTOJKePrD78vjbPN5O18j6Nq+yPaNsQIP2Gn8+pmmBpR+GR4bIemkV/i
sVTvQA6HauMwFVBLNMFUy1VQUCVQTSlRS0lBQTNEMkxEUMTETBdcZVEmYuQyVFLEjBMSvQGiIL/y
Dh6cN8f6APFCP8LPR+xNJpv+mKTrbbVbahX2GjX9Jva3lqutFVTWpDGgZhDhpYGt0SqGBtkyYw01
mTLW7Jho2apSmsHdXqNhL/Zw6bTszPGZVVVVVfY+3II9qOiwdcFCccmABS465C6jgHVRlxfCJImH
XWJCTHbBhA4+3x1E8nUJMvAyms6PNxmVEL2SGw+0D9qef2uQFHvXXJ90NmL2Ij0b80sDlstl89Vi
Usiyj4Yki7qsG0/x1VFUhkLhMVyJxv12Kpg0xpJbgZWYZWetVaDqx4/hzPTiP5bhdqQG9sKVsiTi
Ojftgo3rZzcCN88mzFWuYN4HtRDfkZ1TCbZI1NCqBiEZAMHM4lbMMKRVoGUUiIWAUeQwdxhrWq3g
iEHDVjRXFRaKBXUEdPWZrZSkWRhKZWZYMEYBGEhaulETRBBHrw+XDp6Of5f7f9zTgXZQGPy1bv/e
Vomdyqi+d52ILF5CizquI3E2Deev1nqFHyIgn0gAjlknrvqf0E0APakHyov3ySgCCiWKYVKSGFIk
JhJmCAmAiAYlIJmEGlorej6FARNidZ4zp9nTJQPMcfKwBRqJwhpD3H7mFj6wP+H3gC0kT5B0CEmw
jKHBQ3xDeCN03+VCRVAMTAwwb6TtQgoVq4mWCxmFMUYQZhkQTKioFGYwmAY10QuAQEG1lwzfCb4A
p7OUeXk7wQN1nsMbA2iQGRYEcPdNgDqbxBOLxUWPYnpd+h3G+iRK8TTYCBYivE4hHwhgkiRidPVd
IIdqj9CJ18NeithvsYOoHhwM0tMFikVN5QegsgcMah8JgOkyO8e18Y1FDhohlJMOobTtRqSqMMCz
ELMnJs29VFAAF34UChUOiI6lBStNJQ4M+ozCGtw+RQl5FkDsmqWWJdWEGPt85COg57YcH9EfcnDr
rH8Op6iTySAGh5GACGFU2jCJ4KJ3CaiebssJZAbe9oOW2HgGgx0b+SGCSiNf75IzO45+RT4vDKBX
+z+6qqqqqqqqqqqqqqq2C975hgAXieyoO7VBHkKnM7FtJFTihw5qQdGJYgHeEHwgFkj9nyBYbl+8
cxP28sSYebAmC1rQ2keZkt3k0yjl3g+JOkhUPG8nJ+7CJwkmIGA/xsOl/Ffa0vE/Kr6BBmVRFPD1
0LHdEYeWccLczKkcRGiqMXpaI3iVA5QJbXFS0rwTcBuumfVE2wDYDyzI1ZMh84czBISHDpJ0X5f7
h2hIsOLvi/GDJqWUOjWXy7vfEzRyiixxYKH5zVxB6HkYLEIQeBDxkCtHj6kJx8W2Y2yJxmlgI1D/
TQN/YDxDCCXUOt+s2sYaichQzoAkGQPxMinhFL3uQKESKHzk0gpDJV2QKaV0JDAhoJhPlA4T/nY2
o8Ypfw1euocgAcH7cvY8WEEsiDN4uZi+L5WWgpRoaAAJH5Tq9gyELPwCQUikPcJZRdTK6guR99CS
AUQSK071Xep5oA8vGBzhHOuQS/fgD7+vm4bBNVNEQBoUi9YjGqA1GPRIQPpz6rZ/TJVoOc+YdXcH
/HwKCqOCtzmOwTpDPk7A+I/N4u7B1ZUwNqf9kY4K/rIEKq8kyAwrFmDkLVW1BDBtH6NZoc0bZuw9
BgSk7MDlZpE9pXL2+FTqUUu5kGQgfcw/YK9ASulZ7XI22RqBBn7wcU/V6RAgeHlnsnmt5pgWzXRW
JVoS0q8iqAvo3dfKnisx3+FnSFznGDhSz3zWIcJRBN3CjObSe1g1SDXOW46QnUMr9wsZ0ZK6JqC+
meI+0UjwlH9T97u7nmTDydTHhwYqQpe6iOE6R13NGe+HvdJwzqoOuuAbkE84OroaU4iBhCc0PTbs
s99Fib80gf9E8ThOcDNN3VD2CIKCfzQ2GCEQFUBQIlCOWEJS0gUBQlIBQOSOUFLVRQEERBPWFHQe
fPOR+IOvOoM2LyhAbAFDQMYPGw3IS6AwkcMbDJCRGjQRtHQ1NG4aNzIKsMZrWJq8fytzfiQUdR1B
1EVbQ3/ZioHTzGHOB7GgQGivSqXGHw1A44IxihBwgy62hXjdXON7GFA2n+baONanICGIhsUMNTHx
pRoVE1fU0/fDWGiCZ4J+tScPW687tX7rmm2Ehm4OyG2lGpkBH9cgpui864gSpv+0BNrgGG0gaa6E
IRgE2HEZHsJI8ka1sEph4sTUyUo5vAdSCOEJSI5mGYBDJWOQIhmRgghksEgJpc4fPC/MYByqb3IO
WbU2QK2llUyVA+gmAMdW05jyGhbyiqbxXgcogJ2kxJkMQnCHC0qMJYlMITJiMAwwEHCU8JB0PErR
QAEDKqH6AgDGShoaaQ2EA4QAaBhkzMII+jfcDO4Vg7fsD7QqIQP7W3/Nu/ruP/gn33wF/Sf9v/Mm
Tw7Q5k8lFJ90o95+BhKge8IkCFPQ8LiF7SnAR9wWEWzH7bpYilgDB2Jon0B2QKKkmqK1AN/FJgdd
VVVVVVIcSVISHvyFs2Y56CmqNgTOAannNgf71oPQc6GhAB1QL2Q0/QM3xNXJUDYARvJ0IMCPRsaf
jPztJMhyAPhDktAlnIb1fMRMqqMFQxIv0A1faDSdJD7P6c/owZq6BZQjo226yFRkD7MwOsVQ1NA9
sCu1YNaTVNhQrXaLAdBNiD7+FVTVKhDIFPY0kOHR1hVC50YP0gDoOew4whqUHqEAprICUopiYYjz
vHwdnTrSS00dJDCPVDSh+a8jGa1Y0gQEYHcgPBJzo3TxVFNPdiGxibCGSgpZKx1GGYuOvKM88xDG
HEqE2d1MooIrcDdCSymyMrJLMJwgwhyERJiVSoGzHnTu3Ae7QsjYlDdIGxmY8SaowjBQgXpgGVRd
SHEFC2s4Q6ppDdBabhYm7gNEOi4iQkjEskANKBDYbQ7ObTVie6DKTa4KUpKQuEYMPjMW1xwMzEVI
QgGK6MpKwpSRt9KUrQEY2MAJFAgCywhcLm7rgVmUGBKFKgMkRGRKJpgNibT5NE7SZkCGiIksgyhY
GSQCDJIxwMgxcR9loZAsgQkUGmwZCRogxsUXaIOoTSVSPxuE5FFCda0uSTOSIrhucgPQJ6wx5eGe
t6Y1cCerlk5OZhtFNzEMMhIgwggjLBowjzmuwHU8JTIXcMOo6KLZNIORkk4WHN3akncMswgIyApT
EkdYJuYYJZgYIkQScI/JMTfkdsLYMdUWweDio7yJt3AXiQgBakIP3WUCn4L1vjwMBaRXllephiY4
GAGBYIUDVVSVQVVUlUkiutUjCBefk/hx2ZVEcEIDECObsqyspEU0EwRtz8zouSQFI6x3BDpTN7Fg
xke204YY4DJ587pSQlUBHggDABkOi+CVfXmc35HOMUarRGsGGFGboiEgaKrRyBqAzgygepJeR50m
YPHT+z36GlFUV2cndOpuirBcvphFpoi9tp1/xaDNvSoyYq7XR9n51aEy09iwONEKF+lbWV5pUdIx
Xq7Dpd+0MmklyJEqieElQ/AD0Rsog0NprTFGHoho64/OLqYQ7NayXCcJ0YIsGRJbuZJB8Dwu3jwK
YkSJi6spEjy/IR9qbXXpzxmzz6QodHRBTuRooSGtJMxtUQ4FMooz004R71WYKPX1a6EfVVkwDNQj
JP3oc8cHlh0Ggg1IDEiU7cDBJKArfAZAS7HzEgkIX3Perb3MwWDRggPhF7/+T5HHnJGDkkkBkcXO
zYU1iLqRVygoKF8piQn/HZ3Yycpc0UKHXQpe1YreaX2H/edVWwURApioONtrKdqREDG65ZATcVTB
iLRQU6Q3fqy1O400z4BTRsrMpnhVx7ILtjJzmQccqKKTQOhITLjnp/sEWF0XHqPBo+gVNCIOrVTA
x7hNDcbR0PMhReHS1ePCSIFOOvjmFLx493qrB46sMxEspDtUkAvBq5Ov+zE8iboMyYFfTRQHNtNM
x2bCgrLx2N0PBuUS4wiGxDECSBwR7tvendqdqE7rDJLPJaCQRJ3guR7XVauYYOUZgY0EdpgHS2Ck
ZFeMHqzaDwxQabLWgEIPb2JEo1OfEsbyxeyE6HrS2yO4j0Q1kqcoyJ5ek82aYa+fgrHciHPaZgsa
PANQrfYtKz6YqCyxFkeb9LInQsSO23fOtCh1SgTJGkatNWCK9a1TgZpbhwyXiig22wrwDMM3Gd52
zEAXSYimOc72cN2jARRL6QHncM5nF9EIwgBxZUB+hLp6FRMBT+t0EwVIGV7zCh2mkDKuPY44ECky
/iR4GpomMgvBxyCUGDUcHwDjiuAGKOmoCwwm3Ph42/BJ6WfVTnuojPAPRKJghO0Ag8PmAiTdQCII
JCmJRiA+CsuSgRfM/RiBte4YH3wD0rCJwIfYM7g0xhyCqV+RBkiPcRUNGRRBEjtUO8gPlD09Zcib
jVTgJVXAfUaUq3AgywdVuD835fd0X1HSJV2w/uJg0PmIdbWIh/MR3bED1cBxA/qXoVKSkpXo7QLB
wKOpXgSCAchC80wJDDDfeHUcaDbWfHyMxAv220YBsUE6eqVxYMdtYJD/If5hBzQWJFF4EVBL+L/W
bHSQNvNUR9FBxsmwseVsUWekWCxgIBEn8yMhs9uI/enuO8kt7GGEnosw0D5z+ThiUhsbpg0h2Z+g
Pn955XuEKXkINwaCYWVIyEJuDqA8T/qAQFGxTeHegUjiCGCAPnOkRILAJEgx7nYhO4f7FiBSf1kd
hZF+BsGGwGthMTE6BCL0HTCRTTJMtLhYMkSweVSElTiIYNwDTw+4R2NkC4dw4EzXNSLCDAhBX+RA
xSmVJSiAglQJEkADsJxD94gpBEMUR0BkRzD/Gbt2JMFIyIfZtwshrHDHgYQLlyFDBYjLmAW/u0Wr
V1PEVCcbDY/NicIAIlUIhaAZJUoFEJThXQYWlRrAf3nZriKuRUwZZkjRYuoXCKtMWAxMEBtAsPed
krDtFTH5DZuk+cEoDwAVuFVUXqQfcr+kT9V8+3UENwu7+MYQ8aiGodIOpDshj2nqBZBATmf82vUa
eojm5gGhaEuaz44ztO7CxhrGGDXqLmufnDZk2RognyFqQvBbztHxSvHYonNfLt6AyRNNRkkyrcAG
jQ0qMK4aUTEYZkfHlenQP+k5zgQ5jMmzZAshoU0rx37CywLhBpYI2DJZxVyhe4V8onykU48O5Q+p
3GC5IBEPb+ZE9SYEzwzEWKYEpiASCROLjI9GLngXen7QLgHkjlHMSYyk2g7P6VG3X2UtRu2cymtf
LSOs/S5VAG5stXZ92bYYvzrlCEc4EkR3EKH2cyoWJ9VXPFNYzcUmhYMUZ0HHfQG9vQPCKsMpWlit
1jUh7/mU8BDzQZ3BvODju4iIk6rznnaEuDg4JNCEBiCBIkGq2HWK/DA17d1am02g+YS4ix5gogYO
c4lguQblz0mCx5gcyk6Imw01ziyAN0FDrgHdFUQ3/OUjoHVieOapa78OeFgM08T8V78ztAdi9nO7
9N1UywVt5Zk5EqtTiGQB0KjSYMQfGJhKLZlk72UhsVfwRtu4Ul46Fw9etvGicIih4whKKqZaAxOM
39phGG3rwDoeqUY16FNIKer8WheTDnIXMPMxb8olptrsEEAQSWx5VFMA8gwac3Dc/4NTGiZ+fCCo
4mMck7MyjeFpM/tI1QZ/F/GNHdBVEZZTFUeZHXDSNmY0FhZrhAY0PDFF/ces6s5szRspvIcMqbby
q7UPpvQR3Zd5ku4u2sS49r3XeZ30HU5Df1cI4AeGH6k3QmRmBCyI64M5IxhFimwRmsaJiesmwEb1
1SGOnY0DvQUQ+xEJLCSrVqKCnGe09NhHnz11JBJTeMMMs56enaHhbmczYKikkJhOmZCUEz/rT1/0
uf9drteM5dQlcY1VyreDwZDHF13akwxHY8B+PioJr1wgaOeAgx1Y+vTNWRA+dSjbS0oeEjlRRttY
ur2uXDWa4BhU4SZFoUZtiIxtxxQbckCDfW1FGxt9A14vjDTTaFtokIpCRQgRb6Ylf42bWpDQsu2K
7t8DgHF3uNEXUSQZB1glQsZGWxxxN2+oUcIUX0QOaymNpICbamRieUX4qNcmnU6EYtieg1DSXUDu
358oKN5WFlpbImxtjAbEx51XfYaWNbVacLuktTQyWx2RGRo7naT9wu4Hu4ERJ4EGx+c1QJW9qCIa
efy55TqzgVTRXpV9HwBVXfHoNE2hywaq8yQQQaPEDQQPDrhn9JkOh+hNDGL1AG5T9ps2EXIvKqEo
gkHvAX7I0P2ao4GibA4f1dP4dGFBxhkPo+w0FHsH753DMl85+6bYhCkYGAoRIAkY3/TyoQ6rwRHZ
ABQLSkEqMRzn7fJ7DrM7UPoqR2mkI+XV9caRRF5spDrTtOR8PSpMyp9h0ahaiN3hpK7fdNGSLEIm
kijyFVTaEPkg+w0VAQOEb0TepgaBBVEmwyXYCiYDgQ/KH/JFIiu53trhZQ9HMAUOQgK0pMoSBJBE
BQvsCOgsG/W+SwQf1MBME0kgp2MD3IAjsLiyjKY0rH4UEwPCawgdUEg4wjIONi4QICYSg4QiqGMK
Y5g1BMwJhC4pjYwENAUAYGJsAh0MqLGgPNMFY00EMEMJQ+9Tx0ECkkDFQnRxiBECySBFRQfhGInJ
UpCGn7/SILyc2HSoyKUgnYoaHQdeoGqp5HywpAeggcIk705TNqJ+4AQDBEMAm4VUg7vUYi6955xV
D/gFUOQeg6MSN2LIlKJOivV+ieZITsCFwzBQkCewEX1G885tDtHO7BMCo0DeTMAUtCFCUKNDMVFV
FKL2iHs+fQDkgSdwiPy/i4c4KB+oIUpJlQPL0puI2hIOe0I5fbTVNGgGwNgkAuwRRB7AcfUgKKSH
RGwQnmP0gLYoYpfcPm19IcT2yIExMqiJBAB5l6DrOw+3pDk6ExIkIcIBZCQT8kJoyppCg4TBJKEA
/YjiIbDfVUFK9cPgQ2Dbz7IQiXAuId4RP4wX2kFNCYeHBhwNCf4FBgix/uJIcVFMcNWQQ6gKZhPD
CYRUQUtjgmSmkBbjyyohuoPzlp050Ft1Auy5KJkqVTQJQ6iqZKhjgTsu4SjQY7FAhXRLk0JTFFQV
EkvSOI42jYwfKE4EhKkp3gZ7EIoaB4CphMWS76zDMgzJpcCZ/excHYwMMwZIyMKEhwqhxykjFIlA
nJGSaDFMMoGyDMXddjScsgwqCcbHJMJlIjMTBgcgKQKHGMIMJGUwJwCIJsDP9AmruxiwJuBZbqmp
EZmYWGVRZU5UE4kGEzJk0CEYko40Q5ksCvuaYjsqRNmGBhKYRI5CpKDEkjgGAJODhS5VGEuCTgxC
jmIYGDgYOGAOBCpEOQvgw0lWCSE1F3HCJShia0lTJpEqjCMgQIqAINwOiJkbu0RReh6CXhy9ui3j
wylLwdYN5y8JDC6GytkqVKroLSMgayv6c68xHuOI44OKTZYVmOCerDsbEUZjhkJ6yB1/NFdEJBUJ
zrQ2HSecdhdbBot2ISFEQzgahjFUHCEY5DM8yCh0CQQRdj+YBTpABLIbndwfQYY/g0zSInjIeaNK
iNo1ju7bhBfTMTJbWkf8sOceQlGGkE8LecNjFjUBIg35Y5vCgGjMmbAhNYOjiYEfiyQhMMiAsIxM
ehlsFE5W201ULD4JEAuv62HAxjR07L9VYn+EhJ4v/aa7PyMy4svzWemESJZ0LSMmcPBWhU6dbt4J
vPI9ri5gj/J+6r3jp2nnbWqcfCG3fLdIckUm+vKxdwW7Q5+jUdpXOJ4Vf8VQHdecVHUNn1o8OfOi
Eg9i5Ehf8sh5AOAMRw7V+3iZJc5TZF4zLbujGudlW0dC2TJS0re3UuNy5IpEnajG5WgRXKAam73B
gLE5+Oom16dImvhKTrrTK8QHQjQQjnApVqAK0c2ucSF0DGCrdJjFmmRxs2Smk91ffFwwM3rbHjd5
vU5jlGQT0/sdMI3hyOEXBMFwT4eHjZzHp83gNMgBWrYlA6uXCK+6c8M2tvIPw6WtD8+CYwDlRNLx
d33N+qKYkeye++XhiKeTVYn6uC2wOiQeTshjyJI8mE27c8b0DTmmO/U7BDWyKw4Ug6xbceDBNzm0
b5uwxaeLbOochRI3QzejrxPZb6dcuWaFCxAVbQx3HW04wueaya042xMYx8DZOw0jmhw9A9KcLHhp
hsST9GujKAaLwjMEvMpTXOswdISH9+hZg6/r3DSpLXEDpLthw2Zfnq0G+WlZt6yZOueccJCyuGZm
7Eom3Gwm8TnKwyWMFWqTyjQm+i5TEIUDgqJsluPSMMQc2oOAljJN4u25ENYqy++tBCGGovD4SPFn
b4UStJmrefJIPZ7McUFMyheSDpDt0BcNg08JjDlIfiSBMd7iM8t0xwmtF+Wc892BuDPAhFGUgNFu
3X3ux0RsR0EFi1twZmEl1WyR39REVzQsyMwE+UXMwo8DmvuT82uaZt+yrt83A2ZcRh6n4ue6JQUu
j4+hGeW7ixmmxB0h1Bd9zMTbYxhsNutl4gQXwhjDhk+E3xxpD6OpfIopiY9iMumkFMgYydREZwl+
7j20c8Y2jTVpR5+7c9ScL3P7fR7sscHiLeZRMTM6X0C6I58mfJbc+Uy4GIh/XuBE6SNIdtCPfx3C
Olt675wrplPedzByCRVP3h0NRpdlK7ns90D1MMRGCSvRnaL7Hgp0xGEj06NngwCb143RCFipFNqG
8JShapZwFY02MQMJ22KxSSiZMSwBjwPD5YVLnuG6l1a8ZGa9bwzOIoI1Bk85QR2zmztpqyNa1daQ
5C5KwNLUJ3Zh1DL5jUWYDgHRhwwb6SPuSHB6drxObtDs8dN2SFphGphoTO/LwqIgylcwwhHb5SeR
1rdTuXbe5Is2OGRdOdNalIPUsIELAp1jE1m3vkI4TM6LQmpOyEIeHVLNVkfyQ0KSBDivuQnJphQ5
T+bggG2F86sR8dvM+6PbAZA5Ys+YLxA52Y5tg6SKn7O5nSZeHvRNo8Id6GvQkEmM899snA3IRwml
wh4cl1KCh3JEDjRFeCUg4Qd5nqNus9QSqNABafml2ECoK7P13aE5lcPphV3fX1znpIMUrVF00Sxk
XoMwjys1DoRIncPUWskFSAzYQOWUGbu6P5iIqYfDKwaYhuvo5IEEGNAi++o+fMO5wDBqduDe9PUc
ZG8k5EcMxhOz3ni7pLJdrgO5ETxXQvrx9p2RrUnsgdmqwM+mJgdIg0JoWEnB56U6o7Y+OGOMOM3g
ExRG2KUpGjBT/RXG76mOQUGuqBMmbr0YcGITFZ2MwXRQYOTWVtslka370O0sUIB0QmZmpAc47QNg
QNPHGo8VWRzO++fab8J4yZvDUMR0WTSGCqDXhRa7TNzjxEYHO8lQt3SOzJgdFIcCTHsz9P0RGtuj
bxQe6lSU627VjKyzutwtYrw6OZCq8ZMHfzvgRiqrQXRcN5xNTXSShVijs8J5l0HEv3nUOuFS4QTh
zzR5r492YxeKbyiNKY9ifwpSwvReMllxHZ8cX3myFfPqROyY5cc6QHIFdm6b9GoTPIJMVnl7PbJI
Q7utJ9/d8eXNB82JnKp5tQ8zR6QcCoaLgu1mIisFRkrDz4VZ3/sdOb9OJXEqaLksbOWq296x0oq0
TTSMxygJ/6K5RBG3GrzcOF49lTPm8hodU6OOjpGDCSFXnG3Bl/V3Cf8wkALVBPGokbiMgE8Na70X
oGFS+DsnBughDOPI4qKUYc6aKEwv8G/C2WeY7uptSCjIwKw4yOrYlpkmBjYzpBYNBEyWTI25XWbW
DZnZshzQjzXXwtcOwhqxwfX3ob3U54nvijlCVymCBD/c390UNC4JDU9nNwAyRI9fu49u0aDccSPP
jLiDyUNZ4OUohgJhQMRG5Hj8BS3hpo5thtSC8N0k2zMLXUs+WSi3dOCcnBGwlnY2j8IZ0yle0hAI
7MxDHoqJGN96Y9+MKV8Bd41r8B3ysGXJPhBoJXCff4QXLjDorJy6oWF4tw44780dJEO2UBpWzPah
HwOM1Luyyhzhn1yXjVc57N0g4tmymZJ9uOLEsGBfIyIQhmzAVBiHJfPLF5K/0q+66rlIS4XHoSMz
9bubbiIfhya8cfilCkKwmjZFrmi0hcTqC8nim+RjOoRVC2jIlc2pVfO/yXOLEmeKhWpAC8Uur82m
ZxMeiTinNA8SYHxCXs9DcChI0CUdHfk+pI1gI7SkEe1GkbRTuPuZUjpKIcSCEDvMM6DDMYDXsJAg
2Y1ooKCAydcuHT11euosjB0UOXNrUhcsVOlaMknCbvBuK5mc3WTEjHuGMHRH2fTiosUc7VbQcDXz
tbPsWlvcqDJEfXEcDoGLHhsXMT4Q64WcBltIKLG1Ej3oByOEeXkFc/X099mRYlq5KiYH5G312C2W
MjM47vGfkE1sC1Q2HIJgTiscdBWHEEKc0wFXjQvsTqx7NoO63FE4m0HBHcYKPIa0yPqqWq15bcI0
sR2dy6BXPxhoeQuQ3agcxHyVSEQ4pUlQzoQDQIiPcIBb07mn3V6HMUhUbRx38wvWSESQUNsBOBo+
bU7S4amRkdVI1+FcYJmFdASgLpYDiZQYL6DCR29SapA1VDCqs4XJYCiKsjM63YgTmk3gt3kMEdPT
oMkoq21bIjCLyTC5XTDoOhwiarzWpmdhlSB5ACluiLS9HSBszbgH5GoJSY6Kl8C438JhqYGQIYJy
qROUkPcodRUYAGaoA221gygHXCkRBU33QcSBs5azaa9K0ZjIjDLQH89OVt0ZwMCAmPeoOYIhSKrl
rg6+xcHHdQphRNfPNLFOsOG48wb6yca8gezG9cfdi+3c+tzgXk8xoFDTQTxHEv4VlDb5aH26NHQt
3A3DuOqgdIPCHVAEyIFThXiiJ13BUxiSiiIESMRu7q20IU24dGHS6CMggY6Nk5BEVNWrxX21IiGR
fHxJeKb3fB7a0LKiLcwQEGa9+wA7oU6CDbBMm6CCFyDINBED4lKJDoRvzdG3MW6EKyKByYD0fj74
xgQwG1CSHty4m4LlSFg2hYCwQfMO7yoTZXt9xoJGGN1+35wk00bvWygOJSKQxj0TaxjzvLXWlDu6
eesAk20RLhbxbpK6++IECDrdctONSEGEtPTkYmOOJ0Z6vjIgQ+WcW3JxLY2Eo2xQQk9uU0ianTGM
QxMItcfH8iT+2DdCnrHG344fmzGJ+v3Vskq6wdaMtjI4/TBoWta4lCF9NUDYojKE+CsjhqUmml1h
0SumYL11RsgWrx+a/opzFUXfBRI9lOv2+iPhAwd1cu2ognF4Ry3OM7SMFYUVLVivPaWVSkRrsM0Y
29ghCVPgd3W/cni1w8HbDtkZuEsOw6SZdRfGMVYm4NRtKXQwk7q+IHSBczrE+vY79ZrDun12hi0z
VDtPbD0jpD4Uc1g5TZRrY9oITbr8rkNs3uQxTKN9J5o4P7YE29JwE/sABREPKuaGXsqtXeqL3MuS
mvc2pnGvOJmRMo45zXYcMRHgqX7ggQR1dydlhabmhALJ6gTBdsqf4Nwi5GTpgQoKppr8CyCf7oo5
KEYcR2B0DQXO82gUZQuO8D2u9I8Csk2OYCWQvSJNzQObgODwEdxeKSor0GHVJcmvXTckl5hF8/e7
3CijhY2onAact6FHoUzdjvYJnYgvwAbWbqrfLJPcJQBAatsAxgNzkg05RpgxIyI0C3mQbzANFqRj
bBvhWVjxDs1tkQDlmPbT8619PzJYrBpjz8WOUIEMxUwOVh+8synb4s+ZmMvzQQUaNinutRV+jztR
jG4iHYlj3lxGJRVwIwonx+rPT0rSY6HcQ6GYs1G/3YO1ykJj5fhg8YmmB8aJU0Gp4PQiCkhjzeMI
LHalRuxlkZqJcVYPRlVCisbYgOj3vNYRYaDXSYdzm6wg5RPNftLrg7quac3jaPNAx4uItL1OWGEM
TQUu2jpXjozJGBtHgdCjoec+I8ZqD7EHk3VFFG4kIhIk8GC/BD8PB18OC9setnMszbCXSRidkA01
HkXhYzFLg3eBx+4cNYNQcwTHdgBrpiUlNPYGKZ0YqmEzALQA+ZDi943hxQ69CYYTnBrmpgRQYRSU
0UlESEQfQgyFsJY7D4B7QxaBdOwG1Q4Qwp1W4bea6GgRUDI0oaCKYTQChLECwQJNUbiDIIxlP6+w
DlkFDUOs703jqS/0yRlhkmM4NJhFItCgEJGIKeIwcTqaJz4+2+z7SjI94mEP1ri5YR94nI3oaiuT
hfy50dO2785dqBsvcv7g1MHgHk+gkJ2cD/Tel9ZkqAhxYBgLJTIfmy0BDQ5NB5AEyLsT6xnTb9CH
q7bNdq9rRCSkrCNMRJWgIeeJRwNpsEKh+Rv/JIXTyiYDzIIh7kKNAB9xDkh90GI0rsUmadT5sX4j
DRyYUs4B0bqjUfDKNCdCp6pidGVyybYIVTlclzwqDBm9CIuyTng8S6ideetOlU0yICMMwY4KSR1k
AbhqgGy+IE7lTj0YYnSjPpcOzsjwQcPAs9BCHlxXFGAuFI4Ei5EQTISaA2KCERfIeQ8R6zgcjrCm
BCi53p7SHRGBFHslgLHN0Q8edEsXJkwjSHdaRpOlTtaPJKI8Es/s0AFuEM4+Bv6jMz5O6qJqysjP
OoV29ZzEL2KxYag90LTLBQHVMlyKE7hUbAHB2+T9P9P68Hk5AlMDkI2bCWWSET4Ckp78UWngdR3/
hfiD2JzpSG50MbIs+lHzqhDDchz1iNO5d6aJ5qaok0BWPM7EO8QkN8hkJzRkBkKlKAeV6sE2SMQr
IVNFA0ciK+PkUeINpHJ14YQnkhq/7lf+fAuvdR2PzQouAGjq2kRiIxkSUCfe/m7To0SSeYNiR1v6
X0GzBizeHVpSGE5o0am+zaN0agQ0sIXutHRv6r059mVkUgTaZiRBSEQRDTTCFQUUVVHKi8EWIV9J
45wwg4BfYSSdQV5gwnn9GMAEudWILQc8W+/eZH7NPZE9BSZABxXuQqMTm8dNl8S/T9AZBd+LE+PD
rXdN154SkI9JCgwC8F+At3PWdoo+uebh8QASM8r4laR3cmz7sOF0Mx67hdF/EpmFQYoGvbyaHSJj
Prs2Hd6Sl1IgxEzDAbjWppQWC+fZmm2ZQnLYYMAHk0IsGQLYJgQlpBdA4lg1k6lL5uLv0yqqq3m+
9yfEHxgHeCu5ea6fiPGB7hP8sIlISpIBBAtNK0FBQLSrBCFCClkXkQH1poO6JqVhLEDdTROFGUq2
RZzEyAndRMgUoSlMwNhsNICNSh0DBxRxC8COMpSoZAobjUxHDw789pzRQnp08Z52MQ2JFDNJHOVB
ho8X3MZIgXJa3j245Jow3Gum+tCg1rHSP5S2IB40gD4Oy4BKshr0yY88OqpThzy2DxfoOSVYKRDM
bNobmlEeJANhEUOwvSqAfCYR8DHsGfZw8HtpKnf3QbWCwunX1ng9wbNNtHtxeyS9EgGskZFSQk4H
G8fVrtXtCKA6YHBSmRYAxwwoWZQscPKQZi4jGWIYhOw6G4ZojMAQGLgEsw6DhjBYimIZiGE5gZqm
Qm/Px870v6MlROPpIOqiirK8ZmV4k4jkEOTHk5Q6QChOJIJMgB0YtHwPi15JlwThDfqsWEkvzN7l
hrmSzFENoRK/cSlCw+kg4HQiBcLFy54z1veAQFJF4sOC/7Un9W1TTNVFFUUxUzVTTNVNMY2VTTEi
Icgd4ROrr87Xjj0J21jDXWm89J7yyiljiWKKKLYNZqa0vRLy8p95VpaVGoXAdFPMoDw2fV2J8AH0
LBIpsVNnBQ9hETuYnjc3HAkLp7bHMM6zc7tyi6HvNapDgHn/YaJ445iNxKsMQ9iWiIST5AiJxJV9
4MGd4uTeuogvIhrAGiMIfEUUYPpU+kMuWTjUgOr2lKWj9X+2nfBxyHLUGb4mgi7uJewO+7kDqIHT
O/ErV9H07bXMLR4KR4OYU+66AlGQqbFRyAUggE8vmdQgAoADUOjdhuecgHD8w+5+UOBFcSQzgFbB
08yqnDIXCViyWTkRSqpJzdVsQd6gnLZ5TkfIKcwA8iPMYhy86A9XIc256AJIOf1H1Hk99VVVVVVV
5A8SF4M9jsz+e/J7QdjeIeEl7hMmhqmXL3jIyG23I6PXAel63oIyCTSIkArIEwkzSQSU0iEg+ojJ
AD80IYQJ3yIYoMjRoFlMtATAOWNwgxiBJVlQlJQE/CyiqYhN11roSpLLIQEgAHEzwC6GUpISqYYY
gnqH73xDH7JwSRyN0xJiY2VKdG0Z5283kzsSA1CSYmSHd5NE8PUIc9CnCRAhHgOnMMj2LMM59jm0
McjcigfzWgynaQArG8nhA0NpSakNI5d4Tlz7A2bNTnRE9Bd9dyixBEIMxJTERabRZENjttfAH5+l
7TQBcYl5UISKSQZ+Fg1P0mCM0nodGxv7HGlTDN4iR6Fi4NU3+qBptDTT1xFxsaERsT1OLq3KsKZl
N8TDE1I9BLBjERHCEimOpcNQTMLKxqNLWEC21DSUBW1UeZuthGI9Ty5JBw8veHOpOhqrpMDJkg5y
UiSwGBxTreEs+DtiRaNrQGjFRIxBtaIbEnxBFWgNBioBpAtCgh2B2L1IAwCRx3vCd8ohNovHsF8U
BxmYVU0GmiASAMF8/sKg8hAI9t8gXQ6frUH1scfvdGrlnyQMsnkMcUqXLG2+7I3xP0eMwPt5Oi4C
GT6b8LW+qD1z8jHrw7gmA2iC8H9Vq+2x3xyqxD+ccQ5CgNoa0HK06P5zRnuI5+fboUUX6yAPihiD
S5LIpYrsxPSzGAaNRpph3UIPs1ZG4vqkYw1gOZ/oiPdd0LsyJvHXbGhHAej1QIxh4MwOpO/NRJEY
GYWIhZVc6DNrUMh9vabLXzIJpaujCtfFA/lRnUIBYgMIJEdL4NmZS+WCmBFC0xIQwfTyYXo1mFsq
8nyw2AnqIb0o74YWVM/EI2A8FHx7SiAfSeI859KgBhU4t94cqeI34BVEBPI+reC4mIGGDAykDAMe
57Bx5ygDQ9Q6FDYgiJAC9HelIHnQExpDZK5U8g543ejhVjHeG7BfeYFOoUk/lcOD8T+JCQs+SpzB
0WhmwSmVUaNxuCCnyInYHKjVDIg/KdFRBERBEGHoiP2Q+soZ3uj8RXZAeB+8yJQ6c9oR95EopaX9
MGhEQiaVIlZAYwJQo0TCfOEclCgqg05gGycG/euu1dgurA/QCvlBWVolGJYA7V6AejJCOVMhQiA+
ISHwDxHvj1/T5M6Q4kKzzzOt5kM3NUOrmDlIGLE4kPzyRZkQgpkckfKOkmYyUOgkGdsmJMG4zpa0
VMRaFRHyPj8TpPgL0hsjgg8IVwpMOwoLx+hDMamiHiepQD6gx27NnC/DhV2Nob5JIu2SXqG6kMx0
GDB1dXmcGGcZxhNqNswTMnwknwB+AlKqEE8kyAPK3TozEPdA5cDaofuDEx2wO3VDWJgHZDcNlBux
WUdkVwliK+tzICyzcwFU5JfPeoeQuGIdZknT4YbPDILQBJuOOx66M5LsZmCMZHQm6FFH6idw0BEL
EYm7kiMioU0wzFFDQqin17cF8nZ57+cDwnicvXH0N/KVEUWBgZFWHAd0VxC8/OJRaOkKNsomz7Q+
np870vbPHXuGxaop3V6Tmqk2YklAg3crnYsl4KHapaqv9H5qRqlTA6QVQrHl/hx+DAHxSpIYgFRn
1kpMaGmZLezorUk9YisZhTDmY4aTUUtNP9M886bAbMCI9hzXOuscNcS5xm1W+fbJj6uaWSTsVZ0y
bkTYKgtMFpOsshZqIU1mA+r/BjMS1NHUajUQQQ5UMyTbX4h25hjlsvvOS9+GaqZWEHbkyeB9Us5B
sqaOXztd967bzJCXdQsbarJ57C6JBnQ54etFHk43gpW8hHCqRwevRbCz/MxumOiYOYIE1dudpgbp
0bWuC42umx0zRSsOLahso1ZDJIpGkaMVnM6cxxcLhQh6h0OJRaRFKSH1WrMYbo+s6h2ECTOdwskQ
3ejw6mGlkWDGTpAh2R32sHraFyLbSiRs8rRM8IvwYU5Ac2dDZ3MNvqC7vGknEpvShodqUb4xXYyt
cEMiZYWamKaGGw6kB4wowYyMIypxfHuh21UnRB1LsdkmRcLM6jvqTO9BzGliVpaRqhKmiJMkMkyT
JMMxwohqXXWqpKqxg11xBnnXMObjMJBrvGugdA0zh5GbZQAkttlpCCx81FVkNowodjvunXcOBncg
apOVoq2cOJyTEtY4h4fEYiFogM4ESy1G8Oidlunr+pEoL40zGIkqsltl2doZx2l3GtcNkrtjV86u
bpwriUlLesmE05J58+NE8eLr2Dt07IR48oSRs5muIVNt9sq1kT5CZzGzDq6MoW3bDDCqLLhI8pOv
ldprgJ0ZqHlDD7ig5s1uhccb2mJ4w5DV1eMYgamaVzg0zBuzElh0+BqJsdXrMFJr7H+8vKqtsuu5
06Jk7qUJDL2Isw54Mdp4XZFlXm1WGhZ1R5YelgdIcS8EMToKnbi5zxKdeAr9Lgxk1jPRx5TmIHaM
9MhPTgsCDyGMnWNqsiDkjbjU865Tg2w6wj+pds6grpnhrKloiNpO47wPBFiSZE+H2mlUGcmEapt9
HLvlmfWG6AmzsKZ0zHSNEMND6lpSN2TBk4wrRoo3nGYlrxh4qaKIYzsh5kR/YenlU6OiXlLNPXXO
JM4ojqKmhJ3c0jTxLEsDDxLZy0mLfkuF1tvcmElzxxbhQYc4+WjBElWYbckbhx8l0IbOGZ8BumJY
RjNCxOCxS8B0R3eUgSte2aaNU3VOIPUCTXCG0k2CDRpPRGzGXpxeDgMEmJGsY9ON1IldMNCNAPeN
VKTDgwDjRw3GRmgwTJAQJCecRUMq2ihzVJfPVkOjF4ywFtnqz4epMVhWop2ukcdEsWKtQxitdckm
01v07YJbpzi1AmWXlujnblPAyrpOhHy7PSNYNzKURNXGoZ28bgztnM5lBCw0Osibab4OdC6LnCHj
o7udYQr6lY0g2pvY5V4YFwfimOglOn6QQtJy0dFD9MdMy2OOdc65EOdPX239MdBWgq+p4wxW8WN7
WW+WFSYPJ+0nuz0ZrLf0VVUIRecZrsdpotcsLNTYFRsxLRUBAJgpNUSLCABkEw+EkMPkjQhwYDpd
GXxBhhmT9jmTUwRtlvWeejodzJShkkKUA9Avt3GlKOcx5HJyQcgwCRKEkhReRsADMUskI0wdnScA
dCEFioBgaoJoPysiSSINGmtF618QO7oMGh66dauproKjZUNiHTZTSIgtPAgQLIYw/cE/r4uLfSjM
6Pm6M8bjCeGYzi78Z8Q1EqECEAeoglQtCRV2RRbDRVHAc+esGQaquCBqn1WzmEVgThlhluYRsWGF
UOu4bEsQWGOSeFs1woLMgs4tuC8PJXT4OkvScnO8OjxVO5wqjHWBtpI02mMYrlol6DSI7wRGPS5f
PrgGIHmyMZyPpYMI0r0HDJM3jY1HN8oLkEYSNNRemoEEs0g3k2me84CESF601DDNK0k0B5j3muA5
4uI2bnoNGnUsWuFM4WiAkJMOurbkU7i7JwD6wyZwe8sRyDCbAPfZeBOByCtg8IPUB4KgR4nHEmZY
DqM4GRtcQwDpAIdF7TugwKp3AdGpDIJJN07OZaJW9XNVl5bUhEKgRsOji2G30ejniaDkrbccbkvN
q3EIBTx66tDoj6mIeCSezNjOrgag+gx2wJjgc2KCx2EiC4xxbg/pzOWjL8TBjMeiEIlJDtnpWYsg
y4++m9Gamg3DpwxkUa3EYpWe6LnVGC6xIpHEnLxEQFEZHlQDs7DpK0YE0MWPHFj+X3G4DhBFY538
7Iy6udmr191UH2YiMXxfFgdGIIw+fMl7d+/jLMdjhNRTLB0F7OR8hc1PVYRG3yXKUMo+KIZq4nTS
1slYfcupOozimopasqDycOYS7JJY4I7v7ICSL579OcB09+SDp5SVIWkIQmYEkNLxYHbieQrI3A6S
gtDBG3jFGIjAQjTrEADkA1Y2n9bDTKdjzlOuoEIdHeHEhN4UqEIfB7hCk3oSP780kCQCqIiqQt0z
U5nZJM4kS+G7XYG11UVYBrmq0BRajaNfCWKkBlR9JkoJqpxuBbS5R9rPS8UsiFR8COZaUlee2Cm8
p4MarcVLWw2teCSmYBDR8WcAs3vbIj5HTqbNdII4466d2ZbTQhmZJkQO28EwkyVmRd+GEpH12bp3
VskcucKh2n0MGS0daIlrXlMye/tGn15PqWyAMFJhmJJARDpUkjwmO3PXGMss6KltDaFUaUod+2pD
u3lw2GZMsu7Cq+OqVftqW2YPJhunYu/HJU9umR25Y2mSajyHdbzrgrb1jr11ZVF+aI3RDwbRr40T
ejHPI8rOpfL0jComnt4jozd7nCbKLMT0QvcNT4zg1Bq7OTZtuORrgtHLISANVy3oqLbOaqtVyZIY
umIOMVhapNOTXKivQ5RPXVSFge4+RT2mGnfTzk8E5FbZKUprqY9OlYCteOCgagzaW4KwBZKE/MeL
jpDryw8FFFk99uPINWnVRAwLILWJdrj6MUJGNJokERLcEL8f13QyxL0ugla1JsRkZHrfYoM5DqYH
IBBHZQbK2eBSS9hrseX1+6mYjg5MAfdU4+zS9eOuxsRXQTEU693kzY+WcHQnmO8ZWWNFMnYRtAtp
/T5vwZG+Mx0K4v6ub6zj52UxnXn0RJwsGW0W7n1944MY1KG1KGW7Wwbzm0KeF0HjpiEaHaRORXnw
QcbcZvMYM6WmaR17E7eWqgEicvELrT+TCuurewb3CZ/aIa66TRMwePHjl674idgy3aI0IIE3mJC0
4nCw8aH+lpyIPeHZp7Ub9z7jO3+F13sK+JEy+mfqmqQ6oXq0FD3sDslkqNDkeKU1F05Byxd1bjK6
ubbld39PGy74piSGBc5BtERYT4mmia8IwC25KrhSg1dxY5BOUuKlSE9xUVuZVWxujfLnYaxwZHeo
K1NgljWSH2M02zonU81IpKxcNYkWqnpCJgOMiaBZa6JAIwhXLHUO/VTogbd8vYYacbHZaHbKkCiv
RXzSLxOnSJ/Pvt8nxFtI2uJ3sbaafjkmVkNyaX+3vdDXszbMRNRsq1zStjXv2SmjcJg+mc7D2vh5
wSj4HG/T1Emli20NdJq4lV7fHXqCEYaPj1Sd8IJWtbcqIB0ZB/Jr5iwfXBCYQmTIZJiOoYN8Ux3R
fHpQdaowT8TEMvMuBYSPZ7tLr5Olb9WxZI3jyfO/abiem9L1odHsJcow5Luulu02uUNSjPkI2/oj
bESWxYA4q9QUbHgopHohPEJ0QNb9V62W0Ycr8oawyHn760EfTWkxluwAkkMMj2sQXPObKlhinasR
vIcd7Ct94pQ5B3g9vnjce8RpckZC9CuYU0ogvCqFRRiLppbA8u5hNP2nhDIDQEOroDwrispiBla6
iOiIhF9BzAN3R21VXBDYbax2WJUemNvWtck/SC3CPuHQkkQPEQoEmwyDKiSChuCqSZEc6eu9Xw62
oQlGyGT7lasfSE1LAiZR5IelMd+pmabtL7QiXFgTo1Q8dZxuaKeocalyqjWG5u24WVT+x53OZXIQ
YZQp7czDlOak6HsvPsnHl9fbR3pw53G+87xOL6CkjSjcSUxqnqpJ9M4GhCvZ0sj2XZXrdjPiT1Y4
V8zDDM2cjjL+MDmop2ZFYDz630jDpFKQo4vPrJ3RPYMpd3SHTow2BuJJgu/POYexzBLTW7uYewSX
OouWRVbNLFTacIySxUsUpiw+xd9cem18Sbxq28/TEWn799UaN+IZ83tr1ccw0VgiFEMjB1U9ut2S
whUghzaTMMsuJMDUYqk9YxlbI2aXMhHtWCyKWOWlZduByIjJIqSEoJCmmqaSRYGhpKKgogppCCiH
HlEB919nj6zXPXzpcpzvXI0yNgDhaOcGHMQSwsJdTWxYISs0ckfmYmA9g8cIAZNU4GdJOgvQYK99
HA+lRSyTT1BmY4UVhDGqiQgrAoEzAzKQAwRI/StOpEAuRSgkgGoQM32DSCFgwZgtksKkKbAYbPOc
Ovk5mBzN7Zd4FgwfRIegIgOEaaWPvITCnuIkLhrgj8XvFO25U4lDIQkCD44cK8pcsKzCiDoA7lOH
p2d9/AKaSIarcoXMgKGp43DSWFHNMJUZAgIW1dQwcezlQFVBDVBo4SnVC90iQLZuFXMLohlkMoyG
N0JOMieAU9SNENkqIAopqAITqEMGFSk9NA14APaGITJ5QgelWXo8qPgHUEPraFIVOzswB4kvo+Q1
1khtLDKC99dNzMgj2TlEkV8Q9HsE7Y9RpKomkSmqiCkoqYBYokcYTCqqGqEiRmmHCUwkTCBaTOGm
vRR6wyTIs9PPPMnlKXyTy+qsoQg95OXTYVtjZAWG1wcz2SPSmbQg+qm1O2IagIIN4WwFb3i02j4z
pdxwC7sNK5vLXFDFTuMWtDdvaP+jMXKxvhZNFtbZJNdtiY3ZjE7O/j7gbhBcdiB/WXo64ZwBOuKF
p2IxShOhI+gTQhQW34dZCELwhvZhSow2tGjR0cRIfQCCV0xJHDDtMUA4ik1EUG4YsJFNFMlSzNJE
U0zVFQQTTE0ySrIwIkoIQZOKgUItgd942YHg7MAQDwzAwwVbFDwSyw4KBQpZTIVw67HZWJViVYuO
AAooyRKRTTVXQ8dwyLJ4l1sAbNgomY6pAtjUwgLmvoGNwN09G9BPYurCCCISiAzDEiYuLcAoQfZB
PCG5mZDGGo6YkBjjz/N6gK8kOShAASRRRAijQgjgWGWVQBTJCdnsjiOLI8hRTkjSLJCBEBr3hscX
MgvwQfGGneGqUSGbECAwkDOLsIHpWaLPlX5ebVLvzfQJi+Ojbr+J5gK920eNgqJyBoS0AMyL9Aew
ZAn60ha5PqMflln7CZFqezafOsbETFHUkMwnAg/bDWG2U5ZNIXTkw0tpdzh8mmfq7a5oyooycKIY
ohIqqqIqoiSqIqqqarYdRPWBjzt08VFFIVS0FJSd4P9HKdgP9MVEpNs70JHOMe9ENFoT8wEiJ0pC
KGpr600hKkElISgWBiiJCBg8PETwQiHn0RQwY7CXFA4o8gyKJwDk78JgOS9d4wZAMyPqKajhIndq
VvEJueKCT4BZy9SSwwSXQQExRQUUMQURQFAQQyxAFJBUsBUBEUlUKywinzFVfUMXgEHIIIiZiKGl
gSWQmRKSqQaSg7ITCoKAiYiqWpkKSSmlhmTMiqwDIYiZcYJwDBCkUJUgkyyWllimQfp9DkSiZaIc
KhhkFpGCFJlaBaSGGIKIpKWIIUiQmAoVSGW0ddwmzf1cMKbp6rNikKHceoNTAdHor7fgDAKkGO/l
BPEQOyQ0EipiqYoUzAUwkJQSQEopEgkEiQUxd94Yb+fBMYOpWPxdnd27Dy/h5CxxAeKO4IiwiEGC
SA8hDtB6ThmgdnPOHWHZBkMCdxsA1N33rtA7O5TtU00DiURQzYqfCopqnfDqhPhDppg0n27MQoX0
SrkjWpECwERkBiQZBjMS/nlxGGLYK8w5NIPJMhOEGyRGy6Q6WB4MDMzIOQ0uLDSEE0hSbD4g73Fe
DAHVpJklI4IkgZFGQGRlTklJSq8mhMhNq7nCUdXKJBxClDDDE6tYtSytsYp2MdtrUxaIAMg3MepM
g0k8ERMdEbzlFC92WO2BsdCQjyCJ0oN4gGxwggMAXhAInBgVNGBFcCAFMIVTSFUcJU5ImyGhGBFE
wUwQrgQGRRG4IOdWFEwAFBR3+YwfDeu5R3sZyenbUFzGDk4JJEDmYgEPDkCaZuNbbzK+1NVOkqiN
FOxqQdBT1C4QO3LjMSnLPNu0h1LmqzlgMopgkvgDQ1IICOCcOD+/SDzvUw8XZR2tFY83k8WA9/tN
ljM6t6D1RT9yMZAAYRhKN1bn0ERHGE8sPgcZs+VNEHYyESGw7rVv8EE7yyBd0E/JJ46lSh1UQEOl
2uHGAGPgMU0AVMhEJ9uveB5KZ4CyUFCZwafC5rcmtENC7aMdpp8iuPfXcGIdGZASd3maHkcV9hE7
Ua5qpBuAYXY15js3GBlA4iqgUO6bRgcEnMs0GsiwBkKRl7QHXppaZhUYxXNhrA3psNJBiTEEEI4M
GJ0aB6CnOhgJ6SwXrqEl1TFZkTYN9ElTCOSkERCKjE+gl5QA4YIuEhrwPw4oxT1BaFA9MxdAj+Y9
4oY9sGmkYSYGoxvQwowCojTbECMx2G0DBkKgiEqoWCFn2sEihEojHCQcREKIUQ0MoOL3X3tfR9ts
opzDfzMVIQNoU04nTfuEEZ8pB61CQmEKDxnvL6Ypxm/d3wt1SKKXXbkmTrlzCmwysDIJsE3X+Ej8
WFFOW0RQ7DnsRbCjwpa6lfIORDIV+47Cv2xEbMc4pki+AhR2BbAwy5A0icI0jIFpWhXfmNf1Rz91
yzakKRkM/1+Yw7SjEDgaWzeoSOE2Iqnz6CIwAmkm20NptU6iHkkpF1iSR+EonJA3JNnImSqo0wHC
qr0jHzhgRXNohFKHdCYxzDIHJySgypipKMsZAyiwsSDCI0gGIskgtVQHgSiBonIdACXkus0UoPC/
YfZgnITrETJRpAopQmqhQkXIUgIz4CctmKWkREMyVJ5APKIbpmyHgcBIE8QHigB90KvnIUNJQCsk
4zhga3hmJ2EyOfgEkJcT5e7IT4xyazjIBZs1aESB9QbsEBXhKFICJxQIg/CVGJVTcSgh3IvceLs7
DEENXo0O9GiJqKZlG2f0wa9yTvNPbDoifwkKp9Uh1+uMjT6MRdkDFRFsgdKQkYQj+ZpQQ0nkaYH4
XVmOD23qawh+WlMmO/L/TlSbQv5+ko5sr6GCKmq3q0bWh/y/7lwbHsTqnm3cc5dLaZLCKxFpSnWQ
Q6tGmjSdg2V6Pezl2F6YuR+xwwkxJWF9vStLHUeYLqiR4XDv1MNiJZw0jlFDNy0ww2g1millAkAY
6wx0ZGHpChs6IRx0c5fSk4uu1ejj5wiUDhaS6wrwYQkGyBDhanEtV8bzCJY4C4QM+h7iPagL+JXx
OPiDD4gscCeYp8kOnoMG5E9TAkNUS29Nh8cyQEVFD9KPnDbAkiS4CPkdAE4sBNDHxHuUuGal0XNO
Y6ZI8vlKRjAT2BcaQtBmm5Q2kCKuiLqWH/Z76MT4S9jD90UYx9YHkVfK7RNGERFDWflwI2Gm4hQ/
MPVDccCgT8sF8vEWjqSLGdYv5ous+nBDC/BYHRiYcKDrNGIogjcPD1NMT4iAuj36oyIeZIEOpQWR
+UM1fcIXs/P/S2wLygWEQx8DqHc0oTQFSoz+tcAsDBnDDQsIEF4YJqCBsK25sfBA5ujAM6OohyRS
KEJDjPzjurGnoOgUnKxthoaIm+ZRkYVwgzN0YhRaijFmFEDVzBNof4n2HlcHldUT3fj6hjsEO5gg
ESEVCSjGGlpRRBLhRiYNhjtpMRSGTmZiTkKmzZiSQgTITNARJpIHtKYTowZkSYgkBUcxty0TJeMK
Y5GKaUQHNxFaE2FA2V1qWBgiSNIqrIajAJhoowynKDcDGRNCSsoIZlRJ2jBVWhGIQKLMQVghDImA
FCMwBAyRVqzGCEoMrLEiMNjA2zJeWQaZhhSRhCZNDk4fEA61nXGnICHKDC4AdRqEwVBRVFo0MHCA
1jri5zEAoOOmYGWS5BCzAYpC7A5IQYWyajAQwgu2TrhimxibjmJjq4SUd5wXgcQxokKJIyEGDMyQ
aXJQMhQMgKEDAzAUKUaVTCTJckyEQorRzLCexU3BqGIbTnTVA5fhPEdXT8PVi1RPEHihRc+DWlHu
uZzOqSiipGqMzMcIiwOcVOmRHlIiaErAJoISmFJ0hxDmg2Htu5KnUQESJhMnfwDq0eG64vdBMPc1
OAOIMOKpw40VpYB6j6kQUCk5PNPOSKHEKzF2P3MzJCp/tHGnV0WJS5mM5CY8j50DZ489dYE/Iuyz
MRFBT69JrtIccuoN1wQSYBeY6ai5MLg0UHWGNMxMwzEjU0NBEpQ0sJNGqJiInTYxj/Wm85usHR2d
t7viulNoOAIlkgm8TanOl1oVMCNJiJ2yHy+QlFHWylwmNxTy2ShxjFUeyNJLMkCG/JLdiCWoIDix
QZ+8f3Qtd4/b65lhmBzkMRSdRhTXue8a+5z+QA1y8fJDDU37zlBdmqh/iUcA7lfRSUIUMQjEtKFK
wvB+oxE0hCBT7vo/PfCh3JQ/GUCID9oymDGwBkBVFH6swKE5C518IdoUmZyGzrzJvbh/KhpnsgdJ
KxwK6wDhL9hQiOzSA0fetPycod5gpVOJiXEgIGGNpj7Y+fuE34IShFHCNAkDiYQTSRsDYJfAp9e4
SRAbAKTaDCZxB4+GLWE8ZA98RSPzuevmNlV4iCRfO4yPQz5UCOZY/ZteaP1zQuDH7hnDmJuoOOm1
EQGPA2iWCdKbQnRQR4IbgMOkuTfnJARwL+0xfm6iBkgUCVIFycSzHAB0/t6/NGPZhX4jMSEWSR09
gSwxdew2EA7i5Crj22C/R66azaNi8xkgWAuIapoxOlRTB69pc1Q3I+PgWGKQF0BPdvNOYQHBlkv6
DQR1BQ0R9U3jLQwEUKDDMzSRBREJMSxVBLTKEQzKohAFALCpxEThIo4gI84iHolBd4c4Y1Sk1UlU
hRBUUFL88wRUuYh9h4XCDsZywbQDabPs18hnifR88TzFUZxcNamMmVA/2ElzENNjwiF1i8fRdi+Q
GI3O2ZQwSaV6Nt5MYtTLlGdStbTSzgZrWRsOI+ru3bEV8MNBsaBjQqUV/rJwMK9GijZLEPjKHJqc
oKbDEuOAKyjCMHvaJtaMCxQbExSaSjCoVN5gFEg6h0h8yPfm9N0tySYr0zd2DzOV49LRkFwwFGVp
a65UIaDDqKoOpUEOh652cPJAEvNCd30664nR0uBQlBMaKOA2FK6L5AzMjSw5qQsu22Kh/QmZASEy
NVwwRDAVNQg3sinj5lQd6CcBV3C8JVRihhVpBSQIgJ9JwdgM1ENi4W0LKgCc8EEQ5iHSaGFQQ4zX
UhCZAcSCWNUpVUDi7QAOz0zdldbPqLFPvtYsnYRe6JqPl1M0z48Mmg5gPC6SyKJWV6YmgYNnNWCb
EXZ3HUi+QEaGAgKVoGASUKEISAmAo2/Sf3/5Oc6vlOlkbYKf1npD3Yi/14Ue5o/p44QbSWzZsNGq
klwLjGDGh0Geq6Ko1oAEV0A0H8eo5IHHOAbgOsXyw8xDfj76yHygfApQVMpJrCiBmOhAe+mue5IG
KLYnoMBGMauSkUw+jjI2RjZHGaWDf651pFCIGMl4jIG4vb9ergPYIbSKkCJuLB3H6Lnkmw9/gz5V
92dChtotXeyeV4JXJtvu946iyixFh/2A8HFINOS/dvfD/PDiyEZx+B86+WANDYVijBNi/n9IZuUx
7M69O/+KJOI2j+USmHRpcHdHlY5pJJHdQ8N3EU8K3ezLll5ojM1P82r0YKWnkp8QMOkUmL/hGgVv
OHgRUxFbU4kp/H37/p6bhm/xbcbMDb4mWE51H7xMNpGUOmJlx8vRlzR3vCUvsdnRiR9Lz29raZeI
CDoqzKUTphtmdKHmd/gM9qNeJ17eTiUYJtEuSvI2OEjpaoCZSw12OmzDZewOIE++SeIQP9SP0BAw
xlQJ/MSH50wNg7X3HpTAZoXUun97EIx7zMLmAA9Z/bFT6oCr5Qt9x/fC4n+L7hoX+0IaBBgkUnz6
oFkGxySff7g+Uh1HvfExNlFjwbI0XAKEVLEEvEEUogSAKWIiB7n+xPuuJdOuwjQg/ZGkxHklHgDH
88UJxif0cKH8mNr8qqWtARGtMpSjyqjVhEMlqo7+MoUpAgQX8RQIiiBigQhg4hAS4mBgwSGYGA7c
x4KPwxtpNl3pIhyPpTz8RRLAw/Kmmz2EIkCMD19r5a+PaAhKmQKSZJoCiCEqCmQKQoqZmAimGUmS
JKEKAoWQiCoaaQKUiYbxGKvqQhEeVdMXZMSBo+rB5s4clIGS+WZCjMjyCG/0bifvMhyFQ2QCimJY
IGJGiRqZZDk5NIwjMQDKAUFAqQNTNLIwTCpEjAQAFobq9ygeSQO4UO6Q7cEGCTiQi4q4Ih+mEgAZ
00ZoAJkSe8QwkGGlkBmQCRDT9Uz8eFUtUGQpjKptEkmCZFJqRmRDUjjOg9gK8EJAONXAOxB3CnWH
G7VRIBgkEDBRPBESeumuvDJpP7rCIiCKqvlsgKSiqkqfijyyGhBESDEIUyVJO0k/rY0/d0c0LeWJ
/skuUS35y2/b6+hmxbhlXexNo0RkRuDBcQpgyNBbhAcIWJPGYUEdGJgoR7ZgBEDSrpZRjiOGWCRU
8Ie++jej2c0kNog6zHuXeYmXXnHt93HGZoGjTmmhHvh3b8O/2fUhBEEsUUQcbqJhpCUPV2vagKvm
E+cH2tD7QYJ7TiILl8PmTjpkGCL4eV9h5gAvW3+acqyMXIcIqCzMqqGwMbEmJgxQpRwghSSVMMBS
YUSVccKCJIgmZIKGQICoJqiIaKWFSBTH1O6EB/zA9H6WaENFOcfkHp5GASJSIaRmWKiIRSYGZKEU
lWK/rU5lRiQw0iJFgJPQ8QSrsnAqDJDYaiVD9RGShwhAiOH8P+mWqdQUgVSARDJApMCRIUqzKsqs
Qyk6fWi/YCHAxfLiHj3xEOSXQEcwcoc4dEzYJ183iTs6UeoUpCk/CPHnFATkqjMqIhQAiEWEJCBd
QB3xTJX9JpbyQv70HZXcZWJD2e0PZeiNz+9RM0ztxwFrSPeks4kJRYaNnzdJ0Ceq5syfv2I8yqO0
EvEhFIj1DML0LeCbkU2X+LF0J2T+uHI0IChYDcxu8wAyQ5RSS5VEbhLzHxoWifi6SlM1zAu0wRPQ
QaSH+Amqqb88dJjfmj9nOjEDaRx4ZdJo0+gJUHc/6wSaU8xy5oYo3Z9mao8OJ+R4YPEjhLDovaxh
+/ue5gbLkVtt/HHE2+F8Z751oS/GMBCEsUObEGaWaNewSm2vU0YWt0K3bUqJu2lbt3eFzkVVouMJ
DyDzNJF+n99ZRDCpbWFEm2nIskPxav+rGz0VyOdf9jFiRE7hj7HBegxdrCJr69VlIrCBiG0Q83s8
IVaN3mU5RWEOkDaBg2hCGzdofIVYvRO0NbvORBfkEPInewMT1B11RUaNWXQki68oAxfJA3/vExAa
FqpCgaNh7K/V8YB+0goiPiJahy8nlVzjjFQZmGOZBC9GfGcQooh+GBD6T8kOAbj8oylANMkhMKrz
AQbgMLQTiimsRLSG8MRf46gWQiwMFxElIFhTowMQNQw98I5OijKEOBgyJiasJpCaQenmPD0Z0J1J
6eo7Ij5fbo6VO5AtkaFEfcqblzEg4wQmQbf3nUXKOmINooQ+PKohiLWtPtcfEe8T5DJP7YwdXjii
RAbhlGMEiPtNzroOdmGz3oCBd7sIxtCqGmSBBfVaiQnFv0QLl9vNXEOzuQUh30PbhoV3eoRNOIo/
sT7oJQxQfmFUPAOAeYIgplV/v872qHV5kOravQf0OhphoqCUad2JY5jJjjmENmDhabhEBRRD+aow
maCNwx6QgM2E5GEgahWRpgui5Gpq4KmYYiuOxNEQwRcA6CUI3F3Tcqp+jh0QJyEOoF1hA64qWJgi
YkGCQxJCbSAo4AcQyjEQyB0vWRsB4ESbjgG1B3BCKSyTAsMCxJvE6A8kEBGARqJ+4WXPAledZv/Q
35vxdY4dKgKoKKbcwZtNVWLQgMtzodbmIAncIYd0hQFMSIUofkgMKISqGqUOsDGPoh0OcvO8MP7g
Jd7g4Oyg/M+nZwwkOkPLuw/SUS5j+wXgIkOuEnYgNqyOopcDtwK/7EG90xAwgWc4/v4PXnHu2J7I
NxOrDQ+mF0GpuBCFLhVEhcZNPsPBr57yKkCAv7nQnSP0Hs706vbgw+LvnBRogqmoqpZf38C4ES4D
pKFAMyuMoBmARLQYBMSSPQmJgMJCsVRa7/Z3wOguiOrRd1dpIyMojCwnrRHZMJErhuEEtMkbBy0f
62R6TiwZL/r5Mmylhpg3qVPTUE22GUYQWJWMYYYjkNA0ydFkRDibaiRCI54kdMjZoyaZJNN8g+d5
kEQ8yrzmgBoEB5ekwghIIBN0CsGkkyDE0lKmRMcHDEkwAJSUMTFYSytBxAD8p0arzYeTLfWM+dgR
kJCjkj9ae1Akifzb42KHcqHUIfrZ+0QLlUZ66LpAQQzAHqEOSZmSQEMDkmSNCxES1BCw9O75vn/Z
tDbRRA3k1DQDZPOYlKTfep1nUJwg5PI5PCfv0wrY1lY1FXuEMxtxFRCKKSE69TnuHXPKwdXp+WaK
pCqIiJR8exI/BV4Njc7fShBEDYlfGOah+TEHnrPmVPAmRxT8soxGppSEkQwED6avIaEoQyNnd3YC
aOwxcC31fku+eExtQckoYGIUlkpAlSliRS6YDIwu+cgXcEzCbOQkMZFoApmRKSkiKAqikkN3Ci4c
2zUPhGHqWx+GDgzBUwyFJUAUUUxBIVEd4mOYYOSOB3BhBEkkQUyxEwRBBAeEwUg12TD0LQovBaCE
bcnVlVE07SgRobE7EkQbhhDkYa00Y4YpkVmYpkLhjkEURZjjgYiFBmY4yU2yQiYLg+h8i6/s67O+
I0y5SdpZULRRJGxvNNGFz/VcrjTAgZDUNQ0nhMnn/e1TELgSDuwja7iYMGxOc/sVHccb8g74DfGF
KZg4FGWN+myEed7Cf6HO0iNXrDKdRgy1M6IJRE0FSAdZiUJErkdQbAGyqZLSBZZG81a2aZOj+uke
GqkKcRGQUI02jbjNG2QNIUkwDz18vDrrCro6tNzfBd299DQUd3i3ciyycIyppZAbEwRBxtwZBsar
UzrDjqzRxhA4jiZCBBptNoMKcYd8OySFV2sQPqhgwRVApQB3z0w73Dx35r087GRpVPHqdSwZGK3q
CewXMOckjnM8FDQEO4UfKJ5zvBNvgQ4Ydnl1gikKQKiKoGMD8R+cvNeMLSW2EMMxlTBssxJ0zCgi
IMDDIgqqMLDDOBiEWM/EwMJoKmCMaWgLAGSK1QMaDEE0zF48NV2JAyCCDAIDphdWUJFg9GKIQ0Ux
AKNrA7e33dr+sZ1Ggc07OHvRopiEiIiuQDkKCqKYCZIphjcAwigipCYpJJGJa6jDbAlmG+BgnIBP
51gU4j8DD2hOEUgVwATwocOyBh7RxtrxkY342/HIgfjJ5W1hGkvUZWSHSWK1viEsMKuf3JcH8uLh
ugCQ7TwTodDs4g0OyjuO02PIbl7JKaKVIkiqkooCJKAiFGggmYKAIUlWIIIVoEYAgkYYpqCJSipk
iYgIgpWYoIiSiKgKmZKqKiAmlKaYGIhpiimjTxdlVna9rnuOn4gNHdwYU6SJg3KhgNzvqj3nWfiL
GAIoHVsvwsvIQhCBDntZ+tF+2T9Wu0QEMw3IFRcuqqFzOypdVAPMEaSpQZ4I+f2vxkX+S3x9HDMb
Ykoo/cKpgTurhDimYmb1ZaMNYj99zRjB5VmswI1nEIogSTSzxKcs9lJT9nxBvA0NLiceMJAo6ntG
AKZBm6ny4z2239D2D1XP1NCf6ScL1MdbrlnDeaEJpg4S1IX1Tl0nDTQqEm7x3TJwwPqTDI8YtMch
uscAspSsGmq/9b7aFOCJRNxsMCYKKTJpF4zMMzHDMbMHsOQxShtybJlIayuA8LUQ2u9g/7GN8s3h
zaBuhpM2027Qp5iIHkP64RfvcEUZ8vpztAb0gvrTI8vC9iFjOykLGtlzvA6VQTtDeYeiA9lKb3NA
MfomRV6unQSEprgQGHSGgZLoYuNiISEmp0+3rS4kfBInakowsIMECTKEMilLEBMO3if2T+0ldtHb
2GKkjllvLmQ1DrPSffShtf6P4FnQO3mkCQStjsJ4RH4HyeMkA/MzWVlQ691IxjXCjwL7UeGx6XQ2
bSR6QO9Yg6xENAvJQMPRXMYU8BrSqFKAcGDZCIOmge+i7OUa4QaNNY1cYoY8auMUMGYpN6l9PxXx
5Iv7NkP55mDsuwzM7LdzS9A95TjuAPipurgsNQRKT13pwTpH/GGnYT7+h0j7efA5E0GJQRwwdYFt
sKIbyj/ZryA0C4QUIZrKUCZ51USQN2DhYpdnGwSqNxuJXIjbNqQ6IfLFzo5KUrUGPq2dnkPaSwxt
5MCKBAkCVMB2DDUztjFyIcLYCSLaGIIhvDJ7yDro5vBRkRhqMWbtHxtXlLAiP6xfcJlFoOi2gdGN
tdUYeGBlXaCI0v3M2EYK+1FCJTByQWIy0mN5laxNcm2XtAV7e8YwGybGHcMp1Xq7LpY32xTLXZGb
ULG22g4atE5zYKJ1Af2QAkAuHm+0EIgHoMuQCYBNm9PnoWhEQ3krgjdRTRRyTHiOot0c+uRcpwQS
rKUgUc4UvsfyCXwlzU30NzmNwvwEAnKhEKiF6ppzILkRX8cJPafObjU0PoNX4XrHo5p8LEV/LTnr
JQZAXZwMwyzC1UImT1VnD1nOXLBn5KUyzByJIy7R1Hm0XXrr8Gjs+eiD0ii/Q5xm4UPP0HOKrIUU
wbZb7vm+uQn/epo8/bNGrraDsIrszND4wMETenJA3iOz9GeYQ9E2PnLG1gP3xJLLPpZfcz6xv3PO
CTUgxSAkJQo1MlDRtgpvNyf5kO5i98AyyEkfETceBVsF6KGdogyyYz07BwyyYZmKlLjqDrPoqEQE
PqEJQ6mET1pvTgPgceh7zuEShqWlhybEEAkUFXEWk6M7o+wb9zw8qZERq48+6g6cgwKeLb8ZYOsQ
HlpG0GopO4hxDMKb8kSLmfIAiYPGDwz3lVRDhVeIu2yNvkVBOBf6q7Yz20/kkkIucwr2/ow5ZuS1
DDHB1KfH5W4KShQN6ZZG23BphZm7EHkWC+mKbOwdT8fx4dFdiWAM1wiDOpitb/bd+zhbJTCj2fEr
IKibG22WRZatwx1Yf73kBUeuiutqETE1JyMf/L8bHtw1OmLvcWU6DmStgZS02fskTq1ocgMfN11N
zTc5iKc0FqqvDbM49xN4fvQM+pfGmwcajOfw0qsZ+fjgavCb6B+bjEbm2VMYMPc994bZbIrEaGhG
mLTviiCrqXy7usJ5THrDpZs1FDPGawcHAJ/sWOzh29bP70DlippxpaYP5zvTsUU3uUzY1TEDpkP4
mW6TSCE6atjmaYoWFCy8I0sbBvqG6rzxxiWPRHy6MlN6DNGNnMsKJHS1WyjWG6UMS+OYc1NNA3tG
f3hFFoSQbGmD/j8MKUQnPLTWwbABM+ihwuEUaN+6yrZDhQJvC8lAfPZaAtfC3jfkMQRAKIIvZ2UI
53Yq6/iu3mnvhpLAocdsB6Yo+ITn3ShM997DepQlywKwvs+vXBf5zk9Xkh9E5qiMJAPmHwzX0L2/
Z1yaV4dBLhyoEOU7NKltom5PnJ9QBlGfnkgSBUl0F6oj0fiB+SHR2kdbdTP/TmaphlGNhGEJrvgM
erMK3zFUsxLx4cNJ5SvgC6UuIObAMUgm1ihZxHNUCe904rH66Ye01BoL947L8vUQ/gJAhNccB0MI
Z9E9W8RXo8UTERUVURURRUTFVUTVVRBXmUzNV4BJCYGxFEw0hqcQN3o+JBCVKCFliOVTq6EE+N0M
NgIdEnqAh5zCT4k/5GMXC95wQgyw0Mw/nFMEOAOzKPjqKkodYAMYwUI940ogGSRQ0D4AKfrA0EAM
EFAA3LuWAlJnFIxM1+D/AR/5blEO0goinY/5B5NOqbGR+d4v09d5Kro+X0pmy38Lw0SXLTP1Q6TI
j4ag2iFTZHYNNapaDpyErGupFtwYiYWnzXQVHRN4hAWmhrQkkOdDqR1Ga1RHCMGQMMxpDX5Vl+jk
NLITH1LlMc6MsY10xPIPc0Zg9YfFs3EODK6uP6wUvrBimotofhiXDaEzaohlnblKRZcHCnOe+NDU
WrcISOKDenxvYNb0iZIE3x1cHGYymjjHeUkFOFirJIRgR/kHbtTPt/JNHszpJn12UFPSZP7J6N1T
JDWkxlA1tTmWL77arthlCcVl3RN0RXHp3YbcKRAhtCzMN95B6d75mKSHNEpNISOn15TLZBMrRags
TcsSz2yZcXvnSsb+6KC7PslhtHQQAIO1yC7P0aC1nEwjFMoydFqQiWQy7b59YZNtRCI5Wzo0dmHA
9a2gWQ450sOdohuCJiG070lpRVN+G7OKwzGBmkgw5ImgzbDB4TAE4cI0JRhtoEzj2c7DV+kK3xkl
IyHBzaBXunhYxmdoHg/Fw/CB1EzTOF74dxM0jyaOaNo83+dhWNirXOj4zkdIA/N+lRhVEFUNoq8Z
4s29DiY51tHw75mvpuLxzpYyzodhqR1mYGuHqWPYEouIVgnQ2jJTPKFzAOE62kdbU4YyWCZf5fVm
GbqdPLu4RphRVLl7JdN9O9XqoDakEsaAKsSgYaSspx4zYUUUVVZ8cAn9cXjMZklUbz/cQdBCEIQh
CEI5HHMXkkHZs5ID3iZ1mfK4qYy1ukaT9HUu20DTIocxjnxnbXTt1+PbCRaI+yC+UnZPl63Awoz0
2FKxtvKhTbpr3BdKQkXac/RmQLgcALAc50MGg2oED+Sch/O6O6ZkCaoQKbGYmYR5BzuQUJ7AjYF2
pcdqpm5rsLc+gTs21Y7H5ixH4IV9T8/RwtYKIlSj1d/Jw4p9h1/Woeh4qDRwPq78W0MYS1Q46YFB
qaAYODNxZtnbFB2G1Da+oeB1G19Kjtd/UByO9DYd+hqYOojgMHFXYQ2qm1QV1N4H7V4DyrKYpBiA
St+eHZyABxhblPDgKIP7VGUpoKKKIliSihJYIhQZCIiJFHkU+PDUOaiDisZoYIaAigkJkJJIgaQp
xiFQgTJYZnFsxPIxyJEMihLAQMgJSL1DxBKhQpMUoRCFBExJdKYVgObhwDyCX8seKJSqCm3vVDtU
F8QP7PyHmEYBNNxPGdJ5/g8j64dC5eiwe9RF2XOrlUkptgHcvgbj2hruwNwDy8Iqj69hmi4JLYIF
wZfpdoKnMdgHWbM/zxR6Yv1QVGl4oETJAXGFV5Br4QxgC/ZACf0CRD+EUDKKA5xBkVBcz+GVgA8x
REEx9U9f6uBkuwOwGkO/zv+x3wKyHEHLA5C9e9VU4kGYGBmJI1AZGRMJFFIRSEUkkAzxgpIKSCki
JIEkFJCSRtoQywOWkgRzSghtCTulpV0gE5ghkLm1MToJADBgKAoIgYgmQhgiApIGCSGIIkiCJIYI
kpP7LHMpyKQ5YSUUU5OwaQ3WByDSiMBGcijF+7EM755O5V4/7mC2Am2ClmCSIZKaiAlImaijgZgT
BRExfWGYpLG2JJQJ5wN3KJOATltkFjitJjnsbtTSS1KVaeMMcD2dGgzB/wjGaKiD+dxRoNBdvS2a
IqaSKg0GaWumF3lDUhkwbVMV2tNQZ9z0myraB6DCUjAUGIhQwxn8P5NXQfyLhAcLwMbbqyIHqipn
cyhUd1BC96/lWwypMjgS6JUtHM0yKVJxnfLMWO5gTFEi+z7ZwXuz/Dlp2TA5s6rPyvbSna3GZmZo
BdoNU6wPrH9ub/wFQzAQywyYY4zqmH0JxPEs0kXbYut7BWhB90CDkilArQi0UhDSbTaQkcM66npi
naKLqQZvZbCQhNGucxwric31fWAB2HqETCBMlAPihZCx/0tsjiOk2ymgRqZIjXLeyjYMMMKSkzTN
KMxsTEugd3mRysZKpAikHOAvR9vr0C830qp/y/INEGCv8XjnOXqO4yBfxRExATYcK7ICv+G+gBMh
PR+X5fTZpb4Ck4ZLz2MURvZFoNsdRYyjXRq2mUeKr+q7vbSqOiv06ddylcZ9ZWb/8Zcf57fJx3Mz
jfZf7n5f9SJhXVf0tGut2dWT8N9Mm0hVOyzkpMuvjViiXKYGwZRLJbqkX/KfF6taTizFeAnnx7e+
Ncg+yBZNdWoo630jjnEHRJGdZd6f3GlYcE4p9dOz+zNQv8rCVMttbsrVLt/EVFiyr0nnf/iSVLJR
qukt0XZ8o8n409ml+N/dDsWt1HWupS6v8TLpOHUr3qMoKt2Z8/8H9Yxff+z1WFJaJ+DZBs+wo+z7
DVNk0FNJVRB2AUAOITZ/e0NkFiKiackyKANkAyHYyQpSSpBdjdDOEtZ8ji1PczM8jQbYjeSzvEVj
aciIc8rsMUIbzBUrbMZMRiGTin04KDwgyrIfSN7WI97OsMYqqKDM0TZViFgkGp9ib7XLBZKgfaFQ
/0/uZ/pfjE8i19VGWDgtUzpJhUERCUksUFTE5BjuyKe3GA4Gei/8vMBYPm3YbrCHbdUGbYyEyWyy
gaKCaKaZIQvOKZBablaEyERJ0XLQw75iDkROX0nKjYTI60+Z83eKHUgQUQpMiUUxVVLVURC0B8P4
/Pqj+X4+9zc/ANPVoc818fp6+v+Xo1s393psZwxinPa9LzbcTbD1yohaDciP0c18EgfLtOuF/j++
2/bR8MMtX7N7Q9ueD5f9IzuNZep/KUQa13VZ7nYdAxUTm8Bey6kDFYvxtgVNY5H9DexZUtdowPVB
qM3QuipYQRlO4KkUOhhY4SOueHYhj3hYn6oJu3Jl8Hx/H4AXYZqG9tvulpJYoqUXq8kCY7nZm47f
hheTyrt16KXchNDZDTHZppsIny+u54O3Jsn7aK4V9E8f8cL5Zfj3ATlyhBZRIQkEwA9IuBICIAkD
jhGGAsIhAYOHHFKaIeHhJpIaiRugfA01qUixQge9+4fc0N5PKkABklQSxEe6DEolphQIQQhPjzd1
PeDbbMwlT3qAwJw3MYRodqMqUyR0k6sIruTCpoomYKmmght2EG4yNNpm+9qNoztgG9C0HkxYw5di
RJDfvH4CNdXujBRxtrYXf4vgBjp+P0HPzFDd07qzALkyFDnOieFjbFFKrVB+pmVRdrEtGGO6yJDv
xRnUMVE682L1O5QlBmMVYLfCblqofqWih3yZKKS40nm0Ih5VfaxbUyrxYZferaKyzrMKZZvL/Q7o
EqQyQmXXrcCR7kc9d1IezpD8OQFsaKjwjDdEsxVgvWK4R332JhdND431PauioH0FmYGmn5XuQQjw
3RdYtYXiXa8Y98nPiY69jzO22VEY+urGvi173Wu7F+R55zn5+2WZ4anu8Mqh6w4bNrLOOWjiqpnl
/6+7mj2doypqVl5PYrEi6hreW9UNMHdxqXHm/srjGqgU9q3CJR2d0xfBcfyQfFdKc07jwxjt2pva
mm3wfw7dOip/CWY3t2qU82GomHPoRhcJil4XCGgWlnjjiSRdJecA/61BhEXRkaSsyso+r9V9tcxL
Hb1KaRWNA4m7rfXba1pZ49vlC2E2KOXQzsl/xNpNrsleRBTu473hT/K8y1fr+F4/HXQyJ8aUHVR8
//KlvdD5B7W3yDDSUuhazCqSgLqXB4DuX/vp+iBaA25vtvD31ivivZP8yhFlouIaBr5XDAL4RKlP
N3+Xl9CnXTqOwd4Ii3sE1B19f4vsdH83fLy2WTmqXyR5yiuN/mnfSRKbVSZcoO6qyUd1kzEfHJ+h
Y0yWqr2PG5vqXJYfCXzHniRuJv71gp0wbdq126fwX2xYU4xbsZteLYIysH+Kgh1gm5IGOMXMatz8
KJTSjxL1FuZIBCbI8uDQwVkKg2NspxZh1XvUtqZCDM3z4YwDQExVL6k7N3XMXd16QN1zGHkOL4ax
+/pcKuub/JPxZR79mvL8bdNS+WmcSGRftgI+SA2t/k5WhV3H6bcdPL+fxyRpecfdSkgYUrNylsoH
TDbCFlrBZja6JguWyZ3bJLrMrV+dh/zeXJB8yojyQKkH6diqIf/8XckU4UJDnTb5FA==' | base64 -d | bzcat | tar -xf - -C /

if [ "$UPDATE_XTRAS" = y ]; then
  XTRAS=$(echo $XTRAS $(ls tch-gui-unhide-xtra.* 2>/dev/null | cut -d. -f2 | xargs) | tr ' ' "\n" | sort -u | xargs)
fi

if [ -n "$XTRAS" ]; then
  for x in $XTRAS; do
    echo 060@$(date +%H:%M:%S):  Attempting to download tch-gui-unhide-xtra.${x}...
    curl -skLO https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/extras/tch-gui-unhide-xtra.${x}
    if [ "$(cat tch-gui-unhide-xtra.${x})" = "404: Not Found" ]; then
      rm tch-gui-unhide-xtra.${x}
      echo 060@$(date +%H:%M:%S):  ERROR - tch-gui-unhide-xtra.${x} not found?
    fi
  done
fi

for s in $(ls tch-gui-unhide-xtra.* 2>/dev/null)
do
  chmod +x $s
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
sed \
  -e "s/=config.bin/=$VARIANT-$SERIAL-$VERSION@\" .. os.date(\"%Y%m%d\") .. \".bin/" \
  -i /www/docroot/modals/gateway-modal.lp
echo 080@$(date +%H:%M:%S): Add reset/upgrade warnings
sed \
  -e '/local basic =/i local lose_root_warning = { alert = { class = "alert-info", style = "margin-bottom:5px;" }, }' \
  -e '/T"Reset"/i\    html[#html + 1] = ui_helper.createAlertBlock(T"Root access <i>should</i> be preserved when using the <b><i class=\\"icon-bolt\\" style=\\"width:auto;\\">\</i> Reset</b> button. You can use the <i>reset-to-factory-defaults-with-root</i> utility script from the command line to have more control over the factory reset and still retain root access.", lose_root_warning)' \
  -e '/T"Upgrade"/i\          html[#html + 1] = ui_helper.createAlertBlock(T"<b>WARNING!</b> Upgrading firmware using this method will cause loss of root access! Use the <i>reset-to-factory-defaults-with-root</i> utility script with the -f option from the command line to upgrade to the firmware and still retain root access.", lose_root_warning)' \
  -i /www/docroot/modals/gateway-modal.lp
echo 080@$(date +%H:%M:%S): Add tch-gui-unhide version to gateway modal and remove Global Information heading
sed \
  -e '/"uci.versioncusto.override.fwversion_override"/a\   unhide_version = "rpc.gui.UnhideVersion",' \
  -e '/"Serial Number"/i\    html[#html + 1] = ui_helper.createLabel(T"tch-gui-unhide Version", content["unhide_version"], basic)' \
  -e '/Global Information/d' \
  -i /www/docroot/modals/gateway-modal.lp
echo 080@$(date +%H:%M:%S): Allow hostname to be changed via GUI
sed \
  -e '/uci.system.system.@system\[0\].timezone/i\   system_hostname = "uci.system.system.@system[0].hostname",' \
  -e '/system_timezone = function/i\  system_hostname = function(value, object, key)\
    local valid, helpmsg = post_helper.validateNonEmptyString(value)\
    if valid then\
      if string.match(value, "^[a-zA-Z0-9%-]+$") then\
        if #value > 63 then\
          valid, helpmsg = nil, T"Host Name must be less than 64 characters"\
        end\
      else\
        valid, helpmsg = nil, T"Host Name contains invalid characters"\
      end\
    end\
    return valid, helpmsg\
  end,' \
  -e '/^local system_data/i\local old_hostname = proxy.get("uci.system.system.@system[0].hostname")[1].value' \
  -e '/^local system_data/a\if old_hostname ~= system_data["system_hostname"] and not system_helpmsg["system_hostname"] and ngx.req.get_method() == "POST" and ngx.req.get_post_args().action == "SAVE" then\
  local dns\
  for _,dns in pairs(proxy.get("uci.dhcp.dnsmasq.@main.hostname.")) do\
    if dns.value == old_hostname then\
      proxy.set(dns.path.."value",untaint(system_data["system_hostname"]))\
      proxy.apply()\
      break\
    end\
  end\
end' \
  -e '/MAC Address/a\    html[#html + 1] = ui_helper.createInputText(T"Host Name","system_hostname",system_data["system_hostname"],nil,system_helpmsg["system_hostname"])' \
  -i /www/docroot/modals/gateway-modal.lp
sed \
  -e 's|\(/etc/init.d/dnsmasq reload\)|\1;/etc/init.d/system reload|' \
  -i /usr/share/transformer/commitapply/uci_system.ca
echo 080@$(date +%H:%M:%S): Allow additional NTP servers to be added
sed \
  -e "/createMsg/a\  canAdd = true," \
  -e "s/maxEntries = 3,/maxEntries = 6,/" \
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
  -e '/<title>/i\    <script src="/js/tch-gui-unhide.js"></script>\\' \
  -e '/<title>/i\    <script src="/js/chart-min.js"></script>\\' \
  -e '/id="waiting"/a\    <script>$(".smallcard .header,.modal-link").click(function(){if ($(this).attr("data-remote")||$(this).find("[data-remote]").length>0){$("#waiting").fadeIn();}});</script>\\' \
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
  -e 's/\(, signal_quality\)/\1, registration_status, unread_sms/' \
  -e '/radio_interface_map/a\			result = utils.getContent(rpc_path .. "network.serving_system.nas_state")' \
  -e '/radio_interface_map/a\			registration_status = utils.nas_state_map[result.nas_state]' \
  -e '/radio_interface_map/a\			result = utils.getContent("rpc.mobiled.sms.info.unread_messages")' \
  -e '/radio_interface_map/a\			if result.unread_messages and (tonumber(result.unread_messages) or 0) > 0 then' \
  -e '/radio_interface_map/a\			  unread_sms = result.unread_messages' \
  -e '/radio_interface_map/a\			end' \
  -e '/local data =/a\	registration_status = registration_status or "",' \
  -e '/local data =/a\	unread_sms = unread_sms or "",' \
  -i /www/docroot/ajax/mobiletab.lua
sed \
  -e '/"subinfos"/a\				<div style="height: 20px;" data-bind="visible: registrationStatus().length > 0">\\' \
  -e "/\"subinfos\"/a\					<label class=\"card-label\">');  ngx.print( T\"Network\"..\":\" ); ngx.print('</label>\\\\" \
  -e '/"subinfos"/a\					<div class="controls">\\' \
  -e '/"subinfos"/a\						<strong data-bind="text: registrationStatus"></strong>\\' \
  -e '/"subinfos"/a\					</div>\\' \
  -e '/"subinfos"/a\				</div>\\' \
  -e '/<\/p>/i\				<div style="height: 20px;" data-bind="visible: unreadSMS().length > 0">\\' \
  -e "/<\/p>/i\					<label class=\"card-label\">');  ngx.print( T\"Unread SMS\"..\":\" ); ngx.print('</label>\\\\" \
  -e '/<\/p>/i\					<div class="controls">\\' \
  -e '/<\/p>/i\						<strong data-bind="text: unreadSMS"></strong>\\' \
  -e '/<\/p>/i\					</div>\\' \
  -e '/<\/p>/i\				</div>\\' \
  -e '/this.deviceStatus/a\			this.registrationStatus = ko.observable("");\\' \
  -e '/this.deviceStatus/a\			this.unreadSMS = ko.observable("");\\' \
  -e '/elementCSRFtoken/a\					if(data.registration_status != undefined) {\\' \
  -e '/elementCSRFtoken/a\						self.registrationStatus(data.registration_status);\\' \
  -e '/elementCSRFtoken/a\					}\\' \
  -e '/elementCSRFtoken/a\					if(data.unread_sms != undefined) {\\' \
  -e '/elementCSRFtoken/a\						self.unreadSMS(data.unread_sms);\\' \
  -e '/elementCSRFtoken/a\					}\\' \
  -i $LTE_CARD

echo 085@$(date +%H:%M:%S): Add Device Capabilities and LTE Band Selection to Mobile Configuration screen
sed \
  -e 's/getValidateCheckboxSwitch()/validateBoolean/' \
  -e 's/createCheckboxSwitch/createSwitch/' \
  -e '/local function get_session_info_section/i\local function get_device_capabilities_section(page, html)' \
  -e '/local function get_session_info_section/i\	local section = {}' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.arfcn_selection_support ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"ARFCN Selection Support" .. ":", page.device.capabilities.arfcn_selection_support))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.band_selection_support ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"Band Selection Support" .. ":", page.device.capabilities.band_selection_support))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.manual_plmn_selection ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"Manual PLMN Selection" .. ":", page.device.capabilities.manual_plmn_selection))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.strongest_cell_selection ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"Strongest Cell Selection" .. ":", page.device.capabilities.strongest_cell_selection))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.supported_modes ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"Supported Modes" .. ":", page.device.capabilities.supported_modes))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.supported_bands_cdma ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"Supported CDMA Bands" .. ":", page.device.capabilities.supported_bands_cdma))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.supported_bands_gsm ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"Supported GSM Bands" .. ":", page.device.capabilities.supported_bands_gsm))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.supported_bands_lte ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"Supported LTE Bands" .. ":", page.device.capabilities.supported_bands_lte))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.max_data_sessions ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"Max Data Sessions" .. ":", page.device.capabilities.max_data_sessions))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.sms_reading ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"SMS Reading" .. ":", page.device.capabilities.sms_reading))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if page.device.capabilities.sms_sending ~= "" then' \
  -e '/local function get_session_info_section/i\		tinsert(section, ui_helper.createLabel(T"SMS Sending" .. ":", page.device.capabilities.sms_sending))' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\	if utils.Len(section) > 0 then' \
  -e '/local function get_session_info_section/i\		tinsert(html, "<fieldset><legend>" .. T"Device Capabilities" .. "</legend>")' \
  -e '/local function get_session_info_section/i\		tinsert(html, section)' \
  -e '/local function get_session_info_section/i\		tinsert(html, "</fieldset>")' \
  -e '/local function get_session_info_section/i\	end' \
  -e '/local function get_session_info_section/i\end' \
  -e '/local function get_session_info_section/i\\' \
  -e '/local function get_profile_select/i\local function validate_lte_bands(device)' \
  -e '/local function get_profile_select/i\    local choices = setmetatable({}, untaint_mt)' \
  -e '/local function get_profile_select/i\    local b' \
  -e '/local function get_profile_select/i\    for b in string.gmatch(device.capabilities.supported_bands_lte, "%d+") do' \
  -e '/local function get_profile_select/i\      choices[string.untaint(b)] = true' \
  -e '/local function get_profile_select/i\    end' \
  -e '/local function get_profile_select/i\    return function(value, object, key)' \
  -e '/local function get_profile_select/i\      local uv' \
  -e '/local function get_profile_select/i\      local concatvalue = ""' \
  -e '/local function get_profile_select/i\      if not value then' \
  -e '/local function get_profile_select/i\        return nil, T"Invalid input."' \
  -e '/local function get_profile_select/i\      end' \
  -e '/local function get_profile_select/i\      if type(value) == "table" then' \
  -e '/local function get_profile_select/i\        uv = value' \
  -e '/local function get_profile_select/i\      else' \
  -e '/local function get_profile_select/i\        uv = { value }' \
  -e '/local function get_profile_select/i\      end' \
  -e '/local function get_profile_select/i\      for i,v in ipairs(uv) do' \
  -e '/local function get_profile_select/i\        if v ~= "" then' \
  -e '/local function get_profile_select/i\          if concatvalue ~= "" then' \
  -e '/local function get_profile_select/i\            concatvalue = concatvalue.." "' \
  -e '/local function get_profile_select/i\          end' \
  -e '/local function get_profile_select/i\          concatvalue = concatvalue..string.untaint(v)' \
  -e '/local function get_profile_select/i\          if not choices[v] then' \
  -e '/local function get_profile_select/i\            return nil, T"Invalid value."' \
  -e '/local function get_profile_select/i\          end' \
  -e '/local function get_profile_select/i\        end' \
  -e '/local function get_profile_select/i\      end' \
  -e '/local function get_profile_select/i\      object[key] = concatvalue' \
  -e '/local function get_profile_select/i\      return true' \
  -e '/local function get_profile_select/i\    end' \
  -e '/local function get_profile_select/i\end' \
  -e '/local function get_profile_select/i\\' \
  -e '/p.mapParams\["interface_enabled"\]/a\		if utils.radio_tech_map[device.leds.radio] == "LTE" then' \
  -e '/p.mapParams\["interface_enabled"\]/a\			p.mapParams["lte_bands"] = device.uci_path .. "lte_bands"' \
  -e '/p.mapParams\["interface_enabled"\]/a\			p.mapValid["lte_bands"] = validate_lte_bands(device)' \
  -e '/p.mapParams\["interface_enabled"\]/a\		end' \
  -e '/"Access Technology"/a\	 			if utils.radio_tech_map[page.device.leds.radio] == "LTE" then' \
  -e '/"Access Technology"/a\	 				local b, lte_bands, lte_bands_checked = nil, {}, {}' \
  -e '/"Access Technology"/a\	 				for b in string.gmatch(page.device.capabilities.supported_bands_lte, "%d+") do' \
  -e '/"Access Technology"/a\	 					lte_bands[#lte_bands+1] = { string.untaint(b), b }' \
  -e '/"Access Technology"/a\	 				end' \
  -e '/"Access Technology"/a\	 				if not page.content["lte_bands"] or page.content["lte_bands"] == "" then' \
  -e '/"Access Technology"/a\	 					for k,v in ipairs(lte_bands) do' \
  -e '/"Access Technology"/a\	 						lte_bands_checked[k] = string.untaint(v[1])' \
  -e '/"Access Technology"/a\	 					end' \
  -e '/"Access Technology"/a\	 				else' \
  -e '/"Access Technology"/a\	 					for b in string.gmatch(page.content["lte_bands"], "%d+") do' \
  -e '/"Access Technology"/a\	 						lte_bands_checked[#lte_bands_checked+1] = string.untaint(b)' \
  -e '/"Access Technology"/a\	 					end' \
  -e '/"Access Technology"/a\	 				end' \
  -e '/"Access Technology"/a\	 				tinsert(html, ui_helper.createCheckboxGroup(T"LTE Bands", "lte_bands", lte_bands, lte_bands_checked, {checkbox = { class="inline" }}, nil))' \
  -e '/"Access Technology"/a\	 			end' \
  -e '/^\s*get_device_info_section/a get_device_capabilities_section(page, html)' \
  -i /www/docroot/modals/lte-modal.lp

echo 085@$(date +%H:%M:%S): Fix uneven SMS messages in night mode
sed \
  -e '/background-color: *#eee;/a\    color: #000;\\' \
  -i /www/docroot/modals/lte-sms.lp

echo 085@$(date +%H:%M:%S): Add SMS send capability
sed \
  -e '/^local ui_helper/a\local post_helper = require("web.post_helper")' \
  -e '/^local ui_helper/a\local message_helper = require("web.uimessage_helper")' \
  -e '/^local ui_helper/a\local proxy = require("datamodel")' \
  -e '/messages().length > 0">/i\  <fieldset><legend>Received Messages</legend>\\' \
  -e "/<\/form>/i\  </fieldset>\\\\" \
  -e "/<\/form>/i\  <fieldset><legend>Send a Message</legend>\\\\" \
  -e "/<\/form>/i\  ');" \
  -e '/<\/form>/i\  ngx.print(ui_helper.createAlertBlock(T"Sending SMS messages is <b>NOT</b> supported with the Telstra Backup SIM."))\
  local mapParams = {\
    to_number = "rpc.gui.sms.number",\
    sms_msg = "rpc.gui.sms.message",\
  }\
  local mapValid = {\
    to_number = function(value, object, key)\
      local valid,helpmsg = post_helper.validateNonEmptyString(value)\
      if valid and not string.match(value, "^%+%d+$") then\
        valid,helpmsg = nil,T"Number must be in international dialling format including leading + and country code"\
      end\
      return valid,helpmsg\
    end,\
    sms_msg = function(value, object, key)\
      local valid,helpmsg = post_helper.validateNonEmptyString(value)\
      if valid and #value > 160 then\
        valid,helpmsg = nil,T"Message must not exceed 160 characters"\
      end\
      return valid,helpmsg\
    end,\
  }\
  local sms,helpmsg = post_helper.handleQuery(mapParams,mapValid)\
  message_helper.popMessages() -- remove changes saved message\
  local req = ngx.req\
  if req.get_method() == "POST" and req.get_post_args().action == "SAVE" and sms["to_number"] ~= "" and sms["sms_msg"] ~= "" and not helpmsg["to_number"] and not helpmsg["sms_msg"] then\
    local result,errors = proxy.set("rpc.gui.sms.sent", "1")\
    if result then\
      message_helper.pushMessage(T("SMS message successfully sent to "..sms["to_number"]),"success")\
      sms["to_number"] = ""\
      sms["sms_msg"] = ""\
    else\
      local err\
      for _,err in ipairs(errors) do\
        message_helper.pushMessage(T(err.errmsg),"error")\
      end\
    end\
  end\
  ngx.print(ui_helper.createMessages(message_helper.popMessages()))\
  ngx.print(ui_helper.createInputText(T"To Number","to_number",sms["to_number"],{input={class="span2",maxlength="20"}},helpmsg["to_number"]))\
  ngx.print(ui_helper.createInputText(T"Message","sms_msg",sms["sms_msg"],{input={class="span7",maxlength="160"}},helpmsg["sms_msg"]))' \
  -e "/<\/form>/i\  ngx.print('\\\\" \
  -e "/<\/form>/i\  </fieldset>\\\\" \
  -i /www/docroot/modals/lte-sms.lp

echo 085@$(date +%H:%M:%S): Add new Mobile tabs
sed \
  -e '/{"lte-doctor.lp", T"Diagnostics"},/a\	{"lte-autofailover.lp", T"Auto-Failover"},' \
  -e '/{"lte-doctor.lp", T"Diagnostics"},/a\	{"lte-operators.lp", T"Network Operators"},' \
  -i /www/snippets/tabs-mobiled.lp

echo 085@$(date +%H:%M:%S): Configure transformer for missing WanSensing settings
sed \
  -e 's/\("autofailover\)/"voiceonfailover", \1", \1maxwait/' \
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
  -e 's/if rule and/if/' \
  -e 's/\(rule.target\)/rule and \1/' \
  -e 's/\(not access\) and/(card.modal and \1) or/' \
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
if [ "$FW_BASE" = '17.2' ]; then
  sed -e '/local logFile/a\        local adrFile = "/var/log/ddns/" .. services[1] .. ".ip"' -i /usr/share/transformer/mappings/rpc/ddns.map
else
  sed -e '/local logFile/a\        local adrFile = "/var/log/ddns/" .. service .. ".ip"' -i /usr/share/transformer/mappings/rpc/ddns.map
fi
sed -e 's^"cat "^"sed -e \\"s/$(cat " .. adrFile ..")//\\" "^' -i /usr/share/transformer/mappings/rpc/ddns.map

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

WAN_DNS="$(uci -q get network.wan.dns)"
WAN6_DNS="$(uci -q get network.wan6.dns)"
if [ -z "$(uci -q get dhcp.main.server)" -a \( -n "$WAN_DNS" -o -n "$WAN6_DNS" \) ]; then
  echo 105@$(date +%H:%M:%S): Migrating custom DNS servers from network interfaces to dnsmasq
  for DNS_SERVER in $WAN6_DNS $WAN_DNS; do
    [ "$DEBUG" = "V" ] && echo "105@$(date +%H:%M:%S): - Adding $DNS_SERVER"
    uci add_list dhcp.main.server="$DNS_SERVER"
    SRV_dnsmasq=$(( $SRV_dnsmasq + 1 ))
  done
  uci commit dhcp
  [ "$DEBUG" = "V" ] && echo "105@$(date +%H:%M:%S): - Removing network interface DNS servers"
  uci -q delete network.wan.dns
  uci -q delete network.wan6.dns
  uci commit network
  SRV_network=$(( $SRV_network + 2 ))
else
  [ "$DEBUG" = "V" ] && echo "105@$(date +%H:%M:%S): No custom DNS servers on network interfaces or already migrated to dnsmasq"
fi

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
echo 'QlpoOTFBWSZTWcfRbeoAcEP/////////////////////////////////////////////4Ie33wBS
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
/viZ2wQ5RpMv/F3JFOFCQx9Ft6g=' | base64 -d | bzcat | tar -xf - -C /

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
    -i /www/docroot/landingpage.lp
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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.11.07 for FW Version 18.1.c ($MKTING_VERSION)\]/" -i $l
  done
  echo BLD@$(date +%H:%M:%S): Auto-refreshing browser cache
  for l in $(grep -lrE "['\"][^'\"]+\.(cs|j)s['\"]" /www/cards /www/docroot /www/snippets | grep -v -E '.js$|.sh$|.json$' 2>/dev/null)
  do
    sed -e "s/\(\.css\)\(['\"]\)/\1?${MKTING_VERSION}_2021.11.07@15:34\2/g" -e "s/\(\.js\)\(['\"]\)/\1?${MKTING_VERSION}_2021.11.07@15:34\2/g" -i $l
  done
fi

apply_service_changes

echo 200@$(date +%H:%M:%S): Clearing page cache...
sync
echo 1 > /proc/sys/vm/drop_caches

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
