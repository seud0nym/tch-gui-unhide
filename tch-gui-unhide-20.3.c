#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.05.16
RELEASE='2021.05.16@17:20'
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
echo QlpoOTFBWSZTWZMNVV4ADhV/hN2QAEBe7/+fu///jv////oAAIAIYAqfBr1lLtuzHaxVa6OQABQFAAJBJJqGpkFPyT0FHiaj0IHoT1PKDTymgaABpoGh6gyENBTTST9KeJMR5QDQAPU9QNAAABkAOMmCaGQyMjJoaANBkYQDQaNMhiGgA09KSE0AAMg0AA00BoGhoAAAADjJgmhkMjIyaGgDQZGEA0GjTIYhoAJEQgJo1NABTwgT0IPVMJtGhNHlAaNNG0Ro/ER/jjb/8xcp1ATUQ82AHbeCBFbPs6X2OnXvOwuA3AjCst4QNb0peGAjAuBDQk2ADQkMEoEhpAwOg9ntf3uP1gpEPfKifuDuojpBn4OjRF3FJY/WI/HK6jdajaTd4moiW7rBZGAVSsFwZXl7KnMwqrMwr+X56ZMMK67ZpSLJdYK1kjIREZYgJH2WpfCSG01ZRcqMFZSr2FvEaJIQe4QwALMSBMSBXMQGpI1EVa1VUW0nCaEIBEIEkhAiffUWRgeJqfga6cqvyxOILJamw+FQBcWY+LjRG6IQxz7O96PVJ3VBtINuLQEd9DV3yELIClbWn8HemCsAB8hfuicmYOqi/XtUzEjWjtHOt38xK0gz3Q4ubeJbmmVARTsayuOsfvWuklDa624uB1DOq6q0rQhEKXrJmREjMiR02jAbBioCHetBGaLOF2EAZD9cdwYVvrXKy1CQq81vQxpFWCMh4wClnFbyFLvDNSzWqJZ71OkQ9hhoNtibojPcvquEuRUcGpwhzBhx3brRCJdqmLrxchie9JDcaitJlCrBc/lOrIRkkCu4W3Qx0Y1A06tV8nkLDBUQZbQZ4RlBhOteXhlQDan8RD6rFFMyoiJSklzIXX4YZYbCIvwPRGRKjodG3LiRwxANsiASUgTYTIEMUoROVzdXck7+TLM7ZOzYiJv2sh0VcVWIXpq89UBQmJRLh4SeGgOobhUMEIkMuOZOOJwzDNMLYIrI5XRdhSKLUidzxclFdcejZ4no8A1izMwalcvpNXQpdaJC0Q4b+D8tJusW2qAykhTsS3lqMfxGV1V4yvzN2QoDVPPKIpRy5x1RTvcpjLQhQMuqpjgVuOxRMj9IBXEJg74cayVG2302o26CeXOhPXQhQUxd6cosjUjeAXVMZH3lbrmUWP9ANjBGtRhSGQiYxRAxXVSc75JPYOsMCLSqJMO6JpZCIRqPNr1gJL2ts1N++f4ffj8ECO9ZDzF939X7Q3E7SWXA7xNhfCvtYLlWs3SFoCPZgQKhYhDpZQWCshBcBQuAsX30vLgGE+2LgmoUxJxC8LRZ0IvFS8ICcsAOQCYL3FfFjSwcrOzOanglju8lvwOnfke+wyBobAYMEm0kev6PX9epprplgAYX34ea/d5YZDRzPLZJSyqtDjn2ShrfMl0C6EikUQkCvS0+Kej6CnoSKwV00kk84fmEzYINSbpIH7kuYUy66khu/rpfnMakp9gTwk7rKDUHFUTAEa0JXidAELOqyvrnDY22g0e7fUPUDVLvcEXeoNpW3jOABKcAFiBZvL0LS6pdiclJyQE0JzNtlwsO3SrR7PZIucPMFgFoLgPulxDgTIN4HgQIqmAZiJSlVnwD7QOX994HB5MEecZ7u7MxBJ+AbV54GUlfeqCFqGKE0qnxYbih3+ghMaKIWYjeSpM9AEUSnN+uAU6qvjMD2dtB3StYJ7CPp7qzFui2LMGz3vz0TKoV/QBQhKyFlNpaJQijKJVioC3C7AYqF/lXRmhYnmGKb9zFxGC1RgDaMphCmXf10gmGlSZCSsFEVAkXBZG57FzhdYC9b1xcc7iFxcNOcvw5Z4EbSRkkG4GQNR1RQSZXu8J9doVYvl1pV0urVypMwiHShFGUmjl8XQqO08LReq1reijbvvFMSDFiNhhgiwqu5J2QLAGIVRfU0u0VQ/V48xFRr7Zxovs2KECsPlsfaicBIp8Ier6zJKqEBqNB2ZttnKsRLFfYnkhovOLiWRjx4JZ5AVM4EXBPmEqLI7aWy5t48GaMbdiZbzaPgkGRdbRAw7bE3Y1kRKFgfKlkkbFqgIRj4qeNg0Pk4iGeKCu+7YGK5200u6fGXpYEjQ2BAQqSDbFUiDyVT6NoFpKZFyF5aBnILgXKGdYUIZLFHAIMAojqYNggMT58JXI88VKX5rDg9y6UloLnH1m4DkR2HlZRFjo6c4aYdqLrze34fZCDEQe97/ILXOni6bdGi1DxIaxITAvJBYKUBnJkLer151qYwYoi/pmiX5OG5+PyEMr9nCFc7CC6wk8gEFBdx83OY8G4v0aHyjG4BPWiJld/v39DNEc0kXX967i6A8n40kKDQ234bwYlwUaqXiRimJpoOJXSgNwfPCCOxdMgy2Xnyz6WNHCutbdu95jcEDabbxkiITiGk2kCyeYSsbY60ph3AScW00IkYM7+dtbEG0NBVGiNIIpc5kghr0VoiVT5pp8t5pkl2DWAlbeghCsMTCUAxNC1V3R6ZOGYm/cSg7ND+DUm1QhZozNgwTCckXlUxczYWbPOuC0wJcWVRDfwAaWGWbfA7TCmyLMSzLy+xRoY19DLJI+lrt8q7FrkcA0i3UYa1SsGCOUEbxh9dBLg6YOlqqY5D0hz7fT0ai97NGCxEF2iCykOHfWGcmgEqRbOIXmokrGSvJ3rgAxEVhm0gIErQEoGJdrTQ9DVK5JjPOXndRFEE5ihH05I2c4oPQfw9aOQGmcbEd9+olRLqrHL74Qwg1SKAth8YfIbwOVCyXasBH6CL1gubhD4jBYJtj5DRQanQjpkfLFMYpHmpSnRhJFKBKVnFIuupWnfcW7coIK5FCiVWxXrFVS5f2bcUae3klprTov8w0R4xGwM94uHYjfuFXhurYVkbhgN3bZTCgjIptQcQaBYEclNmcZQ+e0utHQKMaCyasz+NyuOYvUAbFfF1G1lCgFOi246BLGBcHgy8zw5cczn4McjgXX1QFfk1IRIj1I3KKtNkkqJY0NIL2RnBe1kIy3oYxcK4wKl2qDQIvN5RKqpyDSMRoX3Bem5erFkhMMjb/my5hWsLXmTQlFi6ipvOg4ZrBSkbiFfO26h4fIjaIdVFjGzUBcHEjSFMN0170DabfSi1EWg+MojHcu3nBZ9p1HnzjOeuqWzmD93ESJeQE0vjD+ZMBJGaDvBqIGhI/+LuSKcKEhJhqqvAA== | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified tod GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified wanservices GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo 055@$(date +%H:%M:%S): Deploy modified wireless GUI code
echo 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 | base64 -d | bzcat | tar -xf - -C /

echo "$VERSION" | grep -q "^20.3.c"
if [ $? -eq 0 ]; then
echo 055@$(date +%H:%M:%S): Importing missing GUI code
echo QlpoOTFBWSZTWZGIcewADjB/pN2QAEBf7/+/f+ftvv/v//4AQAAAgAhgCv8s9xZsjN3GjTu2tBoaBIdsoAUBbCSSm0o2mpoaaZGTQepp6mhoNANBoAAGjQABKmTRPU0KeFJp6nonkgAeoA0GgBkAAaBoBxoaBo0yNNGmQGJggABoDQGmQGBMgSnpKBNCNTJ6k009RkD1HqAAAaaAAA000AHGhoGjTI00aZAYmCAAGgNAaZAYEyBIkEAQNRiamJiaNTU9TMpsiZMgaGINDT1NNqepp/kX3G8OB0h+RQTxYkGdokAc7Fy6313OuZBIIETl1HXTcDzvYriEhoEm2AJwxCaEwJMDTuikx+XpjqDD3uZw3WcqkuwwqFXqf2KF7JqXgjVWQUlGVd7TITSBZCuzF6fl9aLXXS/STZlitpjcoXNwSsSuUiY+CslJRSJSAga9C51rHyLSMyM3Jhz4bsXG002Xt1RVqt1GAnfy8CaG/Ul68eaZtKjdXF0FGW20litBX5AhJtX1IEC5hMElNMSQpCSF8drlIBE04iAhgtK/FWBpRSAITBjSGfevXjNy/0Zll3CV/DMurBuSmHWN3uGdg/rnuD8zZ4ht5OifurwGKrWUx4FbZlo5qsq5RRcZmc43tbajVMUU4Sh00C0vutEK7kaFJtRprBnlYSkRHRIGssm98N3fURYlWc3UDTn8VI6ObgU4z22F2Joo5Tun+M1ZTx8/ome7Jng7N2RqDVKfYLQz7hlY6IPPVzM38syVccJeKqQhwnZwMMhlKLzyrA5TqeLicLro1024zYUSGDY8ztTKsRSB0XmTVA4FcJFxdILrHcdTnIgDG/tkgsphlvlKdDIHMlGGWnn1SlLhM53Px0lSyw29N5HmzmybGZnWRqmR5LbJLbtuHtNOX9OepkK5Abd0g87Ory6/TwmWbK1H2S/DqQvssoiplJ4YSNniuumnbyGYGRZWrnkt6rFI7dUYRoIkpRNOD5thxkxy0KQ10RF0Zdh+Tj0ipAHhssgRAdRgXcLaabFFaSk8DS00rmiUyArDGPOJitK1bhgtvI0KyxVVWXEjisuICFwmQu07Omycj46QUq/D44l5LTypzL4IygLpQikqOa7jr/YZ4d/Qqn4R3W41nZKBsIa/ewwu599aFFoOtXD5oitfTK2eorrQtqzNU4Yx3gH53E8rlg6REDVQlDik17md79OXZq48c8MdxMri3lCNnEz44Tr5lBnojNR7DqPyNLhPHL3GWQvkpTIWBLXWL/hETar/Ici/FCkcJbiQHzalj4wJlX8Xyxw2w72nspNoHLBJFlt+wooDoQZYFavvYz5QaaJHImE5kqBvbYCcpjgBXOyxd2vXXEEvG4GGYZzhbO/wRmrPR9Zr2UPYwxF+jKQ2fYg+dC5j14FZWH2519H8KllmcFmi0zsCgrIIb2BFoTslR2omzOWuRO4UisMetKyQNQYn8NRfuAHDYbCWdrN3WVeVq671O7uqqhDvNIJQiFKALRVV0prW/klXQbY3Z5ypIoQQW/C0xv16vWrl834fQjDMzwy2SJnn+8gw9cPPgBMpjCGob5AkbO5FsfuTg4Y4G5CMH2zMtIDwvTFj/uhqc59BjIwZXnyQz2dHIw501Ql9QdQ000Vexs+R7YdbzqDpaK38DhNgqjScx7xwRhAGowmVUFdCXhSYxm4ILTITWFHC3tA6rsGzmUDJop9mYE0cigsBT8wApZiXm5Xn2eoUFTxHdcT6gywR7ErEjQW1IO5oPbQxLgyx6CRuJCgkGYfsWJaKwVh+ppWqv6wuQbFmLYva0iBreDYUMEHwMBjQe86rtn3WrrYSP5+qPr378IAm99I0J1w5hAIKURBMbFD1bx2Y7TmrSMWg1kheXombgJTye4gDwqtitNtIJMEe0eY/FMlKUSk2mu1yblKRbOYTtpCKJB7c3xgMxajpw5hhupcZrIDNT8Ty09h4g4hZUCFwNxyDJnBaradvUloroBuqHXwitjukXP4RmlpFHdEX3KrSmUWWkpzGrZldY5qLSrTgGlbCvCa0HTQd3MLWZUgk2NBj5sjiH9pngZAIyaSbTYxoG0dBz4YqYBeukoH0r0q08UOFKaY9AvDy7tQZIOJkJ2MBtFkiAAk0lnOBI0Fx0a4lgzFcV9pkqCYNWk5ZF9CFawyWIsQD6NaXMdPU3I8PBRvHvONbbaWM87DUdm6OHXAST3ilAFSRB5F4BvUGCDWYBYaBV2pC5gDkxsTtZ37riVvrz6H2ZRx7JTsSyjcrlb6CLEjR1DCERw2JDUA9rqwrXAx0GqJtStoWGuI0Qd7OJrBgvJwXMPCJB6g7aW3Pj429JOJznO4+l9XQGB3rV57hdZvMuzXsSMlUIEDGHAzGmxFxnPFBmFgqbjvRezkMCC2t2/iouTe5FlUAtR2ETS7AgKMY1JDIZ1lho8AHuK3hcSCyWlbDenJV85mcE9abDxkKOpRzSRngVsu+kkHEhbURFsnYSJhJQ4QZLJHwpkooMfUJZACLkNtiZwhI5Ab1Ao6V44OUlqohLCzbEl2IC4Dd0jChWUtDipO01K/kEJLUlcDkkO5eRaGJdqFl3ZBK7zk0erDtp2u0LTluJs4j2AyHopbQe42cxiJGqBtkIko2IV7JAmSLwM2zOIOUDec5yYw4WqF9Fx2jYxjBl5QMWAmshURGjfYLkxrtQC8r6ByJrrxQjtoFPGIssMkHu9bGgmEkEUCSS+Xt05JtteEJiqaoF8lTgNGa5gXwJrc8g91yqlQmqsRuQWNJZBsMDkLWdaXE6e6JmdCV3d6COzr/utQBzoGCSekBf5FbDLEG34CUxIZQYiegjNI7hQEIWuCBg0goxC7GhToJjEVlUgCnSBO+1IzHzxYF4KBIsuSvyGTOioWi/EVqodJh6XIRmkZCaFkmDL0R0V0QInjhO2/iaVM0MmnOI1HKfNphgNQB5nDfDvucBO5L5znlcWmKKo12re0o43B0SMQjkFoXln5rKyLdqaTR6Wo5s2GYs4W4vF6jExkY1BdGZtMSDdNxRbODmUj2SqtrQtrL2KSIkBOhcnQJNVsGxyKpLE8ryolMKiyncXFL2gvGeO4JuziT1lQC9pYoRfAGrWxoNaA+quQXwLvaux2QaoSU0RJYsx7FfYtYb9rSY2e6FwWJoaoGNuPeg1VA8hiqFVY5IkM2EveLeHlheYiC2DDqNKOCHNqddH26qQM60zIX9NpsAq4+k02JL99j+YzJ+4DDJIo1tSAUQiQsVF1yAtXA17x8wyCUJI4e/9IemXmilAcNKYAv/i7kinChISMQ49g= | base64 -d | bzcat | tar -xf - -C /

fi
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
  echo 070@$(date +%H:%M:%S): CWMP found - Leaving in GUI
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
 -e '/uci.xdsl.xdsl.@dsl0.enabled/a \      wan_ifname = "uci.network.interface.@wan.ifname",' \
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
echo QlpoOTFBWSZTWdkI9AsAbZR/////////////////////////////////////////////4IWd98AUUFQFuvb3O0U+vPZ0l0rz6e9L187u2ty253XSgdtq666uju3EFtm8+pE87774NPr70lVfbjXp9z67dvL767kBvucK3c4tsQjrDvOc297zvll7267ty77c53M9d70oW7qdDnnPd9757rted973K681xV3edtvPZxNd7ubs3udr3ee917zu6bG2o3t73nq7Pec5rHwO7upmMPbKTWiAtGu2VplzAG77y5o8pvd0zNKX0wF9MU98feekO7h1XrBQD3Ob29ej77uZstfaXz333vte+znqpppSrb653d27l2WsV3YU23SiSWm6anC660tXcjLK23d2u6pli1tZc46rkmbZuuty23YqUmmggmKmg7rc3c5Xdw3OuWxtjbW50mu27t1bbrKt27bUndpm513ZR1jUtWnU7rPudy9sOnLqnd3c5uNoV10Lbr3vZxdMRVNAAAmBMANTCYAABNMJkwmJgmExMCnhMIyYAEyNGTEyaYmJpgBMJgIyYAAAAAAAAEwAUIqIAAAJgmJgBNNpMAmAAAJgAAABMTRkwTJ6GgBoAAAAAAAI00YhkaaaYBMIGJk0MJoZQNUiAAEyMAJpiZMmTAJhNMADQaCYAANA0NCYmNCnkDTKp/pqbTQ0wIxTYJkmGRkaU/CYJgmphoAwgaaNDTRkFQioE0AMmmk2Uw0aMg0ZTGjTRppkamARpkMTTTTJoYBqbQCGjIDI00yYSek8JowFPIGBBknlT9NNBtCnqbJk0AMmQMmIT0aoIkiCCAIBpNTYUwgnppppo1Jtpqn6ptNlPU9RmpPaEyeqfkaaMmSno9CT1MeoRhpptTTUek81E9JtkFNiT0xI0xqeEaeqbQ1NPSemUeKNtU9I9JiGZTJppoEkkQRoATE0yNBpoA0GkwmACaYBoAAABMBMBME0wJppppoyYIGAmmQ00MTKehppkwRPTTIZNNAyGjTFNpk0yhgPFBy+90n5d4dT73bMXsAd/URaNUrUnq8X567/ltc6LaXGMIFhgeM4OPmdfzTa850+tVaLg6U75KlTlGq8Wn4FrrfTuFR08TRJJKKJO9PvFcvjpbPW16C0Fu9PWVEoqBUIwKooqG70V7DHsZRDDyZ+Db8HK2jnzkCMHOEm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 | base64 -d | bzcat | tar -xf - -C /

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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.05.16 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
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
