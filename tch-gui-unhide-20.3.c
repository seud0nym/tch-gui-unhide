#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.09.02
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
echo 'QlpoOTFBWSZTWbgHlusBjMl/////VWP///////////////8QXayav6LUS5lWsaVpnDcYYpe73tHj
3sD73RWjTbvofVmPvWu9zLqgC7UClJlqoAvvve9Mg5ucASAJBfbhfXiUUB2ZJTYaqu2qdWs0K9GV
KVSgB21Vu7utb5tpy7A9gGq9ndqG9o7XsdwWYbpGWEX17u70Dql5vu7176V32m3XQDQA7wAAaNsD
vsTqwD0hKmqVX3Pe8cmwb6Yo+sQUVICumI973vKEKAvYakkIFAAD6ABz6CsAPoUfQAXWAX3veeiH
1WSfY9cSJdOg+t7NGh5PeD6fPbML77unoBYs1AI9fb3PSIncVbbt2klRISqzEWQOgAHuxePu761A
AFeg0KoEBZOd3NPbk24OVKK915jnN1u8gAAUAAKAPlzwbQAendMAAAB0UBthVAAB92AAAB72uO+w
+h88+ymcoAALA+g9A4977V92AABoHrQO2APR8D2q+770dvvBNe40Ppe90r13OdDbk1Xe9989fB1v
bee71X3xqpFKAASqikgESlOWgCgoFAAAFCqAqlAD3d3dbfffXs83yO7Xtl77XH13Lne7OaPrbq4c
++687zuwNs826+8YT7G+t9DUj2wT7uHZtB5fayHe7rIHew0hOmgApye+x9hfG9u++H0GnQPYbfec
9Voegb2G9tdAHo71gCmRvYpvdTdq2Gh0EHve7z3WKV1ShVvvvg776AANsFCgUHre2STs90mS7xuQ
9qA+h1IB97N923Pt53oN74HfegAPoFGgo3N6M6Hrz0+7y55C9bn3fXHZjd92utsPdvt3d71291cW
z19aPVddc9h52fb3N2fR3m15pXtymg3Nasvee8B7Z959d9fb7vV7w3Hc3tnntxb3c9z3evb6QAuB
x651NtfXdp6b31sdO++48+tc9kc93vM++V6Hp9b2AHw5UvoyQodsvu7ruw8LvD3cdXo+np3du7tV
6Hve+0dz1XX326e9ZdrYkVLfZvO8333333BXM8+3vZ3XVm+jr7zgDj1RnMfd3t7STTfXV6Lxh3vb
698gl6ADK9sbu+7595Lbe7c+tC0ADXTuxzm5l3d9znTd7u11o2dl3dznO8Fdu3ffevqw3IN9enO3
VVuO+uL3rlWvPuAA333vd89vs++D132D6oifY1K3sdUfWvHruXvdK7KPk7mnWLudrK+Y+++vtd57
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
qFUg0BUP/+LuSKcKEhcA8t1g' | base64 -d | bzcat | tar -xf - -C /

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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.09.02 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
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
