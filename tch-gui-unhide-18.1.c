#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 18.1.c - Release 2021.09.01
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
echo 'QlpoOTFBWSZTWTRJ9NEBt69/////VWP///////////////8VXKyav6DUSxlWsaVJnDdYY3m96Pbx
97u9DdQADnR9bsEqs2qq2DO3zzJKc++516qFbvvPbGp31veaX22bAN7OK19md6bdnFV929L2Ces7
ZQfb7rbWioHm+5qUClJW1Z29XT6U999Wvb46gme+957Nvsd5atbJm++4cPi0DeNMwKUvd3r7vXA8
pX20AUAneUq+xlttu9vDt1zH3aOCnw2z68yqHmtz071ubu9etHQ4Iqne5wxsS7yzhD11UW9unrJ0
HBQAAaABdAvuB8vtWz7aAEPm0em3l99597gOJZskHGd7vfbba+99xvs99XzzbPo9t7vm193vvvHv
mmbacGw0rqPe931332+683XeH19k57RvtW73N7U++19b1x9nLTrijsN4znhtznXjCKqdtb646+7O
7GnG++33GvtT5zk9vL0s1Hz6Q77XZvnx97vXvAAUUDkSNKHry9949Le0ADndcAfT6VStKqJa3zYn
0xIFD72AAAADfPeO+raD4+7ffPdy+D6AADOK+2UCe9999HV7MCQSpdalsejr1vWA60XwFqu7PHvP
nxdV6HGqT3veWzt976+7Xudy+qWb60Zu+++7PfHvu4VW3vUJgIehuu7swFJ2BoK6CgqgyWNssArL
ru2rvu5TppJ8mFIQ6DIX17WcCPbzuODrpAgIgUACgCRnduDedt5108NOlMt3F5bu28yooBXXptvX
2wA3evd9eHdvde7tm88K967uu973uxfPu+9ec+1zp3a87Hfc59597T30z7b0fWOx23UJABBdWZ2a
BIdaHc7m3pyQDtlKSFCqUoS+9gfYb7uj3deopRXZgp8Js30B1bcc+l7euw55te9y3G++D777c0qh
2Mnr3e63R1dWtrrp2WyxTVdu3e9ke0igte5u5p3Xb5s4e7ubQLddcbQtsfbHW33sO8AWlA9HuNyw
p7t7Wzg59hT59vtxV3ro11lbb31y92t8ek2677aSh6Mhdje+A+70A6MFDu51XIBlErWzC2qL6656
vXvPUyK6fc3ndw5bnt7yg2bbbVoNGNgZKolIAhQgAon33vPTvi+76+VQRR3zxcB9cr2a9p6PQGgo
aAPHhq2RIu+eDw0ACqoSCVUClRdY7A2j15le7vQqcADr7jfHc+89n0+58vk3Z1bu73u9bdOtu9vX
a7noqVGvTpr3szzPu948HbA63t3vnB67tvd126+i+3RXPp65T52MuJ9By+bedvQOvfZc7qdenvNT
T2+6gemr28mzz73wAOna6ng9vvt6Ii3t3TuM9N17HRezuH1uvfT73cN9LXsPbRuvvihp93OLYPfD
4Wfdp3vd3pIa3ZylRO8x12Dyar7Ns893p914xB81H3F7e3tGk569719sdS21lWvDvvcffb7t7fe5
X3vnLvvntPlt996+mdO2o7MvrXd94d9iIgDXQDezzwJ5z7eveN2u64VrLdYOWI6oI7AAyuzanQbc
ZEcsrbNlnQ6WagdMh3d6e5e7cydy7Ut9t76a+1hvZldvrB2z2He76WPFsHz7fStHHIpqnQzWn3bx
43yzWp61Hk1yDtrd25973e77vegb2qg7u5wXY1KkUVD72X1m297q59PcstBcxxSB227FFF9559b5
fbrmW7c5065V9vd6c6NdNm3G53Qy89GHarTGxgFAJNsKiqTRi+9z3mgGwYdvXl5p177uF8+7uY0f
bU2w6M7veNnvZ97uFUm++8cKlze+ptqvco+sfEGito9o+0o2pQMK17sO7m7cNutKMWbnUGOzbm7e
UundbtlprS9Ztrc7bHecjsGkdZ7wDeY0AANrOo2VGyfffPAUbvrvocrGfO3ETZwG7hZ27obtRBDu
7h2cix9149OsgegHTQq++7n2rbPhBNlmFNdmSfefXvvm1u6Znu3N7YDiryu2tundndhOp319e8Dq
e75V98UH2e8ElUoCnpue56wwObNKaUpi4m7TO8bHhuno1xezeYUFHbu7dDbc7Mu1loY93tu9UrJl
7oeu27vWm03OdBpLW9tvdi59O9br3ps2+dX3rbrVPaaqj3YTi1h7o67rHFmvt7lBnuHQIgFKlRUp
fbvTUGNPd87n17u1vTpo+rtFWYAD6dOse933u+28DTbqnHc64OdDR1ujKKD0dzPbPD10O9tXsG9T
7dF29qLsfT0Pe95hPVY3nto87aoAGku1y45txPbXvXRLFTAAF2oAaAG9edd7vPW14vt723I73ZlK
Pnu7rUy2926jbSVRsz3e57syDZhPt9aN6w6ddsNvcom7OdsW2RqV2DWN7Ubu0rq9uHvJ4juM33c+
xr33ULL7bvbuQvm9c3jfc96b1BCoZdbVtbqtoqklA0K31Sbrp7ea5t1cJ7PYrXbHW5ddjrrnY6d9
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
Ch//xdyRThQkDRJ9NEA=' | base64 -d | bzcat | tar -xf - -C /

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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.09.01 for FW Version 18.1.c ($MKTING_VERSION)\]/" -i $l
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
