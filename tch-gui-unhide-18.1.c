#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 18.1.c - Release 2021.07.21
RELEASE='2021.07.21@15:52'
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
echo QlpoOTFBWSZTWYroonIAV9//pd2RAED/////////r/////8AQAAAgIAQCGAzXvuz59wDQO3mx8O3gO+29hovTF1rbUsvd3vfYXuvd7x6ABefcddFqn3Odnc+7Qfe90b5tvhVt2jS7eT1fe1Il82AHr6ULQA5r16vJqh41hbzrvvd598d1NPbJ5lHpwvTZbVExmkRWzG2DwbBUys3vcUdTN09259776wSRCAjCDRojTJNomqem1MiP1NqamZR4ieo08jUANNA0ZGgkQgQgIhqYp5Ew0mp6T9UaPSZDTIADQBkDRoMgBKZJNIaKekGj1DI09JoABoaAA9QAAAAAACTSSQVPNTQ1HqaNT9EmnpomE9NQNNDQaMjQZD1AyDQNABEkiZNEYjahRtMmimmnoaaMJielNNpNDTanpA0MmgAAwiSITQAIyaTQaCYUJ6J6IaNBoNDQDQAAAD+Apv+sOxJ6TZZtIB2VO2p/++fnhpFNO26FXKVqUVfZ8Jp02r3W9/wq/wlZXxmGW0Ez5BpUzUgBGtY2uwh1qLUREWnJ5xEzedE3xe9sXEjIaQSKEioyAISDCAKxBILRFUQgIwZFVGKcIf6/TusS11rCal0IoszJbBcaN7bmMqJf0MzUoVE00+RmOk0eC10G8zPzkJ9EODKfNZqGGPRbgoO+NoMEOacc1IKVG0hiUNv6OK4sWJH9VsBl6MxT40QGMqb4sSfkqZkgzULoBuEKjHPorrTRa7k2mZ1KUiqLabuRllqCAFAhoQ0A0XmFJgIvZrHW0WTMuucXcF2bGfxPNouNrGOXHMChgxg/ZfGr8y9lsnqYNDMOJ5rAzEmrt5yW5RdY1RMoRstabwXgu2FREXQiNkbKZVNFrVtUEtJEOFHEpaZSmE2CaG01CFsCUWUESRipzBkzk59S3K++hHrl33xwQWl8QTi2TcssVFxyxsZF5rcbTKEiJslZuGiiG0NpTIRwQiZILTBQNVBCA6HTMiRZFZelYqonXEVEKphjRCIdQyCYgZzuErNb3qGtTBp8Pg028ZIo6a5EkOGc552qdtJyWedO2h9iIKGVuE6dR9fzzTesLoYkCGpSG8jXV4alimIDszMBFCYxBuaUJCaQ2a07hHLMEDGxdvy1f8rxWIdRRJglw16Mc9mhmHx8mjJT9lscMD7NC34JUmdUpMqogySJdS2s1BjypVe5/e5FjDyigNoJ08Iii6iTmOtU6lKoIxjtIZEs6CqIig6hSKIocUsXLAqgxBUHAIICWkCNmEoaQB0H8/K9bjX33KIECSCSG6BtrlN3W7E+yOwO28KEYyFthl360CmWFZMskUCqhEEgoAgwAUgIhIiiSDwtVIbPKIbWs+U3fGHI7v2t+Wtq/Xfy/09diPQUJVKK3s1AHU7yYE32GDstBeRIsVy5SFctGIaKYm2I/FCODjEsgNTmbrvDva7g1MmFpMccqMkDOirjcVYcFTxhpUmRoTK+V0z+b+7xZxs8LY8l7xJcUjUXhrTpH5yoLKonpt103t6qMiRN06Tp3qHs4dctUKjRCo0b7JXi8zlRZ72qaYSCJOjJZk5EsojMACmWzflUYwJq8IR6ohSvK60b29RqA2VfLSodGwi4xURCIIp34sfZMBUc9lvmzcbbGLxLghXTm8BdnJ7bLKusWTdc/fbTu7zhkZigyr2+moK9nBIOocEFwP78/ybH/IZNuKFzBpG9dLdHbl8bkNrknfS7+J2LNEulIwfLniu9+lsWcBHaQ1PTir4bUEPBTmW01DWZULzHTJWolci5SlVtu9USf6hqC3OTghZMO1nyZF439JxYaY4ITbgntKZZRwZghJyYaEJkMjHsL68bQsgNsmC3LUWFbsy0wO3uWNuIhtJhXZHGwwQWBiwY0FdKJu0wjfaNYULuPzY6/EeC60vBu2B1UZgId/iRax0OcjUf77CFgZKOa4oJjOqjPyFSDIK8nWevwFbZ9gRCy3wCiJzPm3ObHhKTANIck6YdzXI3aMeGWWkIRpz+f9Hz2sQzw6DYayuX2GQhrK369ZhlpqJPiSPICjHHYBeTAMFjbp8JabDdkxOCTyGQaPEDY0uVPOFY0k5eCYSKcAUgaYMScWiJz5MIaJtnzInLeeec2v5+uOS+vfa/Lx6eXhAsdZQhZKQixOO3YZSOFOXTNzEND4OaWjPew9bFIuYlFYgw3wuhaoAxacWraatTeSNLXPYEoGGN84lvzA4YWXqXrqn0iWcqE4UB+aA48QfuJs9sdz7ha3h07A2Sb+1ejFPfs00JpsGxphUKCcN0IZcErPy8NgELihVIj499VY222elmbaVT4U3nYM9La8NmDugWqqqJhU9TqEeHq+2EDWtsayZjCawtuaTcvGMCpUhaIqovvGZxoRiVyJKxnAnd0wMXynNzrD7JLWoUV89SEq1cqEl3j08eLR+rgLm3iQHPtztOVNPllleIUsLXMysJtpptMT9sRaOnhmW+Qbei+S9m4e0HyNUuRov07dZ3x7oPQRNvUqFZo6DSb8GJtHvqLuH72tOeuVpFRbc7hVO4wxXtRIDMrLeIFdx1MzIaybRXzzskLBBi0Ee3WSMtfdpdC2+3EOuIXsvwT3V2aC8jHaEQjBSHzRz13782SNCpK6NSOaSmZMx9R5OsmJXCRsUEjXbotZPLvcyZXjJo9kVyMhPDcXVFWkkhmBIKRlYGUCDKCStonr1Pu6wxdO7YUJ3kU22erUh7rDGFfz+/sdNf0jvk1L62jUnDbbKNz0l6qsnwlul/gdU681uSOXs+rpyVC9vJq9WxSuC6+/mhnjFQzn0aJ9oeHWqqqYdLhneOSLOma9VJRE+NtOr7YPwUivpTpnW/x9220CumAQ8RudnSCDIgzsnhJMsFlb7MC+vx3ztdxJ2rxo7aapn8Dc5qXDHIQtHGoLeiPmIckAnnd2QXs9N0DcEM6dvkquR8Zwib839tFGPn93C625x7xnagyuplQglaJeBUnolG52ao+lx7TmGYagyRMh5w0vek5thqETdDnwabjIS0CzhFCGdggRgBBs/KKcoszahuqmJTeXvM/Nuo/Pc9fIrfx4zwtmttt1mIegSKBjra23M2ZdaKBhCrYjUktYrKDyaVGJuMlMm1axN7gdYLoj2+ZnkG4S4sLqC0qoJBXod3xt1kDBpJbacTlK++Fnp6HrPXi+h7fRtwDl7PYOiGCBQvauYgMEDsunETUUyUM+t7bUfF4bdVHYkhQR8829lK13a1EptsZVFb2uOaJB0u2czNcWMQ/vCkPT6rh33RoobEsGmXDMBJLKeWJkNCk0kGBclaIYhqWZBJrVmLPsCJZmiUhQ2pIGxFGLFFiJEdW01MBguEJUEuYxMgwhmOS4URUbBRIRoUrZrNu3sdBMUYIdNHXsD1GvX8bGIQJGLkCyNohoMypBlPn8uHUdEy+UU6HoDE7+YUAdLn/0wht+bF7r51dbGM8c3yyuLnI8VWysj83wQyWCxjLC7eug3nOfAtyMFw3p2peq7OL51JBzHLMZ1I9TZuWuy8CRouQiZEefRagTU8Dy1mx/PPu+Dy0Tl03ST5N8ytbXsThRfLYSwQiyBCIrCOQNxPNPIQPl7QatnemiTQba5vfHxJ9wLh7xrmWw7xkRPOUb0rJH3ERRxKJlDGvu22FNA1LhoxGmVUMEeBsH0/B7/mjcBGUGVrtIQDlmpCJ4GnMEIICaKOKBr9mVLdzPS9qg1raqgZhEqvefyw3lzQrWCtBzv5uA8UQI0Rx146ljtZKPnNbI+5RsbbfDi2+Q5XDG8SXokxpHw6yhWB5B8PBt4o2G7oobT9X+ihfbWvThe3PGFRHAdwdUVtWuXY8EJrKAnJm+duM2O/2l6Ohv9cy/0+vqU4NtQzxtZyzP3olWYzNwrREDIIjvCsnLfoDw9072PToKMJoxZnEFnwOjwgKrp/PPa91x+C2rbXe+LUO7VdoqYIXqfm+tY8b9759l6qzCNLwUV13vi3Q8Ctm2+fTrRLEZ7wIsxSwqIZAc1KlLjkDZbPhZVfniS1vltdfGTVXjTd/4ePo+QX3zgg3NfYNb+jhT1vxSm6rrchTRC6XvN0N3GrxiUVH8f4YwUXdw479EiYaKGkNobTJdEnnTTA4GgPiDkpKBlKVJlvDIPY5uEDqCYhRUWJg9kWttJNmgxb/GgriAuHGju4KJMmAd/aELjzFk7Og94nrUxrg0x6Qn7HabB7n30s08IPEms8fT3t1wtgFboiVAuiV+Gw9YhcRIFqRKGQIoe3AxteIhiYUZTQZaom1qdGbm1ciZ76Ukiuo85F9UoDsJCOIUaqJdg8Xbdkahi88KQwEQkjlBCPDjgqGq+niEFwtB43DsvdSbCuHFjvk8WScfWk+cOBcoNG5zriGd+Pnah7Hu54xslPGsqvo/SvtzDwUjmF+bCoWaESBOwEBVaLdckOWmxQWmhy0hUE+rv424d86NpitBlXNnYGNz3VBhes+FITvr2/jc/RP/I4gBQE7GSUeUTQ9SHB5bNnXsujK3M+hY5WyelBn7UihBYsbI0GaPELo6LhclbwvEkREQ3cwsgaDa23VWTbQ2Y3yZsqm6OCUb7XWAKyr26POEXLQiEe3shTlfJmUX3lEi86m2K84NvySNFRLfUJ2mahYJC824s7eQfVmmA3eLGjEk8IX6SQcwRWh6gsMB5kSE4qmWEgkR9yfQ8jPSow1ZtCcnQvmG06NkMjBfJYUctUGyOFIayCPvAsTqXKLKVQ9aSWyM2TIjg+KFXTRXUOddBuEJV8sergWh7AnbhVuwS7cuFhKojzGvjF0B9fptrSa9iHDj7x5xHviOo4cjIjhEyDT5A4uygBnmGyhI5QhLgNmE91S4r1B4h5B6+exncDcYNMRsIohU/A5iJuJ9YAsEc8DHzDsJembOnviK9UCvb9bG0OgJSovR8aF55RFwWykXZ5MrbRQVH1ZihmXD7pd31i84sDStKNoBHOZ6faStEqUHdMrWG9XqSgxCEYJMU1o9BoTEM0u+wuPkhfu+33b+kxXkLGNxtl217TDDAQhcAlwlzxvYuLsLdi3xYG7tb80so7gQg0hha7eA8QrR+6dy8syJFPUeb6kPRPFYBS8C+aADR1Xe91TxuOIh54IBIl8s1HuI6o+nul93+f7eT1tH0/b9v0eMQBh62cLmHYF3MF5IMzIbBnB8HNrJET1SIAbNZ+wOfrPKNj35+kO/gIYMiLRR8PkOxdZ2jhDk8zj8kdmzrBaxGBIatbuT+DUadmO+ecAeYXcPwpJgrqJZYYX4NlzZs11Elna+JEMj5oWSqC0NMWoun+Eztoq18yXd5gs7KM23lyGyshSjCRi+YoJ8gJEEJo1q1ZisSCtnCqQOInz7rhI2zl7JG+o+XbLu8xws1w82iJjZIq5hXUgWm9KdmduFrBrAjUlNBEuIXzIU90jV1cvMzDyNU1mK584o5j+rCMaAtskXgOmQNxiZ9/LPN902/CEurkfFhFgE1+0ZYt7VivQQs03g1kF+UwQZUF2Kr8PJQp5e2e/j5MdCpaBui1NcMfLpsEYIhE2Z8TRgP37Dvzp6/c+EmircwMDp6uIduQWyYYQiWPXXElTKNG2MpobHDC4biZY2Zvmm7E8h6Juk0bM/Z6rIoqFAGVS0oxgkRlgyMkFJAFhBBkIKIMIAVCoCkFgBJ2+XT90Og23AFQMwm5LQofXsc35L8yc9+ExiPBHwXJTDEjR03gYipcC+ntD8pgENG0sW1JupOad32w4VmuZj2HKDISB95xD23TEsB0sIVlzRghZNc9ZZPYHj8W2ctjJrQo1BiFSuKcb9IfKbHxAc0nmH8OGl2a2QkjTqJTSa3GuiYu9eWh/rPjon7ltppzDy1Dn3kA7EJiYh80p+1eybNfJfNf5USyjBiHULKHIqU4NNgO9T72yhHSLevXao93hHd07und11pB3kfSvR7L2Y6CzkZQPCrPpXUm8X3JBwKDdr0GthsWOezQtf/Beod0I8iMAMpTXQoOEHXIqMVijNpArp8QUhjykkunmn6UfoFrDgF9Pt6yGnjmifh6PqH4ZDwQfE4AeKhuehEuTI7kJ+vd76xNylhV86DFkV8DYT9skbwH2cTjAfNImt8k/zTYatyUcvGxLGW1xdsHziSARgwYyMjs5OlPWIdA6U19nXke28p0B6VDy5pOrk0xsO+QMxmF3zmMkgYGJw92JSsBSzJag7kPD2fJznqi/AZDDJaUi1JZWqLFKT7gljwcTg+QrnLNorjgQh7J7VhMHIEpHQH8J/GKB5sO+CUch8eVJ5iGOIjHW5900aTawrG7qaTTsO+2SY7JXbw5uNQlm+/mMxx4VUB3yzdHZhQTWWlFY0ntIiKaInSdMj2rgXdvnsJsH/d87ez2HsPmUvoTbCgZiCIgLEOOFAwwwbJwPhnuYT0EOPhsJ3563ngwyc/vgzlJ8Cpvm3pasKE/l5niNSmKuHw615JrCCjxFCAoMmiyPOedIHEVgSYLNY27nR+NiocRiYBYOgFm6GPVsI2YmeYpOAW0RpYy0JLJQUMnj9n3a2tttiLBWdKXpksG208sZHVXQRO16n0ZVEIPx8/gwGsm/vgcLQbBcW2MBIQ0jsI7XBtSlpUilEiwmt7EeDWVCs3Aa3m8T91rGVsZplFEXE0fH3PTJD5TQcVR+uqDRvtfco92Cp1vTc3v8QL0F3lE2CSRTpoj20MGI93WG5HQ2mmBNTQToD8QL7F6cewflgoWu6osiEYBAIpEYhRAshEYzlAosv+U6htYVG+fCnJ3QTmfTXMUYSVM3RRxzr9hDkkSIMi7AHUwFoRlgK9KvrQSA8pRUUkBIECBFeX1B6BE/5e9C4h1X1L5eKgSIgyCFwoMpILIoW495xv4Henxkn6NMk7mkfc1uEV2RQDNvHhV4gDJXIIsCAPKiHICnzsUaFzCGfNTWRTU61sKx+1iEBkYD8qGyWGIF09bmKawN0Q1agQ9rFIfk8r/gSAad3wjvHfBgbFX1wntSUcNHpmQqeF7xaNL+x+yHAReHMFfaIoOAG3GgKi6UH0wUWgCKuqADiySKkEwA0ZnIJIZL/mPhgy49x3EIGoU49juPqrzfzQyJeGHu8B0dKjONMk0etWi+9LOGDYSJMRge/+lmwR1qCWwEOgiejKu8M2OnsjnZE6LkKbG7GB0htBaqBJ4CyCh9GIWjue+niA4NwOKlbEyodpihzGzx8fKqrvC1znu+JQ+sAPQckqgaVYHRtl0CEFDoIoZwaHS+OHcfLPaGSd4Afm2BgzAILcefbb5IBDA4RYKEWBjsAudSNDZFNbRPy9A4hcN73H6kB17kTwA4zZZOIm3vbSI0ihEpPadt1O0UOAa/vcl70NyF0ccZB50klROcLEA6OJuF3GkwTQ+QNAXnAW1iAvE24YsIZDA8ii20YJkmwKouRmhDILKPcinZxOZSdyGBuQ2hN4QAikjB4AUoSLIoUBURtAl8t2RoAM8QeOkPXBVkBZEbIUdWdMwhmc2ejaHg0BvzQO9HJZhmGBFD+OoUerC6Fm5EpSd9KF+exacC5UHmnL1Gzc/VxGCwgs5SW4d1JIFAEQZ0EMiFwNrsuIH9lZAtgHIBcTe98IyYhqEoOSyN3oQD0QtHewj33nolMLjodDhNK1KxNDQ5R+JWsNqXh2kmNB8YJLSGLxIJtO29uEbEUpSLH1ISl4XDmFB4NronYIk2twm/fv5Szfep7DUAamapF58THx/16/AWwtzRqULIU+5CVDFF09i1kORFe42gY+vADoZV77vpwD9SWEtEqSXoaHlUKS9Sen6ht+oDptJg1fNj7QQTRxvMiw17CJDdFcMJCsIM5aNAk8rc5ieOlam5lrWKRk5+NWvHLG4StSD2cxfE77gXxzRFpI98aJOXNXU6XKTYIMok2442saWAMb5Wc21nBAmWl07DrNkWIoshUtmEkkJtkFJFxhtEYIIi1myIUhdjYWbadTOkGCAymaoUZQ1TJqgiGCLJXYyzILSW5BrKgWjJVodmr0Q77ZuRERGDSxjTOdBNoITjV6dbcsjCqKhcMMXri/GxbAXNlNjBANU8yuQHtYjYBoTa0w3liyMNy0uNVNmO2xzNuEytHBOahQoIRKqBrVC6OhrQEQAxjsoCBiMktwMIBbhS3zjdvIWRLhQ1on0pQWPbxAqyj7gJGITah1A4gNnDDvhMusGhL8kqBRcU1EsRPXhhRGaKWIxZbYoqPd21kk7FApF7N4oaldHwgdcRexkHwOkCgzrAG/WEDkOq1UdAXB2wQrUhBOe+LrQa/ERZESIygwsXzxAeg3BknogkIJCCS47esUSHN46TmRoNBULGbf7SXvxN5uYIzeJvQIcGLCwWUiXQIkCojI5AY9zWFGRsu5qqJaDQQEtHW4/DkB4B35ukLjURDz1ppVIR6zO1mAXm9iFSagZMwNFmkXTUhEUBCPVDwNDwMuBdAG6cIeWjmGQM7GDmN6wILAgp7iw7w+/QfMW4IfikSAkJM3eicsTAPlD1KbkJCMjBbxjTSG0IlRbReCDZm1d7ESb1ckA3C/mQ0OJg5Kei7UjVURiUyiASCuMDvAhsLC33hoU2gYA0gut4Y+dMR1I7mPgh3wlgFgeWwEdWaimmMn88Dp8fuYCPniOsNShgSHydztiNzbiUHqA8QPA3B9fhuVMlfsM/fJLEuCba+57g95DF4phqC2QXDUGF9UTOBZnkEO59IEaxYuqC1EJIQQQ5efFiwEDzuEtETMzEMayUL4MDzQdTTJZCzpLAlMspQpRJWMG1slrKDAjBhswsiMZIyZQsIQeQlLAwZRGJFXC/opsNxcONDTk4m+ZgQQYEBjJNMAHxgICIMidWAP0MtwHpAuhiwEdYHEIY0UNBQQkClaAp39ULOGsTbYNIQdJZbk9iI1n1jCJ8TLw1hUL1+Br+xoavuDkQDg0FzKxBDxgttoJcBS4N/X3+pohCJAQloFA6hCxEEIsmRhphCw4HdvuRTqRIKfERXtqmuCDhoFE4jepBV1yQuwYA96oSHSpDZcLEMgUlezg99NqtRqIeQQ24n5KBhm+KAK8VKr2lqGNnR0hgi6XOgTvExxG3BXDKyoQoMEIHuDYHZuut7hXo6TZpmWhTjn5DxIDh8cUQsLUi5gDxrXtyvtLXoVmZRloa6E22S5HbahtR0ZmWLRxLmZhCsbYwbCk6IKK5/DjDGrpEZ11Agx8C9A6QIIlwhHxVJJxXq9iEgnOHQHSHAHajKkVoOzsCS6EEMoTEjnRvdECaYziT94bEj85Y2JzOUQkTpE5fYFrBdYMUI3KH4tr0JSUNqYlQQuF3bk6yz27tsd4eUO50fuXu70BYyQkNI9xW2oNDoxaxCsjyEJO0EhUYxBAMO4dtZEZFEixkRRViIjGKiSIxgqKM6IwsRjBgqhJFggqMUYKEfa1NpXmNIEAQaVu5EJbTdjalXxSTJgCoqKpjYLc+YXskn3isn6AaBMzT6L5jzQyQ1DmC2CsUGqDKlvCvWbHOw2A5SYAbGAsGBxgAfujE14h9AisJ9bGj1qWHy0KChyNRxPztJRy5drEbKYXKfn6aBVxghIEtI8KH0S5wmwUsOAZLNS5wNEGkrkDeMILhgaMrpccHITELHuz9mkRNqY8nlrea/WpBEunLtGh+kkatKnQj4ISzIeq50A6bZy2ciQ3Cbj5IPREbMSEUISbtIWG+8DpNGmPdQJx38e7zSbLaLLzSzBgsUQZBgQXoHgGGIdeXOZ+NjAKB/Cwa3Y9IOnUJtXJMUmGqPRPmHoLEA+WWQDyGKobEeg7EZy9D6Qcx1qd4wxSuYRzDG0JWRRyz+mvWNtGk0wzawK7ZmQGll0OUyYCQQSpZU1QEMmYZJ9teUIuzb4k+CTPG2O4WyPx9ijTDZ67noQVANsCgaBbHSRcg7qIf65cwC0kgDIQGKHimY5ajdrQyFHQBgfVDt18vBBh5uiRE2rnCwUU1BGQpCRizmZCwQJh94LiW6FIfCrF5UIdg/l3jvzQ9+y31AYW7RiN23Oh1IGCXWAbzr1C29HgCGY8EGMYRjAIQCQZCDAkRphQa+A6g5DszDENoV4ZrybBmEiYCwJJbZLaCCZBcBDO28mMmQJL4iRDAAxdoYySSQk1BTtLlMOCCbA3IbcrkrJ7QcgOOKhgckoQKhIiyMHKakGCZedhCsmCAsJothYHEMNaoIxGLijApQQsZGSaLVjBZ7ww8jCRQ3PqZZCwTEgxUQVm+AwKCACJFDcyUQvIlMLQmuBZumMSyHNJkwxC9ohdK/XIGB4IHQIHn6yndzMIZg8VPP5qvMDVXVHvOmtibdzhEIWVWKrviiRRkWrNrRAootZC8LkghdKLWE65tBquiNCJCIowYYgDNxwOsfAITpnbKPSZMk0IKCasTLCzTVo3kQkwgGaidQHKXOtCHf86DazhzEJooXfZr2gvH5w+o7WkhAd4N6F8SQgiCwERGQRhJ5YwZlJRgEQgDAAuh1EIiFYFqafJKYAXR3sW+61gtDvqdPweJE2/JZubwJ80WdFyEbzaSyYH1gRPoqY9wUoofikYHimpDQbFzX0QAcj3Sg1R8AFhd2odcQhCQCMAMi5VD6gdKcqHAulUBTsaBsUhgNwC4luKB3wF3kHCCOTBC0kIERcWAXJ7kIiBZSXIIe+wgfoEBMgNqF73gbIdYXmgIIESZLwBgcuAA8bEesMk5psgE7dpST9vhPAX3XndubMUkJBNrWKXQWQQPP8tC74kmgJvMR95l8dLb64MwT2bYiHpIZnA5prEiDFBBFUH6vLCloYoqR8SutMduwG4DwIdeVgLEebQT2GAFrp2Eltnt8QuKHqMXEdzon182EbW+g60PEAMh9wqbAfiwoZIxSbe42CwfR53caFA3B8cQoMUQ6dvaLa8zHevx9qWEtyi/a+QEKCN4aUn1XqJAiBIaqcIJZGzQcw0BQ7tr1g9WV4e77H34O6eYzkEgdU4ow1impEsJkGWQdpd86A2WhDA2EMxA3KPvQWSRUkV7ICGYOATlQO0xAgQBIY6ZQGCDjBIQSEcgsAcHiOIcjSBRXFtZgFmTa2bcqKQFu8kChUO6I8igpWmkobhhp6ia0rpxoHNMUNw6QsGLrTDSEvHqvE4AUie/iBoQA0DjIZcqODiA7Yp4SUGPbEyRDsditApiBJSa6CgW5FCtJQ0O0ONmRMY0saULBTswZIbMMHbdowBtKLqCLE3RwYAMhQkaCDSR5wIWgBaKyEJIMheD4UR23gjwhjgD9g+J6R2ahCyOKTQgbNiOwbMCFol5+IxQuC3up4Vc3d4cAA38H4KMDuJoEPOjr8w56LxCHQIbPGplRrLBDD1OSHy3gaSYBwhJ6c3Ae2GTpAZ1xOVE8GD6+jCLhclkkEtX+bFm/dzKJmPY7QgfPBAviprQA7Khmew/vWYhp0mk6uWCekgOLESsVMQvKRSiqpRNQOkZ3lNARDcKf8il/GOrAKiT2h4dgP5EgpjgZY8hMDBOAjiVuTz/C8eF1KWKCUQedUhzScgTQGmEbBgRMB1fa/WD7h9Pk3ICFskt4NOuOjzwx5h7vpKwzS76OkI0wLrQvPEnRWLuCmYtDNIpaOI5MnEBkaY8jQ6tEv6H10sFhUNp8oWJXVg0jrCRuwGFid6RgaGrmPlz20dEJhqcmpXHNET0De4uaA7ooaDgvXisWOSv2q8Qhx82dmWsJsUHCHgqOi8UJx3BLZiNgFnfHecQ7RwQyxLtFGoLJpsqez4ToDnxXuPAPIKPhjg/jHJkChSw94Ach4oQp5BFgFFE1awhNRtPjckk7zsT0Zg5hyfuEVXbaaiAdT+Gg3BmJhlweKd+RB3Bi+h4wWQAZE5jjicTWBozAzqfdPxCfHO54EfMdXU7IX06gdmfAHiKfxePmDmHguKART4kESlTSRoxC1rMrNSBBsBEIAmaImApg5DrOFOwD1CppXMZoUPJcTeI6xpU3GohDBPcu1Q6pCqXJDOrEhL85XJy0cCdmNSOIYlpam+FGMMNPcNgKZpm8dr3/BRn/OF3yQrPMo9i1SLe0vZQF9kGIlyekco7ohWZovKjNXdhNQFsl4tpCLsZAts5CBi4jeJaIuHstizgQfBLRZADQEoYvgvpXn1wKldbJW6dNINCL2iZ+VA2jArLQbwwAjuKaCJk3IxgUriDZJGQYSYhoutgEljYqrURwD4Gvv4+PQMUwgpkiJiEFLPKRe0dzHI8aPAGLsg+SCzkRVrHC8hOCC/WFwBWF2aQKQaEEw7IW9AKwZGEnUAtpFDoUQIXzLttcQNXE2cfTv+wx5iprOiAU7HzNwWCoN6PmOAWNu8N7p3clTbM0OVxLoa83bEPOCTMEHoj7JW5cR4Bt34fGZbC0tCpI7N0hV7PAdXe73ZIhVYpFVjIIgUGkYMLSKFO7ICwbkNj6wQ8A+i4o79hi2ruP0hsneocWg9cNaHGDRYhxDBXrAyXyTcGAF12ANOCC0jAMgTlTG6VGcxoHW8SNh77A33oLQpWg6QXkRMjaRd+dZwOwegc0No91nwx9VrxT0BIBJAQjGRYwP+c4GLEOhAMR4R4QqBIwgQJGg/wi93n91XnE+EeADhE06ep7qq5lkuV4cJ1OJPXSQ49WhCslRSvnJcGgcEwEgsMsqduhYdVkhYi7qYZBNTd7G3MZi+HnLCGODln5GSAEkga4ByxPGG/aVDwsPAK06IOOhnRykzxC+RAkbROHeo/7tQetNOZIfEP3hT8byANaHGtYkEsHVonvt+M1bvojxtjavC1hbDT8iLIQiD7DTfhGGghSFlhMwS1IT9kkH5eX2MQWm4egNByGJ8QSWlJNx1qHOWxHjzXnYGlYAxNr828oT6MhIT88mkf0AT4OOKeiDp7pd47OKW32jVkzJuZA06IJyAI4oLEvR8IdQTRoFiefpzgj/3E8THPJ/d4z8OARPw7DaBcmR8DLtQoiBHgYOCiggVSvSCSBHOYAHvgiwgvhbQNmPXs1eJuD/pICUCvcaJvgSsMIGqySJhEhf1oW2HOxAf1BFDQAXwYj0ERCuJHvAcUOAQh7EIMD5IpPElGDEaiyhJF9giJNGmafN8wcod3H4l9MZAp4NXeL9A0Vci4XCVBCIGpFYtMgw+irCoQQhKwrGFayi1K1SX+zmr0o5kgKECP/xdyRThQkIroonIA== | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified qos GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified system GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified telephony GUI code
echo QlpoOTFBWSZTWRv5FDoAS8r/5/30AEBe////////7/////8AEGAAAACACAgBIAhgLJ7L3lwPWuvvvH3W8DEKH0HnGkq+99Ps9u+OoPbRRvOXW333OhUvted3YAUPTmh3e18bZq973mbur2bd7ngq+33edttQaUd26Wwu+577uvZuY92HPK995b3va+97mj77Pc57wqn1ed1d9njWqsnr22prLXc8tuEiRAAgATRNpMmpoyaGmmqexFPSf6kbJT00ntU08UeoDyIAGEiECEEBMpkNU2ZFP0pmo08oyPUyeppoGgAAAGgABIJU0jQmk02pkZkTTQAaYmgDTQAADQAAAABJqREGiGUyTym0j1ME00AA00GgaGQBoBoAAABEkkJ5Anqh6mkaemk9RtQPCmQyaHqPUAAAABoAANBEkQTQExCZokyZMEp+p5KBptR6eqMnqA0DTIAAAAPtEPqD3O+kqIbq76mv2ZmEId1kJeKD4EEAeAxBj269sv217Mu1VDxOMi7RSldIgSVayREe2YbbjZz4pS1VZcTOxcQNbITSSYCrBWIUwRYAkVBikUDQJwsBURZFE6nO8Oq9B5/0tdyz+tr9yvjctzA1FssZXoNhWwb7t7b+zIr8hPAdG9vjaZkcZJ9ct3bSz57G9zOc6TLtbuq70UBv9N19kzgFBg+oAF0mJsxdOFssPq4/uwweHMdFgKKRYlt+CYRBs9nhzEMfBxQ4ELTKMQhK0sITtiUJfbGUmGupovlnTqJtbTW7j1Pnwb2OskbxFRBxWNh2UDpD5eBDzyzy2B/2R5/Zjhru3sRtpWqYvc4C7VFU45veKqj4XTgzDquy9rbxNTSqmKmT6aNyd22xudOCpYnN6NOzBNKnAy/eX8rPsKbHKXF47d3knUTuxwmJ/N8GeVR1fhrsthhrltaMjGMtBAzHfLZImXZSoV90RSUSkwuWJKpv8E0DGJseOWEAileey0UXk75UtJsQ2C7UySCMiyNzHRmeAaDT1qxU0CGIwyBn81EdBdNbbP4Xx3R3xmSBnfm5vpOJNvgVVaC1dTmUom2NvegtVm89naQMZjm3I7EyH5TmebVnSTaYPS+xEs69ZiTeVKPwePJpFq98QQRRb7M7KqPoEIKq6TSKKLKiCrzEAUYEbEU2QBRotSq3cui5zNDBAjASMEFICilWlWJOujTRx2qCNqpRgwpJQlkpgqhEZAUESLFEFYAIwWBj9lTRlNrg8WpW8C2oKyxXgwIGbMcyRTESjPHLhli8ojNKmr8nTaG/kOrLN0DJPoCk39A3tvIvEebDH5G72UXpSPui8WdbWGpdqU8RmjBMlBBLaYfBEhptuqY2sjzKKUvVKryIFL3dUggi5DSrZuDXFoSFqlLJtPEoLVjSFarnoselfKMQoLmr4cTlxMgXvbX48TYjOz1rN9YLeeCaeVaNWgtEfzmd+ysdX21J8Rv/KfxGc9Ae/KZrWW7fmcgyKe7q+8o+pNB4b3twZjfFXtvLtNbp2KiPn2S8E/8H+O+NamPHxtY59HiOURaoEdRWCudR7fOFIbZDAg7UI6DSgVu0+m0H46t4QYaD5OuWHEwdzp2cnu8FK0eD8M/5n49Vdt348j8j9oTzMNunCIlc5PWNVGUHhXb99c6wyfTwvu9FtXuo9/xKBi9TIak0BDILDu/LyhPqAe3NeuZaTvXZaOp+OWzxSsvXbhK/jkQ56sTdKbbeaJse35Uw5vwEGRR7jBLaGw3iDePB791lIr8EbxQESY6AnFnZBER7Su/5Ej5mfQbizlt8ZqXGa9FCD7msZY202Nt/pPpzCE/VM+ZoOrLZwxiGReIYVirNlDQ22TydcY7M5C7rdGg10oyepilrH1Vsi+vHTTKuNTmYN7amhGNGczaeocpnaNZyDxNF9vezeo2AC4apSDCDJJPaqRM3mVLlEzY7yJQ8iwLf10uJ5zGs7XsU2vuiPAtJyNlISuHoRy1HQuddHHY+eJ7G7ccRERb4PDa/l38icejrXThXSudHthW8I5smyUN6iZcjkK3iJccTEjJgm9uNowKwxpQEI3HCSW1pDzfE24/j34VoLRvHpxVqZC5X8ZVJsbYPv45mX8N8rFktKIhwlWs8BNN75jwyYt68F7Ggm5kOORy7e10+U9PtEufBXv6HTszseLWkznFlWdow8xBBy6j265oIEdorQ0P4222l2KVpXs/Qs4pq4WI12xEdjmk7DTGhjSglp0u38d+m++IpHIigUMFKgIUQSoNrBNqppCyD9apGk1cOhlus/k7Pf7MPKd1a6+PQsH06nTvl2duj4v3rXx9yOKzfs0h0BahrOLH4TIjZolvjbvc6iiIWrvuUDHDiCGlRs83e0so1C6kjPyIRs0uuK1r1c08e8aYhmp0DNdIfWWF5rZv8kcWURE/qgJ8yArBAeTj0cpxqFioO3EZIhVmZ+po3vUl4cFrfZnnaLU12NG9xAQaKzS/u9savft3Hxc1F3epobn0yg7XhqViF59jcQMo2s13wo9W2moywsJycFeTqpgNWnaipxL0arBUuNX06TuPUaxmYzeMPN9FWfu1Mtoxr0M107FjolxrtvbefXxW/nfRVa6VGLO3yRh0Tfkz88Vkx2Z/XKfz/h/dPvsM2uv8bFupu2TkYaVBpI3XYXvd3Hioy/Y/Tju9/dvMbTvkPSF/LzcdOvClG0WzXyy6JTrroeidV4pTMJY635sNWU6s99sJ+IhB5X1IEjv1E7G3ZB4ohISIp68Bo5C+pKI0qw62ygFsFGhtIM39rjvo7/4c89WzitNi3n1QY0proqRSKSIokFRRikTjyDXmtWybOHVTiOXw7W9yONvcDQSOV0KiKOujEVlTKWyobQ5EmhlMtYhtK8CZUDsCpIUxVvG0bCMSEGjALQsKQIILBSEgjGGDKCDAcd3VRfqbdBAMW6Xnitdb78+Pknb5nZq8q/0BMel2tF5Mc4Z5SKKFR8h6SSxbXzM1yC2/X32q0VNJQtgZsZhZ33PKZMyky9WK6iBIP5JRIPSAdIHelyckg/Um4ptKqC53PmiNzRaoBh4Zr5fQySBOjJYJCMaWRsTGL5vLy7zB0YjkwOgYT6b7M5VexTl5xzrDXL8naNbLq8uGxkhy9z1vbRBEEw9Z1xYviB/I7fiTDNgfPukLm51/ZXuyXLLujNumSmu0ICCr5s18yhOl41vRKhCZKMDMRm8UFf9Jt+u47nwXezlPYziIj7PqmFJxKze9/n+f2YLsxjbcsbVfa9dZqJ0cZxOICSeDJzaXx7XRtwXN8/0Wi5sab9GevPTWIEZNaKc0lk0HQytiVGqFxrsNdMhyozS0qZ0P58iU1Bzl1iteu3hdPyfQ9DOz3+AU2ZG/RtklTlSMyghYxHJ4zG/1Tvs33ZiYWvXEHF6/FfLLZVJsnMKoiBYVqPP6ipFZyel+hk/SWA8N7hd4Ea2htJ02NA1iSuQoNlyibjAkpiwA2CDFjRUiGC+Dzl+v6ItmZLOIPvlpsvJ7stbGwhRUIE3FUpHVOocNAlxyhIoYgydDL88YbCe6njSKs+Tfk4mXJQlDG4HmVpt1vVl7LFcM8lefrtgVPmHXJse2DBNzx2+x8cUlu8NZGXnpqIju7dGBctHeyN1a3uHZ87wi5g3m78Zg7PDjx9OS5jtjjbaVzPYGmRJ9qW5hG0Vfd2JUpDP+yKaRfWVgc/Mt5Rfs8/EgoT52lOxQe+vAINAze3pfv+/f8L+a9rlV14IdbhsbhAWdo106lbfa456D3B5rlY+QDDSDplwKdvvQo6/TzdI66MiMNAC1r3eaSLBcswUmUIUgIgKQihC4MZyYEhBhAPncErHhY2IKc5cdQdz5G1ivQ5OYV6d/JPCGiG3wvnrLjbSg9owyADB5oieFaJ0oCn2JrWboOcNXf6JlZ6Y6G0nnpwVjQZhNO8+yA/imcGKxa6epSC7jC9/fkqpxisWKveNuCq99FRpdRzoIcIIQjOFIiX7xuOEZ2qSIIRV2e9fz6gxuEcl93sU7Gd0CHeZMFdDkQr8FIQmLaikg2po1iKdbysX5HfDjfaERYDyAyolPBQGEID3gUgmrkDYSuyKFpz1vHLnPqt6JXl+GPix9ejjJxQEJedSIr9WZAscLv3SLzLCDlbEMiRysTLjTtm3pC91L1jyA9YoBBUNowOXYfS0iP4LG7rhrbMwVDj2YxTr92wn99CBB6PZDahgkHj8awnQ8b8Ptq+z4Pb/R+393dP6vzp9vx/IJC+Rc2rwblt3RUBDQSb37yJxEG6NjlMIQnivOpD9+yjCxDzIr78F/g/Uf4o+BHsB9v1QG7UCfCmCtjhW7+QUeP8LcT909Ulq0kLcKIIBVETK41YQieuKRF4de1exIVbSbgEU11PhGrya49zDhNoWPHfOqgVESqVeTkcd+la8+fZmN+k3I1GFOsLqKHHx3atRmxtJKrc52YZHALpUCxkEUWpRVPbdFIQc+p8Tv0hYraScs47kuLRcuIe4e/XPv8kjG2Y2SHrRTCwTC/uTxqB1rokkTxtuQVOJvFhJyox1m1+ng57r3teSsKp5kCJGOyVR52hh041WVbHJ90nhJ4B3UInBgOSRc7bhdjEQK8dzV+MUrRM50XI4+oimXyoPg2+Rnp/mig3nyrBoPY0Bkw7ukPpKT5MPglitmnNbDbiFp7UbLeWh0msDta+nhDocJo9H3ufo8fJ1X4c3U4v2dA2fbPJjTpdkhQNWKUJVCXg/uVCkGDJTJQxiBTIpKSALAWRVlUWieywVugA3QakqgEKijZiyAIF5x6GtG9rjjKTeyhZJtLmyNwK+7P9OBUCKSvdyy40wjId696KCY5REiBxEpeyIbHPmYXYvrvB+vc1tA0zuCAqKUz0CF+flb3vvHYFLsUWTK2fKh1m4Y3nOiwJBTgYBprlHx+nzmg2hRqRapSMF52vnlQ2UKKMxcU6DpdPWiuZqgi0LcSqsxZXe6f81g7CvzfoGdEiaC4KdJpPb5V+HI0Onxn8GixV4GAzCAaI8ZKDZHr2bVVs4+7FQwY1wVmksDJU3eMu5lPvFqleHVvXNjt3rmD9Uv4BjMLzvkstKHNKUFrjkqadyDbu6u67sfRnX9fZyoyzkC5paFhRrUjOUdqrUa1lxqVntPtGdLIaDLwpOlbdGzN6OynMRDCWMibZnRGUHX7mzD86uWgLOhKy9XmRND2rfqS1k0xGJUMKCMGbw6wjXNnMoCQ+XLPEVRGO1U/SVgV1E2odAuHe2kaMR1ssqygstY3HE5x+toYKRcX3SZzG2glUsF0lWK1qCBaoxJIM7C2RY1DJyJF32VTUggxEgowTj6OC66M3XT4bv0XQmrMg5gYZgZrS4GYj94bvhaWk4ZzqXQFYcORFzw77fOusKM2H2oVC0eu3sKwZ8ciAYwwABqHs8uc0ebPHvFREtP0JylBKugiEdRILvCQK0HlHq2NNDGDNhqn6+7u/X9X7KaDbXCTBiXS7BBLgGPCN3sQrfFyLeUybg4tGa8hgLrxRVYj1Sn0mxefIHCGeZeaZe/A34cYOR2DGR2kzwhJZ+wO2VfVw8EdaWOaLJVygbcOUROFTX8288l+6m7Fr61097lz+8iCC5w6zMtxEzYsSFqbA6JfMnJ0AbWbSjqQQ5WjxF8PZelrq1XLXfzIiIjG+o39YMaGqthK3W2RSBAAYhjFaogKY8horVm+8mUIE5cM6wssfSEfFx2S232ZN0T0FJfbez2hHcukJg2MtzyuxUu1w3p44c9kEEI9FoDPdDiEZ7jQJfHtmYjrbca59d8FmWzVEQYtANPMY6dS3HEGs7b3rFdTHDgcJ2IBHHXXKvVF1YoooLHopjfs7plwatMtnhGGOfRmijaYKWqLZUVDiMlcqF6/CIP9itH3XEpLrIPHAeSJcdikygTm6ekLQdMd8LJVAZa1iiKMVFkFRuHbZs3u1xaUghyZukDn5Ommx6x2pKCGHVPR3eCHXHNQgWIO9tbBJBvM9qBXmFpO5H+MfB8nKd4REPRFDSCg0GbC0YSEXACKXtQV0KTEFtZ/W3Y/aQhFhCERjgGO0a0iVy7H7AuJzTJ2g7YFjKEDhgBunXYAoApBBiiCSAko0rE3cpsClkXZCRtASyDaJoGyJZImNte8Q43PED5ZEpkq8V1qUcDkhx1mBYeXQ2Ud20Qx5pGqF42TcnEV8pAVdpr3BEGGEL6Jrm8xGIZHY8DHZAMkmo3dErmgOvJyBNsDLNRK+0iMNwHZEFRvGRmEiMggJGAgJQOLoNpu7drTGCG0G7C/+wMd6xlRSoNNhgyw+MEsTSA2IoBqDiKOO0NO+pf7rXkBnCrJwFuLxL/Wm4CdbWRCHcu5/BcX4CzA7N5kuEQ4nkQKcdH4bQ+hQ70QS5qLl5lPFmKXQh2z2vTtx6BEwIGbBKE4U+FKgmxp2nZCQk2Q23Peb9ZuXumOkKRajuHOZARAEDTGIbUJjg0BEB7ixypWkNgJEyjDIKqDTTtZuAVwumfZLQKn58o5kPSgL3NGnVrxRqsusIIarpFxGOYYha1FzwrwkNmN3w49bVN8MRoTYaZvosAFJxW+3GpuvsuJxiyKETE7akiZgGyWyXLJSGEgkJrSvnk21wwdIRMssq0ooQbd1AFRqFF4nqhVU1KmndeFNsEZiyZmwwRjMWAsXFUWnCLmC9QREH7YZC1Ri6G7qlP2iTA7CwnYAxRgNTMEELTMWUBdqYqBAoRwv9HyXcDKDqCIVYKsk8ztFIEeLuWGgWIEnRpAaCTbyx5CblyWg6izoCTYrBL6HVggRcqgunEC/OO3c2WfQy1rNzeEdpdBnSFcvuaLdpaH2HgA5wKMwKiysAnQlm2VcgppqJucDwDuesmDFe1DHabObYwPZqHnAkKoqWjRJbRTpcCzDeUr0WC1QVEGZIWYiBjhfFiZLkWHdksZA6ovaMHz3z9P5G7MBojUssbKVNo2brUVZ2tLThROokcVHzxsXaq0B8+qZrk7RVIo31DLRfQb7Kk0BGGUMwZBjNRBCbzAndwsYh2q5ChM3WQ4vDpJogaezhMuahhKItNsRlWB2gOJCw0uhLjEsxKGZsVkAoIBiBlquXKO/ZGMcuSDFZBkzstFsDzsGfligmttUVU2OYr+IKX6omSZ0AHqAcwXPM2BhfL4wWiEXHkIMaZXMWoQCoanLro27CSRqHDGNSNhEN2aSqTjJzkt25wUYVwMW8xyIrN/TRus61oBVmKCsWJNJhcMi5fJcInLZWEh3gMyA+CRCROh9YaJEj+ulOYSEu8HhT8B6f4/AUSzBpLq+xlI21jK9GtopBYAC5/OWrZ2zF23sOZ4MkEKQ5DZKouU54/oduEDAH3qjuFB3h9FIm2GYagZDpnsAOgd9CbnykDsfGB8xDCFBFkCEUOKgLmDfq5GvFcRpE5Lxvc5lyzJXZIB5MEHX1SBL6ezPqEF6tLu5uqTYgurWtJeoGhha6cxHGlsCnCKOAYl6XSNCU2aazo6oOUxFZEOD7gahK3w8yRfUS2gwLo/CwewXHuHvIQnHBaruu8zTG4xDCanyGINBOsT7w7S0IxdZolg0LnNKKIQUSEki0iJKRgyAFJISMIa0CvRimpfSGTmYw62gtZXUfqSEFRmfrTWoLvOgdSJ7AgZwP7twSSAnPv+BQAyY/EqxUHkYHB+2JX4D5zkxWCoqWp3qdy3JYKstTPk8HlOYY22Mk0EwNTugrmBv2bZCFsgfd7j3ae8hUvHFMtVtr7StrNzgUZQPdLMSpRNpzCGAZYUeXo2m7/KWGTAuPM4Q7LwNoYDhQHM0STGpVJ2EULXqojrYhmBmwKDIJdgwid2wSwBw4IWwg9+0WE0DShZB/LwDeG4b6tiDYTL0B1h90NMDmmfxHcjTGIa+3GwIGozHTZIiDIJOBoGMlFUMkQUUhjGgi+BdxSPvjLI4Kz7NI9YJatkKgQbIIpxgFkBqdpWNjGDAwfYgX5O1y5iYXO2naHUADmcilDZy+JSJDcSae6B9Rbp8HvHSMwoeJ5yvMFhUYQw/bC9riJAr4HcB6fpCgW8Yq+vpDE9AEGAxyPnwr5mJRolIWPFtOEC99XP4UF4Ckkb66o4Ru4RPh4N7mSyKaMWotFWuWmpzayxBHwX7V4ByQfmigR6y3d2nsgkOlxX6iD+d2YIFgJ2KHx2gSDCA5r7/p6F9Y7vemnNGB+bahtz9de65gyFPsb/OWbWIQeM9wNxAed6X0EOvHsKNPTJws8eRsPj0MADvz1DZCUTPxyoMOrU6IT0m5BvMeR1nv0PlCEQMQOkrYCG0XmeiujroTaY2KMRkiCCyREEjAWMUSAMSIwZGECWILYDguHMERBxYJdNWq4bvBTewEjrYbUfUWE9cfJuaej2IHn7QeURwPx0c9/4CFruZS6wTHcPWPFQ6zUxXADL1ZxFQEgqi6lWLmXgL9vGCkfW4WSJQnAcQgTkDm2A3CBi1vss54PhYTxHFw5r9lVyYYjihaAI86CrckS6qlXUFmzIIFghBpW0BuMAuAFX503TCIushU6Fzl3hORi7ICPRjaKbmpeuwUL3OGEdLpQ1RkCjT58Bu2NdAUIiqWoKWD3hAzGPEkC40QcHXqxiaRUxEDee1fcBLBc9ToeboN0DfcaS3dpLct2/MEMjbPDCkyYufKkguacpBTVpnxdADrnDjXYYL81hYHTlabYVoOpNGXE9LGMTbHdC1dXC44XEZkVZqar1THRsGM0hlJIhVBhNZQes5nPq7Os8B45wIwWGkJ5EUebEOwxLAwGlfOlxLwIBD4iXAsZviaA2RDYphvfHpSQfANMoQF9O6ihpqlWEyUTYBySwz+BtoZksUgtAVHmQ+jYKugm73lQu3O8xFxEHREkFYzO9lzGw6QZoZldoRTZA6oZaRu9xcpN83m05ifI69oubqVQ6zOJISQhmgGot7qaddEg7mBZwRE3cFKsHIIGBKUrIg6hLycAzWJcGyzanqg+1EEgO9eozTohc5EQbR/SiRWJ8DFhWEWtEj1QbIzR6NN1MNLA0yDqIOgyidTqTcI9WW4yjPokMYtNNVQFKhcExEBjfHCxmwtZRCKRCqISgrGoWIqKoiVcSVKKCVDExoZIEQNNQgkCxFRVUooyoUiyLEtM8EFkFBQW0NernzeAk0IkQMY2J4RBobRq3eIDYi27Fg8Vk0nMCHZCLWlCAbiEHRiplrWYBPw2gorAXJIgDcDEJKw1LVyridTuggO57ew4MgSCKbwvwky2C7AdsFChijtFo4QYMDXb6o9rCGHx+Lcey17heKsVeyTZ/XSRWmHyMFw6VuDMkGQZmj75zR9Q+sYFFm3fvpJsqLoaALlJNbFpi4k4iBBtskGRCTm0EwhjeQoRnFSC0iyDJBQWRggBfJI+ZiCWcXBaqpIwIQa+YkGYNAQ3Uuargg2Auk2a+jOuTEzHiL0Kr6P1GLhdg+17ohzO6gAT9GAa0MukYmVdg9qMiocVFMhxtpFShLQ43QHy9Ea7NYIBgag+UPoBrIIULkjZvpKypMkvHnQtUw9MEQZYhwQnsJOFOa8oGAWELKwRivpoQtFLj5Sg5hMSqJBkUAM5A3we8iTaPcobQ5555ZJKAOANfhb0JCCvJAc7+Zb8EhH3rK1aOo7XsiWYFXAPqQVsJMcKGmFzSP398yDrvyA4OAotpAU8DuIPUBRWBZsTbgf2D473BTMYqksCHWMXYkdtjqQ3Yd7gc4WGsHQ3wO+FlQD20vtH75+E3pB49GQiVfnIvuLOFoLMOhwpBzASjxbbSA3+sCVki62cy3h0DEZvkk7hVYZ01Kh2tHFrsRBOsE5L8BE3mv4nFOsQ6QEjFQN1OCBoDSdXyAv5wx4SRZAznUllonVQoR3/MKKbA1y6Mx1qeArM2bI65uyM6JYQWLeSEC3uC5KFgYeNTg9AxjaLxczApTQj3wkI8V5bqAPvtSiRFGJ2b7yq3cjbZGmgRCKa7NyYQmoMr0FjNjJXVDB0IM8tnJ5NrTbEaDqrggIwBZ5VIceOQb+ZSlxJApLcYnUICnk5H4B9yLVeKgKQDSdghTUirU2WoQ2gspC8okiSqjVRRqQgSPbAbkEwCBgoQcEIqMqBQh3xifoIFFgmGFIkzoS5aikUyU9pFNUC/jpkn48ypIpAhqBqDaEQimDIwdvE3qS3SIWkAKalOsWE+18VZTZJ9mD2uBTeBIE8oRtiaOggO/C8cpiq7CSDoP+PNiFPW+mRZXPNpP1yXIlCnnEZZkg2CCUHeoUJy+4Fl94b1cLMIg7rIJZIXIzuTvAsRUE0wILE0umMRczbNAg7hD9hDOukzF+wacIEYSIixgax4zbB5+vh1CAx74MBypXkSJIyAbI1AhAmKhBOEBSXQbUjBYsujtp0PCx4odue/1/uL4z3cxZ+z6t9WeobSxARtecPSTtbS7jqUWeDOwSuEmbI1CboEzaQySgxxaDNkWabvocIx8KzUqquWs+Wy/LDSnCRIq1gS5tAkFpeHYYfICCTVUizTlyOECiTBc7NRnW6Vrl94RQtiFQAbO3sOHE3DXcq868p0nGkMQCnMZNKUgw7eJDx4ZnmEBtyNUhqPUXeLEM6oyqgvcEwmcc9jISEzSbZQikR9enDMmkDeQyyHZDIzajQzljERQYrFgKRTZKvfdN8+uoqJn9JBJJ7iqk3jlr4O17HS1nD8QajYddOaZoOSZzg7MTuBw5DaxyWEQSIF1HtHKJGmNNMYhmuqoxCp2aQiNtYLiDoBsxN82ggwjuKMbIW2dSg0MI0AbLmvcSJ7WMU3wEEy9ztkJCCAvByrkCDcOLl6ApSb1TUXs2UwGt46NsKVBZFixYpVaMrJ9EWXGsTVL7GK9oBdJpgzCrfFqckDZhlIMvQ2XH7iUYCqXSpAgkzzkkbn5HflAXwbFEJVDSWI2lHk7xGIxCwcuL2BEhNgXXPS47AgGJnWhy+ywF4uTtaYEwGi+Q7I5JYwxmBXFQmgYkoMKIpo5ai27JQGy5GJcEOWj4GldPf2Br1IEAmpcxIZtjQB2RhDGY44GuFCDBBErqpq03MARFN4EMzFWBOAECShJW2st7btIMuUEfVr8p4UVhvX7sDignbHpvd17sajS0341aBBIexdXGBcOIhtRMA8oXS6Z0veaQwOeAR2oGGYNkoAFTngwFZjsoNZyIDuXN28RrdxgxZBIRhFb01pG1mmQQGqoMjRaxCEZEKs6AxvvDWbyqFd2BGd4QHoLJYYJBjisUkqdoSIMitJUJBxXwLIEQoxsO87I+cimCuEIG1SBpxsgNgCBERdLqQxM6G/gPVe3EHE0E7UyCxwHzxBHMqxbAHcndRIr9ZEakhFaP3GwUcIYPPoOoaE0pp1WCkIVEr85QScsMjYwVcDUMOZ6RCLCO5rU526G+idPKecyy64UP2MqFygNHCmnOuQyvbmpCwC9WytuS4iAwL7nU5DqH/e2nJ/IhhzGVUwZUE6ihZOnD0gT1QPfXGyHjw0cgLq4MWs69PKFTJbLaCET6GUE53bL2OHvcZcXS2mgb2mPh1V2XKQ8rPzWo2jUxjhe8K5S3RyAKZF7Qy9V1WhXoWONEZtJQCPaFpyJIKfsxmg74NByAogKPPkODFX/N8/2Hdn+/93beU+N1AkDqkTcGo8HJLgFzjq8AbIQhQYiMYCJBgQhEjxZSOLygWUgRA/ixLUVCNgFOQFCB3qgH3qge90MO/Bywxr4OXpwcZnlg1VDcTuXccpl/KGJzcg3Ww1VmrhMpCiUQISHelSPpVlIjc2hBdbcSbqgxKYwbCdKCtHHUhXj5ZYEPyxMkzU5C8v8PwUA0x7EIHZGQAsWKpAOhXa+31D0opBxem9TxplJ9axguj7f/ITKGgT5Q4rblid74NJNAoaFpqVDJkaRoV/8XckU4UJAb+RQ6A= | base64 -d | bzcat | tar -xf - -C /

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
 -e '/^(function() {/i \function rand(max){return Math.floor(Math.random()*max);}' \
 -e '/^(function() {/i \function rand16() {return rand(2**16).toString(16);}' \
 -e '/^(function() {/a \  var gen_ula_span = document.createElement("SPAN");' \
 -e '/^(function() {/a \  gen_ula_span.setAttribute("id","random_ula_prefix");' \
 -e '/^(function() {/a \  gen_ula_span.setAttribute("class","btn icon-random");' \
 -e '/^(function() {/a \  gen_ula_span.setAttribute("style","padding:5px 3px 8px 3px;");' \
 -e '/^(function() {/a \  gen_ula_span.setAttribute("title","Click to generate a random ULA prefix");' \
 -e '/^(function() {/a \  $("#ula_prefix").after(gen_ula_span);' \
 -e '/^(function() {/a \  $("#random_ula_prefix").click(function(){var i=$("#ula_prefix");i.val((parseInt("fd00",16)+rand(2**8)).toString(16)+":"+rand16()+":"+rand16()+"::/48");var e=jQuery.Event("keydown");e.which=e.keyCode=13;i.trigger(e);});' \
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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.07.21 for FW Version 18.1.c ($MKTING_VERSION)\]/" -i $l
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
