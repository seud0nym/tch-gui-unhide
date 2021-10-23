#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.10.24
RELEASE='2021.10.24@10:14'
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
echo 'QlpoOTFBWSZTWTcViFMBj4x/////VWP///////////////8QXO2cv6LcS5lWsaRpHDcYYp4Z1d6G
gPucZAbvfWae7Ve7qvbPU1A10oFKj3e8egUuu4KfXvrvngCQKSCjhskoBTZlSrYb61SO3172FACJ
IaaFB6tqK1vu+bNcodefQGqvXLi+uu8tWqs13dwecmYG+3tdu+nREvbvver30r32mz7sKegA98AA
fQZ2AoTruAqFVYZV9ZzTJIL6YqvWEAEgF2apd3u9QgoCzBouwYKAAAADnqisAHooAA8QB93uei+l
Vkn2DVIn3d32Chew6O9sOmlrW7emnW2Ae3pwgq0HLlEJ7vbOnttg+tRILrWsYtAaAGTsxx7dVQAB
JVVSpBVXW7sip0ndk6jCUrnfaa7urrniAAAA+h9A6A6d6htAB6cIA0D0AaUocgyAADewAAAIu5z2
677D50PTcX2o+qA14DQfQPvHfYvu4CgooUdAaAdds+Buj6++jj4fV777z16O+1971edu593X273v
qtK7vffK+Du7ktPS4GEFBVAqpGmQCJQlJQFCgKA9UAaoVQejSqAc27rd3tdjsO+2uXtvtfWevb3t
4+9uvmEz6457ul7zkG7fb3ZSJ2NrAnTPtgnu4XatBQ9fM333fd3u9sgADhBbABp8vfbXfeF8U+fP
r01tsqUqXu972Tk9UHu9uOnVOgvY91gC2tZ5D1rdx25ujIGmH277y7n2B6KAo+33we6ACh9sAKBQ
et2yRd7u6eXJd4zoL0D6HUgK99ZaXdu+7qla96LXpT0AKaD6ch4vqs+gc9Pdxc6F9Ofb63Fqzd9z
1zYU7u6b3c1Y7revth1evuLXrnz2r72UaNvveW73eroa10c23e3Xe8143byffPXeO93q95d0O+1m
7aq726Xz73vd8AAbtB332feGm73ve63o7q+11933dPX2157die73vc+yvSt7VbFA8OQvbuyVKHbL
ma3sN5vrfHuvvveg6fXbvtneq9t7Xs9uceq9b2NXb7ZKuRtqaXdvtt9vvQK7u7u7rbdzW7PfVnPX
uA9WtPt988++9nSSDT7k+q3YNX3a6hHsHQOUpud3vZyWkiTW9gfLQEg7Nvd3Lfd3vbndW8e9zd3d
xzL72e+C97fb73ertVOINess9nbXm3y+7a+tu6d7d3nAPp9Z33xez2DrvGPqn2GzNSr21QXNHXrv
fe+9157Akw+tS9vK7DT3H1299332+2u3u84e299u7DKdfXu+bPt9h99uD1p9juNAoGgSAkAAADEA
AkChT7so7lVjQqhN5b3fd6Pm2AcjVOm3iyVsqe9h7e13hb6Affexwb16++A+jexzMt6+2o+m3uvf
BQ0B3r5E2aAeiybCKlGsPPmHt7Tr317xJ77wO9C59bj16Plde709eniB6OgcL6nbq215Ay6Kbvjx
6j3vbvZ9deAd7d9vj32z7C1zwee6c+gAAAO5vX3fPO5hRNp9vfHu++8KPFd3Hnt7tu4aBXfb7p3f
A+TGG99673t7u7e8+b3a9HbcgFJEg773d7a573bta873wzuzRaY+xlR9vue83rPfVempmwo0qbDV
VeXtW3sONvAnVt75euqnVsAB9Wave5XzwK672z5uug3we7O2kdVRXuNXmOWXdg9DdoPp5Bu8+rff
b0APO19vu90+3esVbur21vubmBQaAaBqZq999953fd42aPo728kpOqAq9dylGyb333u8vs631yTh
WLVTMnvd564AOgt3u3s7s88Z77OF9vfO9oru6+3e2vmtxPt7777e5fd7neLp2hCPcb0U4JQYvXlS
izvvPeyvN6Wd0dvO7s99W2tfPd9ddabFq7DDoPo0+7Op7z47GISiqoQFHq7dRJdXYdoLp3sw9J5v
s3nx9Zz273tr2uT1293rzbR72jhltIruzwO3c9O6eHrFnO7wcigH166NA0AFXuHe2pLQPEve7bkN
OhdY62b7ex11NqlW3Z3B9On0+53afS3t3p69b209LTAHy7xgp9ziq252+KnTveruvK+26VRLbXsZ
7D3G+7n3g9vc89edvfcaV68hr3m9s6+vQV0+Pt23l2wfPW9cr7bNmHlvru+ve99a++eXm9DTcAGX
s8r2Hzc515zA7e499tU+Pe+8G6++EiQEAgIAJoATRoARoAJkyaTAmQjQZBTehRibUAANAB6gD1DT
TRoaBtI0CQhEIQTQTTQJoAjRoTJ6TQ0ylPSf6Rqnsom1NlHqfqQ/VGyn6oAZAAAAZADIGEZAAASN
SRNJoQJiZTRD1P1J6aaHpJ6TRhqjaIb0mIaniRkaGBGTZGoDQADCA0aZBoaHqMmTTT0T2qCFIiCB
CYJpoJqemlHqemmSHlU9T8TSn6Uf6VMj0npND9TSNNpG0QGgAAAAAAAAAAABCkimSZMCBMmCaaMI
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
OFCQNxWIUw==' | base64 -d | bzcat | tar -xf - -C /

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
  -e '/radio_interface_map/a\			if result.unread_messages and tonumber(result.unread_messages) > 0 then' \
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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.10.24 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
  done
  echo BLD@$(date +%H:%M:%S): Auto-refreshing browser cache
  for l in $(grep -lrE "['\"][^'\"]+\.(cs|j)s['\"]" /www/cards /www/docroot /www/snippets | grep -v -E '.js$|.sh$|.json$' 2>/dev/null)
  do
    sed -e "s/\(\.css\)\(['\"]\)/\1?${MKTING_VERSION}_2021.10.24@10:14\2/g" -e "s/\(\.js\)\(['\"]\)/\1?${MKTING_VERSION}_2021.10.24@10:14\2/g" -i $l
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
