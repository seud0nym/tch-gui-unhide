#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.08.05
RELEASE='2021.08.05@15:58'
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
  MODALRULE=""
  HIDDEN=$(uci -q get web.${CARDRULE}.hide)
  if [ -z "$MODAL" ]; then
    MODAL=$(grep '\(modalPath\|modal_link\)' $CARDFILE | grep -m 1 -o "modals/.*\.lp")
  fi
  if [ ! -z "$MODAL" ]; then
    MODALRULE=$(uci show web | grep $MODAL | grep -m 1 -v card_ | cut -d. -f2)
    if [ ! -z "$MODALRULE" -a ! -z "$(uci -q get web.$MODALRULE.roles | grep -v -E 'admin|guest')" ]; then
      echo "050@$(date +%H:%M:%S):  - Converting $CARD card visibility from modal-based visibility"
      HIDDEN=1
      uci add_list web.$MODALRULE.roles='admin'
      SRV_nginx=$(( $SRV_nginx + 1 ))
    fi
  elif [ "$CARDRULE" = "card_CPU" ]; then
    MODALRULE='gatewaymodal'
    HIDDEN=$(uci -q get web.card_gateway.hide)
  elif [ "$CARDRULE" = "card_RAM" ]; then
    MODALRULE='gatewaymodal'
    HIDDEN=$(uci -q get web.card_gateway.hide)
  elif [ "$CARDRULE" = "card_WANDown" ]; then
    MODALRULE='broadbandmodal'
    HIDDEN=$(uci -q get web.card_broadband.hide)
  elif [ "$CARDRULE" = "card_WANUp" ]; then
    MODALRULE='broadbandmodal'
    HIDDEN=$(uci -q get web.card_broadband.hide)
  fi
  if [ -z "$HIDDEN" -o \( "$HIDDEN" != "0" -a "$HIDDEN" != "1" \) ]; then
    HIDDEN=0
  fi
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
echo 'QlpoOTFBWSZTWTJZhVIBeoP/////VcH///////////////8RVK3ar6DUC5lU8KVJ1DcYYnxbl19i
QO+gUp1598VYx1HZupgHEAoT729eoBe87o0IzBSIBS+2TixDTQddzVE7BlQmu6zQfWpUBQqgqVbY
7T6+jtB159Bqq7OnJbb72OV2C2Djpa+i63t3aUAldbCO7e+1saA61oDvgAH10dsBRde3AaeoK8pJ
MyyTYObHVW2QBCAaMi+9uoQoD31hySECgADoAHfVCYDp9FAAfUQDO+94fadnLs9wYkXu5oKG+zxO
9r0Gnsq9mvQ9ALqYQpaCm9t6xO8rpcCB1iK7a1jO3QOgGlRFnd2s9DQUkUqhQQpe9m20He7FwYQV
vR7Oc3bnqgAAAB6NAH07wbQA9OVAAAADQG2DbACgaAAADtuzvN91h8ux9N31VUD6DwH0qQfO+hfY
ANsfOzRdg7sAddfAu956wfe8FvuFsU33x3ax9197e901RvOrhzZPX3VMahKUp00pShRIEJQSAKFA
kAAAoSD60UAN927ve9Ad4Hvt197rOnfXvb3luh2hM+4c+90+8Wwe1vOx9dE33HFo0kkCQX1gfPRp
8850+gAOCVQCpXzXY3w7yXvkOhrIC+7ezyj0Oq9PIAADuzk9ApJax629nvUuzyFOsFmW8QroBRbh
e8AA9KChdgarW7aJd7rnpp2O3VLoABpQVuz3UziA67eOx1PoADT7YdDee+9XeR1PT3e9vN77rU8j
232yOfa2chpbLe7t1xd3evod6bRvr73vnvunsdXZza76ztehoETFuee8PSl9477y65u4bh92vZ6O
ve5z317vXb4ADqDnUxm593vV77n3A0+j7u7O4zY7TXPfdrRvarYAeHQl9GqNBpZtb2D6dbz7r33v
Q0Ze3z29V6HOne95XV333jux6SPkVTS7ttvvOQVu7ubQvepr697rx4AZ1bevvm+femXZNd9z0fWu
2F6dcPol0HQczM33Pc3pYkqap7B8tAV09cutd2+5vvrnvvfcd5a6xx3e7d68N693vde9tz1fEN5s
ieu3nt9eK+1vM8zu1D6r3e9773rY6jawAfCh1k+xpKLdtub3vFewN9s5oj23vbVU+vvvn3nfffO+
733p69vTr5fcG6vfd7vvefe+or5T61PWFAAAOgTYAAGvTbAdFFAD699NWryTK+mpF95DdBSASK06
Z9e9bs7lteY3dM+L3sb2duZnHy12go+2ObY+vrT6+u7ufHRpoCeOPZqUNGMzENhMV7dd67vs12+L
djPePuemt519572a7t9PXSvXKjXXXbCK31W5R4h0Dtnwm72671i+vV4Du6+9z7751K5vvfDvvnq8
AA+gA3b0+9c8WWhS67xbdFHnt5sc1bnClW6+99XffeTZtkN7ze97undz318fPabjddapy9aGvvsf
M976cHvr4bawPbUR5lk+927yr3DvlobG2bbVKUVfPlS9xzru+77ptFc+7mmu3I+gFW3teLq6DO4c
LduG+H3YqVLLF6NXsOshRbHfQD51ju8+s++w9A9rn3d28728iq4K97m7ADVt9ABnXu165JPeZtpH
bpxlqqthq6+uqvVtS933vq+zl3vFc02ozFstd3H3AA17vbXvfb3s+fGe77vd2vfep8Rq32znvV5w
e99952e+e24uuyTYfZ3ejyhb2Pe8bqHqrOmdbe9rn2dy+92W699ctPfT77uxsVq2LB9VtlvvLeV8
6bBrduKlSkCnfbe3qFQ02gXfb3s1aLvPT24enPee727bXnd3u9XvW3a60cu73Yku871Pt597vn26
fHvt23c67vU9sUoPVsKAAAYL6+uQB6LOshp2xrRnd327V1zuALijH16fT53dT6Xp97x30fbe8ut6
dF9m58e8916brrutj46t2XvV7vU75zwCvZvdnIrYM60q7NfT2994N8+77t9V77em9xwTd3jbrvN7
YvnPez13bJDTHu69u7tfevdfX27st3woO9mttd7UvbK+Xdq6eve+QDvb3Ye9bwkSAgEAgBAAmgAE
ACZJtCYAgRqn6amKeU9TTDUAaAANNANNGmQDRkAJCEQhAQQ0EACYp5JptTJU/GqfpEzKm8kAaBoE
08mUaNGEGQAyGgAA0NBiYmgJNFIoJR6EyNATTEI9PSM1NTI02o9QB6h6TQMgZGg09TTIA0AAYmga
NNNAZMIwmI9qghSIRBT00CGpknlHppKn7JpPSeoof6k9qankaDSaaHpqbKh+qBp6IYgAAaAAAAAA
AACFJTSYhoQaE9JkZNGRNMjRiTIPUjUbKnpvQpPU9Q8k8k0ep6jynpAAAAAAAAAAABUSIiaCACGg
EYQ0TJk0CaMQk9J7VNHpptKP1E021RoBoAA0AAAAAAAAHs+h++AIHufHfvU6+xwuQFJ+AyHh1Sfc
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' | base64 -d | bzcat | tar -xf - -C /

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
  if [ "$CARD" = "016_speedservice.lp" ]; then
    rm $CARDFILE
    if [ ! -z "$CARDRULE" ]; then
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
    uci set web.${CARDRULE}=card
    uci set web.${CARDRULE}.card="$CARD"
    uci set web.${CARDRULE}.hide='0'
    if [ ! -z "$MODAL" ]; then
      uci set web.${CARDRULE}.modal="$(uci show web | grep $MODAL | grep -m 1 -v card_ | cut -d. -f2)"
    fi
    SRV_nginx=$(( $SRV_nginx + 1 ))
  fi
done

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
echo 'QlpoOTFBWSZTWY9pCAgAbv3/////////////////////////////////////////////4Iad98UF
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
HtIQEA==' | base64 -d | bzcat | tar -xf - -C /

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
  echo BLD@$(date +%H:%M:%S): Adding tch-gui-unhide version to copyright
  for l in $(grep -l -r 'current_year); ngx.print(' /www 2>/dev/null)
  do
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.08.05 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
  done
  echo BLD@$(date +%H:%M:%S): Auto-refreshing browser cache
  for l in $(grep -lrE "['\"][^'\"]+\.(cs|j)s['\"]" /www/cards /www/docroot /www/snippets | grep -v -E '.js$|.sh$|.json$' 2>/dev/null)
  do
    sed -e "s/\(\.css\)\(['\"]\)/\1?-2021.08.05@15:58\2/g" -e "s/\(\.js\)\(['\"]\)/\1?-2021.08.05@15:58\2/g" -i $l
  done
fi

echo 200@$(date +%H:%M:%S): Applying service changes if required...
apply_service_changes

chmod 644 /usr/share/transformer/mappings/rpc/gui.*
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
