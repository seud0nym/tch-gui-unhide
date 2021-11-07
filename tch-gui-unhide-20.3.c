#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.11.07
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
echo 'QlpoOTFBWSZTWXUr70sBkJx/////VWP///////////////8QXK0Yv6KQQplesaRkFLU4YqJ7lH3j
bA3yClDege26bNX07rU+g19RB9BPffe9V9aSt999753WfW+ucAfNtCltroew87yXt6D6+l9qVWY1
trRn2vrze9QPo9xlaNSoKahx3fedjp9e9p7bApVVaydt33HvsPd894TNvttH277vUmRtA0pPt26X
Ud9qfWhQAGfHoSFUW7gPvZ93q7yEn2i23fRdi3ve+75L3ri67jofXusAbTBpetcxe5uoRpp6bmDI
uxqdNCgA0AD730E4HqlAPoBresHR95294vp9txT2DPrQt3NBZj7m29zyTwV3vW17d11y2GjvV3Cc
aZvX2+558rE89V7J9ffGmolpbR5Z3Pt7xp7YCn3ts3W8d17bYAB5dGqo2sGqvW+73c7nU7775e17
Z2jKHOos2X2777UAaA0DyHo6Au3tvfA7voA2zvTAB6H29wA+h0+7B2wAB0AAAAcO499gB9231TnD
0AAHD6dNBRvPAb2AACQOtA7YDRXeB7vUbRPffCx7d7Pcabnen1u+5uzL10Buz6vBhX209VuNoYtl
CVKZVY+5yFEShH2AADQJB67WDR1WsbGigaAn1vddt99fbz7pRr27bvis1D7c9nd8wveXvHvvvetb
zdnre64I2thdgH3MoPHbQVPbQKG+21Efe92ye3193jo4ZprVC2VubPXdcDz22+98rpstbU17t2PQ
ubsrKWdzkddzriobUrBvt3Wt67iX3t5e6u3Pe4+8N4N8z73EF2KHRr19vD7e+gAG7dBS6sL2G9ee
9pdbzdnK5TePd6HsdK0OgBXesx67ru27axve7N7t6D6A0oAAPeX1zt9Yr2LruM7zQXfdbfYzud8j
uxD22u7pT27vIr6qtA61p27429a7gtufXXabZ21H3sd9nd2+89z5dyGtfeM9Lvde8FnObFMqFHvd
y231KAPtzV7X2O9nWT1q97nfdB0+2IL7tZtV893edg9D100Dou8ehfdW61hbVDe+97133h94Hxuj
rgHO8n3t4Pspz30d321dXtp31ta2ZRvtLdlu9d3186PrXu+93vvb7O+pz2a633mNB3gJo2y7nW7O
tt2cNscQd3088orQANLSr7sOyWy93O+7nW+2oegDrpN43E32c9HndmqIIpz7O++6rZye+7jYXQHO
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
RQ//4u5IpwoSDqV96WA=' | base64 -d | bzcat | tar -xf - -C /

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

echo 101@$(date +%H:%M:%S): Fix WoL configuration
for f in /www/cards/007_wanservices.lp /www/docroot/modals/wanservices-modal.lp /www/lua/wanservicescard_helper.lua; do
  sed \
    -e 's/io.open("\/lib\/functions\/firewall-wol.sh", "r") and //' \
    -e 's/uci.wol.config.enabled/uci.wol.proxy.@wan2lan.enable/' \
    -e 's/uci.wol.config.src_dport/uci.wol.proxy.@wan2lan.src_port/' \
    -e 's/uci.wol.config/uci.wol.proxy.@wan2lan/' \
    -i $f
done

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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.11.07 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
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
