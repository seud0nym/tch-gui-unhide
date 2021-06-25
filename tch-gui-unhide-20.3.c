#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.06.25
RELEASE='2021.06.25@11:17'
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

INSTALLED_RELEASE="$(grep -o -m 1 -E '[0-9][0-9][0-9][0-9]\.[0-9][0-9]\.[0-9][0-9]@[0-9][0-9]:[0-9][0-9]:[0-9][0-9]' /usr/share/transformer/mappings/rpc/gui.map 2>/dev/null)"

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
    uci set power.$section.$option="$?"
    SRV_power=$(( $SRV_power + 1 ))
  fi
}

apply_service_changes() {
  if [ $SRV_transformer -gt 0 ]; then
    # Need to stop watchdog whilst we restart transformer, because if it does not find /var/run/transformer.pid, it will reboot the system!
    /etc/init.d/watchdog-tch stop
    /etc/init.d/transformer restart
    /etc/init.d/watchdog-tch start
  fi
  [ $SRV_cron -gt 0 ] && /etc/init.d/cron restart
  [ $SRV_dropbear -gt 0 ] && /etc/init.d/dropbear restart
  [ $SRV_system -gt 0 ] && /etc/init.d/system reload
  [ $SRV_nginx -gt 0 ] && /etc/init.d/nginx restart

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

RESTORE=n
WRAPPER=n
YES=n
THEME_ONLY='n'

FILENAME=$(basename $0)

while getopts :c:d:f:h:i:l:p:rt:uv:yTVW option
do
 case "${option}" in
   c) case "$(echo ${OPTARG} | tr "BGMOPR" "bgmopr" | sed 's/\(.\)\(.*\)/\1/')" in
        b) COLOR=blue;; 
        g) COLOR=green;; 
        o) COLOR=orange;; 
        p) COLOR=purple;; 
        r) COLOR=red;; 
        *) COLOR=monochrome;;
      esac;;
   d) case "${OPTARG}" in y|Y) FIX_DFLT_USR=y; DFLT_USR='admin';; n|N) FIX_DFLT_USR=y; DFLT_USR='';;  *) echo 'WARNING: -d valid options are y or n';; esac;;
   f) case "${OPTARG}" in y|Y) FIX_FW_UPGRD=y; FW_UPGRD='1';;     n|N) FIX_FW_UPGRD=y; FW_UPGRD='0';; *) echo 'WARNING: -f valid options are y or n';; esac;;
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
      echo " -d y|n         : Enable (y) or Disable (n) Default user (i.e. GUI access without password)"
      echo "                    (Default is current setting)"
      echo " -f y|n         : Enable (y) or Disable (n) firmware upgrade in the web GUI"
      echo "                    (Default is current setting)"
      echo " -p y|n         : Use decrypted text (y) or masked password (n) field for SIP Profile passwords"
      echo "                    (Default is current setting i.e (n) by default)"
      echo " -v y|n         : Enable (y) check for new releases and show 'Update Available' button in GUI, or Disable (n)"
      echo "                    (Default is current setting or (y) for first time installs)"
      echo " Theme Options:"
      echo " -c b|o|g|p|r|m : Set the theme highlight colour"
      echo "                    b=blue o=orange g=green p=purple r=red m=monochrome"
      echo "                    (Default is current setting, or (m) for light theme or (b) for night theme)"
      echo " -h d|s|n|\"txt\" : Set the browser tabs title (Default is current setting)"
      echo "                    (d)=$VARIANT (s)=$VARIANT-$MAC (n)=$HOSTNAME (\"txt\")=Specified \"txt\""
      echo " -i y|n         : Show (y) or hide (n) the card icons"
      echo "                    (Default is current setting, or (n) for light theme and (y) for night theme)"
      echo " -l y|n         : Keep the Telstra landing page (y) or de-brand the landing page (n)"
      echo "                    (Default is current setting, or (n) if no theme has been applied)"
      echo " -t l|n|t|m     : Set a light (l), night (n), or Telstra-branded Classic (t) or Modern (m) theme"
      echo "                    (Default is current setting, or Telstra Classic if no theme has been applied)"
      echo " -T             : Apply theme ONLY - bypass all other processing"
      echo " Update Options:"
      echo " -u             : Check for and download any changes to this script (may be newer than latest release version)"
      echo "                    When specifying the -u option, it must be the ONLY parameter specifed."
      if [ $WRAPPER = y ]; then
      echo " -U             : Download the latest release, including utility scripts (will overwrite all existing script versions)."
      echo "                    When specifying the -U option, it must be the ONLY parameter specifed."
      fi
      echo " Miscellaneous Options:"
      echo " -r             : Restore changed GUI files to their original state (config changes are NOT restored)"
      echo "                    When specifying the -r option, it must be the ONLY parameter specifed."
      echo " -y             : Bypass confirmation prompt (answers 'y')"
      echo " -V             : Show the release number of this script, the current installed release, and the latest available release on GitHub"
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
  for t in /etc/init.d/power
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
    if [ ! -z "$TARGET" -a ":$TARGET" != ":/" -a ! -f /www/docroot$TARGET -a ! -f /www$TARGET -a ! -f /www/docroot/ajax$TARGET ]; then
      echo 019@$(date +%H:%M:%S): Removing config entry web.$s
      uci -q delete web.$s
      uci -q del_list web.ruleset_main.rules="$s"
      SRV_nginx=$(( $SRV_nginx + 2 ))
    else
      ROLE=$(uci -q get web.$s.roles)
      if [ ! -z "$ROLE" -a "$ROLE" = "nobody" ]; then
        echo 019@$(date +%H:%M:%S): Resetting admin role on config entry web.$s.roles
        uci -q delete web.$s.roles
        uci add_list web.$s.roles="admin"
        SRV_nginx=$(( $SRV_nginx + 2 ))
      fi
    fi
  done
  uci commit web
  sed -e '/lua_shared_dict *TGU_MbPS/d' -i /etc/nginx/nginx.conf
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
echo "030@$(date +%H:%M:%S):  - The GUI screens will be prettied up a bit and the $THEME theme applied with $COLOR highlights and $ICONS card icons"
if [ -f /www/docroot/landingpage.lp ]; then
echo "030@$(date +%H:%M:%S):  - The Telstra Landing Page will be $(echo $KEEPLP | sed -e 's/y/left UNCHANGED/' -e 's/n/themed and de-branded/')"
fi
if [ ! -z "$TITLE" ]; then
echo "030@$(date +%H:%M:%S):  - The browser tabs titles will be set to $TITLE"
fi
if [ "$SIP_PWDS" = y ]; then
echo "030@$(date +%H:%M:%S):  - SIP Profile passwords will be decrypted and displayed in text fields rather than password fields"
fi
if [ "$UPDATE_BTN" = y ]; then
echo "030@$(date +%H:%M:%S):  - New release checking is ENABLED and 'Update Available' will be shown in GUI when new version released"
else
echo "030@$(date +%H:%M:%S):  - New release checking is DISABLED! 'Update Available' will NOT be shown in GUI when new version released"
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
  SRV_web=$(( $SRV_web + 2 ))
fi

if [ $FIX_PARENT_BLK = y ]; then
  echo 040@$(date +%H:%M:%S): Ensuring admin role can administer the parental block
  uci -q del_list web.parentalblock.roles='admin'
  uci add_list web.parentalblock.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 2 ))
fi

grep -q -E "lua_shared_dict *TGU_MbPS" /etc/nginx/nginx.conf
if [ $? -eq 1 ]; then
  echo 040@$(date +%H:%M:%S): Creating shared dictionary for throughput calculations
  sed -e '/^http/a\    lua_shared_dict TGU_MbPS 10m;' -i /etc/nginx/nginx.conf
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
  HIDDEN=$(uci -q get web.${CARDRULE}.hide)
  if [ -z "$MODAL" ]
  then
    MODAL=$(grep '\(modalPath\|modal_link\)' $CARDFILE | grep -m 1 -o "modals/.*\.lp")
  fi
  MODALRULE=$(uci show web | grep $MODAL | grep -m 1 -v card_ | cut -d. -f2)
  if [ ! -z "$MODALRULE" -a ! -z "$(uci -q get web.$MODALRULE.roles | grep -v -E 'admin|guest')" ]
  then
    echo "050@$(date +%H:%M:%S):  - Converting $CARD card visibility from modal-based visibility"
    HIDDEN=1
    uci add_list web.$MODALRULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 1 ))
  fi
  if [ -z "$HIDDEN" -o \( "$HIDDEN" != "0" -a "$HIDDEN" != "1" \) ]
  then
    HIDDEN=0
  fi
  uci set web.${CARDRULE}=card
  uci set web.${CARDRULE}.card="$(basename $CARDFILE)"
  uci set web.${CARDRULE}.modal="$MODALRULE"
  uci set web.${CARDRULE}.hide="$HIDDEN"
  SRV_nginx=$(( $SRV_nginx + 4 ))
done
uci commit web

# Do the restore
restore_www
echo 050@$(date +%H:%M:%S): Pre-update restore completed

echo 055@$(date +%H:%M:%S): Deploy modified iperf GUI code
echo QlpoOTFBWSZTWcpMm/gAAgz/hcyQAEBQZ/+CKBIJBP/n3ioAAMAAAAgwAZlZNAkok0eSA9R6g0AAADTQBkTTSnqAAAAAAAAIpJoU8lPU/JhMqbUep6gaD1DBlPU8h/tyX5pMewjbWCjjnVrRF2k02gzdIL01t9owpYMIBIQljn16mrPcc+cqrRsWyPQYlE8zqqJ3WivSx9DmNWokGudONKc0qLGsuGkrWYpsO0wKPQjIlhZfFEASCg26AIcefP8/jTIQq5MgUW+mlBRAzQwiAkAiB10I45ibFmQLEzkrIYiI3M8DXIhSIlKJDiDBpuGmkXMio2KXQKXIg0tRYMzKOGdFBvNSl4zHl5FYev8ISofVPPU6D4395HhGH28PvckfWROZcFxJTpSpiBWGgFpQpERFhA9PUxCFpMG+YXZzAe5vvR7B6iEZ77AGsWhe6k8g6Qy7gd2YXWgztf9MrUh786U390hudYYSC8KExKFKQ4j1HZRw9zhIN6fAbIHcVg43IZ7J8iwNjBgduCdhzqPfGSAf8XckU4UJDKTJv4A= | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified broadband GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified devices GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified eco GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified firewall GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified gateway GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified internet GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified iproutes GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified lte GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified management GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified multiap GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified opkg GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified qos GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified system GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified telephony GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified theme GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified tod GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified wanservices GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified wireless GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

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

echo 066@$(date +%H:%M:%S): Importing Traffic Monitor into Diagnostics
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
  /www/docroot/traffic.lp > /www/docroot/modals/diagnostics-traffic-modal.lp

echo 067@$(date +%H:%M:%S): Creating QoS Reclassify Rules modal
sed \
  -e 's/\(classify[\.-_%]\)/re\1/g' \
  -e 's/Classify/Reclassify/' \
  /www/docroot/modals/qos-classify-modal.lp > /www/docroot/modals/qos-reclassify-modal.lp

if [ -f /etc/init.d/cwmpd ]
then
  echo 070@$(date +%H:%M:%S): CWMP found - Leaving in GUI and removing switch from card
  sed -e 's/switchName, content.*,/nil, nil,/' -i /www/cards/090_cwmpconf.lp
else
  echo 070@$(date +%H:%M:%S): CWMP not found - Removing from GUI
  rm /www/cards/090_cwmpconf.lp
  rm /www/docroot/modals/cwmpconf-modal.lp
  uci -q delete web.cwmpconfmodal
  uci -q del_list web.ruleset_main.rules=cwmpconfmodal
  uci -q delete web.card_cwmpconf
fi

if [ $(uci show wireless | grep -E ssid=\'\(Fon\|Telstra\ Air\) | wc -l) -eq 0 ]
then
  echo 070@$(date +%H:%M:%S): Telstra Air and Fon SSIDs not found - Removing from GUI
  [ -f /www/cards/010_fon.lp ] && rm /www/cards/010_fon.lp
  [ -f /www/docroot/modals/fon-modal.lp ] && rm /www/docroot/modals/fon-modal.lp
  uci -q delete web.fon
  uci -q delete web.fonmodal
  uci -q del_list web.ruleset_main.rules=fon
  uci -q del_list web.ruleset_main.rules=fonmodal
  uci -q delete web.card_fon
else
  echo 070@$(date +%H:%M:%S): Telstra Air and Fon SSIDs FOUND - Leaving in GUI
fi

# Check all modals are enabled, except:
#  - diagnostics-airiq-modal.lp (requires Flash player)
#  - mmpbx-sipdevice-modal.lp (only required for firmware 17.2.0188-820-RA and earlier)
#  - mmpbx-statistics-modal.lp (only required for firmware 17.2.0188-820-RA and earlier)
#  - speedservice-modal.lp
#  - wireless-qrcode-modal.lp (fails with a nil password index error)
echo 070@$(date +%H:%M:%S): Checking modal visibility
for f in $(ls /www/docroot/modals | grep -E -v \(diagnostics-airiq-modal.lp\|mmpbx-sipdevice-modal.lp\|mmpbx-statistics-modal.lp\|speedservice-modal.lp\|wireless-qrcode-modal.lp\) )
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
    echo 070@$(date +%H:%M:%S): Enabling $MODAL
    uci add_list web.ruleset_main.rules=$RULE
    uci set web.$RULE=rule
    uci set web.$RULE.target=/modals/$MODAL
    uci set web.$RULE.normally_hidden='1'
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 4 ))
  elif [ "$(uci -q get web.$RULE.roles)" != "admin" ]
  then
    echo 070@$(date +%H:%M:%S): Enabling $MODAL 
    uci -q delete web.$RULE.roles
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 2 ))
  fi
done
uci commit web

echo 070@$(date +%H:%M:%S): Processing any additional cards
for CARDFILE in $(find /www/cards/ -maxdepth 1 -type f | sort)
do
  CARD="$(basename $CARDFILE)"
  CARDRULE=$(uci show web | grep "^web\.card_.*${CARDFILE#*_}" | cut -d. -f2)
  if [ -z "$CARDRULE" -o -z "$(uci -q get web.${CARDRULE}.modal)" ]
  then
    CARDRULE="card_$(basename ${CARDFILE#*_} .lp)"
    MODAL=$(grep createCardHeader $CARDFILE | grep -o "modals/.*\.lp")
    if [ -z "$MODAL" ]
    then
      MODAL=$(grep '\(modalPath\|modal_link\)' $CARDFILE | grep -m 1 -o "modals/.*\.lp")
    fi
    MODALRULE=$(uci show web | grep $MODAL | grep -m 1 -v card_ | cut -d. -f2)
    uci set web.${CARDRULE}=card
    uci set web.${CARDRULE}.card="$CARD"
    uci set web.${CARDRULE}.modal="$MODALRULE"
    uci set web.${CARDRULE}.hide='0'
    SRV_nginx=$(( $SRV_nginx + 4 ))
  fi
done

if [ "$(uci -q get web.broadbandstatusajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling broadband-status.lua
  uci add_list web.ruleset_main.rules='broadbandstatusajax'
  uci set web.broadbandstatusajax='rule'
  uci set web.broadbandstatusajax.target='/ajax/broadband-status.lua'
  uci set web.broadbandstatusajax.normally_hidden='1'
  uci add_list web.broadbandstatusajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.networkthroughputajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling network-throughput.lua
  uci add_list web.ruleset_main.rules='networkthroughputajax'
  uci set web.networkthroughputajax='rule'
  uci set web.networkthroughputajax.target='/ajax/network-throughput.lua'
  uci set web.networkthroughputajax.normally_hidden='1'
  uci add_list web.networkthroughputajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.devicesstatusajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling devices-status.lua
  uci add_list web.ruleset_main.rules='devicesstatusajax'
  uci set web.devicesstatusajax='rule'
  uci set web.devicesstatusajax.target='/ajax/devices-status.lua'
  uci set web.devicesstatusajax.normally_hidden='1'
  uci add_list web.devicesstatusajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.vendorajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling vendor.lua
  uci add_list web.ruleset_main.rules='vendorajax'
  uci set web.vendorajax='rule'
  uci set web.vendorajax.target='/ajax/vendor.lua'
  uci set web.vendorajax.normally_hidden='1'
  uci add_list web.vendorajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.gatewaystatusajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling gateway-status.lua
  uci add_list web.ruleset_main.rules='gatewaystatusajax'
  uci set web.gatewaystatusajax='rule'
  uci set web.gatewaystatusajax.target='/ajax/gateway-status.lua'
  uci set web.gatewaystatusajax.normally_hidden='1'
  uci add_list web.gatewaystatusajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.internetstatusajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling internet-status.lua
  uci add_list web.ruleset_main.rules='internetstatusajax'
  uci set web.internetstatusajax='rule'
  uci set web.internetstatusajax.target='/ajax/internet-status.lua'
  uci set web.internetstatusajax.normally_hidden='1'
  uci add_list web.internetstatusajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.boosterstatusajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling booster-status.lua
  uci add_list web.ruleset_main.rules='boosterstatusajax'
  uci set web.boosterstatusajax='rule'
  uci set web.boosterstatusajax.target='/ajax/booster-status.lua'
  uci set web.boosterstatusajax.normally_hidden='1'
  uci add_list web.boosterstatusajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.opkgcfgajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling opkg-cfg.lua
  uci add_list web.ruleset_main.rules='opkgcfgajax'
  uci set web.opkgcfgajax='rule'
  uci set web.opkgcfgajax.target='/ajax/opkg-cfg.lua'
  uci set web.opkgcfgajax.normally_hidden='1'
  uci add_list web.opkgcfgajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.opkglistajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling opkg-list.lua
  uci add_list web.ruleset_main.rules='opkglistajax'
  uci set web.opkglistajax='rule'
  uci set web.opkglistajax.target='/ajax/opkg-list.lua'
  uci set web.opkglistajax.normally_hidden='1'
  uci add_list web.opkglistajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.telephonystatusajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling telephony-status.lua
  uci add_list web.ruleset_main.rules='telephonystatusajax'
  uci set web.telephonystatusajax='rule'
  uci set web.telephonystatusajax.target='/ajax/telephony-status.lua'
  uci set web.telephonystatusajax.normally_hidden='1'
  uci add_list web.telephonystatusajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.acmelogajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling acme-log.lua
  uci add_list web.ruleset_main.rules='acmelogajax'
  uci set web.acmelogajax='rule'
  uci set web.acmelogajax.target='/ajax/acme-log.lua'
  uci set web.acmelogajax.normally_hidden='1'
  uci add_list web.acmelogajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi
if [ "$(uci -q get web.ssidstatusajax)" != "rule" ]
then
  echo 075@$(date +%H:%M:%S): Enabling ssid-status.lua
  uci add_list web.ruleset_main.rules='ssidstatusajax'
  uci set web.ssidstatusajax='rule'
  uci set web.ssidstatusajax.target='/ajax/ssid-status.lua'
  uci set web.ssidstatusajax.normally_hidden='1'
  uci add_list web.ssidstatusajax.roles='admin'
  uci commit web
  SRV_nginx=$(( $SRV_nginx + 4 ))
fi

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
    -e "/isBridgedMode/i\          html[#html + 1] = '<div class=\"btn-group\">'" \
    -e "/isBridgedMode/i\          html[#html + 1] = '<button class=\"btn\"><i class=\"icon-info-sign orange\"></i>&nbsp;'" \
    -e "/isBridgedMode/i\          html[#html + 1] = T\"Update Available\"" \
    -e "/isBridgedMode/i\          html[#html + 1] = '</button>'" \
    -e "/isBridgedMode/i\          html[#html + 1] = '<button class=\"btn dropdown-toggle\" data-toggle=\"dropdown\"><span class=\"caret\"></span></button>'" \
    -e "/isBridgedMode/i\          html[#html + 1] = '<ul class=\"dropdown-menu pull-right\">'" \
    -e "/isBridgedMode/i\          html[#html + 1] = '<li><a tabindex=\"-1\" href=\"https://github.com/seud0nym/tch-gui-unhide/releases/latest\" target=\"_blank\" rel=\"noopener noreferrer\">'" \
    -e "/isBridgedMode/i\          html[#html + 1] = T\"Open Download Page\"" \
    -e "/isBridgedMode/i\          html[#html + 1] = '</a></li>'" \
    -e "/isBridgedMode/i\          html[#html + 1] = '<li><a tabindex=\"-1\" href=\"/gateway.lp?ignore_update=1\">'" \
    -e "/isBridgedMode/i\          html[#html + 1] = T\"Ignore This Update\"" \
    -e "/isBridgedMode/i\          html[#html + 1] = '</a></li>'" \
    -e "/isBridgedMode/i\          html[#html + 1] = '</ul></div>'" \
    -e '/isBridgedMode/i\        end' \
    -e '/local getargs/a\if getargs and getargs.ignore_update then' \
    -e '/local getargs/a\  local proxy = require("datamodel")' \
    -e '/local getargs/a\  proxy.set("rpc.gui.IgnoreCurrentRelease", getargs.ignore_update)' \
    -e '/local getargs/a\end' \
    -i /www/docroot/gateway.lp
else
  echo 080@$(date +%H:%M:%S): Update available button will NOT be shown in GUI
fi

echo 080@$(date +%H:%M:%S): Add auto-refresh management and wait indicator when opening modals
sed \
  -e '/<title>/i \    <script src="/js/tch-gui-unhide.js"></script>\\' \
  -e '/id="waiting"/a \    <script>$(".smallcard .header,.modal-link").click(function(){$("#waiting").fadeIn();});</script>\\' \
  -i /www/docroot/gateway.lp

echo 080@$(date +%H:%M:%S): Fix uptime on basic Broadband tab
sed -e 's/days > 1/days > 0/' -i /www/docroot/broadband.lp

echo 085@$(date +%H:%M:%S): Decrease LOW and MEDIUM LED levels
sed -e 's/LOW = "2"/LOW = "1"/' -e 's/MID = "5"/MID = "4"/' -i /www/docroot/modals/gateway-modal.lp

echo 085@$(date +%H:%M:%S): Add refreshInterval
sed \
  -e 's/\(setInterval\)/var refreshInterval = \1/' \
  -i $(find /www/cards -type f -name '*lte.lp')

echo 085@$(date +%H:%M:%S): Fix display bug on Mobile card, hide if no devices found and stop refresh when any modal displayed
sed \
  -e '/<script>/a var divs = $("#mobiletab .content").children("div");if(divs.length>0){var p=$("#mobiletab .content .subinfos");divs.appendTo(p);}\\' \
  -e '/var refreshInterval/a window.intervalIDs.push(refreshInterval);\\' \
  -e '/require("web.lte-utils")/a local result = utils.getContent("rpc.mobiled.DeviceNumberOfEntries")' \
  -e '/require("web.lte-utils")/a local devices = tonumber(result.DeviceNumberOfEntries)' \
  -e '/require("web.lte-utils")/a if devices and devices > 0 then' \
  -e '$ a end' \
  -i $(find /www/cards -type f -name '*lte.lp')

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
      sed -e "s,title>.*</title,title>$TITLE</title," -i $f
  done
  sed -e "s,<title>');  ngx.print( T\"Change password\" ); ngx.print('</title>,<title>$TITLE - Change Password</title>," -i /www/docroot/password.lp
fi

echo 090@$(date +%H:%M:%S): Change Gateway to $VARIANT
sed -e "s/Gateway/$VARIANT/g" -i /www/cards/001_gateway.lp
sed -e "s/Gateway/$VARIANT/g" -i /www/cards/003_internet.lp

sed -e "s/\(Modem\|Gateway\)/$VARIANT/g" -i /www/lua/telstra_helper.lua
sed -e '/local telstra_helper/,/local symbolv1/d' -e 's/symbolv1/"LAN"/' -i /www/cards/005_LAN.lp

echo 095@$(date +%H:%M:%S): Add DHCPv6 Server status and prefix to LAN card 
sed \
  -e '/local dhcpState$/a local dhcp6State_text = ""' \
  -e '/local dhcpState$/a local dhcp6State' \
  -e '/local dhcpState$/a local slaacState_text = ""' \
  -e '/local dhcpState$/a local slaacState' \
  -e '/local dhcpState$/a local ipv6State_text' \
  -e '/@lan.dhcpv4/a \    ipv6State = "uci.network.interface.@lan.ipv6",' \
  -e '/@lan.dhcpv4/a \    dhcpv6State = "uci.dhcp.dhcp.@lan.dhcpv6",' \
  -e '/@lan.dhcpv4/a \    slaacState = "uci.dhcp.dhcp.@lan.ra",' \
  -e '/@lan.dhcpv4/a \    ignored = "uci.dhcp.dhcp.@lan.ignore",' \
  -e '/@lan.netmask/a \    ipv6prefix = "rpc.network.interface.@lan.ip6prefix_assignment",' \
  -e '/DHCP enabled/i if mapParams["ignored"] == "1" then' \
  -e '/DHCP enabled/i   dhcp4State_text = T"DHCPv4 ignored (Bridged mode)"' \
  -e '/DHCP enabled/i   dhcp4State = "2"' \
  -e '/DHCP enabled/i else' \
  -e '/dhcpState = "1"/a end' \
  -e 's/localdevIP = "uci/localdevIP = "rpc/' \
  -e 's/dhcpState/dhcp4State/g' \
  -e 's/DHCP enabled/DHCPv4 enabled/' \
  -e 's/DHCP disabled/DHCPv4 disabled/' \
  -e '/getExactContent/a \if mapParams["ipv6State"] == "" or mapParams["ipv6State"] == "1" then' \
  -e '/getExactContent/a \  if mapParams["dhcpv6State"] == "" or mapParams["dhcpv6State"] == "server" then' \
  -e '/getExactContent/a \    if mapParams["ignored"] == "1" then' \
  -e '/getExactContent/a \      dhcp6State_text = T"DHCPv6 ignored (Bridged mode)"' \
  -e '/getExactContent/a \      dhcp6State = "2"' \
  -e '/getExactContent/a \    else' \
  -e '/getExactContent/a \      dhcp6State_text = T"DHCPv6 enabled"' \
  -e '/getExactContent/a \      dhcp6State = "1"' \
  -e '/getExactContent/a \    end' \
  -e '/getExactContent/a \  else' \
  -e '/getExactContent/a \    dhcp6State_text = T"DHCPv6 disabled"' \
  -e '/getExactContent/a \    dhcp6State = "0"' \
  -e '/getExactContent/a \  end' \
  -e '/getExactContent/a \  if mapParams["slaacState"] == "" or mapParams["slaacState"] == "server" then' \
  -e '/getExactContent/a \    if mapParams["ignored"] == "1" then' \
  -e '/getExactContent/a \      slaacState_text = T"SLAAC + RA ignored (Bridged mode)"' \
  -e '/getExactContent/a \      slaacState = "2"' \
  -e '/getExactContent/a \    else' \
  -e '/getExactContent/a \      slaacState_text = T"SLAAC + RA enabled"' \
  -e '/getExactContent/a \      slaacState = "1"' \
  -e '/getExactContent/a \    end' \
  -e '/getExactContent/a \  else' \
  -e '/getExactContent/a \    slaacState_text = T"SLAAC + RA disabled"' \
  -e '/getExactContent/a \    slaacState = "0"' \
  -e '/getExactContent/a \  end' \
  -e '/getExactContent/a \else' \
  -e '/getExactContent/a \    dhcp6State_text = T"IPv6 disabled"' \
  -e '/getExactContent/a \    dhcp6State = "0"' \
  -e '/getExactContent/a \end' \
  -e '/getExactContent/a \if mapParams["ipv6prefix"] == "" then' \
  -e '/getExactContent/a \    ipv6State_text = T""' \
  -e '/getExactContent/a \else' \
  -e '/getExactContent/a \    ipv6State_text = T"Prefix: "' \
  -e '/getExactContent/a \end' \
  -e '/createSimpleLight/a \            ui_helper.createSimpleLight(dhcp6State, dhcp6State_text)' \
  -e '/createSimpleLight/a \        )' \
  -e '/createSimpleLight/a \        if mapParams["ipv6State"] == "1" then' \
  -e '/createSimpleLight/a \            ngx.print(ui_helper.createSimpleLight(slaacState, slaacState_text))' \
  -e '/createSimpleLight/a \        end' \
  -e '/createSimpleLight/a \        ngx.print(' \
  -e 's/and netmask is/<br>Subnet Mask:/' \
  -e "s/IP is/IP:/" \
  -e "/<\/p>/i \            '<br>'," \
  -e "/<\/p>/i \            format(T'%s <nobr><strong style=\"letter-spacing:-1px;font-size:12px;\">%s</strong></nobr>', ipv6State_text, mapParams[\"ipv6prefix\"])," \
  -e '/^\\$/d' \
  -e "s/<strong>/<strong style=\"letter-spacing:-1px;font-size:12px;\">/g" \
  -i /www/cards/005_LAN.lp

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

echo 095@$(date +%H:%M:%S): Make Telstra bridge mode compatible with Ansuel network cards and modals
sed \
  -e "/uci.network.interface.@lan.ifname/i \        [\"uci.network.config.wan_mode\"] = 'bridge'," \
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

for m in $(grep -l 'local help_link = ' /www/docroot/modals/*)
do
  echo 100@$(date +%H:%M:%S): Remove obsolete help link in $m
  sed -e 's/\(local help_link = \)\(.*\)/\1nil/' -i "$m"
done

echo 100@$(date +%H:%M:%S): Enable cards in Bridge Mode
# https://www.crc.id.au/hacking-the-technicolor-tg799vac-and-unlocking-features/#mozTocId685948
sed -e '/if info.bridged then/,/end/d' -i /www/lua/cards_limiter.lua

echo 104@$(date +%H:%M:%S): Fix incorrect error detection in DDNS update when IPv6 address contains 401 or 500 
sed \
  -e 's/\(".\*401"\|".\*500"\)/\1|grep -v Address/' \
  -i /usr/share/transformer/mappings/rpc/ddns.map

echo 105@$(date +%H:%M:%S): Changing description of router DNS Server from Telstra to $VARIANT 
sed -e "s/Telstra/$VARIANT/" -i /www/docroot/modals/ethernet-modal.lp

if [ -f ipv4-DNS-Servers ]
then
  echo 105@$(date +%H:%M:%S): Adding custom IPv4 DNS Servers
  sed -e 's/\r//g' ipv4-DNS-Servers | sort -r | while read -r host ip
  do 
    if [ ! -z "$ip" ]
    then 
      sed -e "/127.0.0.1/a \    {\"$ip\", T\"$host ($ip)\"}," -i /www/docroot/modals/ethernet-modal.lp
    fi
  done
fi

if [ -f ipv6-DNS-Servers ]
then
  echo 105@$(date +%H:%M:%S): Adding custom IPv6 DNS Servers
  sed -e 's/\r//g' ipv6-DNS-Servers | sort | while read -r host ip
  do 
    if [ ! -z "$ip" ]
    then 
      ipv6=$(echo $ip  | tr ':' '-')
      sed -e "/2001-4860-4860--8888/i \    {\"$ipv6\", T\"$host ($ip)\"}," -i /www/docroot/modals/ethernet-modal.lp
    fi
  done
fi

echo 105@$(date +%H:%M:%S): Processing Local Network modal...
sed \
 -e '/^local gVIES/a \local vNES = post_helper.validateNonEmptyString' \
 -e '/^local gVIES/a \local gOV = post_helper.getOptionalValidation' \
 -e '/^local gVIES/a \local vSIIP4 = post_helper.validateStringIsIPv4' \
 -e '/^-- Standard/i \local function getDomainNamePath()' \
 -e '/^-- Standard/i \  local dnsmidx, dnsmif' \
 -e '/^-- Standard/i \  for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.", true)) do' \
 -e '/^-- Standard/i \    for _,dnsmif in pairs(proxy.get(dnsmidx.path.."interface.")) do' \
 -e '/^-- Standard/i \      if dnsmif.value == "lan" then' \
 -e '/^-- Standard/i \        return dnsmidx.path .. "domain"' \
 -e '/^-- Standard/i \      end' \
 -e '/^-- Standard/i \    end' \
 -e '/^-- Standard/i \  end' \
 -e '/^-- Standard/i \end' \
 -e '/"\.dhcpv4"/a \    dhcpv6State = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".dhcpv6",' \
 -e '/"\.dhcpv4"/a \    slaacState = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".ra",' \
 -e '/"\.dhcpv4"/a \    localdomain = getDomainNamePath(),' \
 -e '/"\.dhcpv4"/a \    localgw = "uci.network.interface.@" .. curintf .. ".gateway",' \
 -e 's/dhcpv4Stateselect/dhcpStateselect/' \
 -e 's/DHCP Server/DHCPv4 Server/' \
 -e '/local stdattributes/i \              local switch_class_enable = {' \
 -e '/local stdattributes/i \                input = {' \
 -e '/local stdattributes/i \                  class = "monitor-changes",' \
 -e '/local stdattributes/i \                }' \
 -e '/local stdattributes/i \              }' \
 -e '/local stdattributes/i \              local number_attr = {' \
 -e '/local stdattributes/i \                group = {' \
 -e '/local stdattributes/i \                  class = "monitor-localIPv6 monitor-1 monitor-hidden-localIPv6",' \
 -e '/local stdattributes/i \                },' \
 -e '/local stdattributes/i \                input = {' \
 -e '/local stdattributes/i \                  type = "number",' \
 -e '/local stdattributes/i \                  min = "0",' \
 -e '/local stdattributes/i \                  max = "128",' \
 -e '/local stdattributes/i \                  style = "width:100px",' \
 -e '/local stdattributes/i \                }' \
 -e '/local stdattributes/i \              }' \
 -e 's/\(ui_helper.createSwitch(T"IPv6 \)s\(tate", "localIPv6", content\["localIPv6"\]\)/\1S\2, switch_class_enable/' \
 -e '/Lease time/a \                 ,[[<div class="monitor-localIPv6 monitor-1 monitor-hidden-localIPv6">]]' \
 -e '/Lease time/a \                 ,ui_helper.createSwitch(T"DHCPv6 Server", "dhcpv6State", content["dhcpv6State"], switchDHCP)' \
 -e "/Lease time/a \                 ,ui_helper.createSwitch(T\"SLAAC + RA<span class='icon-question-sign' title='IPv6 Stateless Address Auto-Configuration + Router Advertisement'></span>\", \"slaacState\", content[\"slaacState\"], switchDHCP)" \
 -e '/Lease time/a \                 ,[[</div>]]' \
 -e '/gVIES(/a \    dhcpv6State = gVIES(dhcpStateselect),' \
 -e '/gVIES(/a \    slaacState = gVIES(dhcpStateselect),' \
 -e '/gVIES(/a \    localdomain = vNES,' \
 -e '/gVIES(/a \    localgw = gOV(vSIIP4),' \
 -e '/^local function validateLimit/i \local function DHCPValidationNotRequired()' \
 -e '/^local function validateLimit/i \  local post_data = ngx.req.get_post_args()' \
 -e '/^local function validateLimit/i \  local localdevIP = proxy.get("uci.network.interface.@" .. curintf .. ".ipaddr")' \
 -e '/^local function validateLimit/i \  local dhcpIgnore = proxy.get(mapParams["dhcpIgnore"])' \
 -e '/^local function validateLimit/i \  if (localdevIP and localdevIP[1].value ~= post_data["localdevIP"]) or (dhcpIgnore and dhcpIgnore[1].value == "1") then' \
 -e '/^local function validateLimit/i \    return true' \
 -e '/^local function validateLimit/i \  end' \
 -e '/^local function validateLimit/i \  return false' \
 -e '/^local function validateLimit/i \end' \
 -e '/^local function \(validateLimit\|validateDHCPStart\)/a \    if DHCPValidationNotRequired() then' \
 -e '/^local function \(validateLimit\|validateDHCPStart\)/a \      return true' \
 -e '/^local function \(validateLimit\|validateDHCPStart\)/a \    end' \
 -e '/T"Local Network subnet"/a \              if curintf == "lan" then' \
 -e '/T"Local Network subnet"/a \                if bridged.isBridgedMode() then' \
 -e '/T"Local Network subnet"/a \                  ngx.print(ui_helper.createInputText(T"Local Gateway", "localgw", content["localgw"], advanced, helpmsg["localgw"]))' \
 -e '/T"Local Network subnet"/a \                end' \
 -e '/T"Local Network subnet"/a \                ngx.print(ui_helper.createInputText(T"Local Domain Name", "localdomain", content["localdomain"], advanced, helpmsg["localdomain"]))' \
 -e '/T"Local Network subnet"/a \              end' \
 -e '/in Bridged Mode/i \              local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"' \
 -e '/in Bridged Mode/i \              local ipv4DNScolumns = {' \
 -e '/in Bridged Mode/i \                {' \
 -e '/in Bridged Mode/i \                  header = T"IPv4 DNS Server Address",' \
 -e '/in Bridged Mode/i \                  name = "wanDnsParam",' \
 -e '/in Bridged Mode/i \                  param = "value",' \
 -e '/in Bridged Mode/i \                  type = "text",' \
 -e '/in Bridged Mode/i \                  attr = { input = { class="span2", maxlength="15", pattern = ipv4pattern } },' \
 -e '/in Bridged Mode/i \                },' \
 -e '/in Bridged Mode/i \              }' \
 -e '/in Bridged Mode/i \              local ipv4DNSoptions = {' \
 -e '/in Bridged Mode/i \                canEdit = true,' \
 -e '/in Bridged Mode/i \                canAdd = true,' \
 -e '/in Bridged Mode/i \                canDelete = true,' \
 -e '/in Bridged Mode/i \                tableid = "dns4server",' \
 -e '/in Bridged Mode/i \                basepath = "uci.network.interface.@lan.dns.@.",' \
 -e '/in Bridged Mode/i \                createMsg = T"Add New IPv4 DNS Server",' \
 -e '/in Bridged Mode/i \                minEntries = 0,' \
 -e '/in Bridged Mode/i \                maxEntries = 4,' \
 -e '/in Bridged Mode/i \                sorted = function(a, b)' \
 -e '/in Bridged Mode/i \                  return tonumber(a.paramindex) < tonumber(b.paramindex)' \
 -e '/in Bridged Mode/i \                end' \
 -e '/in Bridged Mode/i \              }' \
 -e '/in Bridged Mode/i \              local ipv4DNSvalid = {' \
 -e '/in Bridged Mode/i \                wanDnsParam =  post_helper.advancedIPValidation,' \
 -e '/in Bridged Mode/i \              }' \
 -e '/in Bridged Mode/i \              local ipv4DNSdata, ipv4DNShelpmsg = post_helper.handleTableQuery(ipv4DNScolumns, ipv4DNSoptions, nil, nil, ipv4DNSvalid)' \
 -e '/in Bridged Mode/i \              html[#html + 1] = "<legend>LAN Domain Name Server Configuration</legend>"' \
 -e '/in Bridged Mode/i \              html[#html + 1] = ui_helper.createTable(ipv4DNScolumns, ipv4DNSdata, ipv4DNSoptions, nil, ipv4DNShelpmsg)' \
 -e '/in Bridged Mode/i \              html[#html + 1] = "<legend>Network Mode</legend>"' \
 -e '/eth0 =/d' \
 -e '/ethports = validateEthports/d' \
 -e '/^local ethports/,/^end/d' \
 -e '/^local function validateEthports/,/^end/d' \
 -e '/^local ethports_checked/,/^end/d' \
 -e '/ethport_count do/,/^end/d' \
 -e '/--[[/,/]]/d' \
 -e 's/Network mode/Network Mode/' \
 -e 's/to switch the modem/you want to switch/' \
 -e 's/if cur_dhcp_intf == "lan" then/if cur_dhcp_intf == "lan" and not bridged.isBridgedMode() then/' \
 -e 's/please do factory reset/you can revert without factory reset/' \
 -e 's/gateway-modal/broadband-modal/' \
 -e '/header = T"Hostname",/,/^  {/d' \
 -e '/header = "",/i \    header = T"Hostname" ,' \
 -e '/header = "",/i \    name = "sleases_name",' \
 -e '/header = "",/i \    param = "name",' \
 -e '/header = "",/i \    type = "text",' \
 -e '/header = "",/i \    attr = { input = { class="span2" } },' \
 -e '/header = "",/i \  },' \
 -e '/header = "",/i \  {' \
 -e 's/ address/ Address/' \
 -e 's/ leases/ Leases/' \
 -e 's/ subnet/ Subnet/' \
 -e 's/ time/ Time/' \
 -i /www/docroot/modals/ethernet-modal.lp

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
 -e '/^local gVIES/a \local gVNIR = post_helper.getValidateNumberInRange' \
 -e '/^local gVIES/a \local gOV = post_helper.getOptionalValidation' \
 -e '/^local gVIES/a \local vIAS6 = gOV(post_helper.validateIPAndSubnet(6))' \
 -e '/^local gVIES/a \local function validateULAPrefix(value, object, key)' \
 -e '/^local gVIES/a \  local valid, msg = vIAS6(value, object, key)' \
 -e '/^local gVIES/a \  if valid and value ~= "" and (string.sub(string.lower(value),1,2) ~= "fd" or string.sub(value,-3,-1) ~= "/48") then' \
 -e '/^local gVIES/a \    return nil, "ULA Prefix must be within the prefix fd00::/8, with a range of /48"' \
 -e '/^local gVIES/a \  end' \
 -e '/^local gVIES/a \  return valid, msg' \
 -e '/^local gVIES/a \end' \
 -e '/slaacState = "uci/i \    ip6assign = "uci.network.interface.@" .. curintf .. ".ip6assign",' \
 -e '/slaacState = "uci/a \    ula_prefix = "uci.network.globals.ula_prefix",' \
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
 -e '/if content\["localIPv6"\] ~=/i \              if curintf == "lan" then' \
 -e "/if content\[\"localIPv6\"\] ~=/i \                ngx.print(ui_helper.createInputText(T\"IPv6 ULA Prefix<span class='icon-question-sign' title='IPv6 equivalent of IPv4 private addresses. Must start with fd followed by 40 random bits and a /48 range (e.g. fd12:3456:789a::/48)'></span>\", \"ula_prefix\", content[\"ula_prefix\"], ula_attr, helpmsg[\"ula_prefix\"]))" \
 -e '/if content\["localIPv6"\] ~=/i \              end' \
 -e '/if content\["localIPv6"\] ~=/i \              local ip6prefix = proxy.get("rpc.network.interface.@wan6.ip6prefix")' \
 -e '/if content\["localIPv6"\] ~=/i \              if ip6prefix and ip6prefix[1].value ~= "" then' \
 -e "/if content\[\"localIPv6\"\] ~=/i \                ngx.print(ui_helper.createInputText(T\"IPv6 Prefix Size<span class='icon-question-sign' title='Delegate a prefix of the given length to this interface'></span>\", \"ip6assign\", content[\"ip6assign\"], number_attr, helpmsg[\"ip6assign\"]))" \
 -e '/if content\["localIPv6"\] ~=/i \              end' \
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
  sed -e 's/\(if [^ ]*role[^=]*==[^"]*"\)\(guest\)\("\)/\1admin\3/g' -i $f
done
sed \
  -e 's/if role ~= "admin"/if role == "admin"/' \
  -e 's/if w\["provisioned"\] == "1"/if role == "admin" or w\["provisioned"\] == "1"/' \
  -i /www/docroot/modals/mmpbx-service-modal.lp

echo 110@$(date +%H:%M:%S): Enable various cards that the default user was not allowed to see
for f in $(grep -l -r "and not session:isdefaultuser" /www/cards)
do
  sed -e 's/ and not session:isdefaultuser()//' -i $f
done

echo 115@$(date +%H:%M:%S): Show firewall default incoming policy and rules count on card
sed \
  -e '/^local format/a \local proxy = require("datamodel")' \
  -e '/firewall\.mode/a \    enabledRuleCount = "rpc.gui.firewall.enabledRuleCount",' \
  -e '/firewall\.mode/a \    fwd_ipv4_rules = "uci.firewall.userredirectNumberOfEntries",' \
  -e '/firewall\.mode/a \    fwd_ipv6_rules = "uci.firewall.pinholeruleNumberOfEntries",' \
  -e '/firewall\.mode/a \    lanIpv6Enabled = "uci.network.interface.@lan.ipv6",' \
  -e '/firewall\.mode/a \    pinholeEnabled = "uci.firewall.rulesgroup.@pinholerules.enabled",' \
  -e '/getExactContent/a \local fw_incoming_policy' \
  -e '/getExactContent/a \local zones = content_helper.convertResultToObject("uci.firewall.zone.", proxy.get("uci.firewall.zone."))' \
  -e '/getExactContent/a \for key,zone in ipairs(zones) do' \
  -e '/getExactContent/a \  if zone.wan == "1" then' \
  -e '/getExactContent/a \    fw_incoming_policy = string.untaint(zone.input)' \
  -e '/getExactContent/a \    break' \
  -e '/getExactContent/a \  end' \
  -e '/getExactContent/a \end' \
  -e '/getExactContent/a \local fw_status_light_map = {' \
  -e '/getExactContent/a \  DROP = "1",' \
  -e '/getExactContent/a \  REJECT = "2",' \
  -e '/getExactContent/a \  ACCEPT = "4"' \
  -e '/getExactContent/a \}' \
  -e 's/T"low"/T"Low"/' \
  -e 's/T"normal"/T"Normal"/' \
  -e 's/T"high"/T"High"/' \
  -e 's/T"user"/T"User Defined"/' \
  -e "/subinfos/i \    ');" \
  -e '/subinfos/i \    local fw_status = format("Default Incoming Policy: <strong>%s</strong>", fw_incoming_policy)' \
  -e '/subinfos/i \    ngx.print(ui_helper.createSimpleLight(fw_status_light_map[fw_incoming_policy], fw_status))' \
  -e "/subinfos/i \    ngx.print('\\\\" \
  -e '/Firewall level/a \            local rules_modal_link = "class=\\"modal-link\\" data-toggle=\\"modal\\" data-remote=\\"/modals/firewall-rules-modal.lp\\" data-id=\\"firewall-rules-modal\\""' \
  -e '/Firewall level/a \            html[#html+1] = format(N("<strong %1$s>%2$d Firewall rule</strong> active","<strong %1$s>%2$d Firewall rules</strong> active", content.enabledRuleCount), rules_modal_link, content.enabledRuleCount)' \
  -e '/Firewall level/a \            html[#html+1] = "<br>"' \
  -e '/Firewall level/a \            local fwd_modal_link = "class=\\"modal-link\\" data-toggle=\\"modal\\" data-remote=\\"/modals/firewall-port-forwarding-modal.lp\\" data-id=\\"firewall-port-forwarding-modal\\""' \
  -e '/Firewall level/a \            local fwd_count = tonumber(content.fwd_ipv4_rules)' \
  -e '/Firewall level/a \            if content.lanIpv6Enabled ~= "0" and content.pinholeEnabled == "1" then' \
  -e '/Firewall level/a \              fwd_count = fwd_count + tonumber(content.fwd_ipv6_rules)' \
  -e '/Firewall level/a \            end' \
  -e '/Firewall level/a \            html[#html+1] = format(N("<strong %1$s>%2$d Port Forwarding rule</strong> defined","<strong %1$s>%2$d Port Forwarding rules</strong> defined", fwd_count), fwd_modal_link, fwd_count)' \
  -e '/Firewall level/a \            html[#html+1] = "<br>"' \
  -e '/Firewall level/a \            local nat_alg_card_hidden = proxy.get("uci.web.card.@card_natalghelper.hide")' \
  -e '/Firewall level/a \            if not nat_alg_card_hidden or nat_alg_card_hidden[1].value == "1" then' \
  -e '/Firewall level/a \              local alg_modal_link = "class=\\"modal-link\\" data-toggle=\\"modal\\" data-remote=\\"/modals/nat-alg-helper-modal.lp\\" data-id=\\"nat-alg-helper-modal\\""' \
  -e '/Firewall level/a \              local enabled_count = 0' \
  -e '/Firewall level/a \              local disabled_count = 0' \
  -e '/Firewall level/a \              local helper_uci_path = "uci.firewall.helper."' \
  -e '/Firewall level/a \              local helper_uci_content = proxy.get(helper_uci_path)' \
  -e '/Firewall level/a \              helper_uci_content = content_helper.convertResultToObject(helper_uci_path,helper_uci_content)' \
  -e '/Firewall level/a \              for _,v in ipairs(helper_uci_content) do' \
  -e '/Firewall level/a \                if v.intf ~= "loopback" then' \
  -e '/Firewall level/a \                  if v.enable ~= "0" then' \
  -e '/Firewall level/a \                    enabled_count = enabled_count + 1' \
  -e '/Firewall level/a \                   else' \
  -e '/Firewall level/a \                    disabled_count = disabled_count + 1' \
  -e '/Firewall level/a \                  end' \
  -e '/Firewall level/a \                end' \
  -e '/Firewall level/a \              end' \
  -e '/Firewall level/a \              if enabled_count > 0 then' \
  -e '/Firewall level/a \                html[#html+1] = format(N("<strong %1$s>%2$d NAT Helper</strong> enabled","<strong %1$s>%2$d NAT Helpers</strong> enabled", enabled_count), alg_modal_link, enabled_count)' \
  -e '/Firewall level/a \              else' \
  -e '/Firewall level/a \                html[#html+1] = format(N("<strong %1$s>%2$d NAT Helper</strong> disabled","<strong %1$s>%2$d NAT Helpers</strong> disabled", disabled_count), alg_modal_link, disabled_count)' \
  -e '/Firewall level/a \              end' \
  -e '/Firewall level/a \              html[#html+1] = "<br>"' \
  -e '/Firewall level/a \            end' \
  -e '/Firewall level/a \            local dns_int' \
  -e '/Firewall level/a \            for _, v in ipairs(proxy.getPN("uci.firewall.redirect.", true)) do' \
  -e '/Firewall level/a \              local path = v.path' \
  -e '/Firewall level/a \              local values = proxy.get(path.."name", path.."enabled")' \
  -e '/Firewall level/a \              if values then' \
  -e '/Firewall level/a \                local name = values[1].value' \
  -e '/Firewall level/a \                if name == "Redirect-DNS" or name == "Intercept-DNS" then' \
  -e '/Firewall level/a \                  if values[2] then' \
  -e '/Firewall level/a \                    if values[2].value == "0" then' \
  -e '/Firewall level/a \                      html[#html+1] = ui_helper.createSimpleLight("0", "DNS Intercept disabled")' \
  -e '/Firewall level/a \                    else' \
  -e '/Firewall level/a \                      html[#html+1] = ui_helper.createSimpleLight("1", "DNS Intercept enabled")' \
  -e '/Firewall level/a \                    end' \
  -e '/Firewall level/a \                  end' \
  -e '/Firewall level/a \                  break' \
  -e '/Firewall level/a \                end' \
  -e '/Firewall level/a \              end' \
  -e '/Firewall level/a \            end' \
  -e '/numrules/,/numrules_v6/d' \
  -e '/if content.mode/,/end/d' \
  -i /www/cards/008_firewall.lp

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

SRV_firewall=0
# Version 2021.02.22 set an incorrect value for synflood_rate, so have to fix it
synflood_rate="$(uci -q get firewall.@defaults[0].synflood_rate)" 
if [ ! -z "$synflood_rate" ]; then
  echo $synflood_rate | grep -q -E '^[0-9]+/s$'
  if [ $? = 1 ]; then
    synflood_rate="$(echo $synflood_rate | grep -o -E '^[0-9]+')" 
    uci set firewall.@defaults[0].synflood_rate="$synflood_rate/s"
    SRV_firewall=$(( $SRV_firewall + 1 ))
  fi
fi
# Version 2021.02.22 allowed setting of tcp_syncookies but it is not enabled in kernel, so have to remove it
if [ ! -z "$(uci -q get firewall.@defaults[0].tcp_syncookies)" ]; then
  uci -q delete firewall.@defaults[0].tcp_syncookies
  SRV_firewall=$(( $SRV_firewall + 1 ))
fi
if [ $SRV_firewall -gt 0 ]; then
  uci commit firewall
  /etc/init.d/firewall reload 2> /dev/null
fi

echo 115@$(date +%H:%M:%S): Add transformer mapping for uci.firewall.nat.
sed -n '/-- uci.firewall.redirect/,/MultiMap/p' /usr/share/transformer/mappings/uci/firewall.map |  sed -e 's/redirect/nat/g' >> /usr/share/transformer/mappings/uci/firewall.map

echo 116@$(date +%H:%M:%S): Add WAN NAT ALG helper count to Firewall card
sed \
  -e '/local enabled_count/,/<br>/d' \
  -e '/local alg_modal_link/a \              local zones = proxy.getPN("uci.firewall.zone.", true)' \
  -e '/local alg_modal_link/a \              for k,v in ipairs(zones) do' \
  -e '/local alg_modal_link/a \                local wan = proxy.get(v.path .. "wan")' \
  -e '/local alg_modal_link/a \                if wan and wan[1].value == "1" then' \
  -e '/local alg_modal_link/a \                  local helpers = proxy.get(v.path .. "helper.")' \
  -e '/local alg_modal_link/a \                  if helpers then' \
  -e '/local alg_modal_link/a \                    html[#html+1] = format(N("<strong %1$s>%2$d WAN NAT ALG Helper</strong> enabled","<strong %1$s>%2$d WAN NAT ALG Helpers</strong> enabled", #helpers), alg_modal_link, #helpers)' \
  -e '/local alg_modal_link/a \                    html[#html+1] = "<br>"' \
  -e '/local alg_modal_link/a \                  end' \
  -e '/local alg_modal_link/a \                end' \
  -e '/local alg_modal_link/a \              end' \
  -i /www/cards/008_firewall.lp

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


echo 140@$(date +%H:%M:%S): Sort the device map hosts by name
sed \
 -e 's/loadTableData("sys.hosts.host.", dcols)/loadTableData("sys.hosts.host.", dcols, nil, "FriendlyName")/' \
 -i /www/snippets/networkmap.lp

echo 140@$(date +%H:%M:%S): Fix path mapping
sed \
 -e 's/rpc.hosts.host./sys.hosts.host./' \
 -i /www/docroot/modals/device-modal.lp

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
  sed -e 's/createCardHeaderNoIcon/createCardHeader/' -i $f
done
[ -f /www/cards/*_cwmpconf.lp ] && sed -e 's/switchName, content\["cwmp_state"\], {input = {id = "cwmp_card_state"}}/nil, nil, nil/' -i /www/cards/090_cwmpconf.lp

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
  -e '/dlna_name/d' \
  -e '/samba_name/d' \
  -e '/^local content/i \local usb = {}' \
  -e '/^local content/i\local usbdev_data = proxy.getPN("sys.usb.device.", true)' \
  -e '/^local content/i\if usbdev_data then' \
  -e '/^local content/i\  local v' \
  -e '/^local content/i\  for _,v in ipairs(usbdev_data) do' \
  -e '/^local content/i\    local partitions = proxy.get(v.path .. "partitionOfEntries")' \
  -e '/^local content/i\    if partitions then' \
  -e '/^local content/i\      partitions = partitions[1].value' \
  -e '/^local content/i\      if partitions ~= "0" then' \
  -e '/^local content/i\        usb[#usb+1] = {' \
  -e '/^local content/i\          product = proxy.get(v.path .. "product")[1].value,' \
  -e '/^local content/i\          size = proxy.get(v.path .. "partition.1.TotalSpace")[1].value,' \
  -e '/^local content/i\        }' \
  -e '/^local content/i\      end' \
  -e '/^local content/i\    end' \
  -e '/^local content/i\  end' \
  -e '/^local content/i\end' \
  -e '/ngx.print(html)/i\                if #usb == 0 then' \
  -e '/ngx.print(html)/i\                  tinsert(html, ui_helper.createSimpleLight("0", T"No USB devices found", attributes))' \
  -e '/ngx.print(html)/i\                else' \
  -e '/ngx.print(html)/i\                  tinsert(html, ui_helper.createSimpleLight("1", format(N("%d USB Device found:","%d USB devices found:",#usb),#usb), attributes))' \
  -e '/ngx.print(html)/i\                  tinsert(html, T"<p class=\\"subinfos\\">")' \
  -e '/ngx.print(html)/i\                  local v' \
  -e '/ngx.print(html)/i\                  for _,v in pairs(usb) do' \
  -e '/ngx.print(html)/i\                    tinsert(html, format("<span class=\\"simple-desc\\"><i class=\\"icon-hdd status-icon\\"></i>&nbsp;%s %s</span>", v.size, v.product))' \
  -e '/ngx.print(html)/i\                  end' \
  -e '/ngx.print(html)/i\                  tinsert(html, T"</p>")' \
  -e '/ngx.print(html)/i\                end' \
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

sed \
  -e 's/:56px/:80px/' \
  -e 's/:170px/:150px/' \
  -i /www/docroot/css/responsive.css

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
      sed -e "/<\/head>\\\/i '); local lp = require(\"web.lp\"); lp.include(\"../snippets/theme-$LP.lp\"); ngx.print('\\\\" -i $f
    else
      head=$(grep -n '</head>\\' $f | cut -d: -f1)
      if [ $head -lt $req ]; then
        sed -e "/<\/head>\\\/i '); local lp = require(\"web.lp\"); lp.include(\"../snippets/theme-$LP.lp\"); ngx.print('\\\\" -e "${req}d" -i $f
      else
        sed -e "/<\/head>\\\/i '); lp.include(\"../snippets/theme-$LP.lp\"); ngx.print('\\\\" -i $f
      fi
    fi
  fi
done

echo 165@$(date +%H:%M:%S): Deploy theme files
echo QlpoOTFBWSZTWTQi3xcAbbt/////////////////////////////////////////////4IW998AVVBAFuvbPt97N73te27s0y3efT55ffa7u1Xdu5zd2Y6Ltd3OrYbmTrbYq8+pI3et4W913Ule+53x6+nznzO8vvt0gO9zgW7cXQwT0xe9uJ9Zd25fe3Xztz777l3eZ13eJDMaRu83u33n3j3vZfXm97vNvXBd1rXC83l2927nK09e3l7z173eV7a3bdrBOe915zt1c+Bd7uloYionpqAbuO7anNju5wC3fXLY8q9NOjA33OA3c5R74+8Hh93Di+sFAWfZ5e++53ec+dn3Xg+e+++W59vb3XTWQct3c7d02XbszJWmtc2JtoazXbdKt2rrWxc53dOVu12060rqW2W3ZGd225xXW6LbbbDqSddrjknW42KJnd1dm23dw3OuTatFtOaXLtu7dKttdbdlOF2cczVzdDbXVtum5tR172dk2le4dwty3U7adbrUrlr3ezje54iqaAAAAmCYACaYAAmp6YATATTARgAmATAAmJkyYCMjEMBNGExMCMAAAAmAAEwAAAoRVNAANAADQAjJgmATTCYAGkwBMTAJgmAKemBMAJk9AAABkABMGQp4jE0xMgwEA0YTKn6amyNDJQapCTaACGmmAEyYTTJgFMMmTJiNoAA0DQaExpMAJmgIGp5TaU/BMmTJgpsmTIaYjQyGTQwmmmjRNo0YQZMmjRoyChFQJoADQMgZNMgaMg0GjQaAAU8ENMhkxNMJgJpgjRk0yaYmjRiTw0IYCZGTKemkzRT9GmmmAFNkaZGQZMhk0wQFBEkQQQCNA0mgaBCbTRkaaib0Eynsmgm1DyATaZMmCEbSnpHqe1Go2jyCaanlDxTaTPIKeJPI0mnqZNMynhTymynqeo9Mk8kbSaeJDTTZTMo9TQJJIgjQTCYmjQaZNBMaAmIxoAAAAAAAAJgJoyap+CMTTTU8AQGJgEYjExNGjEwIaaDTJhAGE000GmTVCwYO/lOHsvSlyqr4OGkSv0bsQtuvyVUbsBedX/hQUatjVRhQCowMJiRmcu29AySa6ZV6lFwdEeElCnybVOLT8C0Vj8rbTdJEz6SSiiSvD7x416lc3aoEFoLeWd5USioFQjAqiiobvQ3sMetlEMPEn5VvysraOaOQIwc4SaQHy6kBb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 | base64 -d | bzcat | tar -xf - -C /

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
  sed -e '/^else/,/T"Boost Your Wi-Fi"/d' -i /www/snippets/tabs-home.lp
fi
for f in /www/docroot/modals/assistance-modal.lp /www/docroot/modals/usermgr-modal.lp
do
  sed \
    -e '/^if not bridged.isBridgedMode/i \  local lp = require("web.lp")' \
    -e '/^if not bridged.isBridgedMode/i \  lp.setpath("/www/snippets/")' \
    -e '/^if not bridged.isBridgedMode/i \  lp.include("tabs-management.lp")' \
    -e '/^if not bridged.isBridgedMode/,/^end/d' \
    -i $f
done

SRV_transformer=$(( $SRV_transformer + 2 ))

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

# Initial invocation of transformer code will fail if history directory does not exist
if [ ! -d /root/trafficmon/history ]; then
  echo 180@$(date +%H:%M:%S): Create directory to retain traffic monitor daily history
  mkdir -p /root/trafficmon/history
fi
if [ ! -f /root/trafficmon/history/.wan_rollover ]; then
  echo 180@$(date +%H:%M:%S): Create WAN traffic monitor history configuration file
  echo "1" > /root/trafficmon/history/.wan_rollover
fi
if [ ! -f /root/trafficmon/history/.wwan_rollover ]; then
  echo 180@$(date +%H:%M:%S): Create WWAN traffic monitor history configuration file
  echo "1" > /root/trafficmon/history/.wwan_rollover
fi

grep -q "/usr/sbin/traffichistory.lua" /etc/crontabs/root
if [ $? -eq 1 ]; then
  echo 180@$(date +%H:%M:%S): Create cron job to retain traffic monitor daily history
  echo "9,19,29,39,49,59 * * * * /usr/sbin/traffichistory.lua" >> /etc/crontabs/root
  SRV_cron=$(( $SRV_cron + 1 ))
fi

echo 195@$(date +%H:%M:%S): Sequencing cards
for RULE in $(uci show web | grep '=card' | cut -d= -f1)
do
  CARD=$(uci -q get ${RULE}.card)
  FILE=$(ls /www/cards/ | grep "..._${CARD#*_}")
  if [ "$CARD" != "$FILE" ]
  then
    echo "195@$(date +%H:%M:%S):  - Renaming $FILE to $CARD"
    mv /www/cards/$FILE /www/cards/$CARD
  fi
done

if [ -z "$ALLCARDRULES" -a -f tch-gui-unhide-cards ]
then
  ./tch-gui-unhide-cards -s -a -q
fi

if [ $THEME_ONLY = n ]; then
  MKTING_VERSION=$(uci get version.@version[0].marketing_name)
  for l in $(grep -l -r 'current_year); ngx.print(' /www 2>/dev/null)
  do
    echo 200@$(date +%H:%M:%S): Adding tch-gui-unhide version to copyright in $l
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.06.25 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
  done
fi

echo 200@$(date +%H:%M:%S): Applying service changes if required...
apply_service_changes

chmod 644 /usr/share/transformer/mappings/rpc/gui.*
echo "************************************************************"
echo "* Done!! You should clear your browser cache of images and *"
echo "* files, otherwise you won't see the theme changes.        *"
echo "************************************************************"
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
