#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 18.1.c - Release 2021.12.06
RELEASE='2021.12.06@13:59'
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
  for f in /etc/tch-gui-unhide.ignored_release /etc/tch-gui-unhide.theme /etc/hotplug.d/iface/10-bqos /etc/init.d/bqos /usr/sbin/bqos
  do
    if [ -f "$f" ]; then
      if [ "$(dirname $f)" = "/etc/init.d" ]; then
        echo 019@$(date +%H:%M:%S): Disabling and stopping $f
        $f disable
        $f stop
      fi
      echo 019@$(date +%H:%M:%S): Removing $f
      rm "$f"
    fi
  done
  for d in /www/docroot/css/light /www/docroot/css/night /www/docroot/css/telstra /www/docroot/img/light /www/docroot/img/night /www/docroot/css/Telstra
  do
    if [ -d "$d" ]; then
      echo 019@$(date +%H:%M:%S): Removing empty directory $d
      rmdir "$d"
    fi
  done
  for s in $(uci show web | grep normally_hidden | cut -d. -f2); do
    echo 019@$(date +%H:%M:%S): Removing config entry web.$s
    uci -q delete web.$s
    uci -q del_list web.ruleset_main.rules="$s"
    SRV_nginx=$(( $SRV_nginx + 2 ))
  done
  for s in $(uci show web | grep '=card' | cut -d= -f1); do
    echo 019@$(date +%H:%M:%S): Removing config entry $s
    uci -q delete $s
    SRV_nginx=$(( $SRV_nginx + 1 ))
  done
  RULES=$(uci get web.ruleset_main.rules)
  for s in $(echo $RULES | tr " " "\n" | grep -v dumaos | sort -u); do
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
echo 'QlpoOTFBWSZTWZmIaloBuqL/////////////////////////////////////////////46J+d99u
Ec7UOGTyihWaTc+VUV5PdpHdm7Bs33YBwe7jqlSVlobb1xzKihVKErWWDA0VKGYaDQNCh1yt27Uq
QRBUBQOzXS0roBtVd51aFVJ6ypsa7256K1bN1wa+AAA+929sysplIXrfejO+wACd8+VihIUrQ99e
UcCgfQnVFU7O4jQ7unFF7ddAD0PbBJ0RvcA4iUnZqEiQPpEOgEjQDx9gj260UPoASDRcWHdV3NZt
IoXMLme2O2PWquAwemu6ZQDwhVVlV5hx2bl7Dzt62WwdCjuYnboHQADI1rKgADVDoBpoamd73cmK
x7aoZBO4F9a+t8Hm+9PeIoABcwfQANPTfe3u6290AB21AA9CdYAAHdgUA+gAPkAAAB8vR8aFHvff
c7fWi53wStqaKu++B0AD75xQoEgUKUEndzkBdgGqq930Zw9mHnfHj55XNnrzgD3cq+vXu+jnZYvf
N999X2aW7l3peeJ7ZBU9Q+256O5AMigAKaAC2DQV99gDiSAoAobu4PIoL6wAAKAAOL6xHSqoNAAA
AAKAAAOznAeOm6qIA97B6Hst3YAHuygNs73Ovt8QLfeqoL0+6bFc4Vnrueau97k5XufWgAeqCnT7
7Pd9wFADXoA5UUDzagAAUAoAZAAAACgA6K9PQS+56190fO7vevhmmqQiDpoBcwDX03m31rz4vvAt
e+vG2CQACgE6MNVpVGsTpi6HQMtHPYABr3s7mlpcbXStEgoCIAACc2IAfMrBfWt7Ou7uonsfe+n1
2C6xF4vd903u9HFXbazvu+tvvcx4BXyACtNsGneBXzqX33AAA92D7fB1AB925ZCgO7p329q70ND1
tu+tfbkDtfX33wHdu7K5NM6sDkAB0AoDkAAAFvnvdHfU9wAAHm+9jee7Zeu+z6O7vnzoABSgdAfe
+0ufe7bHoPq98PngAAAAAAAB6p0He3X1fdblVvXpDvkUAt763O6uYz7XM54t5cq7273u9Tu3X3zq
Peu7fe59OvmwG2DPtXNtzMWONjsPvtfc52vXuvhfV6+R9vl94X3zx433zcNPI++1s9Hzz476H11u
yd93vtPbR3q33fX23vTQrvvXOPb3e766nfQyI73s+np9W3vb6Pdu9j3297H29990EdR9PPrXDjpy
d27a997vANA+93lBL7aRQb3z7rBffd9HbF7ZFAAAeNh17Ne7Pfe+e486fZ9y713mEDt0XGnc928d
R263N7Y9S97d5YvaVTu5tLt3bZ2LXdky3pvevjj12229uuQrTDfZPMIAYEW9vcw9W3duhd1w7evD
kl51g7u2c4gVdtADptm1PdtO70btW1WK0UrW6wMqBpkEFl3bdu93vJ7FfPvfCl9m2+2d2j273jnW
ezq+718g+1tj69m066XdAHI5Bo7DZDxc69B3tq93u9a8ktsu3r24XveqC80+7g89jBIVKlSkV9Jt
3bbDZe3a56lUFzVuaOtp3bdq6Mh7neGaMiXreuK5antyp3bNV3buZnfewVHwTCxFTYYKVShJsxaD
UbaE+zuyiLQNTXd0OGi5rjua6BQU0qh2wtnd0j7sD3pncDIt47nct0o9mgBRSzmvVTFWto9XHQy0
N127jW440yltzgN2t1u4rxsvYGKKU3rNu7WgNh6oACje8XZXduIAAc1narGDa1e+3vQPo+ep7jFW
Ld7S7W9bcaOZljZt3NSR6Fe890VJLc89u9rdqGgrWgHLFvbFdt985EFDsb1oaHbNw5rquHds+1zc
7Ojn03tnrrXdnZ3Mdhd96d3sHU5veHe9awze8AAAAPJ72dd3r1DlobZCl7hsW42e9G3SMegw9VQA
A1tbaPe3ns4p2YF3r27Xqu7dt9dt4D2ffe5ttPtx81QalrD4k+9s7PWrnbGk9vdbk6teUoKB2Ihy
tMamu2de955o923GKhIFBJQkp7c9m9nVyXdvpx2fbNNfczy72ipsAB07tVgBzgtnvN72G3X28O8G
pI0ZSgfWkSZoyGa07advWdga9q9b2NPew2h5AF3vYHopQAN2zDlx1OGSpRuw7a95wAHWgB6ZBdm7
3te9773vvd1XcuzSkfd5nbnJETUje53FNbWAKXucV0BtgG564tjQVXexaGI6Yoh2e7usvc7tvrwP
r1tvu22nS47ViGPZ72eou73WrLa7j3DOuy7vZ1rjqkVwnru0e5sfc3Pc3evXaFNaKB6Bh9K9zeF5
nmOe6bt15kXrTttdjunJm73tb1s16g9dN7nPHT1ABNrNt63ZABQLeO0dvvqPfavWIooASKV212NA
ChQNatkqFLrJ2qmmmDXbpHNwW19NauONuoybnu7s7bz3OFRuztt0ANXVHed7g9Y93bapz2xA2l7t
13nt9Oem9mmNjbSQJREmBtmASWYibDoAdDSgzdzXIADjpE6UBu59eHtYJXa9ObgBpHYAF7Oe896I
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
2MB3P3X9TuQl9CNMOxLu/5d+EhzBtbt8fD4VF4fDlJ3aObcRxeeLfo1rCp374t9qfS3FlU8NDPL6
6umovPJnEhkV7oCPNAZLL7XKYFTiPtk46eX6eVyMLpHzUUkDCGAdzDJVMPqbhNNiyQAxbMkiJqbk
7LCEdVTdFgwNJ7T+W6m+H++FxF2jTifLbHqeY1qa0d1ltT6dpqYzNW2DGHxDCJIFslwIEQKEMAEQ
MDA1K/7HX93b7/Iv3n22r7rL5ZnPZm0SFS4R/59HO28rK5ui7F9t/R+XD3Gw4eJar+TZOdQ89Lx2
R1uPi8XovOO2vtb06GqcN0ODYnT1WN88N980ZSsw8O9op676afCbpzyLrkrfd7Jzm0Py//MhQVF4
/OJX9BxsdZylPjia1uflJ+6AH7smmrPssFD9ICj0ukZPHY0OUsFwt2LxjzCWry7H0dCL0Ldu6Vfp
Qf8S8vj67naK531p73c9Z493bf1gqvun6d0vIg37N1boYflZz7u8A3x1U8m44/+WPDf74n96Hi2m
8beOK1n8f3aHPEXfdqWbT7Rn7LNo7x1tjZYyz1uXut50dN/rObS59HMfWg7+hu3Tr7RsN33Xqo+I
9JxrBscVwFf14KC24+/2iDv0PWc42idRstMrYsP/1kwWL/6f6W75lF93/i4XDMsZPcvuVTf2+0fu
+emDuvGuNpTkHq216Fpql07L5Z4W/0VurMllNng29erb9KTlwkfPskmj5cvFs87U87Kq3Cj4No08
t7IKWtdJsrq5yPLy3i7WcRiMJyKjU3mVtVnmbHbNJdfhl8MxwTCIU+3+z98ufZa1Bhy7Rzs/+Nrg
/HHZcLs1bq++PmD8E7oeZ0eO2rfzzGXXv+mQwjfr3cT+6jWt1geVf5iAsvpgKC86Dp9jB8TAUXdi
cJhsvqrPks9qqSqYz24jYPvM70rlvheelsf3jXl3b++ev9MstghaO57DOZD116QqLLou32temV27
z34rl8Cz+Nt/H8FaMLwbHkrhP126x2xs/E39svG75nH4XM3Vzy7fhbGyaPpU3XnTIqAG4yaVH2nW
wMqV6PDsco+8Dz8iJ3qNbhPE/7H5tLbva946BZfcuvf1MF13RfU5HL2bAVjn5CK7zLKa09hWuote
PltZm98OPe7VanLZ5u8ViTwNKXYKcw/v4uArMLHM71e3xTq23MUnAbssu+S1cq2I2tTy1HHa6Ii5
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
lejxqplXpAlmSqLcuVi5GAT/xdyRThQkMmAIAUA=' | base64 -d | bzcat | tar -xf - -C /

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

if [ ! -e /etc/config/bqos ]; then
  echo 070@$(date +%H:%M:%S): Creating MAC bandwidth shaping config
  # Based on https://github.com/skyformat99/eqos/blob/master/files/eqos.config
  cat <<BQOS > /etc/config/bqos
config bqos 'global'
        option enabled '0'
        option download '50'
        option upload '20'
        option r2q '35'
BQOS
  chmod 644 /etc/config/bqos
  /etc/init.d/bqos enable
fi

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
  -e 's/NTP servers/NTP Servers/' \
  -e 's/Server name/Server Name/' \
  -e 's/Add new/Add/' \
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
  -e 's/\(p class="subinfos"\)>/\1 style="display:none;">/' \
  -e '/$\.\(post\|ajax\)/i\                                $("#mobiletab p.subinfos").show();\\' \
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
for f in /www/cards/001_gateway.lp /www/cards/003_internet.lp /www/docroot/modals/qos-bqos-modal.lp
do 
  sed -e "s/Gateway/$VARIANT/g" -i $f
done
echo "090@$(date +%H:%M:%S): Change 'Device' to '$VARIANT'"
for f in /www/docroot/modals/ethernet-modal.lp
do 
  sed -e "s/Device/$VARIANT/g" -i $f
done

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

echo 106@$(date +%H:%M:%S): Allow DHCP logging
sed \
  -e 's/"localise_queries", "logqueries"/"localise_queries", "logdhcp", "logqueries"/' \
  -i /usr/share/transformer/mappings/uci/dhcp.map

echo 106@$(date +%H:%M:%S): Allow custom DHCP options for static leases
sed \
  -e 's/\({ "ip", "mac", "name"\)\(, "owner" }\)/\1, "tag"\2/' \
  -e '$ a\ \
-- uci.dhcp.tag.{i}\
local dhcp_tag = {\
    config = config_dhcp,\
    type = "tag",\
    options = { "networkid", "force" },\
    lists = {\
        "dhcp_option",\
    }\
}\
 \
mapper("uci_1to1").registerNamedMultiMap(dhcp_tag)' \
  -i /usr/share/transformer/mappings/uci/dhcp.map

tags=$(awk -e 'BEGIN{n="n";y="y";t="";o=n;}/^config tag/{t=$3;o=n;}/dhcp_option/{if(t!=""&&$1=="option"){o=y;}}/^\s*$/{if(o==y){printf t;}t="";o=n;}' /etc/config/dhcp | tr "'" " ")
if [ -n "$tags" ]; then
  for tag in $tags; do
    dhcp_opt=$(uci -q get dhcp.${tag}.dhcp_option)
    if [ -n "$dhcp_opt" ]; then
      echo "106@$(date +%H:%M:%S): -> Converting DHCP tag '$tag' dhcp_option config added as option to a list"
      uci -q delete dhcp.${tag}.dhcp_option
      for value in $dhcp_opt; do
        uci add_list dhcp.${tag}.dhcp_option="$value"
      done
      uci commit dhcp
    fi
  done
fi

echo 106@$(date +%H:%M:%S): Allow static DNS resolution for static leases
sed \
  -e 's/\({ "ip", "mac", "name", "tag"\)\(, "owner" }\)/\1, "dns"\2/' \
  -i /usr/share/transformer/mappings/uci/dhcp.map

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
 -e "/\"IPv6 State\"/a \              ngx.print(ui_helper.createInputText(T\"IPv6 ULA Prefix<span class='icon-question-sign' title='IPv6 equivalent of IPv4 private addresses. Must start with fd followed by 40 random bits and a /48 range (e.g. fd12:3456:789a::/48)'></span>\", \"ula_prefix\", content[\"ula_prefix\"], ula_attr, helpmsg[\"ula_prefix\"]))" \
 -e '/"IPv6 State"/a \              end' \
 -e '/"IPv6 State"/a \              local ip6prefix = proxy.get("rpc.network.interface.@wan6.ip6prefix")' \
 -e '/"IPv6 State"/a \              if ip6prefix and ip6prefix[1].value ~= "" then' \
 -e "/\"IPv6 State\"/a \              ngx.print(ui_helper.createInputText(T\"IPv6 Prefix Size<span class='icon-question-sign' title='Delegate a prefix of the given length to this interface'></span>\", \"ip6assign\", content[\"ip6assign\"], number_attr, helpmsg[\"ip6assign\"]))" \
 -e '/"IPv6 State"/a \              end' \
 -e '/slaacState = gVIES(/a \  ip6assign = gOV(gVNIR(0,128)),' \
 -e '/slaacState = gVIES(/a \  ula_prefix = validateULAPrefix,' \
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

echo 145@$(date +%H:%M:%S): Fix spelling in WiFi error message
sed \
 -e 's/Mac address/MAC Address/' \
 -i /www/docroot/modals/wireless-modal.lp

echo 145@$(date +%H:%M:%S): Add WiFi QR Code button
sed \
  -e '/isguest ~= "1" and \(\(isExtRemman  ~= "1"\)\|isRadiusIncluded\)/i\    local not_security_none = {\
        button = {\
          id = "btn-qrcode"\
        },\
        group = {\
          class ="monitor-security monitor-wep monitor-wpa-psk monitor-wpa2-psk monitor-wpa-wpa2-psk monitor-wpa3 monitor-wpa3-psk monitor-wpa2-wpa3 monitor-wpa2-wpa3-psk",\
        },\
     }\
     html[#html+1] = ui_helper.createButton("QR Code","Show","icon-print",not_security_none)\
     html[#html+1] = format("<script>$(\\"#btn-qrcode\\").click(function(){tch.loadModal(\\"/modals/wireless-qrcode-modal.lp?iface=%s&ap=%s\\");})</script>",curiface,curap)' \
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
QICXOt823d3fT3tdu5ox59H3eSi+Va3drWud2E6xtu7EOtQ61Z58FC9777wej7PTr1dvt5dhdV07
5d73vIB97nAe+7hBBW+sO77cT6y+7Od3cvffefO+5u9593T4eQzGUPffV773Xfe9fNj0+727p7nP
e1ehu9p26Nent4vdJ7c5w3b3tds2dO1zd3Pd1653d1PgPvdw+bUIpJ9aqAOY3yy73dV3twBedZ7Y
8lu7l3bqBffbgGZlG+HdKh33HB3u4KAXvrN9vd7nd1z31lrdxvpnzz23at87qdidZ3bd3dndy0cT
bdN27VsJbNprW0tMlda3S7nXXYdmWrZOW67mRSxWOx2Tfd67FTxi1QaFVJc1JtXdbndzlQdYjrtt
suMvbnZsba2bSemqu1JMdu2YW2bDa7s7andxmmnRbtaXcG6c7Za7FN2W0t3u229O9eIqQTAACYBM
AIxGATAmmmmmmAEyYAmTAJiMEwACYJgCYFPATEwTIxMTAAAAAEwAAAAChFRAAABMDRM1MCYACMCY
mA0BMAACYJphMJgANAAGgADQAAEwhiaaaZGTEyaYgyU8wIaNDU2JQioIDUwBMCaZMTT1GCYACYAB
km0aT0NNTAmaANE9GjRonpNNNGTIYRpgmjRgE0MRPJingKeEyDBDJphNJtGRiaNFN6oNURDQ0AGj
QAGRoJtJtAAABNMmEZMjRgJpgTJkNA0Tyap7RiNNMmQymYRppo0aJ4hkTT0ZJmmmI0wgZAxGg1PI
xMlBEkQQTQI00CNBoNQU/1NAmEmwRpsmplP01E/QT0jTwJtBBpNNMk/UEzNJijaTI9Jp6jHkhPJN
pPSZPUeSbKaeU9TGk2kMjyJ6anonpHlNNqabRNGgkkiBNAAIyZDQAAEyYAAA0AEwmDRpMNTEwEaZ
MmTCMQJvQaEaekYmIZA0ZNGjCYIaYg0ZNEwNMmUxMno1EUM9w771+mcVHVnzXd09zS637d+mQ9Yp
2hD7rP+nR3WqKARCw6hEHoUC5VR1udTT1ZrPb2GMg6o7RLldlWsUYn4F5s3332t+NF06SSiEph33
l3MvpJ9+A1qbQE8GRKKgVCMCqKKIb1ursNPYyiGHkz2NvY5W6Q5IjAMpoAfRKQFsm5OrmOy9S0PS
y9n76uMo2gIXplpvVnYqBCN4ZZJTeRoutZnOCGQecP5BCyBR88Boe/Ah7DsTrQGzpEZ6MJzyCic8
MxDaEPDLvXhBgQFTbCHT/d0E/ApVbRfwrViITCSFokiYhERemRUHzUDKzrIQyakQZeUpCRkYwioH
tydMeLKUaD9UICfHBLIKf9yIDzSL5Y0kKOGABuQD8QgL7Ugr+/8ikBK9jU0+V2gGMOdiNHku0yJA
sohOMcZs+IeEARAsU+5PTFQLHWFlEsYHyTAuoCPSmoMZICWU3p8+Bb8GhG8xICfCIn+/03wbAv8G
X81n6+f0DHRxMaPAMWZxh24zCFvGAkcNJVLtn4GEYQucyAX/dk4KYywYwcvk8E1I/KMBQX563cvy
IICDLTXIoX1bu4sdPHxJCCHsuy+x9kT2Vyr+jkt4OJcwmOF6wlu2uSxWNWymWWFY2xq0MJJlfCpW
WYbPpUftntPNn3h7Q8LFfcfIo+4gbOLxyx1B+ju+X8wJxVVgIvT5NuPOHj5kOPDCtlrww5FYTGGO
OMKwxtajAwtMsLXKlSqxvdxww+OS9sss6yq+GeNssscL1ljbKvL5wvnQXM5nDK2Ns75TIl6Jja9T
PSnh8P4fX8Thxf2ebQQhuKrMIJ0viqTz/rD9j0t0j8mjGF6WMqLyKiyI7ZuNSweZUnLfsfYxKMIV
VYVEwpNjPsu2sHDwfWk6dCN80AZJJf8Pg9X3H8OwvqqWdnCojAiBU6/QPTjY5rjfq1wD4ooAEEQX
piE1WYwDFmiuO8joL2lkHEPQfnIv5f4829KwHHJbuDT1q1j6jG29erjx+vrutdhdhWhMUz5ommfs
hTK9YX/Oc5GK5czUNlwHdB47+Xk0jyOzXrzVN6Av0khJTNVpJ+6QPHo8thPX+S4nsww/AjXGsr1T
718SS5jFFJdu8e1RRLBgJKq8aaiAVatd1dcVt4PqYOdeN/0pP7tZHnC1FViHUbhSVzd091n8HUoY
EOX7+BhsHBSSJJkkCHlw+r/o7/23e7K/e7I9rPVzTPNa68TJBedOPP3b7tuH8zPj/W4cu+Vvec3b
MM+v3+dfyGVad+vZ9HWEmm8seBv/H4LrpisnJho4tt0BPMTk7SHUHwOM20J7VPfH/Oxofqlp+HD/
PvL5grvlQ9jwHGQvtjlrjKU6gL7ciJAjnOgALz+Bu4KgwjTSJLQiEsrQ40zbQTQHMgOXqqJ4fJhJ
FnK6KRW/VfF8qw+7HBKboMghgSCOPDWP5zYSOTzORJfZJGLn9C6eOHFSWrSeJE1w+i0vif2PVfnH
AABCQFafl3dvEwMTRIo6FKec4v3xrI3Aa9ooB4akTZ0sYN+knutQnHoc1zzAAQHt1kc3yVCqPnHf
DGMuNh58awbjP7n5Vp80dS0b4mhIog4UOpzRW3Wb4Dc+ANCM9jPPHM7lz+37954sfl/U8/Ewlodr
8dBNt4btdULjyhbkpOtKkOYrPGqb9btzE61org1GEFP+8yxHyd2HAi3hKcqPCJaBbF+awlxdblyW
gzOEZHPZQUajIWV92/H0pLMTEPwlQIEi/RfgTWZJk0BU/3MXLcsNRaI1rJLKsrFF8M2/a63XSsI5
rs6M91hwaKhIZMLIkMMdvgLN/B/MpJh4Nk+0FgKppDGlO/4U4DE+pX3yhzOVEDsSwu1q2Jst3uF6
1dwO7g2Yc592CIQkks7r7Pn7tEVfvfY43LNx431aS3bGtxtU780nz0TC9X+TChXc9TIIESSCAg53
S40TMxgpzprYX3YkZmmgI1P93OWsVOCxkUWs/mpOpLu5Lm9TOCTqiY/18lWsPLltocpemOHsF0T7
+Oksr5Ib4TtaUklb5iut+Awwp83GWfDRtuCDE+WwyfHoxIeXp3aqQLkCnJGtp+QOH1YX68ifeMjM
EC6HcEVvxTRFz8y28dw4Ev5bZSo9n0LGNFdLQatj37Vva75GV/PM9rtrPh5uNE9VDtdrkVCgQHmk
kBBJjedEdfnmTM/wtGEBusGbwPO2NVzTinGxQ246CARg81Bd2SG9QkdBb0Mde35yukgjNjkJEmC4
tqCDIM3ZTHvg2+iKjBdGUl9baW5mnlLiPdtzqS5PXKUNkmbQfXzjKnlUs254/EFqWbudXXH2teLA
4euY5JEeAARByQWi/duyvCUSgS7oT2rtDt6kUKF2ahrYqNfQGTacDqxnwUC0UTR5g8pXZWaUVTdS
tlbo3jLbv83k4HknvlEh77a3csV7A2X+XWr9Y62OLa39XnQ5uJcAmYMxaGroOSwpvjPbH7QqMgeR
f7Q7XtJcC9OqBz6EuZKV7WA7Z7LHpJontHGYiJuMHF0uIHihI5Z74RiI1H30DN/iaCeEZv6yCdK4
TxKGL588S53w98uzJDEm+wRQIBEKomP5f3FokXPk+gu7ti8EYn81U4gMRS+MuJQV636ddPAZ7ZTb
KvOD9+ENV6ria4BA044ymdlwXMwv983vJP0FgQDdJGHAJoufmBcDlVcRD/Y3MwC6nc8q6Wsel7qr
Lm0DLfT3sfllcPbN7DqtiIF62xjJuFlwFh/+dw2tszJj84QtfNIQl+aywqNbAvMwbBrALMcAFAiE
EBm6uqiWcw1WU5krnu8cuy/jcfbGqkAFicOYaW+7iTR5j1ffkwMc7zLH6wyOsna7IrltoqZCxcSK
gLVmZlmNlCPe2PWHiVEZW9PilwjBSWfYeshBqxVv6pD+QClsbB2X030KeX6z9gg0ZYcqYKH1H91b
U28rNic8440FlcATeZOCRqDjEvNc71IfEsymA0rszLXZTA5E6FQ2FoNYUoAJgDYEgrU9rmdF3bWp
fgFTHQngDVgUgIAu+byINojGIMWZYp8BH9oJXj1WZ4KYC+XoZwsK2cKwifluj5vM/VEsBkQGoW4I
n5QQiMG3AxlYhA6ewFjDSK1kfiPvR/LkCbq7GLjt63KEedgVfP/Q8C8Q3wIHQ3hmOyts4C5TNNDG
aaf+GWq++JZloq8WENVvTKkjse9OCxT21Cg5voZkMXj6MN9dJY0IRO2dmKiLmGXnRQE8fjft33zi
pcm6iq0TaRd8aQ2z5CureKWMElW+btTqjaCi4HC/yrYyG9JCtwSmqW1OhF3/Xo+VbvMZpQABAKpi
h+QLFOw85QdkA7gg2bhR/r0cKeAgBEOlmAMlo82wRUMc3WntZmoeFC1S4ZzBLuDiYyHRzOcUVti3
qnu/yj1SrFx0k/6iybDKHIe3m0qluvVg+QuAS1l2MbvbOHJyOS0ZH+Njw9qYTH/5I9PLVgo+QTlp
NddSr4rFMkfvXhCu2IxNstvyEMhkRkXYc7Lf5efTzNS0lV2x1Kw6jqns+nWLBkYQhcvxbbvcIj9M
unCZFM8XlrEOwFZAZJ5JIYmDGOJzCjo2BVqhliYgkdjWxmo+nHzsIQMyBINbXTO6/i9//3gQLZf5
ra0fXI/De7qyczORnm3+k6GP6urjv9dHv6J8p7f4xI1lmGGxoww5hwz3Z83YZF9QF/e9hjxYWLsM
RdTDrN4gY/N3SypEJrOXD0Pg6dRum5sXP63Ppf99eOobjNg+TYrpFjtxfcIxLKfx+LdkDysyHDk3
CE5a3qeT/yVOjVmTbiOW6vfsGj9KqnZMV75h4GbAfWzjiK9TTUf/2U+PEYu6IE4DBvoRhBO5ia35
oTLZHcfpNnBnHut7qJuALRoDvggsQD/uIjZyBtmzecBZA+LEeOP3C2XiFgtUGaKZaPqgZD16Yj2g
oTCRE/3tBoe3DcuaAgLzhXRN83WFU3QwZ50bP6bFqbmlbAaKH3xttmRFVUxx/HXd9BMxa+O53OMY
3L99xZ1N9qsJ7Rx0b5MVxmM9YR97tfd0YTtFP8hmubwtXD8pNhdaAAI6UwLRHcVLgPxg74S8G+85
PjiIdcmMmmvP/Xug7Tl/7igV7CpH+Rj+95av/mcB+3uUNLhyF9ISJe/cHBeuuSE+iPYbYDqPAc0O
NsJ1/ETCi/KWtou9AWl3rzAPaABqxgiCOjJQ5NydO+fYrYfP4t2VuF6GUprBKjnpUZyek4gx4pRy
FdgnUAFdxXJljUtipJuK8vMHP0pUEcj9Yj4IbkRnFRuywfD+rhZ4E1AjNQEAIKdAosQdjM6LUcHd
w3MUK03mHSoJWA90ARgpgy+CZnhl5IghxRstp4CgEAIiTf9doKezoGiG3LkvkaBI9l5Kr68mbvxM
15xCBHp+uTVDCmQuetDfjAv11RQhVzXTXHEL+ls1/z9jbvqtiJ3eFag1KpiDwjL1hBTxTCp1w7F/
XIu1vn8VHfbddu/WzzZluCFLxqgC/ZdmTcT0RuVJa3oRAbJkBvSma8w7OfSpeLnU/bwuXeZvqDlV
iuxeTR4YTEYrdjSgOv2eurLEo0vq7v8qvzvj6b6aCqobk0mzk+jQ4zoJq7ZRebMFWVuT4RrUy0HV
q2y9SW2Fww+1Mk711fe5TQeZX8kCfCp4A3BIAIG3apqqLvZHimbfn2T0xv52cLk8abY837di7Eqi
DB6rozQVD74R63nOY98F4L0JYFy/antC5DTYDiQ7aWcm9FMQtLaeFz0JPxYqd787YSiK6/HgThUP
OV4/W3R3UOTZRYglLkFrq62hPIybQBjc6VYqCOYnyJz6ZE7/u+O5XZxTNOg9dBloc7PzSvkl8P2/
AYVf8t8uZGxi0p6Q1Ab8Bv3/ixk7SlqDQJ7Y9V+dB5yEwfMTPc5vP/GH0LTgpDH7PNoF7CSoSnNi
PQygMoG8KSSqMlCi4u9LQQ130XcvVayDbtLalq5dwiwdINRCANPDm+2n4Ec/0tshlFBuzJH5HQ27
bTQasy0sDu29XEkrZndc5Hh9mtBumquy9PovUIrhDr6op1But8LP+C8Cn2EzUNDWVPBLh7qojM0a
6V3EjKZk9RfL+AIXdI/WEgK5ZjmNmqwLkUUMUlxJeT4dhO4GOkXWhxG5oWZLs6Bo0+0ShReSneOK
YroP/XWhmDzSFx5FHeIzjCWn1uKmPS0WPy7khRPJk/C75V2yTxO8RGWYQEfCjuNIjnKsjyGnowaP
h0/7GR/JCsQMGamLeZTYAf3xpomlI+CAXRkIJfFAClu70gHueCHRKk8I40AGlYPQCQEHtAJGvJD/
SIjDbEt4lHoBm4RLdpmkJtErji1YpJ14KvIelMMBl7IiQ3k/A+/RTuG525gOtohYv1vLwZKTXtlK
U10UNbBLec77JO65zeNZpZXVXEzeNUNOTpVO/Vh0Exo+xWIS1ky+bLbfYXfiFiFiVnie89OIzMH5
h3pGHNXBPK941jEoe+juQKM69qiG6oxvFpI6Bm8WrOJKlHmT9IMwdOU2iVI9avjLA5oopeQRlzWf
fGHYnenn7hlskRIrDYqgKo1g6haFWc8SRCHUPm4INEqdaF8cDnFDI3oNwzgoms+vUquMsn8VLqkv
qmaW9BPGX8a/rhNxVNlJvVUaEg9WgHXDO+WoreTCrWuCoAiCZ+J/D+m/J91vwtOZeX+39rOfq3hv
iRm7e4kEed7C1lxaqHf6qGKF5+z3WfRmj2BP+DFDafPkUU4D9QA/oVc3ge2WnIe7WRp71TtNTkRl
GpCbdooH38SwR5kzgbLVb0+8vvuyVwBhckqVGU/LXDERPmTX4OafRsIE5ADrYMyHIdMJXZ8QI/Ep
Oiz29VcoWYYh3kmWr/sXJY+mGzEny1/YJ+UIEGW43NtFLPQERWYR9QBTPEE7eak4UEQkwaGrj8qf
WA9vKftZnQewoxCnDoBUIbdaAW9Dwccay8UM/jbiROB72PQbQPp4fbehP/H9xL4NB0Znkkwsfvlv
Tcsgm85GO49TjEvueS7tXUf7p43SeeUVA13Y+ylOC68HKj+ZnCdZDgNmrtzpNId75eLJvLKLLx8u
5hK/tqVXsmGv1khJiBCWpc/ryrxkc1EsV1iWQPtyWETAKojYgNxJrPRPABrvobzejgrCBDKpGH8f
zWzRvALxeqF3lq3f9Vger9nTuEPRbbXb3tPlGfcCS/PKU4hY9kcynL93ftrPPYcOc3ih5TxgqSBf
UyKOwct+F7wnhpVxs2v8/IYcVMUIUWDLFyVkemCKsNUDkYA3uHOB/ZuLAOAMt2QrCK4z5IrGlXPN
0A5UwgIH8mCi/AgjzeHs5pJmXvTeYwTujSUmuWeP8B2zLfae4FT1jikFbJKUZVioLgTw8ZOx8785
tGVcNEcK/XBPn8+FA1Dh5lepUN5JAM+Bno9PCIhc1AZugODO+YKqLt1LUrz2XwrrXC6Wa3eRP/KD
4vQMOQS+q+aqFCbSMJqomoIORoQ8TDlOtAJrtOaLJTbtFJ3nrBtPT/1N0F7sHvr/6lmDOOdntJYW
u0rJaMC2eXZOhpFfkmb1zttCrbq9O07QpiZpT0kpcBQnkv8o39NJxSKKhBMuLNn0wYmgq7dqIvKC
RVH1XHSFiyp8UL2ZM938QUkPvu5omp3i4xHBU40aC9tQWzFXxXqTzxsI65fhUNjzwkHtLQgQIECC
UKsSWschZ9kSc5/yy1Z1Ux2gyKzmUL6k0VOIdrOtORPJ1U5LUazls/qzPkjK4ZARseFB0slP7e3X
96zzrOAuC+5f7PgM6ISU54CWVcp3XMT29cAv7e9SwYO+Anp9OiMuFKlU1ujvyW4UzpRorTd9Yu6K
p+MynB0Y8TGo/aivvZlm7QuCTAINLP5XlfFs77VAYFPIlpQZysLMa4DYC8ScyUFKBHtcCRFur4z/
EU18VAD0H0pahL1NC3TgWoOO/SIzFs5SF1+FrApN+7BdAKk8QQML6VzuHM3IbB8xafIN7ST0zmbr
6wNIiymCT3CQ+efs9o8Ifizo41yL6hHjQdStT/TbAAnHaCce93S3s3BD4VlT29NC0PIoDaL0mAom
Mj5OzX8UBvb+qVzML7ZUB8W+BAQAgfppp4Q05852h3ktKDHtbA5OlfOF3dtmrL2INOHn60xSMZ49
9bY8sgyD+6MVwJepXTBd+eo/2mwXH9GVZetahHF5vUIO5Ct6LzPxfSwdB5dRQKYVn9jEtrvaSbuy
S5p5wntXUtAPjbsldq1p+8DO/saEbDe0Igg7DTL7dD+i41pvaLVM/5z/7sFJd33TTmK0VanZOjH/
LHvCc/6U5QKgdNqbn7Xw+33klESzie+rxV+i0zWn88Awepb+8eXVXHsrPm0bYYqpRlstc2fmzYFL
24gtx99Xf0mtCqf2y2POpIK0BXVm5gYVRtORQazLt6fw/rZDKjefFQTqGeyMXa4KN2+J30a8U+IX
wXjbZzgGq7rxOt4G4RoJnNQen2+iwfSJdzAdOQ5Aavvr7np8SETOGCYPxtMlexQYkmyXPWh+ySUr
LOvZztbXPiFiPD+mN3EkWtmzKdPaULAkaP19hYoQZAJPiCCE/2He5wJ+xdsLc1cnH6VpVKhYvbLi
5vjxfeR2lFh6Vfqg2DJuxLEKZ4OoCeAAIOmMgNOFEG+OGDuKD1Jj1UMUa5tfgNazIwlefZDLtw1G
/MpvO3C3ffgQ3AKRdHGD5TNy3/3NtRW8kvW4v2ZS5tbpPAeTi8Mt6TaDJ9C7yY32PSKuX3ZOr2Dm
Ia3q6uFlv9RRsMdmdyXH9KP6xZU/pNxL5vSKkb7i8kEFev0HmwsUKeunfeOKh9ege2fN1Nyw1aJL
MvSA3prxTbqkgzci5pjxl+f71fxqz1BHarFCKwlWqwElWt7jPLGAJjvERzHklOuSKYI4RjczC4Yi
z/08Ywe9DznEuvxxCHYmLrkDgoBrepsWg5R4Jv4zEErgeyIzy7Vx/Ux+tqPO/ex8cbN1pwf4Lhjt
KxK04NSK5yuWC2HefFnA4AWSupYSm9HALMnjd+G42uGSBtSZ0IDVr21mcmkYShaVHr0qCH9TJIpi
eOndI4drr2zd0Kr+Os3wEA5paKp/OQTwH/gkb6hY/U3q8cTOna/Xa3JI4rN93ngO2ChfOFqkdOvf
TxoFhkF8vP+AFZ1CKe16C4LUx7brHb1tpS4UvsxMu4PGAd3vKTInxuEk/vqa4A2e+l4/mVnkfT5u
/lgF3zEuCEeSLW+xyo/JWrdymuIogpKx3MpbPbf1UlSSPQu/gVMsEdr4zAehdYT3vEDN7rN6zY2X
2DOVMEYpJJMJHsAhGyAh3nr/B9r72V5W1QTtjoEQ58AgkIEQisCFKB4ZrLjYCDGh5HrMQrF8S5nz
vqO316VR2e6+kH2B5g4ye3hZ2S5/NYouQChSCnwbBjxTW4CXAIF8smwR8eCP8SbWIwHSkB40wUg+
8gGQwLmrsFftjyhzu4L6z883SyJnTfkmJIE2MFQq01t9K430tWpt8vXP9R6QhlyJGrEA3FulScrO
ElaDyPUkqM9AdG8Gs8QX6IsuHUlGJ0nFITh6g4EMEuXPlFHCiXP18vihgZqfUea1njLr1RuhcidJ
RrO0ZthPd5m50ZfnjpADMzPgaTcn/7bDUdYDcNLx9WZ5YNSbQ5me0aS6ZGwOO76RiVltEZsA2icB
kG3kZcoTkEgJoZHBNi88mgLrS181W80ceCmDfkk43UhLleot17nvl5pbBTncD84kfJgoD0UmEia2
s8hLs79yhNxddZzli7AW9B/mHTY/kt01VaK+t+qS/QTJl5Wu/JHa88IF5U2sqJ1tGNl4njvwxVPP
s+Jwgwhy+4igbgnpm0Qn/wYRmaCfcW3ibcptf6KlRRqLisX1tWKbnORah/TLhTrbKWX3TiHCCAHv
9s9Nl4GYjRX4KdS6xj314MFeUi4eYGG9nlZnsbmf5uxeMb17EBdbjA4hp2gl4MUpDOO8p6KDiLVr
lBnASVrYSCqF4LFeLtG9CQINcwJ/X5+vneeZ6X4f6gwyZYliB/vC4Xo4Lo8uwaCf24eJsjWnjKkK
Rikcav4MGfpOdvr9pDjRFKMZ0ZGJL2Cp5YXC9MsZvCOlUQiSIEFwiOGAmA5gg/9P+11q9lOPsL5w
9tHAVEtXfPGdkeok8TTlu/LPO+31IcES8Soe94csTiKjT0OYnhwFeJiw7o+enkwHDqbkovQ8rfYa
twoQxUMbLPvMhZ+ADhTQ2t4XSqGI2mEtnjdtbzTVr4FsDa9E/sxB1pVLcQ2PR/nzoxvm1gZKHkuL
TNkK45DLJVK6ifLjpkalTy8MH4sV91dh/ntfH12j/Exdxudz6Vl9fI2uW694qV9o+53E3csmILwD
DMwmTJmG7LnDAwrA5yQyOOyPVZTP9fJVXxoW71uVmPA1ZM2Tb4j9s7QZL7PcK14+ilEtlMEwOYTy
715ZFMx5MCAzNqxgTkMMC3wJIfRKOuBD/hex/JH2lmoT9bRaJHuLh830H7jl4XrX6oXXx+b9gjOx
p3BIQc+lJP34GaqMJrdng+u+XhB41jf+oeGU/9NSTDlWLyNdlrd/DPJJxM5+giw/FglfcVpmFFOh
6DBVtw21UNh8ZMItUHrHeWhqnM0aECFndrtV5YHNMp8cHLIPPr3xVNjGRRjHpGmSvjWxp4LT9Fcj
0Af9pINFaWH9uRPHWMwv6INtWacYddNN3hp4rQk7Aluw5vcbkkfcvIZZPZYbrMFvvgkZtsQzLE2J
gnRe8PJbErzeZpfQaACmtr+94j+pdI0LsCVC1R37W5kFxYNQmdccp1QZVjnkl5zgPveyE4z3EwCV
0yl4+H6jQNBJ0cOZmx/4evaOSUmY2h/z+8x70j8OT5RFjpngHxBPlfsYmu6clr29IXpvQyDexgtF
GhlPmBXCheQL5vcOgXuYF8UKjgsxtbSAmUZZNCBppbBiiDCMDhV/pMPRuI13cLXpkQ/Lb8br27aG
A0gAoGjEfTMNZetIfJOz59TRA/yHOEdY6g6+6DiB9yRfuzCgDB9PViPu7B61ep7P/uWYPZ7QKpPY
b7ZRI2A70akh7dl1iumX/EYvKWqMBEemvAD3BZstJFoM4gTy1KohGuBCXA0Tr87IIIOYwTcVzCFy
ZK0fHWmoxagUmun9CaYh49jqjo8j4x+BExPEwOPj7XzRgIiQwUERVkzDlrawdDRHPjPF/7766baH
sLN6MrMQqydNPKOCClXIFZESO3wNGkPZcbK6+Wwy8b6fqz/rYCN9d1DTWIt364VmBZcwQdnLwPL0
20jedLzNef/czulcCFReVCwvmgUhLHjNGXYGh1g9vQQLQVEIBTyGeImcakBwrYC4/Lg7ggB0EcHP
HBvhmX1P2oABDO7EDYRtejxAW0nrl3xDSc4HCYTCN6y7dhDyZKJetti/44P+Bgc6bGvfSRmm8inc
rwu68KVlAoGKEZUSmEe2OJh4JSAVEGCoJBAHhJIBGeUahtwu20DjxX+8CFMLzQIRQ+Vz3knm0su+
yvys5F46eVZ7rHDFkfNJ9nJMuFOEC/fl3N1e1PoseYgGtj0seWv/0I0XgmXtKgvgxq1rCH6szy2I
N/o+IeALszMTwx9mM/vV+PK5/Cn+yICvvd/oZIyXq/BaMS90p67cFGISFqpAji5DpviPlZMmMGNn
RwSOB48i9IyrpZVp0tAeeefgszhRhGgaHoXSxXb5iOj4seo46hxPyHD5KniEkMyT04OHH9KUIUGb
FDIJObCERpMER0c9FcRY8jCBUw0H0Wg0pr/62Exdu2kjKz7aqPwbHhRcE7UXEFwVGIx4axTFRy8C
4eOhIoOUcyFRJ/W0DmF419TvxAYVrheenZ2z7DSR6Sg3qsT0wXk4o67xx+2RfB3Byho2Z8dzbp/I
YLhxaqq30tsaaQ4a98LF9IPqkbYpVRHjLJDVt52ct1t32yLiN+CMsqDYuyfEDjMjz/A8D6slIAvb
DbChnfKE9J0d7KEQagR7BcSlm0H8X8HGq2QjBevjQIrsY1sGagJW3e81NWckYxmN+C8+Eppt+u1S
E5rDemoeCH8z9Z17KTu31/yLJyJjVXWGlok2TQ5Zih2yr3WebV9jNxr7HV72twz2uKv/yLQmYa5j
gmDpfhkyKA8X+XPIRACACSB4CRlcG7hbzrxaYw+6FEZent4nG39q4K0xIkx1l5m8Nu25APqGGBuF
BQZFwTwP6Hn/Q7eC+5xg69PG9k/UjVDiOyyGh6MYYw7MFym+ZfuOBaFT+ynniQ6a1rxGyXOIv0yx
ceILcUgY2feYwTZcuqKmzF5ervKSnf0ves7jk2L1+6Ob5eKw5HL4hhgGOWbRIfOUKzobte3/Tx+N
31vpGo4hp0ZoWIV/umBsND4fwKDCMmQfC9p+9zf45/D5Idh+Vq3IWCxRtlzwzAMDAufg+6Ktjw1N
waMwhjiIQh4uTiTS+nncdhmXoqIOIArRxcKD6lfDrocgJgAioz5fro4IVs59hcyFCDnXIIgsQ1Sb
NIjTCz9fd+YxddKRpHi+bwmAhtt4NDRAy1QBCuBCAA8+AISTBnuv2PpR0y5VbXkkZ1fjqFg9VaNI
CVkcQUpHL+TvVH0gG65X895Kst2QtCZJd/OtJOdr5FRSfhsY1Wooon49KP2GtHogDzW+vlSao+3z
KBuPT9p3Z6g/qHVdT77XCMWAxYLFgxZCJFkQwyTJg8YsSHoSOIWBn0PId/cLF2Ds6aQR5NW+D9pV
cSo/28CBWchI/bPGjGBkwczp+Zzfg90v2hqdSXg08F7YmE3hy5f7hIJybmJ/cJO7u5eCQYo4GBIg
FwAPZZeDVtHVdaguflTAxSMUJBKkglEdWoaPaGOht87BzT6raDQmY6W1NI4st99tU/yla0J97rPb
FtiobxpTbI/vuL7X7LLc3A3QjICxrVq4LFFG7upsc3M0bu2HjR9+jCIyPjw8imBmzC7jsNBo6mJG
qrN4qeIMBMuSnW0pvDoIIGfEHQOqSSSSEQMAkYlIAbByYF05e+5ugxXBgay5CEaZTozhSYad43YT
dHE2jTNJ7pA+s0g5PtKA7wPFeE37+Agcb6HmROHKy8pW4p7hoP4Gkqt8n15drkE3cyCc4jY/dcin
IaANd2dL6tu5CmrYD3BJVZRflln4ueP1jvqgsHESSNh+f4GGmenBV3H4gfAHRgE1+G14BeqoHVMg
dDfEebPN40zQuXjUjvX+FhjN6egwx9R8kEzLZ4uB0vNfoGPp+WiyNCL7BU8cMU7LqaBm7ocn3hJw
5mGJQlEKpPt40QjFJ9cSiHqe9rwSFrFzcQ71gaSb3+YqSEhcKo6zvtIOajvbo1uTndcG9vYnqENm
Xql7Ck1gEIMtgWPeb4UdpieU3BuhY5pN0shuHBrw4swSYzGVxpTjMLVCFECBASmDGAP7OZkfaHxz
xRA4hxTzxQZEELl2DdxcHJycNs/L/L4nhbiHTHN+Zx7gGiBv2jxACIb95RQb1FF4U0XO+WxculMP
3fW33+18jzOm7vwvO9t88/W/OK7d2nnzhyUwhkc6Z0EnjAalw9aLp/8zXq58d1m8uKcg7FnIEyHt
o0Tff0iAbxxIuUTg+6blWtw2+8BlAhuWEYBtSAUpbTCQpYMqVtiGfX+wKN5o2+Z1YNTMBNBgfTnr
uuS+Hs7C/2Mikp8uLLXYqijjsjfydl3Q/pWRZiw+D8cScOv0adEN2LPXbZYsU3sWCzQ0RU8Wovns
HlfI6TlGgz0DHtyQkJCQkJAkJCQhRCEKkkkkkkZJJIUQhCD3f+LDQJIxiJ8Mi4qFqWQjFbEWyecz
N823WGCY5shQ0MJhgGPazR+uJseazOMmM3WTNRhoNlu6pDNv7Fn+vD7a1v9TAbEz9Pu2h4Voq8ta
fSwu511L5JT90Tuw/Do0twvnoaAELusZ7ZJ64wNRe/ZyH34QzEBJJYDF2Upw+lamSBBcBkhcGi5j
yR4np5f1uZAJ/MVc5XZ4WdydTpg0HH9D3axcTQ/ZE8as5CVRV0NOsQSqVSn5fjvZohjKGMm345ML
i96/XGQABCAglteEABRV3z234/h42w69nsg/faLZeQ9Jq92Vw8kx+6+NiajDcm2YWlOw3LZfS3+f
+FPgFuy+dY4U9AQRE7mCHEOPcZB2HTG7hcS/LaA5BZpMS9m+Ev+knRGlS0JEXGI55GY2A0GNsLYj
RwN8tVsC8NUv2NktopMJootcLFqGN0S9xoIsC+ccAyxwOnoMAO31Z4yEhPCmEJMXMxdx5QDUP/tu
4mFs/w4rRc35u+xlNLRHh5Vq+u/2ji1PYcuXr1QtTqqJwg2dOfTHRtBj1UK6aYBg2C13qSum1XdG
eg0YZc/TlmUglCSyEASWB+hklCqXcXHYv1ZFIqdtxR3NBUgrrrgr4ntl5UdRcux8kd5M9uS7M2Dr
WTGyD9Y8ed2Tmoi9AycvEmgo2qWGh599YbPXlkWFJciIDgZEsoMy6HIahQsSS3A+bygcnA/LEkBA
Kn8hOsKPQvb+I5las3NM2g51gpc27Zz9oEuKIhaCySFgncvSEIznegOmOZWXYpMjpG/SD009BhgW
wznNCCWjzdRfGhxzxvUwH3vm78PxcdJtddthqDb16itiGzC23NW4N6yKwq2drVQ+lQcoSBDG99qs
EG0XdxTF+Jqy13w7j0JawaySB6zOkJAZHYRvopYJEEQ5l15YLrIBVsl+1ZxP9fw/rfn23wzX6yN2
6TrXWQQTkL3qSncfkhYJkjUFOMAhPELILUHHykGHDccUXi10hJ1Pgeo3vZT014NMVZEXw2wrGEoJ
eqRmepaOtlTam15zqsFrrpTrp9b4fVB2+u228a0VIUQsOzpAu/xcNPyFAiG5cs2RkBxt2mbyjXqg
xBiHdzKXzGPwVAhKBCdCeGP9T+40k98tzhS5mdVb77063kVhmONRXk8tQcdIfW0nmt76713IHKE2
+VYOXfRpDHOJ2RDDouirQdRbR5nmcna0Wwz0TOqxru0q18bBqQ/FMg2wgeRzwbS4PGHV6k3zG2q2
zMtbRBxMQh47DK+W73gO7pAgtbJSGPMMz6GYMB0dFw/8rpVd6abeUyWPb7z68RU8z8nA3v+eFrzh
Uym1hI/7aiqon8lbvPqVDWgipCZMkKsm3+W2GUN8NeabrrwVLE/fyQTvJUaebz410gdJeXdgR4UU
obBTaQRnvOM3J2AyiBJ9zoOkZ/I20Na1+wBg3NpJG3vAWTuLUHbskoUF9oTPuMdwt3bsH81Fwp3n
Zye/PAbC/6bcYP5uuVZ/lY8XqCB/a6Tmcv5J972BzencDmnc4gZwyLwx6fA0pWWeU74TLqMO9tgF
9Q6NQozRNOjzxfp9orVl8vK81VXxbY4ZobViL+8R5hUUZW8KiFFgiTmSI9gIEFx2fA1lchdKUCwS
ejPuNnEGtZHBDLXdKObaYkXajZ7NxM3KExKhABg5LXHpXOrqCFo7mzDJJLASxb/stPAZzL/4gqzP
4xDTFaQMiKl2AZP48tLgljLPKXfy5zB5qYe+RsS5i8CQM35jk9mKlBhP6pvKkvGtBn5vA1prJfyq
v39JJOTmgOgEC0ec6S9r6w3vG+/AYVGb+ew+jdddn83T4jE/Um/imoPNpl3oSnK+omhQECr6d/gy
KkYsUOOPSxNcw0zK1nC3TYY6exQ+PpPt89WkzotolgAK17evG+RMO4y2BqCImGmwUQJ1ODoyJmKP
gen/4+b1ANK4RXl98hd+g+js1103O7m8JGdiK1fAIW7MEQbuHVUCYtklC5T2vaFRIiK9xjlck+Ed
O8l5NkPVgEpRpXLfoZm/klAt6PH4eaR50xrsVcuWDejDUHeghI6RidTAfd+TS7XY+2kSReJ9qu9V
CfaA++Odpf0q9+2GwmO7+G2fdhqnbcSW98lHSNcC477R8vgEP09WjRxsOy8qGD5bGsfrsiprqcoC
2BykPbHXGR6PAyqWLVpEKmnTqxDU/n4VfLBE115wZRMIzM6FdiAdCoKuzOI1OQ5HNxlIbH1vY13D
9Sq00WR4OcJwU6GC5XmfvhfgveuPrmKBVAQiFgC3cyEp4gwIBsK+rifXIFAlbXnyZeM/o3S9u3Du
XpdmphCIePZT9xuDfk36CZY/aHOwFOm+cNInzGWG7hB9lD0H2w/JqWxU/28LpOMTSJ5YkVvCxS6J
aBIDigEqvKDy7DuvlducTly5fLlD707j0zLdyH8uKdRThxJgDPVVUCngiyozjA9X0mGPoTLELr07
adPDFEpkrvTAuPLTFQ0acjv9GQZ+Ytm49KO852NtVc6hMtVES1E27np/RJqHRAgXECXGJAKAIXtf
RqZIMZv4/tzbQnidMUmmCyOBZLyHC65yFUv9lDKgaX6y32214DUkepK1FFUDrnSiaB68AnxexqMJ
+bR38sp5S/hXPPXld2fpUELZFJsnf/Rc/Yr4nz85QGdC8i1hICPajYJNnCBnK8/JbOu+3NesnhRk
auUxvYCXsQ9HAnlin73Q11d6rh153leMrxbPoo3GeMWszPV76O6Rkwpk0lTJlMTQlViA/IEAmhYo
cr3GGLbLIgPaWvk9CYyRvWMC7cLokqd81pwwHUXtc05Wl7Fq0TzuOGCJoHxD+pqrrO+8V3/i/XeF
8D8nq/jbG5QD4D+BVxv+EQaOEAJmM1/JKj7mqzbqXkUJQEhk+mKErvwomzwy4YYqS95hWDl87VCR
bWv6AJAeL5lqpDOSutNNAdJGjLcX6p08mrXuKzkdWcMgyT/oDwgnG05J+69ZHkDfpoVC1bNs8sf0
GKf682M2vo9fiDf1WNYlWNg1EiEgCkkhCAgsSH8TrTnud2BAkOLFN8vHIgzlSX54vcWfOX99mgz9
sI5fXTiR5EpJCgrDbrV00PUBhhLOCQod+zlrNBwPRvzb/0vrpODar/M1/NbfSXDDN0bvWv0rRXDh
7DDQP9f6h/Ra2RUUX0AbrozCTb9MsTm3uwgrAs43Xzwx3mGGNThr/B9AVUB36kEPw0VmpTm9URja
Jnj/N3lxjRg+JIrZEMjGpTC4mCA04BYEcq6INUkVGhKvF2VbGv7iENgfknWmxGzTdlxwNovrtULo
cfdFmPrtbopiLjpOnVB0Opk389trAZKpXXvtYYufFCi8t5bz1KLmFONIp+o0N02/SDvuF9bgu4Xw
d1xT3NAwbJ6pRz8w/89j9vVoETuw9XbwBfO+xSsQSh1phFvn82zg9KAF+KBOIr9/AloOxvBHJYqX
ndThSSeOSEvcktLWCr1L3q099he9zC1qqqhar3vgeO/vneHFuaAlYXNSoSBBiof9fF/0fy874n5l
byPorlD4kPzPQTXDIhvbdGRYPuRuWl27aWMLBYswoI1iX9TfXjiGF6Y2CNEloTKFlGyLJIo2UW3c
P9pygL1XBFCg+CBIigJxZbe9yaZ39OLqYHWaET2ywV0t9Y9rXWKoCjKgLNTAznEwPg4ADuYIX2ut
OwSGLyTzpqDDKmQDKxkFqKIHSECECECECECEYbMBMh/j8viOmGl6mhsWxZ1BKkKzKbRowwG+rmUm
pK34PgwqEIT5TTUZVI0QxT8L9rSWYaYH6AM1Fo3HX+1sMdvENyA61ZBZHu89AJ4zBBNDEsigyPdK
de/PJkXkEWweBL/1vKXysqGPZ5/Ya/FbRnSQFDz4+zn9ZyNzEyrQTPlWL2QnTsFp3v1lH4iYB5Oq
00nVYwGCz9l8JNEHILIrTAQ4FVdyTx6DTHwkSbTUkqGeFyNa60GP5hLcadOj8mSuXb/rRlmMBg37
ZuibOjP5p+BtHPxXPs9WnosKgUJ0CFDhRR43kB+DGOjLhFCJz+Zd9VxdwSefUQi4pPDmws3OBlMX
uv05uy/s86t43T9n6So4yyd/SkfuzHXH9uLda3OpK11iuvuGfHynoQNUFJ+wwMC16MBuVaABmd+N
tGDMoUc01WvhhnoCfLtR7wvROp9ZtX/LgIdv+5qsGMNsYbhcpTzvifFeIuio9kASBLSPRJMUUIJO
n9EebkUCBFdMS8Qe7z+o4IRDlO3osXqkkUdXWHE29rlY5SAyaZVbTZq3oqxuYYFnbDcthKLYVfX9
tRX0M2rAObbKZTVgaL34b2ktK8zbCXvMrzh+nwtCSPLIZW4hcuawuQNhlbl7LljEozNd89cmq1El
qK2pbC5kzPArK8xt01YWp3P4fsf0OZvjYOo6KbJ5N5+Mnt5gZ2o/9SzyEVh07zQHIsBgW8WtQc0d
tqcVnFTPtZ2Nzpu6pPIHNQqxx2vN8UfXlOL2QOXhUGJZAqTYW1OcFaFI+sGX4VaHGopPly58APAI
ADF4/1OswxU+YSxRfZiaIcdkJrKjZjjEoNrTy+x2bk9KwHfvVcAvqyddqe5k9iORwx9iqJradIiT
1noHQvUVOeL0N25vXzLTotLHVzE90Yc/0+V60/MpVCwVe0s+iad7nAluKqozj3HwPtp6a6WGLF7i
MnYkWJgRXbn85JeSfND13Rl/GlDcI2zG+L+EggjzMIYHAMMCDQ6gLN+7GmD1zWbLkbPyD1arc/83
Moeb7shhOvppdJ0BpdbM1Q6/LImJKI/7aTF9hu01a6IOKuNJJqk0BpISos+3gp9joISUlRKwbbAG
yvybcFLI+PdVw7T4hzQxFf6TKJQ8fWZJHBNl4tv452/fEeNrtWqZudNofRBtQIrZf08WgbPF6jv2
fJtDm0tTQah7qUrLrd2HRs4UOMoSc9XUPismc8reFcbQoCSN/5FvGCZxH0+zWsFfN0XDuVgAbTNY
B1w2a7IEiRIlxCEACGFRvFcvN4ERDMbcf11gGn+Mj+WxLX3sSb+xkNjtBxChmyDNwIC4lOIOJqLt
tkPkHosJzDhrq55m0KqiqkNJrLEEaIS2GF9aAZPc/DPd/Ue+6I5U2jus5rchPuTY/hvLNbn/SqAe
c7QX4hVUqDZcr4x8Pf3Wf3NsucqVu87SY+iA/AI6JHVnC/pIi+51149WHHhVAqh6wV2hy9z6X1oV
buc+Ofd26KHw6NvGmmY33YtPIFahGRsSU8BxIZYZcWNYtsHIKsmmIIL1UhAx/lPCuGQXDZ7PVg6U
/h2d2per1rprwus1ac8VXaimqg7wTuWFeSWQ+FNb/++tmZmsculr9VbvF9fPJkLAiyJ5ZuHdhFQW
IPpRaXiKmbotextXDUuwiH7QIaE5aC3nxQJe9qpwa0Kbz1395ywHrIeTDJBVgt6/rQ9/3iq2pgky
olaMMtfUMz8AtgQyJ6KzejvM+9Hx6kl3cqM488l5sjqWOvtTeFGpoePYwJEzW+5PzSpCaKE4m2KE
K2kORNhll3+BVQJlnwMzNsRz/A5yDX/y16Zor+k5o0l9LOf2xNytbzjOVjvz+Jfod9lFeGKIWBiL
qGgrIpJi1rbrGwNEtrfc4gIARMGGPbMHYyeaLE794j0vX3zJi6UIXeSXb+w1YVPrM1UN6tE829w3
EB6URu9CatYxDgTYGbxXDBzWKxF/iCzYCKHsaPs8UQrlcB2xyup6UaG2EwkeDnAWvxYrdmrqdvVq
wplUhOC2j0lvzTQtHLTmDxYGO34BQs/Gv8NhzSLSCAqkel7bEMVPcBlSBjAcpnaQB6qF9KOu5+Cr
eKwNpshlb3iA/63T5RIAAjnA3DteLq21qBeZ2cTk452D+nuIFqR/9GU9AtgFnOFpZUhqRWE5KGu9
e/m0ihHLK0kDnWQ5AuDHDZ8yHqZDl4+25AIEffMoBABB4EAh0ICD4PjNUbZf8/WnyvC6DBuXstxv
VCnOT/E3gOLD5v9mf8xV4gvLSqdC+DksNDcTh6f8kB9akio6M3KScgof1QodYB0gGfMQGp5st0TE
Vzd4FOCDxvDsuSUNUuktXe0LPkMknYpMVb5q8ZT1HRdqo56QOtXVs4AnqF2/XLm5nHNrMbfLHqOS
chKbEaDCtjaq/vcQ0AVOYZImY6m8HnJYf95APoIjWrh6QjQAEAggiiUxuz2sauk4Q3gHn0OI/XjP
R45A8mZi9gR1t7f5UyR8nQfQ9k3+swDisXq+pEuaiymhjerYIFdPW93m3IVkelGY1Z39zQuqJv0U
3kLBCbtwouDs8FNK2xy+vpXMJae+6fHS9RzP0pevcwfU7njRo+Se+fFJcNK+WT5sIyFr4wtUqJYC
J/gexGGh/AR0rN6X4ORhf2Ce+v7Qu0//ePF/xZcQrDVZnSWOHwOHv/Y3mev4hGvvEn+lZ7eDiZm7
LDYAAQAmo7AgJ3FhbHMa889JMANqnVvjUhpInO579lu906jTzZQDBuUKQWQvugYXK0GRgKZrsAIA
RbzOa/ou5p/5jMLAsXPrHTEbdhDxqGaG9bo9rGJl75eZiXoNOBbI0ykIxo0+v8K5/JumDSDZBACF
SizjBV9KLLcBOEN5e8hyThq6kRRHmxnypilPgCJIIAgQQCP5I9iMOMvHes/DZeze3dK/0Vm9+mCF
bDjbI3hq73B9oRWUhhConQnD1sbY/zJWnN2qpHIn5fI5aI0lLDP58wvBN79xWUJhnUreQsAUpGX9
R2b2O0ySbcwbMxGvT2kGC7h3ysspc5ODOOFp7eeYZA6RAgR+0WYBRCTfGAOsBSNrrFc03o5paK93
SKzW0eTusIIAgBveCuPCR3wmJqN6DA109izUW7xuHPHa9MSqiFSOu9CNb1Fh0IG5R9Wzmy8TGWJm
65GR9xrhH3hgFDpL+Y6upVHxb0FcuQ6BS6mhUdlYcSV+1BA5VBP0phfZAQCIztnXA8+N3xsWBIT2
1Q6DHLrP96OELylDkJ9GhaQCSXWNhsfM0GH8S33EeARCt4ZnU8PpWc5GRlNIc3bcKv62X2EI9bI9
29W16RiJvxnzLetY53cKNpsrl311ekMUkkVvz7SI09vdySCC4dQT+KylG6B8IGURF5flhXX2bwYj
90H5ll8n1SflFUrpoIARMe5P5HPVmgbqG8bbUaHBY5QcKvEBMccdZ/t66nToGUeHps8/MfZOVD/p
fgUWRqxJ9rnHGsYeWmHgPvvOaCARGgAKxBAHhRjZUDl+9QE7XJOyBAIUEvDN0k+9cVYWLLk6LFpa
qV4Yso5s58owexE+xfYJG/lg2pY05JP1FunzULWjDYci3dVgSgLCvme6Yqq6eHgyJftzc+GvTr0R
WZ8A6uCh9yB98PsOmdplz96ugVyMC98dFzTXY+NRWs5B6BlXliK7jqWZiQJCmz7oGAP4r62jEbA8
gIqwLwww7ohgrg7bi/x7Sj4vEoi5nSLG6RNl1zpcQq1TeSJQEAI6A0QEAIbY7cFWe04JjBW9VkPW
8CYfloHJ230y4nSOJTv3yeo4u5ywpa+zC1sX/vOUn46vQwUvpk0mkdGJ1s5/bLRemXoZoREQk7LS
woBT7NrCLL+v7kKfAsjz2vsKSNkpAUevDFonQl6t1awrTejv3PUw+2b29gmUiTIF8UaAEjfotKWO
VFXBR/+w44YU/QPCOVwXwym1PAK5XzE25LyCGcXmrT/8AbGF+vbk8P0l+c3gt3k4+bkjysaggbij
qRzJyys68bWYENVVR8J9UH4JicZaOpnKNdraDWxUJLQfFGTHQWFjQtcSMjmvuOLFAoEB28qwaO4V
97H5BIsULiEJ34WtxEDr9IBbqyarXGKdHeRv8WvS/0DBdwH7BWpZy0T81z8/DrCQZKI6C7MlxtGv
Jypnqf/ZwD0duixWz4A8ETk56bRlhl/fXkEOOOAM8Zkz1CJLKoM0wTd0ALoX6n4moWLxYoEwp3wG
fHX3XNJjvv+xeObj9wVdYlIS3G54/GioBaOC8+qDHXTTNad8TEAIE0mQVU2v8uPRpht+JTL6DO6Z
Sg7lxwPcYoMwfB+HmxHSIf3HHfygBF0RO0Q/1s8w8I/sM2xSvTN7yO4jj6AVJ0AVsw57++uhOIbf
D6Bb7c5ZWiZoIa57mAlJH+0kdBqF1qld2ohfUx9WkSGlkfPr5BiA9Xjd/YX+e0B8i6TBdEPqBe1m
qvZ5Pvqv32BnOtmObzcz7v/JV16BfaaTlcS6PwGZhhmGGk6xfbZZAQAiS+ggQALYuDzhl7wk4ud+
6Kr9FQovZcH/ar9l9MleFq6DK+kNNdpmY9PMQr/hd9ScEto2e7sJS0j8X6xsHU1bRHqVnIlZqTW4
+qt8RzzSgs0aeiBQfyZUBawfIMsat9puQcKa2yqHEbC++3qArTpBhFvQjIycsTq1NMMMNfecypLA
kzvYS48sk7wmUga7/fda/kbOitmOWHrZ4tqqtS1RJ77Ea+KorAwcE7Gx3HY9EFnRC3X+r+aAQAgY
d6h8IADgJeAmgjf2DQ/hv8mTb5v2XICCpoNuxzvH9l+RDAJ1xc//XD1PqGqWhzU7YsC+QC+VF+un
g0PgXDUrstnBQjsIPc5ku4QmWkOkv/1ZBAeMHH+WRCLMtOq7WSH8D2LtvqsQclbaUOhf0ObH+tNh
1Q03KsmLboJIfgIQBAhgaXpKQMDtjvPWiJbvheNDsUH2sx1wW+SD8nJLOgrdcjrFU3/45yzYutFv
3ZAhB8170ysAhwz1H+j9Oa1CieQesu1j7p75VHdlg5jH7ImxFmn3vZiK7TDTm/E9Lwxxoq4fsvfF
PEUXfWDMGKmM5gg8QJ8+P+XibHdP9Gn5hLhKlQW9JuR8yAJnuK31c+2U6UkvtRXfAm8Gt4+0fCog
Cj/yF3QkIALKgLlYb8VSVpW55VacnGkVZDHoggEfEP65m91wFjojYiPl0+lnLCPCMBsCBA08hARF
yiS0v6lXi1HVn6JAvv02QdmvvNZZfD3cUPyP2/t8PoxO9hnR2bRDkCzONkzkvovOPhKuJTTwO3/n
cY2QC97z1Vh1jchqJxRC+EfxTcGNDAzzE71QoSGvljFDBLNKGj1O/XO1X3YsPa1u2UDekIi9TA6I
uQB2E59NGO2v+u3fT+Fxvwrfj6P648bijuFRDGJWY9dbie20xyC1vdt/e6T1G+r4AWfng4dvjfyU
9gInFCuxymY6N6zr+UxhLc/rSi9FydqVvhRHbl5oTwUDBV8hJCFnSs7n58nr5ym+p2uB52l3Vwqn
OOdFoOk+IxtbyMwLQmVxHXjoCWn8K9VIBHGvPJ4Pc8rTgLdAgWTyvlF9SC/0elMxt9nLrZ2v1jTM
QCDgaj1WGWMzMcpOELW/0BRIfLW379cIQfbIx6SRCn/T4AIq5GL0dQZ4GK7BKeFEcpbSpS21cZ7+
67lmUeazmWlyCkYLIGcqm0YYXNkvRZTNj4IzJ2EkbXTlpFTmo8NszsWaRwEfji3TcVOe0DaAUX2c
kjAZD8bLxgMIWGqHT3M54ccnbQt8w7qAVwCGxxbAFBAB+9t0RUIBHCM+lfIQmZzaLBARzrQOvE9O
HBUZM+OyKYXXYvcSQSKT8o6shmDjhZS41102O6zWRY6BaFiqLSfjjdfw4BAm5+GHWS6oIMQcOgJ9
HOmZ0o05WTn4L/33c8uv/4OIJ7janR190uyM4/WAB+blAmzYICq2LAI39KxvwEVSN/Hh6nVTYZc0
R7bDjAJK8Bu+Jw/cjBFChHvYmL0CMFw2EiCI7XhSvo1unfyBLogEqhgf+o0YtgI+8fFXvtTiVc06
bF6nfZ3uwv4/na9t4T9lXHopw9giSWUTV8jGkWAaR21pppmhIwwUV+G0lYTc5cvNXmtdu3D5yxaB
IC5uSTPF6McXPUulcAjDgkQMu3QxpMyUBa6HX5GUeM5BNDmdWYLufmgf9XAEOIfXcwwZvAALXs+/
1pa48F+rBXAakrvHi27pANJN1crVtPlCrHtCg/SULnKPX6DmRxB1lTXodXvhRKT34H291sZxwbT/
nAHzgiYUmfasO0cVXIqsZ4HdC8J+oky12NPUOlE4iU3uW94Uvl/hhxAZCdPsSfY3SgCsTiBCZgZ6
wVE9lfV+JjBQzT2FrV9AafegzOnHFrVVMf8hEJdDkd3wj86K6xe85nDrc64XQ9vAkbN3Cx3bPLd2
9uKMou+32BosEJQ3FynKE/WQSSdpjLkNcBy26LMHlLmfg5V5HmYd0XNG1FrC5x9bxfqEXEgqYXHK
1x2apgSqNYTrsEkdF/aIECNDBsrmDzAyY6Ov5hGjL0KAOXWKR0MDOokxsF+fpWv9I7X8Rb+eRGec
FRNA4k8J4T4Xo0OtEGAL6NhOxFvqK5VVPk824dAwwBz5OECcuUwZ30Fuw9A6SxDOWs2m8VEfDFq2
/8yih39BFCYLI/qfAs7DDF5n/qRqpmjGFsE1erx7SQCBNa6DUdVP2g7UAbm42AgPpkVXRl6Yeku5
jT4+9inN9tOAi/ZkSgtmC38rKsuFb0HqAn6+I4n4k/HFG1g8L/0Zjc63HfCTGrGfDfu1VG7nBUtZ
gm8bO2cY1bUsCvZfq4Ba5Rf+V7QFKe+9SaHJLY2yicD9VXEV8Cu+K/R6J4YgAddJvsDPExigajsw
+PPmARw3zlyMn4TYCAEMQJvDqOtOqSNl6xX+ZXOx3JtH2Y844TqxGb4O6yaLykWdIbvOmmJnFam1
gkg5tJqvcuOCRAUjklRur1BUyj5kHbEJqcEKtoOlg2/IKH2XI0Ty8H1vBhjt40asnoTVaQ5/BE6n
rBsKWm92ZUXyK0Xs2JT8fFQJ7u4wKdS4gpeiE5rvALUq5Agn8zzlwHl0WfRXt53oLkmgaR1BuW7W
0zIvZEDi87/wVbq4yTtEjOWLJi7v7Aea3VP23VluAaXEWO/v0gMnp1Afi8kD4wHcPoJpi01OmkhA
8FdAFSFf6gQjznhNhrBYWlrg2xp+49t9z8pobUzD/c/w7e/zwZg7B6MztPuPR0NbG+bJc0gQzCD8
94+ynLQAI1/eAIAPZHnGdxnH+u/v8vxqChvQyLu5g4bnA4j6bsiO70biMYa/WaAhiRZZn1Glb4L/
BRAsOvCcgTuR4K2hDP3Bvf2Yk2A8D7/ABhZ5dK95qd+flT18oYUGiM80BxT7xEaBJbSYJlOm0Sqs
pmeB8w/otfqTrQ9vfcZyERMX0btbdBBdGWWKxSwRI5mvCi7FTWBb+rkBxgKX/AjEPA+Jd1n9yypa
i/k9AgnPm8pM7LH3zWHk9heYn0QGkYdEeoFTrVzqD8IdjW9lLScwrbHv8ixuK5RW9Fq8hszo/fn2
Fknnm1ba5ZwE+vx6LOVmG9YP1eqMWcdRWP/v8O+ddwP0r5OWZbREDIV6wHlW0sfVjZAyWLG4RhkF
anucFg+iJTW5u+lExLIo/u2UtlOla9/88v7vANP2KG9JKjZCGRrGEHiA10oPBotg7LXztfQTcgBC
0K67i/RMFBwbxRJirhbggn2ygCoQVAoHiEG4CXNi0OnVqAb2/Xx0/unJ417ngcuuj/n5oNhrzncu
CfyGAKvZSXpZeX6mRKqk+gERbi7EtuXtA321BJuzpfrUh6+di94OEyJ2kY/r4mBi3qujCWIaV7rU
YUvryTrROs3Bum+ys0/VsGnW7es9pvCVcMEzPVqTNQtA/gwbxKtkDWoMyyUV4QqsWgXIlNYvPCrG
6cv9S/gSyQTdSHZQgAXPLyn2PE2P7dNfXTV02LXoATLIpJGDjnkFU3cXNj6lC1qlyK8zPpTQw0qz
g9g9vpvAFGuQ8FRpaC57m/XvGHZvNZtt+1vv1LYcT77l94jFyyQVl4BK1C1/a3j4Lu+Q5JQ0IY8p
gZm5IY7UHPtgvNnrULxH+Lb8t/MVGbdQkaDwAYFhoHYu9E/scvJSkj35e6lj+AZWEoPP0thNrTpl
44X4ccQJ5LvAs5pNblFkJGZbstEP3ySngbzAf601+4zH4TXZzIVs71CZ+LpaRhxNaifD2vcRWcX/
fABuxQxTAX6jxSYcvekrm5NdybVq3/ByTQdHutFbC1KzL8VHeew9oG0JRMB8AgS6tFvI2xy8E36u
i+DwLnxsVpr67l6UzZOeXYcwkkvSsN90IhOrGboY/0flpMtS3KkwumpYOI7GomS0YvMUospheckQ
pwo0gMEMfoVoGYEGNayC5IGVlHy2YHYfsym+v/3aDHrAAEUct79h4zSPMn8X8t9R/As+o1dUZGqw
pM3BJYCJK15d7Q4NEUWZGq5J4hfmftGEsxHLUaogbAxltm7ZYaXBdK2bau9L/1N5rGFTCzoyXcqS
J+MLBfrR8QfVrknUiIH6Zh+JCEIQgHhh0Qh+PmihcLqB1AhcgnWgnmjupGYej3DFHUvvfHqOv9kR
InnDiWNqhO61nGd6CEexCJks3EGmRF8C5wjubQ3A8UPDxxibZjxFIpvbpRIQA1qDQHCTQbl8FV0O
oAZuv2SU0jANgZKDjtWLBmA53cIgWVviOxO80Dgg8iiQQO1Ti4m3DHUflcg4BTgEPyEAAgf2xprM
EYu+vf3ffuEmvfbD3pWxdxxKmxxzXtGn+cN/oqGplh/a90ElMGCrzK5YPxtW1gWu5CR6f0oNVuXX
a7/CZ5ilXw6WwsePxrnlDSDGJ/qIKTSAFSHP+rfYj2sv3ZjrPpibwmh2NryMvBy9Y+I438zsZfkl
j8UhHrsITIBiDBYhEIAH2HQT172wOJZnEw7Lz/6i85/mH2vcPQNpOqB9FtJmjgREO4bSCfUzwDtZ
JXaIB2vXkhxuGaAAIUQMQjGlqQDbHQsLqh7a5QwwIAFAA2ca39pDpeKiG8dZQv+HaljEyNgneQhu
yFI1Dn61Y9/VL1YcpXLYgE+PaDa9QfnymVJsVsJjoANG0swsA+YhGYypn0b+zeqGMDifFcAuoZ48
djZPnq2rTDCWpRl+NJ8Pkh4aFdymn+cdnzZgnbWKiEBc0UaWuc3y+h0Oh+mUSMyLLW9BHfO9j2Yq
dM/Kc/X7TbQs8aD4+5LnYeuPe+//d9j13sekO8Fi9133bCEYMo7mki2kLVaB29PwOGnGlLVy43Jm
TE4cKugOER+UDzXgMaSRAmyTSVkZWQXETnHABlxtobC+uwC1kiCB0p2bt6fme2/b2g3DDv52RSwo
JESQkBqiUEJJGUUhKUkg1c3jygBjRUkJZAxpINBvrWUxLHWkTAhu4HEA8o76i44ohpEct8wAcknA
howeQQzhQ1Eg/Xuennv/FHxO3ldllLqUFHr5n6ub7mk9PqacfzW1mSButFLwOCaGvnj0mN+5rzhj
YTxzQRPU8uOQsfOxk5gbaLfMKe+7Fy/C2xjpj2MdwPszk3F7v7tlmy1EopoErYM8J5WoLexA2NO7
hAliYeQDr7d1AuBmarIDC1KhX1VlshZKZERoy0/4O0VdWUwz5EQQ4qQDgAgRSTXxj9cLUUf0+hzb
Tzu85582h/X/tJLpQfXqs4J/WWOYEGMQUwRNPU7nvhOJ9AIip6XqGwDrFBwKUu6DySbhEpLUdGfh
7jZRyENKIEgGA1jde2X0IdpIEQmYZhCEZAWLYZsC+L7I/z2IJQJCUwRARozZ0Eked2xKqmoQDIhL
exAJhGXFE2BNpJFOUgNBGITZiJ3eeyV6/btZ14bu+Cy3JiRzUpz0Mp7TfvrJeQiOsLwqMp/gN7s7
V7Rj/ksoWZk/TcoQKVTHevuNwavd7FzhQDwl8f5k9zYknSbT5Jjq/lzuh19lcZVpMP5lC4ksKhGH
gwCHuT9HEMD4osNImYRDkujrhuFYJ1cKCEDVc4Tz4z5YD2cQXuKL2tzfLndJnE1xGjkZBC16SI0c
Awnh5sITDLf4AK4KLBxqtOVC0GwQuTy3ZBz7gYiZC5lw8rY4rRn7lohhKVSizZ0AEVYlACTEIdzy
/AHG8LhMw6vY6R0Qay0hRlEkJ0S/tORWhO9wMQOY+dDozhjA0BmiQMRAddgA3CC6h7DdMh7YyDax
TwMwPxnoHYhijpjzRMUA3A4zm5rDlVDI2GxogsolQdpIcQS4ib3kaN/UnNSAHHLUUUFoTOLeJFAL
wQtALA2LpZsIXocig5BLjiUU8UcSg2BFbBBP4tDRhjnVqqiNOdUS5JTexYtNbpH8XYVGOLBTkJlr
HfCEQOglCWTfMTLdNja5iCmIK4BiidD9TigZ3JuCJ9nl63UG8TdffOKLPwxcx3LRlPwYn5GvjqTl
Y6YKlWM09MLT0kGJjc2feFGjpJ4aGSt6pwMNbIFhfWgvXx9Agq/SfF18pB4BmzbgWFGDC5Vpx3G9
ikidKji8QAixEiBH2LrDuqUJtjNcZ2eRLgmF2qRyxomT9uuW1qBKMMzMJmTMhm7gMbqNJpSdwn86
S8anAnoedKCwxjRZAvAaAoCQ9Tyacs+UwSJwSTkLFvCRSm0I4YME4K8PIkVm7rx4Vz84l1XLPk1A
QQILCIxIySOnjO3tqm2Zo88/r9Wu9EEOKsV9SnEEGI9fI7shIH5AG9lNAsz23rOo0lT9gLBe3opP
+fX+HI3Xun0iO7S7Zw9k9YPdbqa9Fu6fCYYn51PjBspjUGM0AgEex+7nIE/jWLg30XMjMCIxOvu0
kEAYKOiPAuo/N/qlIvRAxQIhIwI3SuV/T6qw/3oOQCRueQ8gfBgAGrvREL5cbVLN9E7p/xiHgcOe
xWvtmjkbqkp4jSVJtmEIwinTaDyLH1s4X9JIRPQkEaAnVBoqBgg0p+lGw+mcogT+kggVYEpgD8Qi
ECPmSCFX5uBKIEWNcwkIAWj2bNFuWax2SvtSxezLP6Y2/bJc8lWIl542qwiRVL+udEsCvFQJ/m7S
mfG9cCBR8vK9+sPWFvxs+VOh7XsH9zACQHPsJKW3ziTWAuakkqNj3Wmm6ODRsJJUsM5Wn/eh863f
xEQIJO3cPicS/flG8nB0Pllpd/OGxvyen9Ryub8Tn+O+bzwTbZFfZpu8l4FSosS4YCNeShr3JAOF
8wQWLHtyIhMMAQKzRdzBjUCUEYRnBh+RsMhR1hBoOcaQxAzPlZuCoJgdgagQWhJPAyYogYZnDEAD
DwMn3XofLwJCQnOzzbn883y8H1Lzse2QaU6wPxUAT8FPe5ofqMyM1MUl7YqyZKiwRIYsejQCLrEw
nqbeNBCPF0AVz44ZbIwtMysmFfs+P2LKAFtB2VVIPN7j+8IbiOsSQQnPTklCg641v6Yl4WPi1zuP
NtInBdc+zsUR7Pn0YkpqZB7NOFLkglPoRwBEeFrHi0FSHB6q1Baxxyj+fN0gfdHZ/LaHdSiCdqiB
ib4hrAyPTGwIzwoWXMd9/5bRYN5qEpSVIQkkZJCSEZISQkIRkkhCQkIRhGSQhJCEkkIZWggSIxWy
D3+xBdoO9geG0Fbo8U4ioaBNs4oCm2/JUPf6neVNM1DpNtQMB3D0u2nGO6uycIoG6vW0BywAIPLB
TFrUnQE2l4ocmHxIlx3DWQ6Myd/Wo6eMGQWEKHlNAXxcn0/jjb4eWabC5pzAoEobiQKOQnugaOO4
DrcwLhkbKENgkMrTKyUarvESEjc1DI+DeefudXra6fR4OT0c/roHs+ndvl3m3smi1HVo9/bPMb0K
Cnw9js+wUW3E0v0pNCyTPCFMHGU0Nz8lmDAew9IavAn/VFRoEmk8Cpb6igF2hJEMUqbqwJl2SzE1
QaiE59UE6ZGEnoeIl8RWfVpqtL/jIOn+MPVJ0wMIO4zdsWfjGsIoRNKteSWFBv05yZU8H2TJexoN
Dq9TdRg5LELd5ntS9DAPCqV974hDqWZ9WlZvZm5HJDb9mFMEPdktBS1yRQt/Q2DH/jnElSBX0UCn
G0afG/H+w/poY4E/nDqbjXQC9w3ncerq+tyyvv22/B3hyepuXirbNFfJ3Kv1yAJe/a/mnLjB8cim
ze2ObrOtdqcHR6y00fe1X1t7SImdZtXCKRkCApVtQUvWpf/IwEL1MDw6oiEIhqeibP/ilfWS7Uqf
sHgDOxv+WUWrFKkY0X3yS3NLpbA4CEXn/e2k1CQcb6rw+cQbbPAVKKPfF1XpCybewV1Sb2Dvr8ZD
50BG8EZArNPgAjKHgyxmQln/0OJxLPzdgq/799+WiulXrRiklcORJAhWKAdWYcCCEI1jIx7Bs/PK
732h7HJMcaL87ZFSjgihsXqCix3FNEdrnFeN1ar9PXvmU4EHpdV2hhnIO2oEUgsqjilNMsCPehaq
cSF/pAlBK0OCKghpgaH44/BAvYGBiEKFnxg2PXGZGBGGv3sEhIRV+0Qu4BAhDuHTsNQ9eG0ies2G
vjHyMRMkkdsBKHet34Aq2gUaI8mNA36mCZBmFkgadl5dDyNlMhLnx3+QfYAGn9DOk0TglGiTxLIZ
Mgjh44FkOOIAZNcKSf46TqNoD/QeTzLCm/RtQEbZYAqAgSAC03EC5KMJ3NGTwISfa4T3Vb0u4BhQ
SwvRx+G3yKCi6gNzRt63tXoq4IwkF/PW8GRVcv/rvKmzGmuWy4WFJHPCU2JZ91Ai16qo0e095sH4
aySkbmfOewIXBVhi7YxyzmSoHkkDxCEagdX/DPBNtJaep1nj5bVFDqOFWOtXYsPBoJ8/gOIvZAB1
QI9A2QCEJJAh6pzWRmKD0O7MeU6QikCPi3tPsUkbPZn5WsRTHtTUKAKLDnd9wK6CQeFYkSKCyZT6
CUigNHS6DHA8hIiICMBAgBIRKUArFu36qCfI3U7l5Q6xcfnqekxmjLa/rcbc1jaNQRI3+AcbIW1d
iFG/6eEqzow/Ftg+BW7oRKRlbbkv2oVYQB0kDcJOAo9I5p31l8prkd5bDWsbzj/l02iBdPxZvfYT
sQtLpMIRiwOLeZivMaW6XdYlYBMMQATyTCYDyxT2YlkIaiyag8vPNfH9b7z1unMfyA1FECEIQYYY
YYGQ4ScmEzUsaZSoPGetmsr988uu3fqWzP8T/NP3FMs3AHMPTin2acwDjgweUVArYggWsWUoyLKE
VIAf2QYRhCQnbD4aaAR0twC6oEBQiQRDNyQKEHqDgPdEsGvuf4C53hNoUPXOPPM0qCfRNYd5AbL+
loH1pAAuBvWADcLhS/CyR9Xkb6SDPNhcdoC6l23DIwiQgtIvnquEkihALDgEFpEAiIEnk8YthD63
M1bZ+FYYDlYZchvPAzQfn0eh/7+FaPRP+w/uBUMuAo9zoqJlgMDtgntlciufGLDQR4LLYcTf2Gn4
RLEsnibwZz1dcq6O60ih13YhbOwrbKLkU/iFPSkHwYKthUpqQi8r1Tkpyumjm986EPoJCUP4el0+
GrJAsAF6ARkUjdnBsx5EI12sot37PT/jO0u5yv83LNynP/LMy8/egoVMDxiwvIAgvlU6VRcgM0MX
MTsQ/Rr4SqYqdbpWp7mIG+f7fPnq5jkBzlEeuIiWO2LjtjYgibpQGOImyAYQpN/t0pslDcWAXQdi
BkGAOIEDB30WgMyI+qu0aDJEBx+zsiFjzDMCaQRJFTxNvJ8AoORYsCugNixByOOobRkJYvcDTQe7
3xVcxgMICuofrxgA4goGah1GoUdh46gpWDd4oJjYsBCxpS5mAIfT8CCdM7YAXLgKkG0epZZOQHyM
GzY3zP+3cHHB6B/QBAqBbdvydsx+3xGadvDjShTn05EqO9zcacog5pTjerK+Gly2HIuUsQY6TjyW
uoh7/apd9OGz/jrPnaMeZmhoKqdX9YU4YAGZAWnI1HxSAZHfY0eBW9gwaMKVJcq9Iw9K/hOPs56K
AaDaEvHMHbfJx8rcriIW4vXN7vaxocHp+xi563JY5CWhZAJEfTDYHBeE6V8MPciJtBzAPLJ831+H
LTunS6wTqXa1AaSCcEH79UDNPoeGmahxDiJIllj4xMPe9lrFTyOPfhgOESBmAKWLASloPm95eAGZ
EDWUongD9YND/PBoPpwgfUjYcCw/gFtaQ98xTGB+rmo0LmZhZvg4F1GHznBRws4wtasCGMZbGzUS
FrgNioNQKMMqlsZhLz/LuncKxCYVVOlCaXfihG3EjbwD3TQ6zGswvjSkUiha1NPfqMzbhV1bSd/X
aN66lV0AwhPFYpdKYpBv2cmcbipLLG0xKmG8YrSDZoyJD0TOmz+/AMYw42Th/DzH8vihDiSV6evy
txcQHa7Ms9E9MwvoXZehf5cmqCUTT+G8yoPhZbT70dK3uATA/IgfIJfrjYkLjRsZzVt0mMpmgZy/
LuNTnVae8yNZa620vkTQIGoS76bSO5wN4vM2Ps93GKSyoaHd0GhzSut5kAI2AbcAUDKIftc3c3Ce
4D8gk23DCAYRK3ok1UBiST+tF4ofvFOiHuDDBRfvV0HlJJKUEMmANqMieEZhxrXu+sN8unkR0UAU
QIpzrWkQIcYiSgUKAKgEGhJPcFLQlGjIXI7VaNdq/V0XJ50WNECIqBmBRqGcmEwIYTi8uTkkrpq6
74+t8qq5609/uL7xsgWq/Xav2zbUymzAuJMOmQD0kB6TsxLC8v59CdSHCmTMOh9DszV6TNFxisFB
iTBuDljzmjt6o3dhnQx5YSEQhCHzADwQo5EdABZuAD+x/PnbSrmOQdk4Afm69K3H84wE4ywib4K9
31gI0CGo9dwZAfkqTN/XOADIPuVV+6VXbB46i+fPzZwWzOSIgY8R+Yu/rCEV5cXiHyuSFKEUhLLE
1hX3/gd8YOIkBwKkggBNJCDVrD+dtQzjZMXTxXssHTwcaQUKmy8So53Dswi/v6zX5HQ5cxyNnHIt
7ydE2n2H+986H81bTk1Ez4dZuFAKEsRV4wKo70UGzB9TwAQDmAgBBygp+nBUNucznT6ve9Xj/fbK
xpqnpjl17R3dYApt6vw+vJH0BHuIJ+b+BSeMKX5DIhpn/Vcl/OkhwmPUZJDj4pT04sURGiWaksdw
9+d0s/fSHGCYa6dXy5W9Ver5zTSdU9o7q22XGYcBcqe3Pl03ZzIC6tThU0Yp4lRH66f5xwPtWcwP
UvfYlsce1n2o9WGsciAd+YHtzIAiUnrYFQYePMX6zSCZbqbiJ3RYPzbhujxG792J4zygGlRTWbrx
rTJRh7l+8y3y4+NH1Ga3/h0BqHcBOKevBOLEDfgjAuNk5Cq8O1o1msyE5Zy0Cy3D/YHECxyDSgZh
oTJPMUV+5/pKEKwIAaK1iCeCSSiYFIBw7mL1P11BY5mAzKnFYkUl0dphssnWsl8y2e2GxV7ALsbC
LxB45eLeRF48bXIPH6P6SORP4lhrItSYBD2Rk5AeazUBAgQIEOfnjd1ym/1LNRPcDHxtL7521Nvn
b5TujdR+FvyB0wXIVYTiKXosY8QXA+4xMcAacjq/Myi5mGQkQIjt3UyuSho0BZ0Ndj8PWmScAhQ+
YrUNm0bCWBJamlzdBEgRCGgscw9DcNkEyipQixAeV8zxv418dVQjCQGCsBZGLy+PvthmH/aUTEYF
2NktalG1pY3ikaq1qThYSpSb58P/V3GZ6czTTbZ+MLPjvF1G35+Iw8ra9ZX6pqTm/3Fhi+uu4vg1
FHhS5jxUTBNAeoHp07lx2hOjULqr1kdXjjugijgp1T0E0tjBwWQJCQkZBazLGI8I4m06DpOhziGs
hGZuwXxHc93ZfIXH24ngEIsCEJsXI7bhPccBxNSC+rRAosRkjHcMVpQhbsjhXSCnAm8PBCoEHfNI
jQtyD7PxnkAUPVIhsTeOMDyTszlO2qu+cZ6IwTRBsDyItweZ7/8kyE3OOVAxIg1EoA0iZtcuulMR
upwcttyUwbxVN8Ad9X5N67MwBXlnsNSL7XUfG89c2c1CcGGBgeIwHo2BFeeVPYlPx8E9jm7o8Kk5
v7EnEJnD+1eza3POd40SOc/gV/3AV4iMmggCQNPo2Lg8Lk+GY+O2+zV5KPvMizu32lO7k6XHE3aG
mrcpa1d0Oap288YjxZ3dUJvNuZweTZch5hJhvxdgh64iMklzsYM+usnkGnwCFKCcSraKi4fnsBPa
/WUYMZM/E6ZuxPJkd9Ho1P0FZkPEmxiIfhAJ8UrwYW8SEIIEbQHP4hwDrFSNCBeqWNQIDvZPO4IB
zluMk/B2rJA4FAEwgNDnUFxzwj0Q3CSSfIGTcOQTYswTSSpr4tP1jgx1gofM8y7y8kDHI4WS+GjW
KP9J63duCNKNXCao6SrJ6WPyluUlfvDywVDQMaSBMm1NRHNHoEzucLb+Zz7t70OnXzcQzIGlFz6G
GHvaD6M78f0bewQH8BhGofwCoGt/XebqLzrJZr7I+Fcuj9PIPx4FPtOMnwLmFWM2A4cOAcFPBkww
pDDlg8gU8UPbQUDunr1of7vukTITNPLs8OHwqRDDlyFX1nZJo2Xlo4jfoFmoif+1Q96iYngOWSi/
wfmhSzb2pssIvOl7XMPPBuy+m+sbfSh4HauFtV7n1UmKI3eT/MjqA4Pno0pmyZqbLn9kJ/3e+UU9
cEazGHCoUPHcXnXH03hZxJJYyjp3oUJuTVGu0uIAW+vnf+5n63TbJnaqbz16195ufEmgm61Pn+Sx
yeGUILrjD4EoBGfJpKIyQZdENOZk8OcOJIRZFGCZns33//v5NzVlzQV8z87BNT7DyXI56qIPd4Qh
iAmlQXgOQob0qh7OMmJBCFu2qKaG3p55wf/dgwmkmPsvYdfe0iuCf2uH51Hzn0tWtTawFJyJlDQX
SAjAVMkjuNVYH743U6wgSyjYMxrTur4JSj7of0pE+K+hVJ6ls8eW7HjMY3WykipNU3B6Ji3NbIfF
B1x4nkA42dj9n1E98Nqs1Fy+vpyrBHUGZw1TSDpGZ9WhPF5uGh9K23SdpyQBmbxVpql32Vr8NZne
DkRASCHuFWUtlk8TEIaOEyIgAMhgAYeYEgTtp9o2pL3zv1bYmoyn/qXIKJ5H6XNDp/H80Z7tPygC
AQFFuTm6uk1ba2vwgVkTbGPMF83TTLVM9GRt+3zOAIrUgytfZ5oG0xLXrhYmZLRY3DpWMG75Tfej
EM6wRiVICcKa+7aO1P8jrXenbrKqrBq9ZpUKyYnYMdHEf+DNfoMsRk2uoedQUknXkonkrglEEsQK
AgdOpjuj9XOyhWqZELlWdy8PCoqOveP0M1KCh5ZDDJKM00V9kAfdf+dfvd+zbRlhOZQQTk242Ehl
7FkS+RqEpEsXYvqH1+N1N/C4OC8DfWrbachU8AymhZAKgo7ttdSzGQKpozRzPxM9vtOzhOyN9KPv
nU4VFd7Kzyea6KM27Ic+fShRdpuGFJDkexo12eLPKOBJONPZCIhuhMMzCarFpn9tFKcTT1+1xAOb
HpNQ1x5inXPhUI2wwI2qKVDFuNY2UFTdQLbnstBxoRYxOmKu16pyBwVWDa2x7HxaQQQBDhlTNORt
0z9Jq8vx8hRZXwpPYnq+3yuX5WG4m9qKQvtwGieAA9ei4empKhc64fK2s0AIAOOZQeaySh2kinTF
A2f424H4yvj905aF2gPjuS/dKCRIPnnAufwBILO95A1hp3dtFeFdUPk8grCczgRJVRK4WPQC+fzz
i8yr17nNIp20fqwP6wJxSogEAFnwXshP6saecB0mJ9UyAIEF/nZn3SJY+6ccmUxdGOP084+McSmY
kASAIAMwTgizAfCBAXAXFxcXAXFxdcAVBplQNikpOHr3KPfxdTteDD32G5x4meoPVkXG9JE5P/qj
g4TidxdQ9Iff8UsrrEWUgQ6rAhxCIfi+9EHQW18Qjh4I5jIGHOmdhqDqJKPhZms0zRiZlZYGNB9N
968/Pa0D5JxXjXx7o2HJvQxcKvwpGwQ3oRLXp6QlIcpLu2tJ3VNiykf1OGVyNKYHlOcwzfa2Dec0
038vSzzR05ZPwfhTUozyp49GcKx73lF80tNMnxvh9apDoTD6B+7E2+sQx0eGUyJNw55QFmuZh2EL
PQfAlVb1+oj2sKCPYJ7gqauVocr9d3O7wQVaUYcyYBSGeJlsBVWdRhHD1KvTO9WFo85M4OURqhtx
nx54Mo7CgvkblSLu8/2WJVdm10oxR2n85L94Zt9Y8VwT8mVJKPGsxbG2z3G51Ukx1IY+gpQ4OyoL
0ZxGiDQMQm7PCl9nBHYwfVkzSasGaxaUeo/BTgYkCdmKsfiQ+gP4Lv9MgU0nsNWxh8KYM0aD1hJn
lL9RRki3V3NJkMBs0HNnr0gF06wm9LhiDy+ytyGyYoGGKa7bFcPwEOaZcpCxWDIhLSphXRwSemLU
OUpX01ngeoDpM3ZfhoBNOxgU5xXmxcMIEMs2UfMM6ADoSJkD6wpF/yE7AqkCEERhKgpUELkCASIe
drAQpJu/oZIWPIr+sqAA/W9RIwtfQYIiX5S7f2jLlQqooEfGBfpgN2RtBDA94b7On3t15Rd/pHt5
Z+zdVC+L6Fpqd2G0GiJPw/Blt6LUlQLQRC9JimWJQ3IO0HCGXK3dFymwtCnp+npqaRrGjRbpmN7P
wItYhHRuQhBJvB4D84SRu3+eDB6z5Pq3MmSJSrnIzvtxKdxSgm143+xoOCQIY7+GA+2NVFGE4Y/R
NVPLAgpYaZXsEfIqrV5ZsXA8g2PqSUli4r3OCs0qXWwLN5sozvRU4CNVb/7IpHg/zql8EBvxeTBe
yefoi7KOYEzbMfs/S2Wm7m97shCfY0tCBRC396pszLMV24EFynIF4M5WVNRFu959oz6YOKDLOovJ
djnx1rWOn+RZwwIwyw9sONGhwVI4GYElpFmO03ob2ANBooDCTZfZ9vXfrVlX+jM5/iPdO0JRckZU
ACrTEUzn+J9UDouw61k+fpTlj8wayT86GpNRPYIqGCSi6zIivIfLL7g7NrMgH8gEOYGMKUq38RC3
Z94QR/m1lvfnjPtN+wHMzm2KN5avaEaTwidBY/TQQZ7/nb+uHDAz2aHNPJNn8w0HTG/zNmgavAxM
yJha9Pe0vUQnYxcZRDk7i51XxsvS+Mf7v0VAE15zbGlawOmVcQClqMR7F5V8tHZtixvLYH/mJgSS
b2pYHo2bmdJa9JKyHG2VFIZITnADMPGBb097z1BO6jheH6tywjgOhzuN4iv4jle3fN0lDWZwqQbp
/ieiWAiyTTVMnRu6FRzoCLzPlAnz9CdB4Pbb6hzLnJjcufv6zA6Vhtbkwxq0Ti1chHlQrtAwKHdT
NXytGJl0+3H7BqmoFsmhIuIgg9/G2ppHPNUoae0DEJ33jd3iRcZeRwU4fzSjz8FLMlPElQj7b9Wy
P+ljd67tB4tmWyytrishGBFAlz0Xi5/LsYS/z3zeN305vN1uMs77nOP9T7OlgcJEL1fiim0IznSx
XJPu8Z8YCAECWvpJ4JxFY6KawOPHIsG/ZtSxstVF9QIpIdM3P6ttkjorV/KSET6R3SCOrG7hr2ZX
rroln0X2lh1gsAo3ns2G/1Q/M2MnQ/fsFmtcf+LaSLjMROqCm5SRWNQeiFvS7zjB9ctT7YGPWKIC
o/xoI9mHr6IDWOZzN9UIULQIT5AbYaYO/oQIUSpD4tsueEF8ZpcSpggUby1mAlSKmz/CwJUjH9O7
AeNH/Px8734rmsUEGzLLNecRcC0N8IRX8ro5gv3P4LK5Oan3xoAIJ/bIvxvVbeTev+hayXlNTUbE
8lCTmo1f3zy7zKHGL+UqRSKWroS0UzYXc6G1+VesmT5xl+Om/cGuwYYuglymmHMuQUnQ/xT6ZfTr
utk4BEMH9yz8Xe6QirNnGN+GSnyfv+MrkxGZPyhps/iQcp7mrKgiWO+xzURWtGQw+/JYG1P9w0Pr
e66qGqdC24ofLsbCnwhPN9ClzKXBhB6P2D4TBg1MyH023g12R0Ev6u9r3cfPJ45tPOqakuRifJ8N
A8+3mIx4KVGdy3bJ2WZX4Z+vLkis/iGYS31HFYIsZ9yiV/yokI5eS6BbdIuW94xtUtvcvkGQK2Jv
XSqhjMEzAyPjOUeqdBWduZnqtvjalXybbNsk85oa95sJjRFWjRgj7SYwulaMHZ2kM3YH23K3KA1A
j2IXQuno9yc1QOF0UhKIam8z+K7B6zTeWY0El/y6G+8Xqx+U/qDbmqFVq3acdN+QLnetQ/6ABBAp
e1QsqVSgOGKhc2C6Ptn+74qXGInFPHa+MOC3ZsGkISogP3I+ALCqSUFFBy4WBsELHzXpm489DSpk
H16pfhG5yYJHwT6UFkGbEpr23ng6TjV6NNL++ir/eFFOTw+/D7x4shqTiQ6Eqwg4zg/KAtcFLR/s
VNlWt6hODAoRe98ApRP8yNU+34FIHLDPqkp6mrLiylyrOJ5y0PCKxMc/CIlZUf7eTe2b4NT1I0OO
H5J3pPQHH5FL/9qDpMEYkbgHTGV0HdpG7BSBIFb3n2JNkVFcq30N9OpX33V2QGsOoc0Im72zBpl4
/htsRuMet9EUxMYwwjnzQ4ep5cdTR8FvticVgSPK1yp6RCJa9fdW6uT+dq6D+ExTl+Sj+YRHQyZx
NUNjxYDdzctp7DeFNwwHQfiU+GQ0qRxlsLlrV7O0xA71Swwx27oeshbiQCy5hHBuBRuB4CulcNV0
rv0YVqZhoPwPnWQk8CiaBmXSZwsiD6kH/1VTnEWWokjbs/+NLslrq0kWJsq8NgFRHGv2P5DQ/XZa
5zuSZ72QoPelNzbW4/zXXIUE/TkM7sVZNwIQfABTutma78+1gLnaC6A67GLHJjHsF69boaX90Zxf
22OS8+tlG50R+L6MimwtlPCh38qFgaDvZolw7fFccgT1m/0Eoa1bhx7E3IlrILuCwNkkrK1Yo3mC
z50E791W/6LTA48fab8cclUPcym8q59uqNZTEsOa9CRs99f7v9YVaRjGSxz+ChNfTbO3vNzMBACE
O/VAJi9MC3Y0do1c3rqJ7yNTKSVRtR9ZpDFyuGk0ZR9xmM9akLe2GVvY7wQjAwAbGjW8becxzJFw
bHzkbWPuBo6q2Lhb7QQre2lJkNI+vkbyUKg0PDeX9KFDVNcDIpbLG2oXKBV4uPqvg+66DLqNQkJC
RI2U3AQxxXCvYJf8Mr2vkGAuVzZYBxbyvfK9S5mI/ERQvRh21VCr1C0i+Afz4hsQoUdYhuoQ2Sdh
iZP5il8BQVkQ5oSag8UUsjvqjwBj43rOodBCiKkjBqjwLZY6bXxJ7Ogbb/Vsfa49DfAO4lMziHLt
z83CZT3xrFGUtD8zl6CLuo6VdO6/aIJhFDtu1UtXYzRiaWxj+geDrq2c7QUvvjHaZ0zX7tOp8I42
Um8amFiJj7ZP8LB+8zD1gn4mifzW/fuisOzMLnvPwTWoIF6WxJLFWld2osc6opx2TrQqOine/drR
EV8FPXu/hq1hg2MbQgM9WryKh24WrEyVKmGCeyJFIifYxmqJ3cVfxnGqBxfCpTciryPbPtTNEkLF
VI36rAxScaO7+ElLf5pge+MUL8ErGD9z2CbwXceYob48ZGZ+CQoc9ceDsfn5K4lo29F235o8DIfz
+tXT44oRyzTcL3GmJwvVYleo2oCe/f7SVl43vMwLc/hJ+Y/JlZRjYdr3+P5S3bdmto359dLMGRn4
rOF7kf6UZ9bJD26LO4dmAliCcRblc0vDYc6bal7B+Ar7tkr/XqSWzasvkW/E8fJEwB3clf0uV2wC
tqdevdjCnK+130ZejLT2fxfAcBFwy9G80O7kw9ljuZsdeBbe+xkNkgsILzl8XWG+KiUmOgIscTzK
AyGI3GCXnV36C17vGralxBdJPyRzV7Xmx6p8lSSPMvMpZBy5/pPWvx/y37LNrTiDl69X0j8p37GG
u64G1sYsec+Hg5fiB4K7ffGwcEKsUpULWnFmrg8fUPDi0u03bzs9iM0i153HuyPQvOa5eV3olRkB
6kumD5p4SRP3OvNaIuOEyto0OTijJ6afsloFBOLvhEqgZq7eBVzNeHIyD14yA5nfmHoP1Q7+3TOS
uylA4cG6MLoJvZ8ZcSTCmYW36MeoVxrH4YWXHVPZ3Nwkhe+rymXlPSw80/7yck1jDaRYLNFgHvNT
ANVX73bxl1X5uPJcebx4njmQ3vIXjV+MZcq+We+1gACKlEskBEQChFBFDiXoSIwE1w3O5pYWcsoq
eh4Wfg3aTE29R77UvPY6gLcEaUhJbpEGQJ/mqz+2apcD6EEvBYlzjSTf3EXeY6sVrATht6YjDTWA
9Qro5QQ6HMPQ2LfrKLQOiScSYNB0suHkFqF3GUiySwWJd9L54v1C/i7QEJ0t2BZ5WHwK+BlZiGul
YQJPodRUYVflvRObLmZMFRuVBrLjWoJthooDg2RMJ/xVFe51bjK5hPMe+6VCTjYRveQOZitPeYMg
7s/KbzjtWhKtvHCV+QMmoklVRP/GiSyikHJJq6x6NAQVMg17cYsoYEpSMoX4Nwk7+IHitG4sR5sK
1+LMuzIOxOHLJ8o3C9zS+jPW+fGOo3bNHr4RiFWbG0+bhsfquuCftpG/URds5awzz1sswl+eXbkb
OodidIfsDwRdN01weCRmKVKkwa/B4yrpWfjM+LsUhmM/9Xf6g2KW2OIR/DxQ2qYAK7s+6lvXq2Oh
H/IXsiS5bWVh4+xHjV+vs+6tdMkCQ4OLKsQM449LfIDiUBQdE5+w+5u4xUYWCqVqfrfEn258mPmH
+sVp21jLV9lsP4W3tCXfv31mokItSztogzaXnWe9R/V4muvhvVXVnHkjmyuDSyw5zutZOFa4Qq+p
Vcbhl6I/ICZx4mjcDzggPYMfZGQhMeprP0PQeQv/RnmaZWR5KPfpK/fGRhb+suJLTIKqWj7P5ugK
WQPnuhhgmpDYvm2rPBnzgqW8pmUyzVc1mqDO2mXYnRerUBG0pK36LFaxG6t34R57ZLNpGvynN7Qc
j+MIKm1+ntSav60QyJ2BQ9Brk3Kr4rb/M31jdstzeTbjAoIbAMG+aXNbWBvzi3xm1KEOzW2YrX7N
3I/xahjmz/iPSyLCc/V8vuFpIzb/jU51ydrua9n9Q+SUMaKnzx7GxqFNI2cBNnuRidFUZ+pYuAyK
K7FEaGa5bQkG8o3GbEK/e19mpYWe/2cO1vng5V7doT3/ItI05SE/34IG3R/Y1lYA35lsxYJcmAQD
8j4Dnhcq/dUM1SlOS0n9HhRlqEEc3Y5B3/8RU13ggx4C/+ntDnXts8aiH8fHn3WR6BRX92A5E9ze
aGIckS4Or/MhAfwK1aqdXH9zSw2nWOQUAv6Fm6drBXh875xwVr6KKMvrFyBe522jCJJ6HpsqLpMl
La4zvjFSipDP7v1Tx+CxtXJt3zpHU5fF8rUPt7GBC4OpRQA8t3uH/l4CjP53h9N/yltVpXzK3qOT
K2BWh9eNiqf4EqFRlAOI5W4vPYzjabDB/VeHiE5TwuQgGfWwbvWKtdHcwvS6Koizi/px4lj7ym7U
tqaTS2CYUIC0Zf1YLTid90E0ZU5EOdQDnMBKO8R0VErXorLpTRZUHPSfszLuR+QHn4O0rf0zOj0p
v8MK8Uf4D5HVYAp4Fd2RPAs331kNeHStFvOgfrUOJvOT6XqGPyduKsLbZoItdLg2dIqUQturjFSh
t98ELq9aSLO06f4eXfbNbDhDU16qflR8+gKTZSUpPYMKT2F8W6gSNc+nxZo/qHPO/RL5rVwISOL9
/GPfUHY/1IRCT9YpCjSC9mSvpzfm82CnnwZRbnHgYSpZICsNDRuJDyc678M8F6/QkiFbiqSFiEzK
5rnOni/xMPzO7hOkXvXw4wpWbLgFkk93IOjrYrjAz1AXQCnBeJNsPwHgcbTECvBBHJlI8Szu08k/
pTnY9uMN5fN4GSDSRjAjjijpOEBvHliafs5KD2XcfpV98dWoUnjivHJi1LN19pXWob80mpNDcznw
DrpmFaEHdsfYTLsPgJRkNnjfrsE28vI+vLoO8U8Nyjhxs1tQ4IG34rFh3a9ACl3dy2gwXHh2BxZ5
5ipmm87avuTBRnwQPYD0YFCqrE780MqB4WI7N6DIjSWZ+B2GhEAHJp1ZwBtFWLuMzc+Nkx6aGPFA
nWoNS4xz+YUTlnC0uJbrD1xvar3FajhzfPN9nUlhUVkMF9TE27uNHyD14UUU6eJHARmpsK3BsR2g
WHH5ypGNCsNjytrjzxan53T6NqJoPzi+vSbEk8xzx7CNHxDiXYKAyLXHnVErche9TWB/+D79OKbw
/fdSkwLXGfVmq4IQbRAkHSmdb01mXJmTg3mt2Yf1WOv8ex7EV3aJHOSGtmxDB5VEnBTt2dpT1xWk
BnizAq/LWuZimXWz/wS1oLRKok8PJSRk4biwH2xwSeHgCf5f1ucPj5XQyNc8bZETaC+SlAwFC3j0
LvA7WdPMpq+BzDFvCr9l5r+vLi8PAuJdWw3ye5bt78LjzkdwvGlwN/mkFnJvGJN7uOiMGKYle/29
WmnBujexnt63RGm+AyzjvY2BNuW+SFSFoyZl+CsQBVE0G19OZpI5nwzuoMGCKeGNeZYu/jrDG7T3
b76caSurQdThJO3OTgcDgQ3G9iipxWZNRnNjtliXZN3qhOByytJE66UB0EA+muBepSEt5j7fDwbK
ZcWPFDoa0ED6YS3y5J+5Lcxd7g4dFrfZ7Z7HcDCfKhtHEOba+Fb7Em938DZjVG5Ar4vKUTf+Vw/Y
KYN6v5pC/j5eXronbzvGbOal/UK8B3o5hBN3VlSgmidA9x0r/kJ6qx3GGuvsuTa7IYRuRjk/Fph5
fD4yiluO1x1pLj3JAEOfpGcUICUSD8kkh/3o2XxqIdksrP2bYY3D2Igdy8zp1Z9eXay/QwPmaUqT
2aSn/KA4fg5to9MTtq2ImsGnSZedkgVFPTWkp2kuC4Li3R08eWrlFpiDzfwERsMU8XBt5q+ZelYf
FMT9ob9RXNX6o4OsxXp/tt6+r/fkzvB1jR5sHqLfWznTCQe861fbjvlreaXUBm++1ljXsxJ1v9c1
Wp4kfsUIG5myk8SQK34e0aUEfkBVJcfb0df1+973WIRv8PZXtja3tZtGkgJdEufM1ogLsfMRhQks
+6D9xpDl2wa3kWswnLkWv8Ap3VR52pLvjJ+sfYDjQM74l+r2+tvtHqqbROMpjPiRsnNsaQYU6TKj
Dwyudo49pY7nEFE1eIc/rPxt+5Baltn61KYvPdVaxEWQhiDvD62hYASm+LWU3b+PqYPXmXF42gQA
h5XDG2aWcttH8wHQ24g6Pckg6LjLzy3idnYkXeOfIXbpw3V6W2WcGzSw9zn827ShkPPJysj3scrp
Oq7T8ln+B5bX3WaKQsSdbIVqyit3Su3OpLOP1brNad33QwMpKyKQHT25nxTOn5zfA3X8zNwzweDk
Q6UZ23jwK9Jv7EgI/5hNneRFUoO/DhxYLYm4hW8FAjegUosxvAs0SgZ0aw3CHYNYpqvZ2leKX+mH
KOeRUqTXA2hkw2JfZwJBjCj81l3wnqB8gI79VMaKslh0/NKFfhwHZ6lZAq+DptaSa8hZhBsPpfl7
+Uw8MNZoAivyJkvXGU6MSslKnAmsk2AvoZPa+XtjwDwKsbL2ryEsqQ5kjxyJC2obxvKZl1txd3Ud
dxhnBzs5h1kp5vUPhZkzzVLwbxf4824AXVivV83yX7NXU9SYy3rdbynU+U/nliqm+PFD+RYUyQ/K
zUZZDyYZNQTPJhcEe7mpPDIDSqDLXbuR+e/v6Ydfk4v42gCHpmPj+gFPilU1gCCPFg0fc3JpvPhL
bN8cQcErjQUBDcQJCcE5qUtEUmX/vdkdISWV5dtC9O/x9uRvOooB9QFymArtQFz/JWBnDRHkk+8r
x50sqk6yVZzvFXRbv5hXAwc+XTN2pog8f936m/aPGCMAmXgJYqY5jDrGfWi/qB2czzeb+AlRsHAb
KXsMV0c3Z0496nuj+Mhfb5dOYltrRood7w52QNQ/vTgRIB5H+9dWEP4x7lr8Ryt2aW4hJ7DRUQ8C
meAcZeFi7xv6RbEviJ+DhZ6jDs1O3OE+0bGcERcaiWYCbPT1qIr95yG7b/vCLXHorUJ2JT09y/lg
E/n98R+I+E9DBjFG4gKxUG++O9cuhwLJkWeuL9KqDM1uyak2tTvZGzShU4JKohrEePWSdcG8MWbI
Hib2H/0eHzWNkq2s9KsBe62Mol3wYGWaKjOUkv+ATALQ84d2pmMQzZbPfHyDFgXKN12lcxZKoMDP
d/k1/KSLPrAKjadVdB7mWdfBrC+JmiR56D8fs+SLtKSdiqSVIGmRotqa3LCv+eHfzEUOh4FlnTeZ
cZ+H3BuLD242Fh33Gen5hNQt0+G6g89i6QFAYdW/YQzALZhKXjSbv8m99weQpPWZUCqxeH2Ind0A
qabx8Yo2NyVkXIFq7mACMNI58F09p+ax7aHk5ts+9IX9aAwKhSZbX2jgnGXxvnENyAEeFfWl339M
KY5yaNCSc0YN8tmzMT3EUabdt0OGn633r1tgKNpec4eeliV4wQ6CQ85cE5eYgasn2q1r06/CbWU1
qukVmgYBLeK4qT9jCqXx6DRh9J3H0jQD8iMSM6VeQjTIfphR7dcPinT0sm4mo4Kgpr7TNLHJ1+fj
YoC60YJI5blG3Fhc3U91bdnO1ER08Dv4exx+Stj+Tu5OkiGyofIL70Daf7tLmwfLpk6H1yyHXCnP
12k1r1q7KlunK+MG4WcH/5K/woBYxD+aR5L6aFCRZKEiiGdZTgzLkz5bwXaSjqfWFEHvKvzZMI1Y
GVrxYThoa0ro3PAa7KhGgz+nHfkTdrtzs77DAraf4I1BAm3L004ywB7zGI0nsWPRYgwNB5wsKNe1
ZH9NMfyx16FsdPhoYa3TdpovDCqHwWebLe8nELbkPS7Rzm80B9ECGChDQ3/jN1YgmMfRQxZ2BHg2
tQ65wwZ3I1+YNvlczCYvg71mHYkd7eVud4QbyIkW6Nj4k9v6ONzlL0vIpiP7DQCGnc6KXG2ncO71
nA/mJ7pPlZUmmcyOycOiSwW2w4YiU9zhljUrV+25rHCl5crwPhP7+ql0vxsq0rv/lmOoVCZsjwiP
5jqmVdAYf+7TcydlbaRYqjiaIdO0bHkjEj2bxsh4dqUve5moaPwbOt4EVhBMTF11fHGqCYyGUWCf
02a5PFMPfT7e9HSmTvwdQKHkOcHfdybLaiAWoOIvunDEv4D4fNYSQ6wTzRvlhmyQvnVn16RccWrb
xwW307D1AWDO60qXBqkvLGqm00kqzyc4PafpkWGW+UZGpL/TIjkvNJaKBVABAEwGSxmhN8eTeY1a
CwmZ2yftaxUeGUFMvV5lFIJ7CyNFSAR/gDhz6rS8UeEY16mk47nbhSwguHb3e3kHuebA4PyvWB9L
NBQamL2TKuZbZvYLmgM30NSh+PZym1AtqhOAeCJwP552i3ZnO/E1xuZZWLL44cJwlkJ82elt4r5g
3Ao9AHi5lPSwMvmemnp+KP/ei2D9ubNVivc2MmCCRBL0NzKeX12VpCh+ZMOFaIIuQ1LZGOuUkCQ+
Ux1X0mOYacj8W5fxaQT+dkwolczaK63MAt4/4DvEAT1X3IAgEbbgHHMZziQe1I+Ph+f4/E0kulms
eqKbA9w4nDXnyQeoAdgJEmHTmfL6G//O9Z+2LdqeSzDhMggtZY6MGv05ynOI+uYH3tx656JVQR+4
mRhzJP9RFoQGNaQuoh+PbO6SBl7BNA3DjHKMLsAS/1bzxdw1FqBSAEAb+HdLtrvIIs8bQn0i9b17
f3Vio18leKU3Bw75jOpJdBC5QgVMJVJ7Wt77fCKYkUBWSBJt0hPo6l/AfI6GRrPXz2daR9CcbE3/
HQfGaL4OdOuVSFGI71Ab35lhXDxanbCV+kHv1spOvTg9/W5zxh/yVB31e79pcTQZMysX/tDkNUEv
PWDs6aiQ10qF4nhBdLzSvqS2BNVOphPje2Ya8jPfd/vkiZ7CSwQ2srGiMvQcug6r9FNyYHnXqEi1
qgMXmgtQv3X/wKE4k1EuCeHYpQYPnNCTQ/lhQ+7+uE8EzRDQtoyHMD9jJQzvjbUI39JpnwIXmbN0
n+PULmeaPxLq20ayaQQrOucIXxjR7DMMDps52huzO4ykLcPXNVv6D94YQ+cG6JzAn6UV6HjYO3Bt
4OnfS731QKJWHRN2LAF7/i2T53lCY3Lq5NPBj8+2QPy31UYxMnygEQ9lGH6LS/82pFLI8snZC/7p
ZPFvHU6luu+DLRt9mXMnIBvr1+kv8jUmvr71BIODG6BlFkFTxKo7g0ZTEvyY7Zd5NM3AD+NgaR4V
oC8Neyem2/dge3K9tkf0fheP04zwKOqWyj81N0M21XpK9Y2bZm2xJgz5DdPysAWOv3JSmtYU8p/z
jzq7z6B8qBOf8jCbXNI2s6o0Qh7TfCml6KXczzMeXuM7PlxiU961tu9nB5xSsznw8iqi5PV2+sw9
S4FJRPgWCq5d7uQIkkhgICEiwp7ulc/oL8Aue7GMTb6Vu8w5sYLdhfZleIEEAicl3mpsb6MRDvC8
tjlLToHxRjBccioqtyFbYd7VBelScf7SX0HqAkzh3w2RhMsGXCvN2v/A3JDkeHOONX/35N3+ZFx8
9qaQoKnNYLWmjgABAs9PlvcAeb9+0o2DoVQuJw6ENumwtF2guBPmJ7XyI39Zlqs0uB7nJbip7cy1
uDjC11S8o5GHJ0cxxYtxoyT64vt0Ru8zkTYKZ8JqyVweGCStqEed7doW3W5cJ8bPMzZFaPHsDXp2
sM6mnF+Y5i/wtCUwc4V1IoD5t7ty+zwZnNTGNeDYHLfvfFtOAUffINynHCoTlnYdWK0RT/mJnsxB
iJGKzXTtU/n/q73vWoF7tfXMJ0o/r9i5XE85NErGiZ8zpteQagnEpswgagrUn0eJAkEoCAgHx/NJ
YhopsoLiRU2E89e41G2phnmcEXrPW24U13h8NdgnOKb7Gi7KsyhP0ONFMFanL8/bnIeTB845usV9
zgOUSTZE7CgKm03musmX2PmMpH6a8fYj3PzJnLJLjMbyb6mcmfp8avFvLe1Mpb1K/rh0BH7bA5WU
GfXlu6DK1AatjylPRj0A/mdym3Idbpvsl4J0tkmYDlL8AR91Gs2kTisTy2MDAIlrQgn7z9ygfdU/
ybTqBc/gqmCOlh7flfAx86znOSeYQnmmsNkPUs09mKXgWnM17/me5+D6FasFo6nW2T9G4ONA2Nlt
iIK6z4ixdM089rh6eCP2xuzxMMmsMrn7tBuaTorPzKcxGfhq05xoQWRe7FCe4xAeb27M1Uj/SoiC
IGrukr+LCHSNwvnJQVAIARHGAxOQruDM4XQhg4egKE5+HidkBA0Pv3sPmLr/snEWA8Hw+Upp+qx1
TxjFSzOcwjbx9310S+eGUnOdPd+pIPcTlrpFxssKywE2UVBNfpqNeK4RVlO6Bkco8jC+lyJ1O//A
K7eDDqsvWD2oHTdBgUBBo9SxuwswWO7VrkEkRK2Dc3vkdkKat9g1yKGJ0j1/M7v9nA85VdLKMl7I
fPEqUwL5l0vHEaGBMcIPOotn01h6mSGyUJzhe+bsPARJ/XUpcca3uB8YeO4kA7K1lj2vxh3tm5Bw
8ULCBDi7eTCw1Ood3C5gG1FuuTulRRZTSCl0qrQUByIQUVmpmiREftqgmegg8LCql6kMs6tsgKQV
3tmzELr5wJmvHSc3zQmMpInB4zCBSeAbbepZofWjN9EYafQmcVdvdof/5YTW9r7VoBW8EWpDN6X9
e07q1UKDvi/HqTs/xe8CiR8XXB0jgkjckG3YmBvp9Z3ftdRn61cb5XheS9TDGL2AAml4PrjOLOfT
5hBOC/dqtDGeQDlAna5yiY2FAel05qsYsdVaMy/RFXbmpvcueM/lf7Go8JY7alExjyf1ij+M6Uz3
129jnCYTuG9ypXHOh8mC7wlz5Flha9gaXOA2nReY3AN6c2BBnqvyv7YEFTA4MDb3vN3vkkYsdLxs
C4dGN4W/F4Gtt9dpqxX+H8AHVNK8Cbp95de4coDY35MNT7ILCUNW91XJlvgWjKtDA0oX5J1Dbojy
znxdAuLMGMZqIzwc1+JNenOCQnDZfF66CxOYx/1cogsCOFE2TDBio/6hpShsR5cRgdY/+JSb3B0f
RIrXHbmZs0ZnS9rkSzJNjWoLAH8La866DcpsCNB4aMRc81o0Ss5SnNX4CyGrBfwYzTINCG9m6pVS
3FIHMbxwPoEzRi5vwBM82cndYyHVxa9GoF/KSb5LAemgFt8Z1JiGiZf4A6L9LQcIuqgHyjlNxABf
CjoZrPiLKKg8EoWVwaiF+jA4Mz/YhaNrH+9Txah0Hgv6kKsmzfGSwblDxZYNTpZd8wk6PM4fcnFO
f0GHICIUZm17zaDRWZij9tqUpp9BgF1KYwzyzucS0T85ltY/hespFH+ysqN3/EsjsZPY3MdwOevZ
MVfWWIJa5tRy6vHqkyU9QeKlfLUC5VO+DvgkpHDJE7vgu2TEmr/ijtI+WWsjzz5PRmMxziwTw0jA
Pj84ugx+vP+/VjsbFs8Oc7ONGV77c3jb+ozk9uP5BZzt454AmaB0Yq2IHLTrfkQtHBa5AX5yyC8l
/LpGh9aMNYX+sQYBMhfGw0dXfTjrxUGSO6Qcp30elBogX8kf2CIcU05MViimfMoL5ZbFKPFQ615t
OexlVjvmeK15XAf00I9+r2VvDpLdMMgwtSYzMLxj2w8cS96oV5HXOWypp2QCbSSxdLbSu6y7WgB3
oXxFwpsVkNsvWXczafxSVGWgaEviEvJPIPPSwP5ztos9iOP20Tvnpd3i6PbhReuzEsB0AryGq44B
IKAecDV8BA53KHORMu5nWdJV/NxXe+abNevMNWC6DO/Sh41i88dT6hQHFNPI66BHCp0hUt2WmvwX
njrDVJCHCbcs0i5xJgbu3lN8AqJJoFJkX3usPq0DK4GZNcmd/wyUUSdSNWObsRobU3h88wd7eCp0
cdFe8qBNeC039P7sodtmdSp3BuTsB2OnCj8aS8tFzFdvQh66ytflm9SOHzNrz+KJhs6gOAt0yZhj
ulqwk4iwm/MN92ua0yEwmTTuIZ0mOk+7erg8uBBHVtZnoY835FMDWe2vdXnariqgnk8Yp4P1tRyf
xRXr6WONgASrzFMH1soAS8kGC202IZ8l3RkQZgW6VAl35h/VNSEHhfwgrqjc5WLgslf/r2MXb/wL
hdkkF7vtcvi+mfWt3/IOD1zS9FwrFRdMDHFFIXfMFu2XJorRvPuOM/3yUXUcuwPvvda+1jIubXb+
mXrAPcYUxDtvueBeU1MssFvvWfwvnhQ8UUsSMRDImCaEvJJNBsnbyefjmfg8YOKDaeXVwfgZvCFX
n/bmmuonAokKQr5m/+LNurU/uAPNy5SGdKiWveR5gV0NBf1gkhX5n2dbyB1fvBg1nSGcS8rfhBR0
rywn3f3j6ETRKBMgNzYNyjEd9kU7vdJ3yNOYWmC71d4uxyrS6FiFtxm/l6dUCyUwikdP3STKqm1S
FGDgzRiLZgtfsVu1d87azdqEFr34hUWjJaSglPld3eW92TIXwQvzHoVcgMlwlg/hJeL5hdAYaies
bAREp2QXT+iv0BnZuoggNodoC13hZ2++fuOSYMGOt34MjURp7KWE67uwGRujCoxYrktqEPT+Iitw
KZIA8wZcbhyfEPWSZZ5I7iKHPoxP6p13KtHAnHaX86ip1tPgXCB0sHWo0rKX+1OVll0CvdGsEZli
3BnNIGmdcV4Er+zTwAQ+wvQys8KqzJIQnCXfg5bciavy5svOarQZGHYRcAzI3bZBuq2xgChwCmJu
+JmB3zIfa2YreeLY8j+HUx6fvCcFJvbZg32MW3AXxphj4FqeN7TuFEdjaZCS2jaybw9LUKdi3PDw
dN1ipo3W1oMsjEEihypaKQ0cUiVsVZm36Pd++H5v0eFDhrWPkYyUlIZZ9r0XelOF0P5kfPgdllJu
TUt8Pxl6+0AurrRtGWIE17UyAzK5/PDSP4EMlahJGu2v7nLUMjS3YFA1+CT/w47H6FKZYmg889U/
Yd3f0ecatrBrZRmhChrjL1W95406FQlOF0KhkzPLn49ZZ5J7K8rLG0PdTzN2BE03lXFikkZv2ido
l6MJKH8Lkwp0t3ucB0lsvlMudLTut5CB/XKJuZY0xZuSR6f8bhxwh0qw6bPsQddjzUqDISeLyD71
FLKo2pnAadToJlLI121hxgPMpyytsoVctVVp1e6dt9I5ICfmqez0fqmr5fmckW90sXJkdBFHKnPy
73O3iSYRY0OtKbaDq8SOEg1ie531+NbjDh6WMOwmrkgIGL6V4CdXZN6/ivkkUKEJTPXA1FC0ROP5
Cs1zYfh3renmno4ygUr5zdkHwtTZSpD6yRLjs4gQMDPPk8poT6JvVFFbvlbVAYzVVfQNsH3ZksGx
ogDV/L1KsX65jyIZnQ4IO7pqNrvksP9IsJ/eMEJtW44IyiLuqfO5Ag9IGRSxraq2ehIPbIaiu6Nh
pksyCfNsWVcvIhy+KcBafhjFrtYvC5CxnyjrhLlUb+njuzWNkP8s7p0AJksdmB/xUptfd5GVDN2f
1RS1kCnCzyZdXoK/j4ZzLRsif1uMDgl9WUasEDzHlXGyBX+jIvMT77+mWZfODXDRaBqWL3STuxq0
Z7qJQm63MLpnjkMBsWEgh7WZ7E8HbyeU2KJEuzhkL91Sc0slVr7QouuiHC0y8pKLznXKDH9GJYUV
QRhX2pIkXHLkyUNANO/rpw/GDEU4Kz9KjKWu6NK2IAFlKqFHx+q8ifqkCm6ouG8pKBBtpzvXSahw
X+A/GCzf3ttAEXM4ljSWbvWLpocmVaj9WuSZUECq77CiWFiDlMs5qCRj9DSj3JUlVWirV/RGqTqi
wJkOGQnUWE5ZbmkOsE1wbu7IezEkQ8GvN1MpEdmC1VLwJltENOH8eHekXG5igTFDD2lMd4b4i+ZS
xwoaC2dD4d/uowAECKzLbdl/IP+w3rujdAocRmXOcdpQ2EPqkefVX3Ne6rOnr216eqT23ca+Ngau
hIREqCZfFyltzPrijKarumt7Q1Vg3li81+QhQ/IHG0XpNzTGGm9PKglx1Hf7ySxmiHTpt8kMd2rV
2jtdDOjGLtu6Wzd0oY6CCPzwJ0ohOddzofvCBAJi+n6YjOIArFmzfUO1Dfl9kDsEulLG6+3m7kM7
VxhA3we3Vspw8wrm8TGolS8SsFHyDaXAEer8+jyOcNHqSH7T6lhOVL+s0aaYQdWdMNLw5cQsKS9y
Q3gaw0UTKmyoxkrgcEfSob2G+Mg0b1S5brM72dQCfKhiRaAsjBi6B7fcuOMg8I5MS892182eUiap
1z8iVSkW3bl1pm3C+34LAfZQI8SiQ2BdEHklxp0Rsupyn94NdZRAXetj1HpU+m1nF7c7dFFhy12t
NcaZmh7sQIr55nl0JL8EvNWMjs0fnSqG0crQF08XfIoZs3j4cvZjpctYP3YahrVmOOddoFmG9lVz
5lKyoW5gK1ilPSoEpx/ltHuAPAbQrSldcqSB7faZSQXYKL27nnvcu+tv52M+W/uS1YreXKBO/FkG
OX8bR80A6KSoOEZk3av7+ehUc8CQaTDD3ojW+pwdGVnQmQ2v0YO8r5wnf4O4QX6Ok7tXx4hZKu2q
PTx/b+oNlVdis5AY3mODgtr7erO6p70KhWm8DcbRKywvtIn0jucSjTcSbuP41mZvpfOtAcG5/oG7
xpHcix/el3SFbwOSLpK7dyKIM7nGEN1l0QL/Abj7IoxzOW8jQjqBq45vB22ZlpxsR5IXN8uKtkZX
hsjbYNzwf9PqJFsJe5qj1EMD3QMYwJFbiOp9eW1FHquue1gAgd7nVvQ+on3oZw3Ren9Lv1XmMJxe
JAT3Nnw4+gC4momr1yosekYM0xwfu4u70gbllz2fV5N1kbQcSGWTDI87uBcrRkeVoua59iC4ZOLg
1UPGHleG+d4rcUvXYTQy3Fads4zH6sL/tee5qGnHr+xAKrEYw1Hww2b3wsOf2omNeq3JLLsiRY2c
9epvShuMMygjbs2pHUxsMY4SQl4xAbePjMBBe9eT4M92aIMHeAmsZYH8G+A8lhi7t3IE2g/htIBK
j81f+iaOYAlgR7pLiKHmOpKPXWPuk8YMlsKDZApCijafHnSQ7kP/4rfoFL9voeGsZktCHYyJ0UT3
PLa9O/oEECl6XEb+3aYlPq2u5Umn/xBCeqnZa9YtAQrzCqNAloSgzNzufDCXzfgttnPDK7A+wa0S
fREF0/4Y/NkHVEHE69UEoHMnirYSU9f0NNS3RckWK4ZWbNibcCleKpP9mTcSBZHobLp5H+Irc+hf
rh2yaCH2xRaG5bmSj3wOp1t0FMo7ttbvgdWwj4Njm2HMBaq6PcOnHU8SxKBKDiSSl2NO5uO6KG6H
pkDBjU49/hszAu4S4H0TWjhXZlg9m7KZBDL/MW8V/aISDdNteZd0kw2VrvMw6pdhfM/yW7i2oLW4
6UoU41dxkH0vyV3t4vp3j1cYV0bG+8NrSxUBmZ29NvaHeO7kBdWSFH5wC3bF81C2Mb63Qz4w3M7x
nmxNryHd46qBJ45UlbjSbf6+6GwZQ/2S8scKusilQ7wVhbLBH1lASFpr0ePCVPxmaVlGdIyJ1cYl
rM/gMRsYEYpblbf7zMBOncWNQlETLuoLhbEDTJke+3usaQcm/9oZc5KpRZV4DuDJ9wf4u/7pGUG/
q91yCGtu8CXNkhv/awWfM+IVSJGVEYq640IZF5lbFrwPpHLoredCRz2de3p/zzlC0ee6AmBJQXvX
uSSLWshNSOtBBE6rGm4O97nz14LuEMD8qkMuJaF5EXfrTRvYaiY4q6MdazIYGG1lnPKiV6TFZfjp
kCxNVeQ/vytnMePB8vgu4U21fQDM2NPi1UVQ6A0WR2e0Q5HnG7G2dnqRsFSOu869y3gw3/O9lTPk
eGwJmNNssX4dgj6ByFhGe2dMSSQ/VUxE2QWyMEN6MRnkjLIOwKdOYE3ayhzgQ++0hT5RANQqRY2Q
5qYFgFywItdLokTHq60cK96MQijN373OH7FZnrw5F66c+ob6nQTnPyEnsrlaDeYX4w0Mb/lTb1s4
WcyMw/ooCYPOy6mfMQaT0XQEGAkU6lggACO8UopzAFHPM3psKwM9BaXgMFVGxILIeweghoZuUf1a
i6emASMI7cHLEaiQvMWUk8Qs60ImLe5JDFLtU8c6GzE/6F0aVFL6RUo4EdzbKGRUWU31PYXjHmSw
iGZRRJEEzX8/QHYk8zthxAO8Fnahe8ot4dKHMxuuxmeYXoc/gApMezZdS7SU+AUQ29MePqWyhql9
zCabYzF8ULPzKV90I4/5Eyi58pspLRRDUbkAgzaOjtrJH0N5hgp2RceHI7LfZK64v9CGHTxrgUxe
o6lwOP/GEIWwlcpqLxy/6w+NX9TTpB6VbW6kckbV+AlLMKXE6JRDJeg3b90dxRdW7Gbv+kiof1iZ
aAo21J0Ji6gundm2L/ML+ZzfEZcPW97e55iRgNjNt32qFgOIWsoyAKdu61YI2LBfngeJUgGsYG64
GDzoImLj7M0DamtGx5PRWjrzU0ZVcUlcpQdnmx/n5a9HB1ZETtBQyyMsxUSsB8pyEVIpfwifONgg
x4pD/6tDA8kBzxCiVEvENpEwvVHuu8ITK0s7o6QoEi+nan1ohrZYnXnnN8V9YoFRe4E2Df1YuSp9
I5/1XImdgjYNQYPguD4gmCUqxRsoUrz4HEbm9TNWPRF/t0a/AdBpBTbLY7y/09sQn7pST7jXkd2t
I5KQbzX0t7iLgmXEzIFxYjSohrZz2qNir1mndh+VEqzcpvPlOKbB9nqyzHXX3NB2O0CMX6hJnKKD
L3FwM6Tx7QIsLXeAe9mXLnTyDxclBcpn2fiRCx2tbUIN5RZz3bJh+/wEt+NjxIAG5VHwKjLyuGMu
44Alzw/fa2MZYAy5Fbmj6Q846uHC6T/kOKsLSUawBX6Jftr8zR3+gGe/LVpxiGtybrW1Juo5wmei
hMGtjiEphVZXqWAJ6MnPLCdhLI8M7XTPlD1dckE1Z7vqkXJt0dGcnwQGWohhcaFBjviruI/zv+lh
Vl0yoJbpD6AtuxmUKL9bnzcEgUgKZ+6ktl5RqFKGGYsSiEgWXyExeChNAwGjCU6b19mYYOw34Rbm
q+wHZpxHY6c9VA/vgG4eK4cK1+R9Hs6jqNCMy3K3PxR8EOWEGkF6kfxfFPiiGRoANcygM3AuJvo0
PiKKhitlDnWHzO/3gPyGIBqHt+JX8DyGghIoLm65SRT88EChsgrXq4/W3dZIGZypLkMb1Wmv/bmh
vPgpjfaMt1le6QZEFW+TOPG6LhCHd/M2bu5EffjdS7zYtGOsikYmLL3hnZjov+unLt7bPItXjen0
bk9W7fgiCGIk02L91Atu8SqcjjUfU5UUMvH8MgmLhzFU7cnuR5xhHTGbMuuFbbe/cqTW8SlEA1Cq
2/M342Xda76VANdM65HNBvpb12INjzYOKiIvgXa8FBTMs5ge5ceu2D2dlkoojHYMjFTB54dV8bDy
9sc1i+DTRQKM0F9ZMT0D7jp+6P1JL9fc45NpMUeZdhovXKY6VGmTfA61K2Fyzh7dk8fzD39EiVlQ
WUUmEz8un5U7kHiH0KfG0QK+Zd1tCqwK1uYWrJCrwMdOz2Oecl1Tq0x+iOHya4/fi4GDbf0FByPH
YxirCbzGOEquce/IpQnl59sEyVkQwSq448jWiWphOrgOYbVp1V0badixf47Qzaby0FCsFLsSQoc7
c+3DnedulTr/DUFilQRRxVNHk1JqaavaThB9UWcZPDMveXv78swB4kiUmXkrw/KSkQYZfOQz5jTh
f609ktHCNU58hMuR5bm5+CxuDwtP2MKYU/RFIhO+G5yJXr+F4GkdN3KwdjQO0tDxrkbf9BqZ+aHa
/G+UoQjCdhnZgZIUJBuxf4JB3l8iC7q1qfEZ2tFFyIDPBmjRhmOryvorO8jgPqEFUuxmR/r6U2zi
ts89UMmG9D9iMpqOFn5DcN4bxtryS7+WIxaLEz0JUd+ZxIayqeH3lVz+hixg8aqQy+9MvQTJpLsx
VA1otC9dhzI3Xy5XEfZp0TCpOff4k52UY/LO8fxQSqr9KRJ7rWkS5SxCGojUBpRDcKWyLlbzMEOX
FBe5Sc8UlAZv6jIcOvii1uSoCpjQeZZmEiVXQ4ThR2uMKK+49dGG2MMzx4Ytk+SA/71ArIwGtwHR
Eh1VWcow0zEnInFEpsD45olPMfwtOSs12YZ/PgLOIOSO5hlI+E8l4JS/qX0IEdahoU4QcaM456RO
LTqkgQ4eKskcLuMu8uH/mogl+WvKrLxpcE8GSrNALN4N1be+jr8oV2s8HE9PvXe4JK4bcTPfyqXO
LjOL76/p1GgBpCHoZG/rMwVV1kY32RhVPn78Uqr305fXMOHwHGFOhekr3eSji1ZYv2N7q1OJq5mN
OpWdA0Vr1x82UwgQsEfP2J0A8BdMrQKi9gmAkrldZDxhRLz8hFiIAwYheX40TDM126OqRlsecVV5
TE1fDHv9ordsmzvsgXBYGngX+L7ItPm8ytjGsemUd2CXsrcu5lNflBotrNgS3i9/+r/vTimgXr+/
wSBjk3AumF/G0AdvnAQeWhuNlp1lDhyvhIf2ISsrO8hEq4ZWzsDzNGXYXoLS++2Lh84LZD4c/KGh
MJiTc79BSCfQGmEuwhio30UVNT4gyb7sa93PHXTDJrFR6hSGwkWo4LhOyh6oO5b5AhGtmrfG25W6
I/E8iwBUy49U4nutJYJC3YPS2q1fdFe22fyGiwog2HxGPFw4ZK9P0MbjbaDb/a+0DoWOEx3QRmZ3
lUeUwuf72ZOcCuKqiPjJ4TGiy+56oVkrYh0SqLGB9i7mbMMqKeSxcc8fCy0Xv7wObm7ocgG0LDqE
UqiEZneNqNfBcabIBJO+m1PGPwMi6zom5t/q/d2lfco6C6ARnQT4LPBMYFJfzUPqHnPsftMjwr64
nUK6c6WuET9fWqM5W2x3RZB+7kzXfhgsV3VhrS7fv97oPVKyNnVqZGJsZONjr2fKNxpqOVUZ3pyb
7hvGpJTbA/3ELAkJpJhDhoc4cp/vqF78SdY554zxS7TDh+u05Ylz/ltTmwsEumE50UeIqbfjtMdo
Cl/EPWY72zp4ErVC8yRRRh0szlB4ccGNpJXM0C6rm+oPDiNMZXrqHg2aq0c775FKrOadM76G/EzS
Pq6rYdrCox9aX7TXoW5nFuyB+soDyuv9YsCsSXFPbHxnD4sbTGO62yFqVsmpLx7V03ZZ0/dc4CU8
/p0BCotPnVjCL1JvT9G3I0r2lAUpiZmRjR8FY5oHV4gMdtmuhCgpq6fC867WYmeVwwXp6/Bmg3si
E6Q71zKQHTCLII6Cvtb23/XArdMYN/DmDI25guvVCQ4ceMcgnxbhLxI2/fw93mKgbIEwuacnt3w0
3DrTk7GXGHIchtNusJqJ6lwzB1tPlJNn+GnmUYi4a5Afx74fL9nh3AyhkUk29fxmHPdSapZ+8drg
MN43buHXbizjByJ1jZRhvalDx81XOenhMVA1WiPnNA86ZdNfzlja4hUw52kXuHg0Q6yVbt3RdSbB
axiwCoDxaqn4phhO9krFXAuPck62lvfbcJnwJYQ0+GoJIlFngC0+S3XBbZ3wOXOp7emdydRoLG7O
RXACnfk+Em/VxYdBFKYDJw79lWB5eZWPeToA+Iqh4q+8RSy883hWRwfWqjX9xGm5UIFnpegecO/t
PY/p+vrf+zmgfeIC4G6rhNzBFZclMBORfGPZCW6dB4OYLKPfP3rcE/ZHgyEqN4V/0+H+/kut6rgA
llfISVKdBf4ZTAT2A51P65J3OiVK7UgdkBhTyJHmw4AwkIO95pFuIwYbX7Ujl4sKyocy9nu6lJ/9
wo8B7e+DxBVjTifZXRF5z86O8IrrDmqKA29adwv8EQ0QMwO6akMPP4OP9RRLkiHJN1U/AUQrbXAH
Agf6aqP/A0EzOpUVlRyZr9B5Tjw5zGh8SQ3C6X7JifIm+DWnk1bCGBXCYRsj+VreNskUJbTG+pAx
7pGCv6gKVs+l2t6mRgYY/VLi517AT/llgjodRgXDGK9/u4oQ4sdJh04rktIYD5s45rtUnmj/iLPt
9uirPlveqELTku9ENXeQOTy2gUsDABfmc8FIx3tjRInw8qe7R8oP692WxC1Y9U8SziuhJ29/lelk
wRm1U/Q4Af080x/3LNlA/Z4xMsYHaNNAf+jJmFXjh1keJ/iWHaZFEc0quEAsKeL0WUvoHkgLW0Dp
DewU8U2XTB653ZlLIa37zks+lu29nwFrPVFg4H6peYl1Yp2skNJZwqL3my9K2QvfLNqkdapVyka/
6CC4Ybdb/EiXuoMR++A7D/ChDPNYBXyf0Sg7XvUPxOJoK7Appl+/odzkWsBwTzS7msC8gpbd2qcM
toUWAUyvV8oUk7Fs0ulhwcuETvDs16zlsD5iU8b/R6wu7NusSWwUHZkQMhfTnpaE7lYP1l+/jUax
Y5xWJxY+S3jB3pPQKma0l8TdPgXGzw7O63hJAlXIweQ/4p8Zcp5FYamioq4Ya4yZWI0JEjWonp+a
yEG5uuyBv3MRYWMVTVDNeIRoTI4E7b2thGFE3UvRGELONA/fqbNbjtIzKtPjwDyXAiMaVCa78MFW
wGI062YeRKAiBMkLGcUVKj6qKrJYNfw9J6hKjnSqcegw+Se9AqWRYw1+G73pIu8asI8R7VRD0j4S
Z4C3kFYQ0Njb9nUsjMtwA0rVnDQ8SibhUKGBRU4fQxmSZqP100r5sCcI43maXVAzlnJjiwtNQaWi
l4Ia2K0ls0lPG1aJEXqzoUF6Xsbe/5CcdLwQbEP+laqfw9Obgs3dApD54TSTfjgZsLzHVzR39gDb
EpTzLFIzNDZfGg/tGEo1XRnRX8BJAuGxxKkdW9D0FYb4b5ikEyPrl/NroqpDbD3Gui3o8/pz3eO6
/6lu+VS03nJRe6GLS/dG5PLZ95qTHwDQYkdxNqDaxHuVFITElnphJUVS0Y2dWjrNzBQd4TaX5mlE
qQ6/q7aszm9pPJtyoHC82T6meTd4YTRQGc5B3mVf1f1kbX2iBwy8nhip3CNdt8SlJGonOsMadvqG
zfuxZvr0eZDxclrG61eCk9gJ9gYpBwtvNW5D5PPnS0ypP+DBgMvTTOY9Wr7D2Fye7h2bhkCQRB8S
QwWNKoycj6cm2EKGOH3xNGp6gulq+QZ6G9j7YwmBpLK10z/HsBxKmhFk5jpAUlgABHyiW6WePUT1
irL9SxgvFeQB/nQ2qNwKXDxMUEMdRBU90JPhidaFF0mVB3+Dqrkx2DQXBxwha/FkTN8mHSe2ddy4
r7sSXLQqkeWLIEiCDLtYJvkq7HuilTmqg1mkabnu89bvcm8ja9TnB7Q/BKrUXUjSqJAzvrkqPXKl
JMmXqSjVHj4H2kXrpHkeOXdNGViZUJQvElYlGbWGq4ET2gZSuq6hYy0b1Q3xs4yMOhAHH4ca2vUc
ZEAt+4pZREqmHioAtVGrIAnszaLLD4K/KkoVaQAbun+rPeZTH6Ty9f7JO5MCr1L1bNxTBBLR5d4G
kvd6DsTQsawyn/jEpk6dlT4zw151ieoRjzrvr6nN3QTZPbFfAW24AnoxSwhmxIE87SENV/45n228
HE6VVRCJg9U7eh87jfBvSulSBQi2jeKzeJuQKpP8nvvoeHhXm0/moRc4T8/Tp4/+bg34aLirlcPF
pSs3RB4sgogl4jmQ7iTapX4stYFTn+h1qOThBWPYQFIDVeuEmNVK9aR/z7pkt0RarP0N9mawbxZS
BE6crOlQUoySCDZMsSAyE9JqaSuGSH0qhQh5EiL9jwI2XF0y6cc0oL5sSctkqzW3OaMCdRbQSdbj
rUj4gr77+H/aHUZB1Qs5r7+wU4DeZwOiUyHPPbr0IKhBClta6ma6GiGcbrG99t7fE6zKkjyaxc1a
H0XAP5yf3lHJukB0NeIDHBXKQuJVYwzoGsrPLZ0km9lZxPyBx85fJ6SUBkFClLYUU1bfOrxnlbtu
pjJ8bCbbI9dNZ9a0kLNzPJdRKigNY0nybocgKBhe8YkdDp/kAaa2L6Nr3ul3xZd/w4vvU5nZxR1D
0DIBKljLkOf5HqH8BwH1+76Y4iiBEGaGnd5A1PQnYkaC/YyZYBPiR066+4DHm6MYe3J1+zPDV8e5
Dq7MWHuarJIU4tWnjBm38xrSJhl/tWnoINmzZuW5v5gNw3rAHrEgARX9in6i7xf9PbZ+wga+6NNj
2BiSDJWJv06XE57F1E5t3xf1JVT2U7syyIbiLBROwFnggUdCW8USfIpvfPNxXxXG1Ibh73UMpNjx
V2cBSWBeuuZohfpaSubzxFab3WpBFSxpBipkAl4mdXsPbOxJYtMNSx/vbQNDjkgpPGHRPgGpv/TY
P9N7vycrOuaWUmLd8LZazXxtdCtK48saR1/9+/zpc8pVT5dRc/sygbdnuRa0nuVgGuJoAhxsnV9k
FBNUeWeL9IlBeM7SFtW2H9wNqoDoJFJwc/GaeB57JWNBwNCRDn15CapHCJ9V0vQzdXHfaf8205RK
CUps596rNjTTmfMiDwAoV5b8s6V6egb9SuuTZgzU6XL+HCTcDu+RFNYg8JmCn7oKq6WOAZmEmdFS
dy4cK3QeEyoJeIcHyq9Ga7s4NyreTZo5Tg5YJGTxU9evAx8+gPYrm8/+aXdlfpy2SOHrEEqh9KGC
ZAoHM2YSVgSAM4grSMFZmLPPIrueK5Z/ZHdrfENYeSEoULsiPIKw9R9Nf+94C9UiKUgDqU4bjEbz
IHXRRVqMTkwOElnVBo6kX4ci9SLO5Lnz2SbifvaxXLEEwZJ7s7j16Sa6ueg3M0EgGP/D1NtFke+M
n5ndoVa4s3TjgfFYflXDkvZReqTdaPEw66AjoQ/LqSwP2gyecjpZlSU0iPcXmoPQzmzPcaVkp6zl
oPuRzIkwNSkG4e4i7sBrV5cNkSiilBuI+zsNusZFtr8huW9pmNmyosF5ONhd/2gQQIPe9UkQDbSk
z/Agd5tJVBoPIGBp4wHFlmrFJzb6EG/rFEnoSteDMAMFYkBkxRUIuaCPUK5jgglDS1d/4f5l4Bbg
R7esHrDve7lvXTRN7BIJCEYnrQGihjUHTMvaXLL16Bm4FNRE+ToJdXErPpVAjBIQ/v3GdqSATi8s
eITCp2g040UzlDWm6OGLywg/NoRSKcI3y8z9AVYMoP+DN8pBQSU1EAVRi/rD91aFNTyel54NUutg
Za3vVv0uALNk0iq8JNIJP3zr7tZgeQcQ/unR8IHEMFC7sp6vwZqlBkLDN8UtXJFVuqgq9D/K0uO+
4RKGYJkhCnki9zp08v1rYMJYDO6SnH/HE+lA4KlVe7XmRI2WZbv0rNGfVg6D9jY9+dWijUMHbNco
7CpaGu4iQORUD/JSkcSnUyLhM+DnEZiNiOf0teufwV+UlOY8fsOwwuFEgp0wQArOkr0N1HvHR2qI
ficx6v37QenVv1zOaDUiDRBPCaNvRvk0XBcwnB8dr/HI4Z3wH26bn4Gtnp3wFcELuCTsCVGs/tNp
BTEVQSaHIC08tIk2HaQWqLHQV7RNgtTIIPuo/4L0BVjd8tYGVNszcRSdXuuZlsBY4fhibGGrlWXa
IsdXOMVspakpSdzBY7xbJbiYx8UIxcmygfMORX7BxFDkh5aDro2HyCOXFHpDPXrbUEUiRL4CwBLJ
Nt3HJ/rbByiO4s2zG7sv7P+Cd5KRVUVlvmPJBUI/NW8QUUQwZYZQeDANhmKsHrpjIE7a7Ng5A1XV
kmUSJ+VmX0/duPnmTh95hY0hHOmi/FWNtUGfYoPU0tzOiyktXQ3gxx++Vu0mB4KF+QNThiBo6yD7
WPK4BDO8UiQ688YhyC69SMM/WP/J54wtWl/7wbQeD73YNWJLSyD88oYsW4KoWSDxwos0u5rUKrwi
s3AWf3inz4lnvJiQA1BR4KvVw0gkcEk5kbQkKtlCql9NcBgiLOOSAQSXaZti49GmWVYq409xg9DH
bdOBkt6cbBT1EQSUdw7botaj355xL0AlVslGhMDbOk3g84G4XwYUPMPb8qdsFOBKXQveL4j+G3cN
ixoj+s6gW7egE6FEH+WROQE+9QSqrbtwdJ5iEX6qnnVZTPn8CAZ+VH+3lcl8ITkDS65QsnWq9ouu
F6/KdReCaq0GvHDgbUYYQWMaI5VhZEoyKJmfE6/WYE8jhHjQylQR8Zh9xAg/vGQPZNfoenWpAPjq
KekQtMCOGtB4oO98I13uIZsLIEg9iF/wreim0lf1pX2zohr7CLrKwccJV+cetZQPkQKMZsnDIU2e
thIlgCw3pzoYN0Y3Eiun8vtU3FIVWldlst6hWdD1SFqSni2L9pKBNf1JMkYse+d5ZA8URoKy+9vn
1WwnZ5tDol6yBKW8OktIW/OotxDrNPijboXIa0A46ZL9LBMdGcz3q/8lpMWex99kG/ZhvGDhzmLU
ouIXP9JJ+vgLz3SdP05Cm3+/JbFKHiDYf4MxZLmp8hRTPj64lD94oMHIjJX0swyb0LOxTBFEL0C9
5rCZk5+uGhHDrCz8S836dWdOftMFqqGGa+cC/RHrhHVeOZ27bnh8xaw4KGR9e1A0OxXmrX828+YD
hkK0qxOBeo+hchS05IVnXH8H4UPsCP8amiYdiRy8islk8KVw70eQaHnSDajiivfRbj0Pva+Dvycc
cqdUcNQan2C6jv4aoQMji0BvSOjrqKDE5TvKfY6d4CTdPy0lSubT4UXxovq5N1s1U8psXJf2By+T
dCC0OZM+5M7h6+OTr8iBDHBAgBAY9hmTv7gAuQoG8MbY63UP1f5Hawa78ClNRG493vkeQlQUUIMT
ghwJJluJOxNs7UjNQZjZ6QEXBPO21L8ku2OsU8zM92qEqLj2+5RfdXFbvrG5ch35JPTTEHzOk7ka
TETfu5DcDwIPH6/ms1PKvi7QgXc6Mt+P8R6ZZfrj3v3ZbNXhosdEuEQSg4aO6CRIhaXk5nCZeoCA
SQ6ZI792k4Lq5U9WbPnQTYH0IBUaqVmCjoETHJNLbh0de/PliRlCjzNpmRXo5M4ykBgU7Ad4huBA
AI3PzeDQZvu2mwgJvyDst97FH13Amk3FCPqtpO4Ie+wH2oevMFicYi6QgNC0Sv+PYgG4FS/HDISC
EdszVi9qPMBb5JxmBaHlJI0KrD+fL7FN8wVYPkoEFffDVfoY9UJ/HI3hKgKj90hI3x8xcEYYKp6C
zz8eSXdl6n5FreV6f7rZ1Kxcn4wi7eIW3AK2LG6BCpVgK4L78WHliiFab3kgDyAj+jIwbPRfnl2N
ckyDcsAzHXAntkNLI6jlT7tOFbGiT7Eg9SGCsORMHBcuvZXhfWFbFwzEBukVFvhBeOmhFjS0h7Z5
gU+2YFP8duUYtf/1kfCBMlCJQkWpIphK4fjPheVJxCvOiifRhJYs0P0Gw2jd9rfkNie7v3PH6Nj2
8Fb/9np2uNxWgiP2nNunNviRcz5gTVMbbgNr3dAeR7kuSlcDS9YXCU0r3V4ZfXMClv+GeheUyLFb
vl9IPl8fkji6Z/C9Rck3wkkEEd8kgAq0suw6Xx4LA5e0y4FM6paEH8ViNK3kDDTC2nb3gFCoSyEA
hdByEqJnjbup/puybJYio1S+vnZwTopjLdY31Er1asO8rhpu1Kzch/imz9XbfukQsJQiyJ8Q86v1
+jiwFGMb/4WGBoNOrb9gcHNFgKr4fCz36iLORZvdhty4hXBjHtJvSf5zzRgdHdf1mIKuo54QeDNo
+SfYuUhKJbcMGkn/pme0T3YGHFnG0SKHdOGobE9DJq1IEiwUgieOLIXS6u0iN20VV+11zCR/5dnE
SHq0jTb6nB/gQXlZE4lZ73OaMF90AK5JBE2/lCCCB+EUpwsE8tq2VtC07pxHVi/yaO4b7tDC+9lD
akUIHHzYm1sxn1n5xkvbIvaRQjteZWr/PVcAsgkJnsKCQpth6lo/GZEFD8kksrqfblblUuNe6HcR
Th6OfecyRLMK6MgX91KH8j6qzZ/aTkX44zyvMnDMD3Ll2QA0OVlC3aXdvttZ9nPuPaGerjOnyp3F
9d3TvphfZJ4zT7HFEAQ/8IyLAQI+LuHB4xI3sX1sAzCPZ/tz8G8s9CvWF8rUxu5rnVh4CTyHP40h
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
echo 180@$(date +%H:%M:%S): Checking modal visibility
for f in $(find /www/docroot/modals -type f | grep -vE \(diagnostics-airiq-modal.lp\|mmpbx-sipdevice-modal.lp\|mmpbx-statistics-modal.lp\|speedservice-modal.lp\) )
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

echo 190@$(date +%H:%M:%S): Add specific error messages when table edits fail
sed \
  -e '/success = content_helper.setObject/a\                                if not success then\
                                  for _,v in ipairs(msg) do\
                                    ngx.log(ngx.ERR, "setObject failed on " .. v.path .. ": " .. v.errcode .. " " .. v.errmsg)\
                                    message_helper.pushMessage(T("setObject failed on " .. v.path .. ": " .. v.errmsg), "error")\
                                  end\
                                end' \
  -e 's/success = content_helper.setObject/success,msg = content_helper.setObject/' \
  -i /usr/lib/lua/web/post_helper.lua

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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.12.06 for FW Version 18.1.c ($MKTING_VERSION)\]/" -i $l
  done
  echo BLD@$(date +%H:%M:%S): Auto-refreshing browser cache
  for l in $(grep -lrE "['\"][^'\"]+\.(cs|j)s['\"]" /www/cards /www/docroot /www/snippets | grep -v -E '.js$|.sh$|.json$' 2>/dev/null)
  do
    sed -e "s/\(\.css\)\(['\"]\)/\1?${MKTING_VERSION}_2021.12.06@13:59\2/g" -e "s/\(\.js\)\(['\"]\)/\1?${MKTING_VERSION}_2021.12.06@13:59\2/g" -i $l
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
