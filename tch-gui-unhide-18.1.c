#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 18.1.c - Release 2021.10.24
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
echo 'QlpoOTFBWSZTWamQt+gBuo3/////VWP///////////////8UXO2Yv6DcSxlWsaRJnDdYY4Ce8Pbx
93cCPFKUOcr6YHT3ZXRbVR219ZRUvnfO8qQV33OuvXK87e81FfNhRvs4qt833g22Sq277vArzsNN
Az7VooX0Pd9y2qApStrVvY+e95rpfL7NfdKgm3dq6z72d3c50dHN7ufXslZYXwO3VUlL77vffXr3
I8j10AdKAnvgJBV9nPsPd73eN9do49PrB0d5appz7veiu+0d9cA+tXgxQ8zaTO5lu+71CHoqN7dQ
S9A8KAADQAL4Aw+qvWtn2Y0HXLmM7ZvPs95fD6YV77uSUfM96999vb1Zusn3W7327twK+++Wfd9v
Q8p5tp976e9qSp873u33333vvFz1330X1s56X2bTkVp97X33Hudz5ae3HRm+5vux7j7dvb298PFQ
S7NpkrWSGHe+93dbfPdvnzy++vnvpZqXfFDfHdrvnzfd697QFDRR9JEjQGnTePX1299AA93twA+u
i+xodQk1vZiEKBQ32AA6AAHjePT6KHz73d7zfM++AFAB56p9agFe967O766HXroc29tBdb263Zez
dYB5NfH0W6rs+XvvrfC12+rntVSd654t33r7Vu+dy6+nafdp9zvrw9njvruSO3vURn0dw+jO53Wg
Cc3Q6BuwaKqg5LM2WAUzt3bVmanTST3s5QlQ9BiC4WGmNq7ru6oIOwGEAAAAFHO7qC++++6Hp8B6
U6bDvl7z1mlFB9D1T7bq2AG1fb74OO8n1nfMqu+dx5vd3j3zIPr33w327t7m5ZPp53Tu+zHan242
3UJGgMF0yNgWwtq1o7anWzJAK2wUkKAFbsOXewPfcc3C8OBI+2iFME2z2wOsoz099n3kefFtj3ne
7rg96x66oexvWddtd0dXU7dU6b21yzZa2Xu76b0jCQvam7u7O7n31vPnzEu0Gw2Khts6DrvevDva
A2lA7ZsL7uCvdvvj3oHu+4+je324q43soU6b5a648eTt7m7u6UhoaVdjd8D271o6bsu2qqfe51Wg
DSJRZhWprVnu+rLxx6or6WnM4bad6c9Ap9vlror164bAaqhEoovWQgABbz3nrfeM48qhFavezrZo
a2z6auvAAAoPQ0d4h1WoBPfXx8+AAepFFAUoUUqPQdxqOvm+30++e+aw3B6Bt0+9jdbl9R2S6d73
eq3bve6vOnXdej3rnfd91L20b2Oqe9tXM9275g7Y+jr3259Z9ffb0D6+n0fbdFfd9Prwedje3eb3
zDJLnnuB32+7fOyl8Zvp533vuL7UaNU8SbNvAAL1e1vaOw6OQnrem3OfT1cLZuxzvmde9G+7jHr6
uUE9t4PIJ4wBvvJ8N8u3duu43YOuWKVE777j17C99zo+bTvj3X17njEG7tH3i949T4d3z63319sd
XbKtuuvD33uO9s09PVd53277n2+N93svvT69uu6b3dcvo7XPO+57wykDcaRNbmBnJM5NDauNa+73
q8N1u67lVRNzjkCXNrK5DW3JRc2TrO223dqOnQUCzKvc7zzlu73e8ryV87fPvdvbBRhubVHZfdp9
16fD77dw7030rTgAWs0LW3d9zvUe+fb31DX0p276MDontrac04C7ux2pNnBQqVQqK9V3Z3e7o+9W
7zcfW2h2mM7TduHtve121QZwTrnWZdtOck1Us2mjXLbrBYGx4acOxmwhAKASKFRVQEX33HKAewwL
3vO8Hp9s+te+93e2zC7ajsDHu4m57fPWXYL3198h9bb11B9VeoO9tqJqRbes8fe0Po1lPqx0Mg26
3M6uQ6MMuOB3Fuy3Z86XTuu7FtrNPVrdk7XS88hBSG3XjA27gAANithszFk+d7vUNs+++r3iw1tn
1uq7AgNyEbbsUdEE+u9vTduybfe++z31WzbApoOnveB8shTwfRrbdrjuSknJuE66rnVo+tetedDF
Xk9a9vXPW7Y3uqd9769IymvfUt69YsueCSgAoHu9d1rt1C0bYyIpjcd3Mt3hNPG0Dri7vdPCgome
gU3e97xSIvbd5M6VV59nOeH3TM97NtrnOg0lryn2xe+99973vraTbL6qe2KrrsVfRewXEQ80qzGp
badcOm1xykQgFEVKi6e3k68pzbO333vE+7rqHofXyyZWAAG2jO+xSnPO7Ll3dCdYOAkq6MlEPru0
T14PbrittQ6ve8d5g3e219YHQvt8wmjHer1AAACa1q8tya45KsUi5pmABrbAAaBpuevO6O5e3eef
TbXt9vot23bAhqtUy1LQyqrbpm53bCLQzb7vd3Hese7F3a5ZMEdHXVBQbbd3U+4fKj77uhV9vDc9
3ddWLqzXtqd7psTRdaKuvXr3u3Uapb14532dR77GySdrqFCh0HS259Ccbxc13rveu9W7neZ0A07y
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
ACJYtX6jrrcr+VbwbF3i+nhJ5X7/TCyUq0vvEaNPRWX6K889BqVoAP1Q5RCERBvPJ663bfY58/Ho
bZKPOprO6HPBsfvmm+TAVq05xfvtkWJjVL8NLqEaRvMecW22GtBhwbld5ZoEzMIQ2f4vzxxObYO3
z1hQNwL3MR4UZWYdIMs9xtawiWHY9If7ogVDsipjFWN5+n8b8sNnz6Y+rotwRNDf+jdsr346i4bv
bSvQBysQ7AbQ3dSmbaxCj3+6f7uy/iCJy4fq23PLyrts8qE9QkIQJHu/3hGIlFVQ8UetvQ+PmPxh
+j+QCY3LOtuRZ0N3WxmLywxJ7UxugOSoobvb2aoWNHLO/ZoHdja2PstuLmmwEycV/zNsvE/ZOyXq
79oGIjUh/y6lk/ogH3xf+TV/KAgeWVB5FQFKD4EDhIxANAYEWMQIES4p3+G2vnoOids6rd2pLX2Y
0v+BNm0ztv8paOjWsbz8kdnj4OzTOj0BF36O2MF9pBng+z7YsVBftgxg/wMDKBEq6ND4Bgg2Kemc
r+UBo6F3/R97zqPqbUnUEeZc4816Y554Q0677OCRnCEyE6MiSFMZKgdNWCzN8BbENj6aQdu4ucyj
Ix5/RGRhlkLQ/L+Yv+EPo99V8TylafNaVMqDnb0bB/dn27RFZMfdKq8oPAKI4AQgalT0x3ASpvIQ
3W8jjA7SJ/zfOdP6H0xr/fYf+CDyf/7/d/9P/tf93/3+H/S+bp+i3/l/2t/6V/+63/wf+L/+P+j/
j/9P/H/0fk+Pv6v+Dv9tn/f/+r06n/4/8n/i/9H/L/5P/fP+I/8P7/p/8//V/q/5MP9f/fb7v+X/
zfj//f/2ej+76f+n/b//nH/1f/P/L9ez83+3/m/T/0/+D/1PxH+xf+r/+/+vw+//N/r6f+Dfw/58
+//P7f3dv+z/X/0f+X/X/1/7P/b/H/+X+3+OP+z/j/m/8v+3/9d3+32f8/+2z/r/zcf/H/1Wf7P9
n+3/wh/8yn/F+Nj1UVGUYj/f/vz/oM/7qQzR+hlD/Y7I1EGJZ/21Kf3iC/XuMfh/r1VjvVhABh/o
JMAmH/lKM6cBCkI8AT+k8hxiQpw7EE9GRrYkGeJGsrr0/38klnAjgOpRwiwL7BJbNXDl9iDqBubF
3Mp23oNHdMQLkQYyuRUzL/3+hg9O/+Szr0Dy6pd1ryffocgQ4bbjjJt/1bLzU5mv9gdUDf/jV5KV
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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.10.24 for FW Version 18.1.c ($MKTING_VERSION)\]/" -i $l
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
