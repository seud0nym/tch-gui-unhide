#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.12.06
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

if [ $(grep -c 'seud0nym/openwrt-wireguard-go' /etc/opkg/customfeeds.conf) -eq 0 ]; then
  echo 040@$(date +%H:%M:%S): Adding WireGuard opkg repository
  echo 'src/gz wg_go https://raw.githubusercontent.com/seud0nym/openwrt-wireguard-go/master/repository/arm_cortex-a9/base' >> /etc/opkg/customfeeds.conf
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
echo 'QlpoOTFBWSZTWbaO8fcBmEP/////////////////////////////////////////////4sme95LH
2fb3V08OL5ICrUW56hFV5qrNDawPm0qKlHMyiQN3cTthjMqKO2pQ7BbGLuGusrbXbu6A67sGgNVl
1rKlQIgp9gaUF7eu221bGhdB55qESr1qB2but16ru0azsc9PgAAN0d5WlYMhNu+6veegGg7r0KVC
XTnN9aeNXrgkDRdVWtVu+91eq0G0wFsaAAeJS7OhMsBVCUnYwRdAB32DSgfQGieO4O0yigAD5Bqr
xYblZObVKCczXTtjZ3Bq4GHXVotuwHCa0U11ZY5E88DnbaUAAWYnsBtgUDkd2LsAAHRuwDQBLd7u
7s4rj012cQHeivK2b3h8IoAA6UfZgB6Xa+B27oAdLa8woKFUAADdgUA6AB0ADIA2dzjVUHvK68Xn
RUqemirvvgdAA+YPqgiCgkVKJKJACL33xa8GQdffTz76dVquKn3vdvnnvau9z3r555762z7rd1Hc
V3ZtqNGcYlAUGtAAAPvvcHkAJAAAAABipQaFrGqA1oBfdm9rueynNvfabL3O3fZA30903udexVZW
xem7Wq7pBe7vd7g97NdKtjRy93dE9AxGve1L7nu33Rdt9zoao01BszWPmGi6yzb77u6+H3jtt9a0
PpSjrb3O9u3XO2HhtpLLIeuuikVYlfQeiQ5PQ9O61pXty+33Tul2Ve+999bvPnS+wGUm++D7efUi
gO3O1QLbEoRt75g873fePXBzu959GnNJtR2Bzs6595u+envL7sPffdvX22N177KUAAkABx5jOh77
X1s+sOb69FTvvrvvvd5nuZ9d8b5V2z3ZvjX33Bz5l7K9n3ANOXvr3nmMblk132d97j273wvgec5x
Rs9wryXs6Dcfe8d9ta82vtXQ5et724+go+XV93T3oGiLqfTnaj6OW+zud19uDnvjVNKffDtJs4xk
WAKERQPefEL76+vG2L72KpeZ589O+++50X3vLvb3XeYQz044rdHUwbl297vQ893bZ5G9p1o0ulve
eveYbvd6d7d03dfO733wueEoGoXd3Z3dl3ds569y9XBRd04dd63oor0ADu1tjd2F0tq2Y7slasJU
ArQ928zY69143fd7g9dHHZ3Y5dN77Xteadj7vVY4OhyFtjQm7xl7oc7t3ne73hKKl17x67qg3t31
g+qIRCpVVKFBVPrW+73N7sHZjlOseXe1dGTzr3q8c7Zbb3dcPdd1HKh7vL22JvfewOx8fWeFiKlo
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' | base64 -d | bzcat | tar -xf - -C /

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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.12.06 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
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
