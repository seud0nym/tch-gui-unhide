#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 18.1.c - Release 2021.07.29
RELEASE='2021.07.29@14:16'
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
    /etc/init.d/watchdog-tch stop >/dev/null 2>&1
    /etc/init.d/transformer restart
    /etc/init.d/watchdog-tch start >/dev/null 2>&1
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

echo 055@$(date +%H:%M:%S): Deploy modified eco GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified iperf GUI code
echo QlpoOTFBWSZTWcpMm/gAAgz/hcyQAEBQZ/+CKBIJBP/n3ioAAMAAAAgwAZlZNAkok0eSA9R6g0AAADTQBkTTSnqAAAAAAAAIpJoU8lPU/JhMqbUep6gaD1DBlPU8h/tyX5pMewjbWCjjnVrRF2k02gzdIL01t9owpYMIBIQljn16mrPcc+cqrRsWyPQYlE8zqqJ3WivSx9DmNWokGudONKc0qLGsuGkrWYpsO0wKPQjIlhZfFEASCg26AIcefP8/jTIQq5MgUW+mlBRAzQwiAkAiB10I45ibFmQLEzkrIYiI3M8DXIhSIlKJDiDBpuGmkXMio2KXQKXIg0tRYMzKOGdFBvNSl4zHl5FYev8ISofVPPU6D4395HhGH28PvckfWROZcFxJTpSpiBWGgFpQpERFhA9PUxCFpMG+YXZzAe5vvR7B6iEZ77AGsWhe6k8g6Qy7gd2YXWgztf9MrUh786U390hudYYSC8KExKFKQ4j1HZRw9zhIN6fAbIHcVg43IZ7J8iwNjBgduCdhzqPfGSAf8XckU4UJDKTJv4A= | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified broadband GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified devices GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified firewall GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified gateway GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified internet GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified iproutes GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified lan GUI code
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
echo QlpoOTFBWSZTWZWN7I0AY1x/j/3wAEB//////+//7/////sACBAQgCABAAAIYDb++X3r7W9zTo19BvfTHbdw8gOxa6xXmwEpnul7Ze3tWg712ex09GO6XCo83t7a1ubW7YD67x9vsRrHfe66plfej77y7fdtimajGe5nZhs32Y+bVUVvZ2sWFmtppnPWeV9DfI1i0raNY1rCtUtINix2HeK9vXnUp50anQTppWdz3jXiEkSYkwExAmpptU0n6o9iZNUxND9TTUGPUymn6p6n6mU8oAAAAkQgQiZBNCYo9CnmQmFB6gNAeoHpNGnqZAD1BiABpiRFPSnqeiA8UNNqeoAAGnqBoAANAAAAAAk1ERCaaaKeSnmibJoin5HpT1D1Jk/U9JP1I09T1ADygHpA9QAARJIImTQ000nk1Mqfkaap+jTI1JtSeyps1R5EyYA1AGRkaeoaeoIkhBABBpRqbJqeKn6o9EzNJlA9Taho9QaNPUA0APUHqA/cL9v+ifnhzj0Aecb3hxFjzSyoQyqiFRVeqQX1ev2eu3rqrX239l9+usK6g6gaDQBVjWDrJr4JaWbXxs9ZmLtTaRUgiLIIDIkVWA0MVRgrEFYMTQBshO9CmCIiCwiWFQ99dHe4r7fsQ/U9xo/nt+Eohn4zi+0OvlqEUTMkIcFmEJ5knAytuS81gulyn+vx6Z99FDnkqv78FtlIlKhShDOrIRupRoGEQbSfg5k+phl+tgUmbUiZJbMZmXTcMbO1i+h950I7mMz+fuAONvyQsMkIx7lzMqQ1FCkilddZvLyNu60r97JlUVy2VmlKNmuubml6cJNyw6QfLN4tczSpl6BvNYs57NbzaP5sWMxXFBmEf8nNuYVNSkJayhZcpjEXbsFmklKvUSHcTM2ZR5Gcy2cxCxluCNPpFS6iXBdVTtyiehaNtBYzBXjOrWQReFs4VtOTligZYTCdiQw5mV3COuDeTRBl0UZsIUhgmKoS+lksUw/t41tzteH6movskbnrIBboA7dEZ5wlnMxwdtcswLBJyUkVYHoYaUtVA0ku7WaCbAQjGQvSWiKyMBJJ3dIVr7iSAupZCg99yVZ9TEYA8GERREN3ij19PJ3iow7tWlkBtKbxmKTXIgKu+NKz8GeZrfNMCQhobPEvYxeBc2EDdrZwjID3o3xsrFHDXZsKGK76oZFhyNICvMfQJ947DtKKKKKKKKKKKKJJJL3aG0NoQbCX8t1uR3BcO7N43dO6kyxZGbsZ0ednrQG2eNdlEtu/wzVrz43KHjeCz8dr6bfuWLYqHLnBeVGIXEnndmWiGKKOh2s1Ys283K+0yWphyIdoWh9PRGGrsxtBEhHJ2GAZaPi1yPVS9owLtLD2e7SqghtEPxZxSHEMT05OnwycVdFOEwaRlxDiyUBtf3mctpk/FdYZhkERCO2LcRQyKqq5fF5N6fvJJ2pFx1knkCEIHrZ6k2+uVnuKAEDqLFFQ4FoUKAho0lgEEoSKIvfGkJiykvN0Wyj9PVSqDaAO5iQpiUhUFWJFBCwHN9WiJv30WsOCBadnwsGTEeLBA2CZGZZKCQoFWaDJsYsbpEQSKLCYYpFhVFBSCU0FCUDFAgiSUMRWBTABASQRBEQYCLNf8Z5hBtpI+fBu0UcI9ZpaGNb4ntQvOZ9/83Wx73zGqdTh1D2WWiJCCyAdQouGobX2Hc7yKIN5gcPIB1SH1A/TPjGTPqsJciZ30UmozZOAHMi7YY+wdhbXak2aJQemFRExSbKgYuBwDFPmgHeb+jFyi6gjCggJCplvaEIqjGIoLlEAlaYH4LyK5fvgiIIglzcMMw242OSMrBflY2eexLsSFsxlNwCeh2d98uaAxeITU1EGB2KD4PwmJKrrHvW5rNHJZUHrHQy8sHMmWF5M3lKcorUoIKByRQe4s9v5hkWIOdOvIfXnWVssLs7r4muiiOfndZjUDLDFKlFKGAaEaRtJiZLkb20ZxGFhGGMkMqCWxJ+cPAQMV1nEGCW+C0rd64wLfh8OQuUVQ3v2sISlw+GEx4yZOLMzAHmBiKUgDbYnDXK/zxBMB9O/pxDd+DBKq0WGxd8Ql239j6jUNDC60gpYas20RZBP5bTI9tQVvzz2nrINyO/vt4Kgh8Xb/1XQSt67WbZO5uUj81az4O/WcyvRp49It6bZfYRCwoTUMXPhryDWF72pnSbjccvq9IChFFihNnfUGWVIjGIwVVlLhgOcdZjgXwNhSMDCdQ6A/gZpC02Hk5EMD5dBjdAKCBO/wqnKXJwxeJGQ4vlfQc8ic2orKs7UmC7hPZyx7tUqUIpyrHS8W2A1Wp0uDkAgFBzNEH0Zxu8VUOyVUXclxRXy8OmzEt1YsOrijswQappTPmQDmal384RE2sQiKQazO3JU0pFaFS92t9ZIe5Isivtkubd245bqIQkJDqzG8DthfjSqYuO+kA5lvNK4wTUkyas1AysIUIkYyMEIiLbTJGyVdVb3daMZGSdPbslq2gFz/SbasUeYmyAOjxcOaxitrHDJNLHY1dE8vynnQSwUs6NHnzjbXAByOSBVb4/irsVr8wmEpb552ra1xlrFF4UVtZSDXEpja7tmNmb3GckIUFIhdp9w1Rrp3Lqc0FrC0bblFrQH6/oROKs4kD7Ol9cd+uHTLSjlmRYeaSlpJ0LnEopMnmFFAHTnFxtGEIurlttmt+BoGrkHJq7399Sqqq1MblWqrzBNuptxpVKjFVGj0bg54RBc0lkycLw9ErcpYqqGMMQiBJbxA0oCA7AYggMmfLqhGBpq69HxLaVVslSVI4Njgi44pAEiD0+ssyrzbMcLwgdqmzWzabG2PE7X7yw6FnPZrQ2NmFBCjeBQDasy5MI00KipFKN1rPZmGpSgub92eOOHR3Hdp5uVPO8LPLMgfsv7y/dglmmrvLG0vZiZE+5hDf3Xg2qJOeqkv5+koqNcHPKXb03R3pd3hHh1IiOqwYGYFBAzsmw0MKiKKKKKKKKLIxZNT9FiQWpZrDRDJ8YhbL5l7IN7WWDI27RVFJtMZaCByxprMG5i+MXpctTGIYoVRHyjib8hz67r0ZSDVlINrWauU1hlINOG12BRqUc1ct6RkAiLFFiAHQzGwMhpFS3qmmuewv3zvU+gTBUFKElQJCL279a0qs6oqsPtDHlQcBvghsN7VxaEjXzEvE8wOOxxcV/Thbbtj0zk9fFAwNOIDqVyxuF5S5nXixJz19MWmlUVcHHpi8zNe75QKappoL1mk7Yo9mVOQuOCPVyI+yhkXrWnjSoNs9nzngNxSST+6ZA4+2PWaNENG6zFVnJXwuuvkrhpmgYYNLIUvOdrAXtfs0/q+QKTk6ocrHo2GNM8vpo9+aez67y5tdmyyKSECG4C55Ssp/WUcZ06/2w8C+ctquVQTY9cJdd23qrp6B7CTvnjzmK+f74+wmnrefQ52P5/bA68mbFu6X56qX0mA62jbfIIk6VGV8F5IHV6rn8tF7QkX7I4LcpfFVN8Xb3cwoHblmOSd5qC2JRIzu8dBQNBUP8x7Rj3HMdJ+QiRIkSJEqPqERERMnXx5+KPR3cMTBjGLqjx5OtnUlFXRxk+3V/v/UifDe1Y8VWvKBy0GrN18Z62fJt9IPMEXh65FXxg3XMEebsCND8yFwntcxLbfwVBiR/CEhUgeDZUhpIaUAfWga8VYxVVdwcPCPUOf8X0dTY7whIg/KpACIEYxIqyO9KAEQhTJSMn3RBiDG7Cm22mGT4j4PFeYOZ6pVyeCVKPY7qlY6PK43RyjPCxelhOIzCZfBYXJZLQlmlQuFUopQIFqKoxoCUFSqKQEGoMmiGJcGUEwuZQ0VULohihkKSKkWQwYhQYYSUwtApICpBRRZdKQtRW5IMJSt/IfgEU6983vhh3dXlwaZUS9R5jlLCw3jDjDg4454yBAgdh7xzETTlCalFqVJAqD/Z7v0ZJf8m3fEN4Q/J+Q8ROV74b2IoNq+QafayHTHv5iDSIMF/PxP528nWiVu7XunvcgG9vHy9DuYzH4m6R+OVCYMZnS5zk9n5JfDrr7CrkH6MbLwNrl7kfj3yjE7kr2IeLa3IJnvsFkxsOS5akJcqbqUdC73fLBUooh5k0MWTBnJ30fl6B64WQkHicXaPqL6eKE07dNmXJnLo23JQpmYT/bsKy4+C8dRrFyEzou07VjfFN+EZWQzCgX8Q+UEItyyYGFRW5W+9H7H37SZStxBo0WDmmZ0MauNfTBQY+vVVDhLjeTNabqMSyJUuLAgKT80NNlEBXoGPrU74M2pmknUVnJzvvrV1aDsnr2byXYVNSV7bnb9QzETaPPXF6WVm99p+5RqxBUVJegqVa0+r61ivKdodJ1qPTO7Y7PAwq0VZwTTfn5X19elmn2N705l5OScJt228p10qMmx2XnbvwoWxp8+RuYY+FfWescjrmeXdDa5mlphLApgSmjsPDmUb4d0zZnu0iKWvo+go5GypY1z3JWy2kO2InMKOrjaJyVaSHrLlSXJCcoKDDMDJvgseNT22+kPRSS/baKCR9rGoVaLN2RQKLvQHBQwCpbB0+pvTv8bGt4wxk7RYn+G0wnXapp7XUrnZwfeG9hLfYQYtcVE4tRwfXVUj32RzN9IbUDm5xTnDd2UPrgpCLbhVvDp44oKo68sVb4zDY8laptZCGEYDICMIXsrUzKsQeguImCJqYwvbPXuLi7IRDZsLORo0asGQXIWB6/u2bN+1BX7vhi4Q9WBNLJuzjZXEAmVVBgIImLq7u4jXNd/JtozUHmwvS+zyBNyzEI2/FxDNs5LlmA8QMBkjBjVtONkXIErHfYd6PS7rXwQ4iFEsbB4aiacRKN108ocmQXag2V5S+7m5qIiIIiIvckyYM+HO5sk+xdq1w24YNm6Vk1/QzlNEXaroSpjeHBOEoqqiPfFpqUeno0AxmJQRZ436fZI3CvBy4cXogJpxVEcJrpKWGgseGAIfCfMcTw9WsfNg+8wy/buOrXf5Z4KfbmNZwq2hWJbV3d3szUzJhORkpixtX6yEKT3frAJ9AlE+pqAYKKhCoQ95aKApBRJAhCEKRKQf5rClUzw/EmcpE3REhuicydl7SW8EvTWXvSZDFtRilk8/o3XQIJiBy4mYiTQXSwGAUi9zRIce/WC6K93qQByScC263knC+1TDARu7EtAFDSqgCXeeD7e5FokgHS7fR5gzSMwXjj5lihtmEf0A9ZkPAI853ITvfwA0dRp2xkZhDAdz1joNraQUoLyCucSrXQOUoEZFopqxRRBMBTnv9FBcS7WODBz8KhnT+6FKRDvC9KQwxhxpSrTnGs68jXWnLzkQwwtSHTsMkXRFTbEeo5fq3ORyPY8oMiHi7k4J2FHLam1ENxvuYHUlmiwiGAm0Mp3YpVWJEwLQXGgLQJaWzsM7DjDicMDDAqdf10BzXB1XGWkKXqpRou4httjc9MoDUiukwRLujAGj+VB9cAJSe09Z7xY90shZPiYmMbyYMGCavX1V8P6MCEvKMCS0Fc+79HZ0EP3ez6PR6/p4dfwiEXHqwgJNy9+UGy1Ug/1VrL/IY2GCbRUm1k8sA+AXCfMMCN5nPCWmRgIKV8TFp91wdrKPKCuLy+Po0mj1CLqe8idtgKNixbq/JtRMPuwYPESrqt4/t8cGDVldnwpMGDmDB4r9Qo2JGEItCBmBFObMZqZFOqZeYkI5rcEiQ4nNxAmVm4QZIobccHU9zlv4jsWJMeFpONpwYKntt5c88mRjKYfwHf4JFb45mX3cjg7jBgeDg7DxkgzO8l6NcsFeXbp0Cxc6wcjqJaNtAPfqYgksWPYqKOpNzxsJdDrnlt0sb2gxcDRm8wkSZGeg5i5i3SqLI5myl8dI4zE7i49wwkYv5TkInLOW9yTkZOCijkSgykMtxd8AW21Y3Sxy09i07amzGkbC0VSRo9huScFzgwUUUQCyZO8LweP0JWzBg6csOBmiIw2qIA1loeHk002hJG0DCxcbRzupA2rwghH8kDbGh/vVp0vvHft2jP1oD5pFr7DfU9u/PjQcZkpSQHIs5tgS11Q+37xCJKEqqKiRKpkoiJUGiQGAxisikaqVUghVUMYjCLIJEiIFU0KhIsiAEikIKpIIEiqMiqhw9fLXizx9eTVAuFVAqnIxu/HAqmCIIRzw1fleYmVCr5Z6VlwMD+sOpeSM1hUIxDMA3VQCdxha6yNgg0y+VyWj8plJeaVaq4SPxRkFxUVWj3lsbs06a++rGBqwKUZIp3HynbPVMgT/SfQDEiQYKsVVgiCrEYoosRii4nUdPgIoaSVRN5c2tBbCOkVB7lLEpX1HMaTSQQf0PhA7KxKlF5gG20iwPARpTCIFxMPoXPauZsfSMdvBrh2bXwYYgFgaRF0EnGfkgM88B4/SFFB5S6wW43HEdx4frIfCbzg7E7MBhzC8fIcBZ6hXyY++xMBkEikNxuAVZFFV58fYzWqvwWpFKDi2nTgzWAzqvHpE80D7jSecuAaarc2PwB7j9o48CTcENm1V9hN+fl2TcRDo3bgPmnuiWtVYolCIqqoir2TQOOkdOkqdwy8bzQELPHE5NJ6lce1MUwi0BiIfWdAK+hCgaGEEDJG/gW4MV2ejvU2r9RwXYa1FjbUlsS26a8C2gNGsmQ0MvuHHg+zzAdUU3Bm7jz+gkYQhJzLkGdPMcTq51Wjo81SZRa7dvS1WgXarqaakMij4oKxboXULR7Odei9FQajx8gPrVYDIQd1hdwHScaUOJ1MeKOZM60Z50zj2JXc0Z2kaDIeoyzCkLD6ofY8d/kUaAbGJGKQgB4EKooSFAUiRhqvMzk/UrS5h+YKWgoKWgoMwNovqKI4SNBUNoGz0KPzHn0Ik8BfRs2bEfa0VUIosoaGQ5tTAjgKDv34Nu4UU6ytQSfxincoUUYLsl75Anj1++Zs1r2BQWfn+Y7puWI0ROsbQP2ehCqrfVLql1MzPOzJCxTKFKKBiGBoYJOJ5ITm4CpumtjyxBjQxgiiCKdJHrPpPs80fgPsqqdFMckiDAJgNlFwACFEImb2DfdDHfzRy83V+c9js8/Lq4xddddttttts9zcxMbdi89rnEIS5RvnIx5zUxgEX3s8JUNEOKeguzIns6i32YKVjchWkISKUPbNo9UqxrB6pnEGR6ZZxvB42Ss1y1o1qSwTLlO/u5OlB3l9Nl4FiULR0BmMiTHaMKGnBQSPyq0/KcSj5TYHuD2op2HYDPcmsH7qAcLPn6vLngiWYLMCF1GjphQVeW82YlllTAhuBgGu+oKpGAk1zpZGql64w3dsxXEaGiFJAkpKQSFMBIMEwQNCVJQisftT89VNTTqqU00sDDoFoQbPE7JjhmhFpRpVziru7vNilZ3SE0zy410yYhMYtYmMwd7sKA2hGzy2gYTgKBAHM3oYF6Z0KAky72axJa12PWL9m9u1UqNj6tcsk2xB/CCCIiRgxjJBGHwvnKiECkpGQpaAIsCleEoE9woYDh+0Nhp0AZyB1TilWoomxJTAmRQcAIKRV3wAwPcQ3+VYCHIIGNhttrmQUaqgVpLJEtLETTAWRrR+fKaA7E+pkgxIyDFIoQhCH4jNMGxcjUE8EDNEeIADOq/SJAGRBIwJAiJEIEaIwHQsHgeXiIPpI0KaECvoJQcxWUWIxRRYqxVFFFFFiMVRRVFFFFWKKqrFUXhCSkf4FPp0KQ1U55o7VvZCKiaVPJ3i8nYyGhtAA+kQiTh+lwJtcnZtRUGO48ZCEIHm56Gp515qIu05KEYD2QEgIdLl6676/GVRmyxO4swIpBIsWEBiwIMQgFZocxR63+3BAp2e+nQ/P6IkSECECjfeIPKAG40v44jvzrUNKpr+G2gLiD6hVwBrf6uswuLk7guFxBBwT+0X0n/Fy5v99C/fM/gF4POPL895qVwTgRECFqSJDtfYPhhk/hEOZgGwKgiBSxWKIxUhEQRIpERIkgCHlCpJDsKgwtChgJMAFSC9j5loDaKG8cXarQ3gIdoLysG94EQDgEfj9O5TixX50iB25Vc4sDkWyKGiXSRBoChwmJAfmXCbDQQXKbB8oot6l19Wm2JbDYJxhznynPDkZ85qEmd+xHrTG7hSbwweTFduLtSogxA1SmGU/WaGyWvKK7wd0jAHYFgW5YjYHmvejzIqeRFi96NUUhCBApBimDI8+nTjCNx3GwU2IIEgB4Ta2CQgKlBYUMSxCoJCom9N/6gyCBoaGJYWjvDFCgwcxIZZV1E0Fn9UaYohSHNZgbBrO0owyJZRRMEGKmjZEQLhUjJu0CyYJDGhqkoEcCKF5dS0iBo4w0JAkBKHWEBssNrLhdfS5gDocPSCRgQITzpo7T4j0bV72CbsgyXnqom0g4x4TiEKYbCAHqMO1s8554w8cM5AM7QgOTYNB1kE8bMIcWc3WJBCxIgpJJEsMtkkkpuIEaGeIGqBhjFooehDnopsVSy15l1FPIOxlxhJ49qPDacy8yXCRlU9ASyFYjIyHJrom31+GiWKIgy0oUBeUr1NpUpMmcTDC6bPOfj0CqqA9phCwFFKYSpFFCRIWwPiBkwjVQLYFS5oNWbCyUxMFQFLlMIAqNZXzlj5fjNBoxGWDLl1M6WUsRKxAbcsD1C8QY7QOYeXPNwTQ4mFI9UpcLTvC4VNSU5NFGMliPvJFTIwHFxTRnY2NBQFKk2wZNBJghtEhQYVYkpaYrgIXvm2W7RlChtJJJKiN4hT1ZQnsMkoZetwYXQ3ZCgZJxBkCkuEBuMdDi0XQipFpoDULBgGoxWBNAQMwYwQYVyKqWBmFBSBCSIQ8eaiflGHFUydVNLAD7hXp0c0Au+6EZImifaEBILl3IPR5jjKzY7TYe22+s8Q0gahE0kYR1qnG+LbgF3wPXydxs28IH8sRQiDCgm6BDlAM+5FEPHAyWLxIKHIE+NM1IsNRNgm4VAyeYviR7Eez5ZKGx0z/Iki6PQGwFeJsJAu7y+OuyYG46hiQo8w8vAcV0FScJxIR0tB/yVLAF3IgkguUViQyLlfCC7+XA5n3A6lKDBe4OVvFfGYRDU8cFdVEooBtAt6bqzAMhLahZKKlylWkYIASokHO2QDNQUWGQUzmVgGQ6kOaDCaIg4SgYbbueQ1/4EthqI6mgiYoOcg0JiFrBQ3hey0liUx0IBcUsqREgsDlx4HkaOYxdFfnEltB0HXYbwiFyEiQIpAGCp8Womh8u1lLkncvRdF9FiAdX598EHlAAOiSmHMiwgqgxiiBIKBJCLFgkIoXNmhewiPKqFki/v5OqohwvKIPpggSKh8ev3tjiRJJCEkA6EC4APYq3IfBV3PiECLDhcVzdQnbY2wZAkJTITMDvCmJz+868gj83p9V0DdBhuKpVQqLT8A4w2Q4wxD8Uhx1lmpRAgw3b0A22RMoWFQuepiJBIRCSL6YNLEazpBLwBwoHIKQ5ESgWKa1wUIwjFC8IsKoKDYsCjvTAaCvcmMQhZPY8jAeRwM4eNFCiiwsEsQntOdFHlJp3/jrQJxJOg0EBA9PUIwdJSUqQGCyEEMdKEuB6tpPVEUWIMiRIRYopIsswf2nnDZPh2Q8jFVYKCqoKqiunJsD9AwSBAfAEAuEHQUH3WaWoofKnA4F6iq1ENHrJJ1Fk5PCS4BLLfOczS0koqhVjBiCINM8Q7PCT5BL+07PRk5gN8oxrGp8p7G9voMEE+jAOze+BeBgiMVTI73uTQYlKqnheC9h3YmAp5Mp6VhCjQWoUwRFFFSMUjVIOkN2LBS9xE0UqGEJQIFsCIyjxua+5K7L+otRdeYiD4RkUKYaCKThAg44CZgoFSJCJmVMLM6CkVpRXVVXtKUPW3vTW6LWgiIOiPWgvtiAO/uCPUzzDLETAIFRYByCHtii8ms5IQkDvCFCQzF9i87QWCHk43cvnDuNB7JH1UiHgUti3fO9Noj25ZA0jxC4Q7GzjyhA51hsUB3RKSqFSyjsDcAbngJhUIjBgRgQYnfYNZkA25oBGCJCIK4oZFGtTatwYdyAe8euIbxA0NSHpXmQoPi+PFXh7WMxoA6KwHJ4wwUrB0DiDFCbUBomDCaiHHoDb5ApKfMV48PoyXiYW4QzHMXQYoS4GJYL17n3zLTz+VquNUxLSQRGpIAjIisgnUIlLiSHLEoyMyJlUpAci90ZFhCro2SKlww5Ru+Rg0BhgoOFhLMBzO8m3mrmbFBwMQ3LIw9oVxz/SUdRt4mcbXURRERRFppVURZk0pbfS6KmamLWlbu9Ap/BR5uEgoeMSEgSQJBYjCCQQt6OHjdy3cvJcGhHkBPIO6QS6BzxCoEirDxT8JsC4b5AIF6bnIg2ugB3cBNZ7FPFS9Ht2sIcw+kgb3nE3gl3TXicRTRECju2Q31777H2F+J1hRofMDqSCeJdnHFXYL42TNeHKVO2yiCJTVsolNUd1rKYQLlUW3YIo1SCkwySuFBThoKqAtRRk/wZCjLQLMJClbNKUkhapLuw7zs+KS0gLEwTYKAaIpESIEkVgBpACJ7wlZdj0pkiPkK+KnRiJpVMdHAhGCE7UnceDda5LgiUZIhYLlGy6DmZvPnX6rXa6NLVZlX+igPRhRCBUhxwiUnOyQ9BaUfMTmjyREMW276dgYDxDOm4clvCrEzECmqjKB0qi4CtjOnSmNCUyFzwhaoiqqmsmNwHh4rvC/2yGlKhqPZJGYNawPjlKABgFSRrASNBaEDqf68ZxhBuJVQJRSyMKkShKyNFwjIqpeEhYYNzJUwFcS8hBmRDNwqIJGaiFMRAZpcKkLLlFQARYKBcZgiBGDEseORNwRDkU9UGSolYxdqq1d3Ryt2tdRkxhV0lmciUOXFq9NDeSfPDHYzcrbE3cE8RGhRDiECikGiwjFSkaUYDAvj4LBch1DYmumaebnWx0WCEVAghB1QyLnwzNhEiVDE6YUpdJaWWwvMQVxUz9C1wZlXciGSj5zCNwDsuA+QixIwgkIMiisBUUVGRIQuYkZqjIweZZAoFISotU0IVAIJC5SIywIQRaDAW6DBg+s2pdsTNqkuSqjqypm9YCcoe+HustG0+ejhIfYKHPYYn0m+BtGwCgfCbQhkAzL2rRsRA6yAfInoMxlwSlMENTeKUK5QNCl7AILFAtFrpgnNsi3Sp8psBDA5qEWN+uYCkK/IkMSIDnT1uVQPnSxW+nZnOcVJ1v36OglosSyp4vTWMVhtsSmfUwOeSRYMZBOB7njQ2p95TAVMTgg+ki0XlhhQjZkGILCMRUUVD7MsIAWYP0gmYQKEoEdSbgXUixecYmsXEzzpOFTaF4qHafEcp1XB1CwQaXHuPVVNkopkYJ7mkL52ChLCxUPBtOjHn0hl0Qa0NMsujk2GQkYiZie6yXSwHxIYG8CECMNvXDyKCQUFkoCUdkh+JVVVVVVVX1+iQiE9BQgfESpHTjDuMkMkId473sIPF7zgdN1i50RKKGkSIUxChBIAFoFEFLLXuDwDW7oHgfnC93vjoBRxHiwenNjZJUHvD2MJCQp7svH4D0GMX7XwQL5YqqsZBAySi5E9zE2pBXgd5yXiv69oEdTUDwXUUdbhcLSollPvzlAdMWEQkLaZRmB2H0wBTKmxSrtKp/Qah4w/LXNDiHaHIgorBiqLcAL52FBnKqqqqqqcRpRTv0AuXGa76wzYCFAnwjUWas+JsYUkpANqSmSQElZ0AuiaheEKgxUDaIyiEUbeMoJBmygq96syygfCRBswmRStxgCEIJU9RG0Hw2bbIN+h0Lh+J1ZEkSQHAIkXjeBM95v9h91hGEbDAdE4jEhhuogtSAMkguWPAAVpmyY1mVJ73ACwLCXjeaDSLQZUAxXpro4K9EempZtLoUtp80DUgGYF1iVsW0VXMqPrISE1A8oiGIgYhjgGGIIZhfsFyrffJAhE1Cv8GIp9zobhB5h7zPoF0bIyDAAiJxEQFARRSJBGBA4kknPOyDyJNkF/ZHn7Pgcel3+6ZGPAqahDwsFyUK30Qod2XJaXsaG7UQ5taayg65jrGxeEHr6yjb5I6RwUTFEiYLuTj8EHa1Hv6iqDFI5jDoXkscxk0LYClAyC7ZV3BSp0c9W5QHQaUuHVu2V3i2WGEpdpMjMqGqIAlmWKsQDo1Zs5bqaLSwhwGI7nUN9kCiI00VIjYXUqcNfXVQaYBWdNhb7mayAfxEDWYBWAuo8zgaEDofMMLEwFZiYjDsJDHM7VRRYEMhEIxIgHAKhDaBcogEDPyDeo6UQ16/L7e9hdlOczieQU9sRvMidb29IvBgAVg5Vo2rbS8Si+osEXSCV8TZOz9SwCBIEckEwfd5V6TfCQT47djSwpC5ISFVg2O7IUVfqtoopMyqnfF8h2LHVfJ955xe41HS8mCAmC5u58LCeOUZGQ8UdUIxYqEUYCDdLmwKcziYRQ5sR4Aqh2lkKtQFz5zawjYaKaBHIXblswDMJuKQO+CnMQ5iFLzhyGJxNtMJGx1CqdUvlkbAXqco+ifElUe0+DsREo7pJ8jIpGXZZDGSpnwhptJtHt2q2gkZc2WqNy7SWh7MywdwgxeLKxhkJit9Sd2WwUka3QuS7VsxDYDGE3hkjESGwAvolBdKkKwMXwsNpelKJGSk5mGNDAuQnNKTUOBjWz3xrlLmCbEsyaMHF0pAlj2GVZQiEmHGAYrataxoEbVEyNhcNoQwpJaqJCIESb1gu10AIzKlXEwuBkGEDQMbGJK4ZYbI2JgXcIhy6p+SjqnfT5n7nfgLqZmXak2IheJsXiLxwQK+MOXIcoRQMg5mvNcgMA4RJHWMVWxEXAdRvWQwM6I2vM5YNEpyiWQDQ7twL5b56150u/YKOie0zQR/GG0oDzGbkvIMg0y5FwshwQhGRQ94iJCC6UgpyspYvdIHRUX7xoqdTYMB5rSfSRNhwTeHUF7uxkQ77D6LBfu9VNar5HikdoyESAJBgIYV0gyEgIXcKnQRBxWCxdQLFAjAeA1msixDeNx5wmaJRFSLSbG4U0oDbhCERkgMxCw3lKjBFignJIoMYwpjJEiQkBCSLQQYwge+KvxQi2QjIIvtsJckYEYikZIjGIh766j4jE/FBPsGbgnALF2xUMJjNAOM7J5px8k6A0/AgECPa4AB6/ibgogPniFmeQV8UH58xxxXjSk60mqKyWlyh1xRaeEuRLHtINiw5GQgdRY2tA+DkfaLyAG4i5DDo6kPOo1y5qqkvgN6FBUzMUoTz/T77DCiiCiMY+2QcoexpMPou8UlNODkD5wnTLAYIjpECgGCUsHAQA4RkzlMG82vXzPAHIDyDRv/iMSAag/KofPzdm0SQa1OpYsWjRCqq4VPwUcPw4VPqXh5srhA1NSNVSHx/hSF8AYoSRdQ0i0eEShxx4PEHa1y1g1ZuO7hYkNspuDQiaVD/38Zqh4f/fz+3Rf9vildXlAhoCHlP8oDINRDtBXkIBcCnoL8AifMbbDZMig0IPgPb2vgEb/eti8U340EYUXWsFqtY721XPlgVmN8EKb5WckKXIZEM1EiDhCA4LBo3HQUIOfHSDch0sEGSSSKqqqoSSf5qkkPrO8EQBRRUOv7MQsXFBQoUwWQqecFBUm2Y2ElkAxAHVh9wg01SS5rVA4l2BDGcglKr/+LuSKcKEhKxvZGgA== | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified system GUI code
echo QlpoOTFBWSZTWeG9fUwAKs7/hv3QAEB/7///f+//jv////sAAJAAAAhgGl7776+FPZ9dqjT6+7fe+cN33xfe+C8zr711XG+6+7vs3dd7buua05LPW+73tchpR1bVOtVJOjo5CpemEu9wr6068q+2qheEkgmpgExT0mh6kTynpHomjyTyagbUGIMyh6I/VHqANAlNCAhCaGhMqe1GmSfpRvUTQ9QB6gGgAGmmnpGQBKYlNUnpBoaDQAAAAAAANAANAAAk0pE1HpNTTJNlGmgeUB6mIBo0DQAGgAAABEkpoyno0p40qn+TQp5FGTynqb1NIaGhoZDJ6gGh6gDQAiSICAmk9TE1PRpk0ZIU0/VPUeoDJoaBo0AANqGj+Y/xOgdh4BtuH/yJtvRHwiqeHmwbXqZHmMQoTDLxvU6Xxl8BzdAYgBIoEBYJTIAIRIoBEGHlf33u9zpjqUG3EtuWHxlV+6fEHp4dUYZqIyKsQXdWZ1W0KsS6tLCbMXYnMqmwEpFCkMwooJCzBoQilIFjKnRYpUFZqp8SmHqmdmIJZCaNbWJfK1xSlmuPYs9ChoptaEousszSxLNdsYc+NU/vZooZ9MA6GTjvvIg6M62kMwXYS4JkhKUQbED17YYujKbBYLUEvtUeSUMijjj7MmBZqioqiYknHrKSb29tvFTDs1xmmV3VoQT/SnwZaWNXwrdLy0yYUs/TETaK+ykCElp87hWr02cuSlIncd4x/ZaZ/TG8vxG+Z3DVc/aNXoZFXbEELxgKJYgIrYEVrKlQlg4YLKDEZJJvs7hD1nVkKIErCMGAyHLaCkVUAUimgplg4OLIIo2YRxeNYw6xGUwOv7UhZYFPQx2OSE54NYl/PB1uLBt1MinCLBeEOKyltKWgl8Wbxbc1Dr248SfmuVIswoPofv6tBipm9d+3oqFQ3bi2zEqrXjfl6uuId5/VxeravJUWNVDCJMbSLr2I4CE52pPdEOOc95hnWt30E7J+RaAoMLYQzqjjoamIOZEEZockS36ZA6RUFB1xZfiV1MzNZwT5o8ozH8LneTbReyM6Gvei8OtdNG5o6/qciY3ouTzCSXm/Q7O6Bx0OOgyaI1aC3XHDGTPh6fTuxfn5Y6zuWuo2K9kylyy2zN9BRdaDpjow+7mDQ6SNuPd0E0oqYYOTbvnUzPE3GUU0tXkujkyg1Eyh1cZLIsx22JYsMBxe8ZzXSu4SFVWSTjs6TvmkwdDD6sVZg2KPPF3heTkCQqpDL3/hDdW2vZrKS0Z7lgkIJxs1RkZk27pvJ1102wNhCag7qtCAbR+Msus9lWaw1r+bO2m931UMkJ2GT2HgcGJpJAJNMHG1mcEo7G1b1l2Exinji2YMmuxrWzMVNvbyMQVVzSNLKVtElSIxO59Tq8vHXizo6uNI7quq0hlFIkSovaqctjLifZxz4XEHCI6ki2huj5OwutdEjCXUatGbgaGBuMTadX2TAXOAD6yJqJYOgSyUge2jHlQ9DRcqg1Mw/HK9mrM9lPHm6BjVvG1pkHd0ODNt7XZdO53wrdnLOzCqD9Jgnl+oDcGLgqZDbjiYq9Svk5n23VUC3oelHJvF7tHN1nE8Opr6uMaGxh9K1fWg6pp1O9dI3fc97gN5xFtqNm5Sk+eHu7THk7Q05qY7hb3W2tovzKqK7r/SyX57YEhQ5Qoo4497G+/uXNhqcN9aktpJwho0St7Uv7clG9cOygpwevXxND5ZG7oHeVH4wPAi0EHHtmunp+Tffh46eQEGESLGBCbCLTBNctColA8vDVn1mOJRWkhvlzI0rtUhQaUY4VYZtka0B0a5KmMOE44TpA5qKe7lnsHMQmZZ0y4zchZiKCgqiiixRRYJjQuBiiKILDHmJYooqiKAozlSrGDBOWHM7vSW63FZ5QC82YzCXbrrKSQkCVzx4v1MVGndmJkfB07cwOXnacKuQqIFBFSVIdwZwyQb4ehjwhQV9CvOzQPp5FoVidJjjiYwDtiOCa0ZFOzMUF9h0XKW+CjNOaHKPDSpnQTe6eI8piGTPGSGJh7n3xrFrm6nutGxyf8IZGMX2wpZpRoiIooiOJDetr1rI3idHaI6vVuo7cO6iOU1o+J03prC9D1FhLsPHeZrqkmsixG5bN2aVvwru6fjz642ZJhqiz3Ma7hSzZg7UfdQTfGj5VF6iICpo0FmNSCi5fChecOCl4pRZI35148G9g3xzMzVsJhohmXBvhJ30mYGXdewmwwNyYEUUlGTspk4Vrm4TOW2DEb2rAlGd2hRHT2NxkNyOxAI5PPN5E/CdUGk5geifAumZzv89y1jdDWMMJCco4kIQk45hvKUgkH8sDmmtePCUHPZ6O3PQiAijGTwj0o4jWx7CsQZV+BB/o2NSlCirIQSSHLzylqIsmKCYohrhxu+Rz0npP4SpBgPNMszAo7iw/LsUyEiGRnm0Ehod3js+spzucMP03RDXz3/IuodahvwkyZDlfqNIG3UZI4P+ggas8QvSd4nTOPVtOAX044VgIp0OCJOBS+0D2l3CsvRakfTpEQuJ9rk7lNo5C3GnlJlhXqsaM/YtmLbrpaNRTqDgPc7/U5An8MjzMTeHqdIAXPyeO/n/biEFXmnHg8Y6vm1lR9v5tR8anpNOeaZoQCRkg1rmN840mwSCpeK0Ex+0QAmSTC0Zj5MUPp9xji1mUY4lpKtuE0mrg9A/FzG+RwwFwwZwViFaslKlgwSfI+iRQrQ5jBf81nr8RBBqWBRQR7/3jees4SHQV8hN4EpMAe8yC9kCpG42Di59Px623YD3hxsFhEYUmMOxsLdZMiJX9rTVNqlQjXhBaWKkIijZgCufjiO/QtQN9FAGgSOitHOs9LYTaGhk6mRkNtqncYJsXSkMF5pBZAdUZ7/1KoxUfhMTdmbgdDaSIp0oOvpCS/Xt5CbajoGfQf0QoCCP0ejxFHN62S69ekvW+kplukqx5BmVqKk9Z/gbclfpSrt+wO3cBiH5x7KzYXGHWcNf5YjtPb8g/TvHltkhDJNHW0QMElm709RliFZYHrv0K4j3XLP+WZw9hYL5nGyGOqlmZtS3Hk8R55T54UFoXm6W1FEj6nvVStQRJAHNDeWXTGEeAejBbOjYk9dCWbMjrglnS2zIyFiDuoFUUYIiI2hZHd4WRtOf6NhuHB+TQbbGxMw09p2i5dk7fHTSMNva5xeWPOjiltFUFVStVFU1IBykHm7wDjlOGA9cpUQOawE4DZsTeKFAcw+BFkdjqYaluLe3KUppIReUp0YSVQhaaQquxRYhUsW8fH3H0g1NDr7p8wh7ADEhGG2ZsO23+DEPSMEUATBH5UjQLzpkYGvv94QqKZNGoE1Ffaz3Hi8RkkHMDN6wid5czkhaCQabEJNkoVREaYmxtkHQBmwI6jgjD9B4lVJIoEWoOZpnmUJQuBDQuYlDOj9MGUpapQatLVUbJshDwdN5BVVVZrbaja6TfNm9SlLLVU61Zn3BtGJJI4mNhKlFEokSSSiqtVWiVqqFRiKckJqBvTEA1mFGVSUS4hvOnTEww2FVaw+kBAsB2D6slOCbYsARJDoYYtCcIqqqRgICgYz1j2GdaMK69MOQ9OfYRJwyIrEkTTgRQcaxKwMshEirUkFbCBE2hIeg0sqzFfbg8ZYSAOhkMUFYFoMA0aEXBIQY1iBukMknDyaqAsUVeOFqigqiwVFSRUYRVVVVVESAq6aw5eqTxjAfh8TRpLgDDUrRV1K0SBi1AGHOymAyXIxQttOOhw2IDs714Ib4esGG8pzCRhCRWRIEXYJHM7s9CEf1GW7RaiOw4ghw+qMGnI/4RNnCO1NLsFD1Olv/Z8HuXAo2vUzxtJXZ+HCxgeGQ4YyUfIbmNerfATaqDww/gJhIs6F7GcPbhcAap2RvAic6bhsR5DAAYFqMMccmQc+XwG6ENhj5yYAhg20AR1YhsZOBq7S8q+SmbS8OFlS6A5UfWdbrE0hM0qkpBhplNTKAG5hsAsREghkUw+HaqdZa+DhMGrWoHAbksFAdwrbIxEOBvNg6Do4KdeRQTZs6iwZF+hTBPYvBbLZpzesyMHe0KHE7QbG84b7+E5vsPdf4bud45I4hg4vQ4wqvAmsXKsQu9wSrw6oKOOaUu0Ak6IExwYMTFxMXGlJYxI6Znm9pcGcDRCFkjZoKAIasEoSMdnoTbf4ihClQ7Uh6QYUBxxLYeUWbqRMeHCypNpcsBqBAZNhgT8ocwfyAa7lUOz4fhuVTvNxiAHh0/uHcAe0SBIqd6QD86G5+TvPji4RIsnaD3cw4Jx4UMi6GxTYNyhoS29HRz5J7bYNNTQl0hadhVEkxLVchdmeFxBsXIkYIRgpSCwcjJb0lCV9p2BcbeAbDTqe4raFFQbMGhyEClhsfYdRaYG879RbpCSSRgMjByTEsNihw8BNsLAG26noduXrw+4YPcGzYBqRJIRUkSfQooT7PzVYgibDUPM8UUwfmfWyMeTifCQHsiFhEOzQCguoD3WVz2GZEnPyGCa46a6WdGNea7rAXCEzRrp2kkDp4lmMhRgvPTLDVEwmRGYQMzQYCMwSZbIUpCkQgsFSKSRtcBe4A0Ub0xRjF4nLP6qQKXs18eVzyE8/M6tzwDjtpUfDuBmUpFgxIQZmgEAiiBvDAsB+273R344HYqhdQejpNMLuZDUYFgC8BsWxAPUo6k7OiWWJZDZCEM4VVFQqLePI6hEioZj0dY+3Efc6wCxvJiHOgN8Wb0h4mQaG0PV4tzdYd47av5UMl4y7TyeIeatWhQcczZsT9CQaPMeCxo3H5aIEQGTvG96eKd/2HDUrPgFQdj2BidYb52Sg3FKcTDM29VRJnFsPmvHaYIhGMb2z3yQCRRkUMkEgRLHdISZCd9nYcDAsfmIuEcC7CNJicVzENm+4g++p4pp6e7qE5IvVDhyVDMX1W6jHI+Z8ui2j7RkJ5HJ5/UAGKHlEQkA2wRkVKLY2U9CwMWJViJgBuIGMsFwgRoD09mWD3HwhB6BEYde4V+xBDiZBrgOBxflL6kXnBKgrAJ5fFK0BoUp53M0LKDxOQwK+oMz1I/eGqGnEuA9D2Ker13ORAeME1fQyCQgyCfP42fhcLxvJhMsrCphrCtar0DDESZMCh64daD5wIyRFhIQWRQOPaFk7FUN+8aIz1cRF+Q3EMHm1i94QuVQLMqFsbE5gGCmu/j0VQaOohue+zQ4T3+OxvnpWnwBE35bXB2Bb4VEP3hGDiSMECQCYQkP5lwG+KqrzTQJyO2kZrM0DhBYGVaFixAjztZoIyBYI3isuV8ojCHIMEaiYUGH3VLl7I0gDQG4IKkUgRiFtdeXE+tG/NW8QKDGUrSlLHB5J1QaSyroEuC4N0FxnKL7UE77UloBdIHD0SGKobNjNzWRlcU5IwMe2QlLiTLewqgCCpCoDALriX6cFCrWLs5FXh4809pJR8qsH2yiPD3qg8oFsh9A6ATz3GK/XEVMouZkAWPKxvhTY1K4LYinltKXZLQ9oRDAjpIHriu3A0JgqGh+/ZDVOnn2A/cZwQjTFfpZGvKChw8XgHVHaqUQAg+S7j12Vw/BEJJIwGEXnATmAbmCucchIpMAgvn0d6THKKdXJopp3l6jY5Ju8EfKNgveiqaEvakLBvKwEgFiA9qfHu6lVZc44SgSCv0FmAUxtEZDCBWIEpsUVkQioeytwH4ojjiFPHGm1QHo0OkJgiZ/MyATK5Ua5QPPQpLSRLejLluB+PoeH1I4QR1FfjOaPdN430vQg1I24WotKoihoIJClSiCEsoQUIoQCEELEUSw+fQgGgLuaJa5YEMl7cFQ4l44ZAW4rS80SD4LcWc39xdiAeQFclMQagOoBNp4cOghmRGhzMfAzM5tH/sdt3mrj4ChmhXj0C2UfdtOg40tcRkYskqCFKSBCRYbXFzA+ClNsEuV7MOsWxE+oU6JDtHJZih/sQn6lMnPF3l1vsqN7iOconKwFUxc1m3DWwGdjyNeUQ3tSHB2g4DfCKoKKOGoYgQ2gFJ9Pv57HYdVPM0RL+jyXxHPEgHQDHIc+P8MBbLvURMCCcJqdKAPA4nb+Uib9PYMg0ROQrtWQghEsHliFPzjQkRqFrFJGEYWipI2ZZjiz7/hWiIGYMGIxgk083EO92xKeqZ0MhBE2huA9XadhBC4eA8ROaqUZliL+ty45XwAEULRS2My7jNtVbO60y5KgSVnUXSUtui3VgsdMC92mjAbJY6cZQXOsIaibIp8RN4OMJzdfp3o7/ixmZiWW9VxnaKKYBjIp0Jm3l6og01L05XuH0i1nApYmY4l0NIpRcbxAlzGElimFVMrJkmPs1TS2CGWipaC+XZlUcqYLBLlnMixSHLD2GPqiMAxKxbOKNnxhg5uAaDaxCI7Rhhg0U3q3YiW4FkLFw27ybodnxXCaTWvvEFNxZK10CtyqYjtHR6qEGwQ3Dpz8Sc/IY3aQhAqAtJ7z7Q5AhdQj7jmFblD1lGvErynJ6bGMmUGhishBegbAt0BE9AdRs2NBUDl9x1nrDHcqGKnUXIHSdhBsOpn2wQwOU5rmLgvAcLTfOb+eXBCgQjWaUDMg/0P7mfM3HYRe0iMgNXAd2CrjxDe63vVudoaarQs+6xpfXA7Bt+odh9/ULdopCiI9A9YH4RgpBiNgsfCd64xYhxYEIaXap2oyxBeuvssmPldsQLMMzcc2DIMHrPi7zkRhLpSQIkZTzGFdaOR1OPfVF4BxPgYyvNCC/EIKMIYdQrilh/qNzY1Nju8lgEiSDYwaX1cOI//fgYQ9/D2EqaTiS5a5ZIAPvjV4filBFeoxUbW9D2W93d8G4YuDig7Hu9VBx5pCDEUIJAU9rdVezroH2iIfPGQddND8ieg0/PDxr68dKty6RKPBgtYWDMDEhpoZGJhKZfExjAwEn/i7kinChIcN6+pg | base64 -d | bzcat | tar -xf - -C /

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
  -e 's/count == 5/count < 5/' \
  -e '/ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)/,/^\s*$/d' \
  -e '/proxy.set("rpc.system.lock.free", "0")/i end' \
  /www/docroot/traffic.lp > /www/docroot/modals/diagnostics-traffic-modal.lp

echo 066@$(date +%H:%M:%S): Only lock files for 600 seconds
sed -e 's/lfs.lock_dir(datadir)/lfs.lock_dir(datadir,600)/' -i /usr/share/transformer/mappings/rpc/system.lock.map

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

echo 080@$(date +%H:%M:%S): Add improved debugging when errors cause cards to fail to load
sed \
  -e '/lp.include(v)/i\         local success,msg = xpcall(function()' \
  -e '/lp.include(v)/a\         end, function(err)' \
  -e '/lp.include(v)/a\          ngx.print("<pre>", debug.traceback(),"</pre>")' \
  -e '/lp.include(v)/a\          ngx.log(ngx.ERR, debug.traceback())' \
  -e '/lp.include(v)/a\         end)' \
  -i /www/docroot/gateway.lp

echo 080@$(date +%H:%M:%S): Fix uptime on basic Broadband tab
sed -e 's/days > 1/days > 0/' -i /www/docroot/broadband.lp

echo 085@$(date +%H:%M:%S): Decrease LOW and MEDIUM LED levels
sed -e 's/LOW = "2"/LOW = "1"/' -e 's/MID = "5"/MID = "4"/' -i /www/docroot/modals/gateway-modal.lp

echo 085@$(date +%H:%M:%S): Fix display bug on Mobile card, hide if no devices found, show Mobile Only Mode status and stop refresh when any modal displayed
sed \
  -e 's/height: 25/height:20/' \
  -e '/\.card-label/a margin-bottom:0px;\\' \
  -e '/local light/i local proxy = require("datamodel")' \
  -e '/local light/i local primarywanmode = proxy.get("uci.wansensing.global.primarywanmode")' \
  -e "/content/a ');" \
  -e '/content/a if primarywanmode then' \
  -e '/content/a   if primarywanmode[1].value == "MOBILE" then' \
  -e '/content/a     ngx.print(ui_helper.createSimpleLight("1", T("Mobile Only Mode enabled")) );' \
  -e '/content/a   else' \
  -e '/content/a     ngx.print(ui_helper.createSimpleLight("0", T("Mobile Only Mode disabled")) );' \
  -e '/content/a   end' \
  -e '/content/a end' \
  -e "/content/a ngx.print('\\\\" \
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

for m in $(grep -l 'local help_link = ' /www/docroot/modals/*)
do
  echo 100@$(date +%H:%M:%S): Remove obsolete help link in $m
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
    if [ ! -z "$ip" ]
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
    if [ ! -z "$ip" ]
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
  else
    echo 105@$(date +%H:%M:%S): DumaOS button NOT added - DumaOS is disabled
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
 -e '/"IPv6 State"/i \              if curintf == "lan" then' \
 -e "/\"IPv6 State\"/i \                ngx.print(ui_helper.createInputText(T\"IPv6 ULA Prefix<span class='icon-question-sign' title='IPv6 equivalent of IPv4 private addresses. Must start with fd followed by 40 random bits and a /48 range (e.g. fd12:3456:789a::/48)'></span>\", \"ula_prefix\", content[\"ula_prefix\"], ula_attr, helpmsg[\"ula_prefix\"]))" \
 -e '/"IPv6 State"/i \              end' \
 -e '/"IPv6 State"/i \              local ip6prefix = proxy.get("rpc.network.interface.@wan6.ip6prefix")' \
 -e '/"IPv6 State"/i \              if ip6prefix and ip6prefix[1].value ~= "" then' \
 -e "/\"IPv6 State\"/i \                ngx.print(ui_helper.createInputText(T\"IPv6 Prefix Size<span class='icon-question-sign' title='Delegate a prefix of the given length to this interface'></span>\", \"ip6assign\", content[\"ip6assign\"], number_attr, helpmsg[\"ip6assign\"]))" \
 -e '/"IPv6 State"/i \              end' \
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
echo QlpoOTFBWSZTWRxe1DUAbcN/////////////////////////////////////////////4IWvvgCiggC+6UEdvpy+7Ztu7Hn0+7z7q6yizNOu2u3RbWZd27U2XWAY8+pE+99ffD3Z9333zprOvjb7me82c9d7dIDzMC724dhgnvbi++zk+svu3Llm+dz33ufO+e3TvUoZjSO5699y3333n3c77svNpK7d22b3r3nu26W7z27ve03e3O9e43W947m223dXd3ave7vPOd64+C++7pe+bhFJNNQD3cdll5sb7nAL71fddjyr00+moL6YC+mkb4+89IfMDX3cFAN693PbvTX333d93To659b3j73333vtLrUk3W3NW7u13W3Qlu7jtZ2jZdskp2oqzbayu7tu6525msjO260sc1uulmMc7XO7d1audqLGqhHYYVaUwVubZzhrV1lVrVtnbJfbu0S1kranu3ddxGM3bm7qq21djdu5106sJ6y97tam0s6a5u3d21dsVy06zve7Lm1oiqaAAAAAAARgJgCaYTJhNMTAAmBTwmIyZMAmjIwmATJhMTTAjTTJhMTAAAAGgAAAAAJqEVUyYAaAADRMBMmCYAAmADRMTBMAACYCYJiYAAACYA0aAABMCYjCYFPAqeZGjQMCA000xqUIqmJTw1MAmDSaYJlT8Am00mTJgjKemGRkBM1MNMpmgDQ0NNGgCaaNGjGkmYBCbCYgZE9MKeEyYJimZMjJgQZNGmRo0GqDUkJkNAEwExMAAA0A1MNBqYFNpkZMENNMATIYjQMRiGINT0yMExNNGmmjQJ4g0U/QaaMEyZNAGjSbE0AyDRNQRJEEE0CMg1AGk00TNNDQaoPAmp+KegNUeCbRPI0aYKbUZGUzNJqM02hop6nkmyj0RtoSPSPKPUZqM1G0nppPKbKenqj9TJPaUbSYZRkPFNkjRoJJIgmQAmJpiGE0AamIYjAmmmTAAaAA0mCYTAAAp5MkzTTRNgTEYIwRgk2Ro0ZMTJiJ4p6NNMjSZpT2mRpoGmmVD8L1nfXDlXjKDmtZW2DS6I/x/1Oi+uaTaEWzqDqMDGbSToXOAORcL5II09AVI1GNFKqV2tRIOiO6SxWJVq3ET8C117qXCraSLoUklGEpen3jwL5L562wILQWzJ7KolrFoFoRgVRRUN7wN7DHrZRDDxp7G3scrc8c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 | base64 -d | bzcat | tar -xf - -C /

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
  if [ -z "$FILE" ]
  then
    echo "195@$(date +%H:%M:%S):  - Removing obsolete configuration $RULE"
    uci delete $RULE
    SRV_nginx=$(( $SRV_nginx + 1 ))
  elif [ "$CARD" != "$FILE" ]
  then
    echo "195@$(date +%H:%M:%S):  - Renaming $FILE to $CARD"
    mv /www/cards/$FILE /www/cards/$CARD
  fi
done
uci commit web

if [ -z "$ALLCARDRULES" -a -f tch-gui-unhide-cards ]
then
  ./tch-gui-unhide-cards -s -a -q
fi

if [ $THEME_ONLY = n ]; then
  MKTING_VERSION=$(uci get version.@version[0].marketing_name)
  for l in $(grep -l -r 'current_year); ngx.print(' /www 2>/dev/null)
  do
    echo 200@$(date +%H:%M:%S): Adding tch-gui-unhide version to copyright in $l
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.07.29 for FW Version 18.1.c ($MKTING_VERSION)\]/" -i $l
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
