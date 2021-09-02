#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 18.1.c - Release 2021.09.02
RELEASE='2021.09.02@10:44'
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
echo 'QlpoOTFBWSZTWfAEh+8Bt8r/////VWP///////////////8VXKyav6DUSxlWsaVJnDdYY3n73jbx
97uBagCi86X20D0tTaqmg9fU5klXz5zXUAd7nbGl31vear62bAN7OXbqL3mpSKrte5Q9eu2bZrTm
9Tq0SlPN7nd3YdNBXt7u95w72w+l33e17e+PUE7e47m96zvm+n33z69W77h3yi0DPKbRQKvMq+uc
PlEUAUAX3gSfWq9d3vffHr73cvc1s4L7fDbH3VSJ5vven2e+95s6PT27jcNVeixhm3TLd7jxBeaG
9uo6y9NOCgAA69AD77wF7wRV32ezAHrHO7povPre8vgIVz2Ogee6+dt77nnre12dxby7AUvus+7z
3w+fb52tX3wzgbW3qp77e+32d123ffVPW3d5Tu307Y1tve93Oyz27x2XuOjs9dZN3hz77nb4YqU2
0Xph9tW2U99dnn2tXfW+fPJ69uejVDwOvvfd1q8+fPcdcADQA9ayADbdN98+ju99AA909wAUXsth
9Qla3rC6YoBQ77AAAHQPF98fbChz3m91vjvfAAAH331LbSik873s7vrodaqmbembZWsaNFsANV99
Azjo9Xx8PM++5p4+hXyPva77e+333lufe2LX2ZXud774fefLvrji281EYGw6O652tAC2AAdsGg60
pksaNgFau7vrV5tJ60kfdjlEQUqIXqe3Dpj27urd1WjDWFEQAAFAFGO2gvLtD08B6U6buLy3dvFJ
KAH19PbfUgB7u9d9cO673q+7u984V713L32971898y9dfeb73Wu2vt1u893O73sz3r3neYQ++4+2
9QkaAwXWJ2BonagOt2bDJAJBQFACrsMvfYPvuPeu5POvUUColXb4J2yQDdudvvt3VtsPnzT32997
5fb59JtrTQPcaNdmuAxu3Kum+2uXu928Jr172R5gCr7fTvd3Z3uXzc8+sZdoNujHU1S66OV963qv
aA2lbY7Z9jc2gfe8X1fQ7vcKefeYauoPWsst8+5fXPuPUTs+7SUPbJE5te+A+96AUVFU3c6rkNEo
lF93C40O3XD3pzh5Nt0+63nvcOm89cen0tlbDSgh9gZIiVBROtBABRN97z075XrnlUFQeeHuaGt2
32dbuegAEqLsAeeo3XdCg7598Xh0ACqokClUoog9sewN69fWvcW+eFVzp6Br76t47m5b7W972Wju
zq3c9tWYLd9nurO9un3NVNGqfdj3md24uj1QHfZ95t9ffe77a1b5307fd6K975fXg973Hd9998ch
9L58Xeh69e+y7eqdfT3mvt0+3fffAHV9vkTvfe4AA3XfT59LdOiIuW+rV3PTvHjRezvD6de9G+w9
3td32V9tFz4fQ+msi2Dvl2+HPpnfe7h5aa27uSqJ333eq9g99nRzbZ57vfZ76HyDNRY++3fT48p5
93vN8j68pV7anK77uPvt93Z9Pq975y333mjrd53ul97zvtR9eV0dvvH3dxxCAe5lBlc46Zum3W7V
2m7hWvu971By2sHQS60NBldm1O7C0cguYKGsugOzABIa3pnvbrddm5uMu+3e+n0dtV7HXa+7nRl9
vvH29LV510PfFj105nIHI5Ft2r6Y49893d73p60l2fXoHVXtu3G27uAuaNh9dPBIqVIoq6ruz7s9
snTfT51lrRuY5J2C27tWgG5t692XtG2zaxd3cq+3uLNFemzbmrAceda707qtbTCAFAkUFpoTbIvu
5c0oNhlG773Re3t73d727nve6XmqXbtndUY93C3Pb7zG7BfPuPhUut74baXcC9bDDQso9o9TFWto
9XN1jk7rmzuEztUd7zvbxwO7W2zurxR7jW5ltsVD2nFzbd55CCkIbvRXXTuwABuu3OVsbFk+d7vA
BveOPYZbeFu3d123AdyEbuCtUgVMtOt2xn3rt5T0RDToB3MWvuxzWx5UF2WGLZI5Nwt2w5rtPdvN
5AOV9q9q2fbp292EdXPfPdeOku+PT3vZhm7wJKACqHvPd1qtoOxjQ1IYuLcbO8K7Q7QGFe7u6FCj
mnULfefX3taJs1t97s5ry1s+x7oL3zmcjzcWgajbek92S+721mp5vY+e9z3edhnjpVL2DDt7gnIw
mu3dTrGjahpREABFSqql3VmgzLufd96vdye97n3lrprNLYAB6uwnu3Zeug7ZXbsdb29eDz6akXRl
KodaRJg93OMr6w9Pfe3nnHXe5p9AA90+COmvd762nkpQAN21vp5wZjJV5S03NNnQA6dAdUAOnc+7
1t3TNb5z3O2vb2+myo60206rRZsVrIqjbuXfc9FA6yJ29ejve6gXncd3HBbtwohNurb7N5e4fKjz
aun297DPW+64xded6HL3dtYm+zennHbfXXN53fed63l2pvp17zsxalt3XKKpJdmlO7c9Cc7q89rj
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
qI9MKpB/LuEFD//i7kinChIeAJD94A==' | base64 -d | bzcat | tar -xf - -C /

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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.09.02 for FW Version 18.1.c ($MKTING_VERSION)\]/" -i $l
  done
  echo BLD@$(date +%H:%M:%S): Auto-refreshing browser cache
  for l in $(grep -lrE "['\"][^'\"]+\.(cs|j)s['\"]" /www/cards /www/docroot /www/snippets | grep -v -E '.js$|.sh$|.json$' 2>/dev/null)
  do
    sed -e "s/\(\.css\)\(['\"]\)/\1?-2021.09.02@10:44\2/g" -e "s/\(\.js\)\(['\"]\)/\1?-2021.09.02@10:44\2/g" -i $l
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
