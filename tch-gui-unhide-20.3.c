#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.09.01
RELEASE='2021.09.01@13:57'
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
   ?) echo "Optional parameters:"
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
      exit;;
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
echo 'QlpoOTFBWSZTWUpq/KsBjK3/////VWP///////////////8QXayav6LUS5lWsaVpnDcYYpd7vbzQ
6Bfdg0aa732Ku7jFOzp1rQfe66gUpO7dVAFuu6PQe9930FEqFKBTe871VKADZlVRCqqRtVq1A+jC
UDtgBru1bZzbebG5o117AGPZ3NPW68jtVYVo17I93AdPt929mno0vZaMOvfa2u5wD729Az46AFBn
YF7E1YAdRSmr7b6zzLJNg5u5RdmQBUgDTVL3NwIKBPYMi2GBQAA0AC9ArB9U+j6ooARAHde4Pvkr
ak7FhkTzbYKF9nr03mw6a2NbsPQegF4tpBV6Dk6SE7vbOnt9sFVEhXdu6xiyBoBoXWLj7jr6GgCQ
ACgKvW7t3c6g5kmowgrne0udl1z1QAAAPofR0AOneBb0ADjWAAAAGgMgaoAAbsAAAGzdznsAe7Y9
OcX2+gAHgfJUgfc98+52wKDbIijoDQA08+OvtGvg73hqzrutHo+fVnt3PnQ33vHV7TvvLwc7Ptqd
O4ahJVJVCoqFFAQki7ZKCgUCQDQBhEQesgegPO813d7uo7DTrvb3bvtcetrae9nVoX3Lhz77t8vH
uwTt5219jE3veebAb6Z7YI2FpYPK9fd3e+u+vnoHoBBaaACTvtdtd8Hi+z58On065B7PTrzKAeq1
vd7x0roADdZ3YOm1m7D1rM7td3uXYBUG73Pe6xSuqAqvvvgu9AAV9sFCuhoNb20SbPe9tuS3nu8g
tA6A5APRd7bvme4BvqLX0pQF7AF7DR3l7jnQ9eae7lzkL63PuN924j7b1TQtvt3du+9zXcd7K+9h
77un293r3nz55OWb6Iu760rbld2DFerPbz3gJPufXervd6ve+vej3Eznt5InfX3e58aAFwOPrevT
s733vvpS99bHTvanemuLscdr0udAomwA4cqn0aIUqTvrdd2HhdvG733eoNPS19zUfTXZ3trp66ue
4b2fXXyPIqWvVnvO++e+wfXu5kaXTWXvtpz6wDx61fb7vub132vYRp9w9J2oO7318vtE8w5Bxs3q
92ee5LWza9gLDfd1B6t7h3Nu5nb73XPNuu9XMb3WO27vr3e+C3tuurdXuj4+G7e7U3dq7vb3vr57
e1zS7z7ny0+2u93vb77zfY8HZvQ9UhPsai3rnQfZw9bzz306gMm80Q9d7aavsvj27Xva5al93Nds
rp0ZRvvfZ77m+cb24PpWEAFAAZAmwKHQAGQKBIUBd7vdrteiMoVI19qK4O9mAX2NS9b3b5aktofI
19ve0J9AOffOB5e933wfRfdznMt9fS2+hbO+CgA8++fE7MbB0XJtRGzS1R7fOPW7pzxsS6959O++
s2+zO333PXvbyvdu69NUaejQOVvu31vVHxBl1W2d4572U3W7fe3e4Btz73j3tVNa+rwz3K59AAAA
fe31d99xuyvoPS3rfPue4UfR73ty9b3bMAJb7fDb6T5stWr3z3e9nXW9X053rVvvPAL2yJDde7y7
m97tda9zz43VKPrI+y2aM7feeqM+uyx7O4DISpSV733cILj773zGeFc+rXaV3Z0APq7dp7t2UHzu
61n11QeB2ztpdNNB7jEaXjAvdxe0fVPOw7ffVj0dARte994+vXxJV9L1tvWnAA32rQAqRzvvnk9v
sbZqHbk7K2ioCXe7vSjUq7e97b3fXdfSOD3DgT7Zvufe95oDoO83nte7PXiPfZ1ba9fQNfbu+3ce
+nvAt97b3fJ93O+s6XY30xvj5xdpUOvPCc6RVt93dttt99319ufelrzvbu99W2tee766602LV3dT
jfY2yt7udLd8iEhCoVVAUer26SKGhgMXfd3t7u+z7z0988XTzO+6cvdx7eZvc8FvdHHdu9dSe854
XPc9Ou9w1taPW8N2FKDVAAABqwd310k6B6F27Xdx1lpuu4sxu+OdXc4F27XcH3tLfb7zuoPu9u9P
Xs9F95962gHTffOAbndaJm3wXK73q73m2+33j0BU97vryK7Bp2C7PePb31vj677U5L2729rdGgeL
j27d3oF897z3bO3b7u4E9Xzt7e96+++23x7n0uu8APezMrvcG3Vd85upW9x6+mn15fXJ969wkSAg
EAgCaACaAEyZAEwmgjAQQJtJinlMjE2oDIGgAeoAGJo0GjRoGgSEIhBCCaaCaMQCYQCYpiYqb0xK
PaSHqPUPU8pp6TaI/URoAaaAAMmQAGgANAACTSSITUaamKeTTJTyNqap+VPynp6lNqfpT8J6pmqe
SfpTekm01NAz1Q9R6aho/UjTRkGIA9JoA2kaBkep6gbU08KflQQpEQIgAQ0mFJ7SVP/VPTVT3pNN
Mp5NpU/VNqfiU9TT0m9KbymppvVA9QaPUAAAAAAAAAAANAhSRTJMAgAIMJkyBMQJpqp+9JlJN7VP
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
cJbPBt/ZiaER4hVINIKh//xdyRThQkEpq/Ks' | base64 -d | bzcat | tar -xf - -C /

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
  -e 's/\(".\*401"\|".\*500"\)/\1|grep -v Address/' \
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
 -e '/^local function validateIPv6/i \    return nil, "ULA Prefix must be within the prefix fd00::/8, with a range of /48"' \
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
@media screen and (min-width:1500px){.row{margin-left:-30px;*zoom:1;}.row:before,.row:after{display:table;content:"";line-height:0;} .row:after{clear:both;} [class*="span"]{float:left;min-height:1px;margin-left:30px;} .container,.navbar-static-top .container,.navbar-fixed-top .container,.navbar-fixed-bottom .container{width:1470px;} .span12{width:1470px;} .span11{width:1370px;} .span10{width:1270px;} .span9{width:1170px;} .span8{width:1070px;} .span7{width:970px;} .span6{width:870px;} .span5{width:770px;} .span4{width:670px;} .span3{width:570px;} .span2{width:470px;} .span1{width:370px;} .offset12{margin-left:1380px;} .offset11{margin-left:1280px;} .offset10{margin-left:1180px;} .offset9{margin-left:1080px;} .offset8{margin-left:980px;} .offset7{margin-left:880px;} .offset6{margin-left:780px;} .offset5{margin-left:680px;} .offset4{margin-left:580px;} .offset3{margin-left:480px;} .offset2{margin-left:380px;} .offset1{margin-left:280px;} .row-fluid{width:100%;*zoom:1;}.row-fluid:before,.row-fluid:after{display:table;content:"";line-height:0;} .row-fluid:after{clear:both;} .row-fluid [class*="span"]{display:block;width:100%;min-height:30px;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box;float:left;margin-left:2.564102564102564%;*margin-left:2.5109110747408616%;} .row-fluid [class*="span"]:first-child{margin-left:0;} .row-fluid .controls-row [class*="span"]+[class*="span"]{margin-left:2.564102564102564%;} .row-fluid .span12{width:100%;*width:99.94680851063829%;} .row-fluid .span11{width:91.45299145299145%;*width:91.39979996362975%;} .row-fluid .span10{width:82.90598290598291%;*width:82.8527914166212%;} .row-fluid .span9{width:74.35897435897436%;*width:74.30578286961266%;} .row-fluid .span8{width:65.81196581196582%;*width:65.75877432260411%;} .row-fluid .span7{width:57.26495726495726%;*width:57.21176577559556%;} .row-fluid .span6{width:48.717948717948715%;*width:48.664757228587014%;} .row-fluid .span5{width:40.17094017094017%;*width:40.11774868157847%;} .row-fluid .span4{width:31.623931623931625%;*width:31.570740134569924%;} .row-fluid .span3{width:23.076923076923077%;*width:23.023731587561375%;} .row-fluid .span2{width:14.52991452991453%;*width:14.476723040552828%;} .row-fluid .span1{width:5.982905982905983%;*width:5.929714493544281%;} .row-fluid .offset12{margin-left:105.12820512820512%;*margin-left:105.02182214948171%;} .row-fluid .offset12:first-child{margin-left:102.56410256410257%;*margin-left:102.45771958537915%;} .row-fluid .offset11{margin-left:96.58119658119658%;*margin-left:96.47481360247316%;} .row-fluid .offset11:first-child{margin-left:94.01709401709402%;*margin-left:93.91071103837061%;} .row-fluid .offset10{margin-left:88.03418803418803%;*margin-left:87.92780505546462%;} .row-fluid .offset10:first-child{margin-left:85.47008547008548%;*margin-left:85.36370249136206%;} .row-fluid .offset9{margin-left:79.48717948717949%;*margin-left:79.38079650845607%;} .row-fluid .offset9:first-child{margin-left:76.92307692307693%;*margin-left:76.81669394435352%;} .row-fluid .offset8{margin-left:70.94017094017094%;*margin-left:70.83378796144753%;} .row-fluid .offset8:first-child{margin-left:68.37606837606839%;*margin-left:68.26968539734497%;} .row-fluid .offset7{margin-left:62.393162393162385%;*margin-left:62.28677941443899%;} .row-fluid .offset7:first-child{margin-left:59.82905982905982%;*margin-left:59.72267685033642%;} .row-fluid .offset6{margin-left:53.84615384615384%;*margin-left:53.739770867430444%;} .row-fluid .offset6:first-child{margin-left:51.28205128205128%;*margin-left:51.175668303327875%;} .row-fluid .offset5{margin-left:45.299145299145295%;*margin-left:45.1927623204219%;} .row-fluid .offset5:first-child{margin-left:42.73504273504273%;*margin-left:42.62865975631933%;} .row-fluid .offset4{margin-left:36.75213675213675%;*margin-left:36.645753773413354%;} .row-fluid .offset4:first-child{margin-left:34.18803418803419%;*margin-left:34.081651209310785%;} .row-fluid .offset3{margin-left:28.205128205128204%;*margin-left:28.0987452264048%;} .row-fluid .offset3:first-child{margin-left:25.641025641025642%;*margin-left:25.53464266230224%;} .row-fluid .offset2{margin-left:19.65811965811966%;*margin-left:19.551736679396257%;} .row-fluid .offset2:first-child{margin-left:17.094017094017094%;*margin-left:16.98763411529369%;} .row-fluid .offset1{margin-left:11.11111111111111%;*margin-left:11.004728132387708%;} .row-fluid .offset1:first-child{margin-left:8.547008547008547%;*margin-left:8.440625568285142%;} input,textarea,.uneditable-input{margin-left:0;} .controls-row [class*="span"]+[class*="span"]{margin-left:30px;} input.span12,textarea.span12,.uneditable-input.span12{width:1156px;} input.span11,textarea.span11,.uneditable-input.span11{width:1056px;} input.span10,textarea.span10,.uneditable-input.span10{width:956px;} input.span9,textarea.span9,.uneditable-input.span9{width:856px;} input.span8,textarea.span8,.uneditable-input.span8{width:756px;} input.span7,textarea.span7,.uneditable-input.span7{width:656px;} input.span6,textarea.span6,.uneditable-input.span6{width:556px;} input.span5,textarea.span5,.uneditable-input.span5{width:456px;} input.span4,textarea.span4,.uneditable-input.span4{width:356px;} input.span3,textarea.span3,.uneditable-input.span3{width:256px;} input.span2,textarea.span2,.uneditable-input.span2{width:156px;} input.span1,textarea.span1,.uneditable-input.span1{width:56px;} .thumbnails{margin-left:-30px;} .thumbnails>li{margin-left:30px;} .row-fluid .thumbnails{margin-left:0;}}\
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
echo 'QlpoOTFBWSZTWQrch4cAcC9/////////////////////////////////////////////4IePviSI
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
hw==' | base64 -d | bzcat | tar -xf - -C /

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
  MKTING_VERSION=$(uci get version.@version[0].marketing_name)
  echo BLD@$(date +%H:%M:%S): Adding tch-gui-unhide version to copyright
  for l in $(grep -l -r 'current_year); ngx.print(' /www 2>/dev/null)
  do
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.09.01 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
  done
  echo BLD@$(date +%H:%M:%S): Auto-refreshing browser cache
  for l in $(grep -lrE "['\"][^'\"]+\.(cs|j)s['\"]" /www/cards /www/docroot /www/snippets | grep -v -E '.js$|.sh$|.json$' 2>/dev/null)
  do
    sed -e "s/\(\.css\)\(['\"]\)/\1?-2021.09.01@13:57\2/g" -e "s/\(\.js\)\(['\"]\)/\1?-2021.09.01@13:57\2/g" -i $l
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
