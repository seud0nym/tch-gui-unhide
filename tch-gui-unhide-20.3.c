#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 20.3.c - Release 2021.08.18
RELEASE='2021.08.18@17:54'
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

grep -q '@media screen and (min-width:1200px) and (max-width:1499px){' /www/docroot/css/responsive.css
if [ $? -eq 0 ]; then
  ACROSS=5
else
  ACROSS=4
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

while getopts :a:c:d:f:h:i:l:p:rt:uv:yTVW-: option
do
 case "${option}" in
   -) [ "${OPTARG}" = "debug" ] && DEBUG="V";;
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
      echo " -T               : Apply theme ONLY - bypass all other processing"
      echo " Update Options:"
      echo " -u               : Check for and download any changes to this script (may be newer than latest release version)"
      echo "                      When specifying the -u option, it must be the ONLY parameter specifed."
      if [ $WRAPPER = y ]; then
      echo " -U               : Download the latest release, including utility scripts (will overwrite all existing script versions)."
      echo "                      When specifying the -U option, it must be the ONLY parameter specifed."
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
echo 'QlpoOTFBWSZTWXGxPF0BhGN/////VeP///////////////8QVLzav6DUS5lWoKVJlDcYYon7vb3d
jfYG6HQ0PPvimVVXNrbE2xbrrY6aE97dVApdgdBHMAioUo9Bw7Rdu4BVmQJB2z7Z9tfbaao+jEUK
UKV3tzre26qe+2nnodZ9Bqq9ndqpbc43rq4VoOOffe970m2fc70aUpc99n196+299rasiltXQM+O
gBXR2wO+xc7cBp4KD1K6zmYyQXTFV2GAqpAppql93u9QhVBdgxLYYFAADQAfb6Gs4Dp9H0AARAG+
93ofaezonsGJE+7u2G9h0OTLvQaeyr2etC7AXrrcMKWg7ZXesL7x9D3ihCkiT1usZ7dA0A0L1i8f
d323sNApKqKKoEGtk52BOSamCFNz2tc41vvtQAAAAfRqgHXHhgACPWAaAACqA2wqgAA+QAAAenec
+zQXsj053qgABw9OmpBveaHtgaDQyKL7B3Y6Cj4HtdHunvEVdd3XfZ0fb47vvferd073vnvanvOf
XhzZKnp7g1IqgoBRIokCEoPbAFAoFAGgBkSB1qqegHt93K3evY+yjL6zXPuOt6XO3s51913O73Dz
e233Hdgnbztr7MVrpxAWmbYPb3DhAePq157559AHToQSqAUl97PWs+F823ffdz6D18hSvtrdzQD5
DEdAAHe09ltR6SOR61fXr69dj7sC2h9999757rFKAA+cDegAK9A0LYND7ue9pFs73tOT3pd0HjAD
66U018ntfbjth6O3o966F9j6KOqa6bsLzvvV3kdQdb3cen3Dfe5fOzmOeq16ZtuvXvLdqxbeXyH1
bbt91brx2nsrPvue5j3durotvTp97uvnc587lemXzxnPvXPWDZ773Re++7ybM97eveXwApc9V7rr
e8c533e8XS9Lr0SK8rEjO77mfZXRvtVsPoPh5Uvo0Qodst3HvsOdjfPnXu6D6Dd850n0Pndvbz1V
q9h72fXXyPQqnTjc33nuwevdzPvdp29199jVfc3QD5pn2333Pvvvdrzayvu3B6ProdeIOkdg6Dmv
czvd99594rW7u7MqtUvN9A9sNZebcvt7r2d7vXc97hZ3Ua+vPe+M9evd13t3ts+IbjNGz6++fe6+
fbem51zOu+wWz653r731lV2y1VdEJFSvqmlJYptvvutt6wX22501ku7b2O+1d997vld6vtsu+uWm
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
sWmLzkDM3i/CSgF9QMYX9tHGhkmrpYuhhTMWzZjJJ6KEMmDn91opNCq04hHIGuJM24pcGrWogvzn
5eqTesXKHv5KjGayD2zTpvSUdEeS80IFlmTOJBUp4+VKm/eOx+D+vjj87inqLL+nAeCNPm93mfh4
fN50uMnyVJJOlPUWq6ioNVI9deUVI4VDa8K50m9I/5v7ez+zP+pX/ip3Pj/O/S/1/5/3PK+z6Pvd
H636Kf2z7X2v2f2/0/2Nf+vv/2/le1/e8Ps9H+L9f1/9/lnnfqfZ/Y/W8H+P8D3P1/t/Z6f0L73/
X9X3/1/zPd9/5n6Xx/++/+b/2/B+Dy+j4/1f+//39D/l+R+QfFft/t/u9P0vsfufX/L8H7Pc8X8j
2fg+d8X/z9r9z934v6v0X4+18X2fv/0/j/2f7vj+x9T/90fb+Ps+19vo4fF8fyQ/u0/w/dp/Y42N
KQ+v7DML+i/xSwy28H/LiXIUkofVkoBh/2WK3Kw7Br53axGxS9yus2c+q8uuxaLwZRwM4DowMF/6
ocmJXxDOxR2ArB9aDSUHyMM45HyYeaOA4S2q5a5K502woOQJsU4Gxw2JOR0Asth+F/a6r9VPz/JA
YH5oto83y/6prhXDUMqLUD/XMmv5dGo01jq5ewXP01DDYzr/WdrxNt4L/qPRcLGx9MpKU9JSycaD
gYaAtrirolRBMX/MWxTYf+sHI5m2EjkLRuqEOmBNEbq9GU/1vuE57hswZRmUUfHgxXxnF/v3D2/6
31n3z4775D/Xve2NtttuxT/zDCkg4eqLZ6/Q+EzKRXDHc66kJlt4caTNUr2baf+lBxFzoK60abTr
ZsbbbfILNtuPCAuK9q64n/Rn/37iR3sr+GdZ/fgcSdH8adXZMW4i1PODDrnb2LtQZ/gPCAqgLQw/
73+uDKYQihqEAf+MPAKhRTy4QbUmD6IJOCaWGggMhAWxhdqXpzuOTAOeSA8j0rsbTGDFh5vE6iC2
SUQjHf9tETaH5mG9HS0v9AxtddKB5gCe3vpQaJqF2dIGhImf997JaxQKE0p8eRApoALCtAlwCwB6
qqFgu7xmjxBnpLt4Zz/8xxQM49qElVCSPS/9C2Mm0xv3pQt1TKoaCSW9H/tnE30PMBvDSySWLzbf
NNpIkjn58kSJ/x52c31hXA+Eknj8DSmEkdh7ZYXaSdx6wpvSlKzgA5KeCBSfow39HY9QRoXW/4bu
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
    echo "070@$(date +%H:%M:%S): - Enabling $MODAL"
    [ "$DEBUG" = "V" ] && echo "070@$(date +%H:%M:%S): - Creating Rule $RULE: target=/modals/$MODAL"
    uci add_list web.ruleset_main.rules=$RULE
    uci set web.$RULE=rule
    uci set web.$RULE.target=/modals/$MODAL
    uci set web.$RULE.normally_hidden='1'
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 4 ))
  elif [ "$(uci -q get web.$RULE.roles)" != "admin" ]
  then
    [ "$DEBUG" = "V" ] && echo "070@$(date +%H:%M:%S): - Fixing Rule $RULE: target=/modals/$MODAL Setting role to admin"
    uci -q delete web.$RULE.roles
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 2 ))
  fi
done

echo 070@$(date +%H:%M:%S): Checking configured web rules exist
for c in $(uci show web | grep '^web\..*\.target=' | grep -vE 'dumaos|homepage')
do 
  f=/www/docroot$(echo "$c" | cut -d"'" -f2)
  if [ ! -f "$f" ]; then
    RULE=$(echo $c | cut -d. -f2)
    [ "$DEBUG" = "V" ] && echo "070@$(date +%H:%M:%S): - Deleting rule $RULE for missing target $f"
    uci -q delete web.$RULE
    uci -q del_list web.ruleset_main.rules=$RULE
    SRV_nginx=$(( $SRV_nginx + 2 ))
  fi
done

echo 070@$(date +%H:%M:%S): Processing any additional cards
for CARDFILE in $(find /www/cards/ -maxdepth 1 -type f | sort)
do
  CARD="$(basename $CARDFILE)"
  CARDRULE=$(uci show web | grep "^web\.card_.*${CARDFILE#*_}" | cut -d. -f2)
  if [ "$CARD" = "016_speedservice.lp" ]; then
    rm $CARDFILE
    if [ -n "$CARDRULE" ]; then
      [ "$DEBUG" = "V" ] && echo "070@$(date +%H:%M:%S): - Deleting rule $RULE for missing target $f"
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
    [ "$DEBUG" = "V" ] && echo "070@$(date +%H:%M:%S): - Card Rule $CARDRULE: card=$CARD hide=0"
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

echo 075@$(date +%H:%M:%S): Checking ajax visibility
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
    [ "$DEBUG" = "V" ] && echo "075@$(date +%H:%M:%S): - Creating Rule $RULE: target=/ajax/$AJAX"
    echo "075@$(date +%H:%M:%S): - Enabling $AJAX"
    uci add_list web.ruleset_main.rules=$RULE
    uci set web.$RULE=rule
    uci set web.$RULE.target=/ajax/$AJAX
    uci set web.$RULE.normally_hidden='1'
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 4 ))
  elif [ "$(uci -q get web.$RULE.roles)" != "admin" ]
  then
    [ "$DEBUG" = "V" ] && echo "075@$(date +%H:%M:%S): - Fixing Rule $RULE: target=/ajax/$AJAX Setting role to admin"
    uci -q delete web.$RULE.roles
    uci add_list web.$RULE.roles='admin'
    SRV_nginx=$(( $SRV_nginx + 2 ))
  fi
done
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
    if [ -f "$f" ]; then
      [ "$DEBUG" = "V" ] && echo "090@$(date +%H:%M:%S):  - $f"
      sed -e "s,title>.*</title,title>$TITLE</title," -i $f
    fi
  done
  sed -e "s,<title>');  ngx.print( T\"Change password\" ); ngx.print('</title>,<title>$TITLE - Change Password</title>," -i /www/docroot/password.lp
fi

echo 090@$(date +%H:%M:%S): Change Gateway to $VARIANT
sed -e "s/Gateway/$VARIANT/g" -i /www/cards/001_gateway.lp
sed -e "s/Gateway/$VARIANT/g" -i /www/cards/003_internet.lp

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

SRV_firewall=0
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
if [ $SRV_firewall -gt 0 ]; then
  uci commit firewall
  /etc/init.d/firewall reload 2> /dev/null
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
fi
if [ "$(uci -q get firewall.ipsets_restore)" != "include" ]; then
  [ "$DEBUG" = "V" ] && echo "115@$(date +%H:%M:%S): Creating configuration firewall.ipsets_restore"
  uci set firewall.ipsets_restore='include'
  uci set firewall.ipsets_restore.type='script'
  uci set firewall.ipsets_restore.path='/usr/sbin/ipsets-restore'
  uci set firewall.ipsets_restore.reload='0'
  uci set firewall.ipsets_restore.enabled='0'
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
@media screen and (min-width:1500px){.row{margin-left:-30px;*zoom:1;}.row:before,.row:after{display:table;content:"";line-height:0;} .row:after{clear:both;} [class*="span"]{float:left;min-height:1px;margin-left:30px;} .container,.navbar-static-top .container,.navbar-fixed-top .container,.navbar-fixed-bottom .container{width:1470px;} .span12{width:1470px;} .span11{width:1070px;} .span10{width:970px;} .span9{width:870px;} .span8{width:770px;} .span7{width:670px;} .span6{width:570px;} .span5{width:470px;} .span4{width:370px;} .span3{width:270px;} .span2{width:170px;} .span1{width:70px;} .offset12{margin-left:1230px;} .offset11{margin-left:1130px;} .offset10{margin-left:1030px;} .offset9{margin-left:930px;} .offset8{margin-left:830px;} .offset7{margin-left:730px;} .offset6{margin-left:630px;} .offset5{margin-left:530px;} .offset4{margin-left:430px;} .offset3{margin-left:330px;} .offset2{margin-left:230px;} .offset1{margin-left:130px;} .row-fluid{width:100%;*zoom:1;}.row-fluid:before,.row-fluid:after{display:table;content:"";line-height:0;} .row-fluid:after{clear:both;} .row-fluid [class*="span"]{display:block;width:100%;min-height:30px;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box;float:left;margin-left:2.564102564102564%;*margin-left:2.5109110747408616%;} .row-fluid [class*="span"]:first-child{margin-left:0;} .row-fluid .controls-row [class*="span"]+[class*="span"]{margin-left:2.564102564102564%;} .row-fluid .span12{width:100%;*width:99.94680851063829%;} .row-fluid .span11{width:91.45299145299145%;*width:91.39979996362975%;} .row-fluid .span10{width:82.90598290598291%;*width:82.8527914166212%;} .row-fluid .span9{width:74.35897435897436%;*width:74.30578286961266%;} .row-fluid .span8{width:65.81196581196582%;*width:65.75877432260411%;} .row-fluid .span7{width:57.26495726495726%;*width:57.21176577559556%;} .row-fluid .span6{width:48.717948717948715%;*width:48.664757228587014%;} .row-fluid .span5{width:40.17094017094017%;*width:40.11774868157847%;} .row-fluid .span4{width:31.623931623931625%;*width:31.570740134569924%;} .row-fluid .span3{width:23.076923076923077%;*width:23.023731587561375%;} .row-fluid .span2{width:14.52991452991453%;*width:14.476723040552828%;} .row-fluid .span1{width:5.982905982905983%;*width:5.929714493544281%;} .row-fluid .offset12{margin-left:105.12820512820512%;*margin-left:105.02182214948171%;} .row-fluid .offset12:first-child{margin-left:102.56410256410257%;*margin-left:102.45771958537915%;} .row-fluid .offset11{margin-left:96.58119658119658%;*margin-left:96.47481360247316%;} .row-fluid .offset11:first-child{margin-left:94.01709401709402%;*margin-left:93.91071103837061%;} .row-fluid .offset10{margin-left:88.03418803418803%;*margin-left:87.92780505546462%;} .row-fluid .offset10:first-child{margin-left:85.47008547008548%;*margin-left:85.36370249136206%;} .row-fluid .offset9{margin-left:79.48717948717949%;*margin-left:79.38079650845607%;} .row-fluid .offset9:first-child{margin-left:76.92307692307693%;*margin-left:76.81669394435352%;} .row-fluid .offset8{margin-left:70.94017094017094%;*margin-left:70.83378796144753%;} .row-fluid .offset8:first-child{margin-left:68.37606837606839%;*margin-left:68.26968539734497%;} .row-fluid .offset7{margin-left:62.393162393162385%;*margin-left:62.28677941443899%;} .row-fluid .offset7:first-child{margin-left:59.82905982905982%;*margin-left:59.72267685033642%;} .row-fluid .offset6{margin-left:53.84615384615384%;*margin-left:53.739770867430444%;} .row-fluid .offset6:first-child{margin-left:51.28205128205128%;*margin-left:51.175668303327875%;} .row-fluid .offset5{margin-left:45.299145299145295%;*margin-left:45.1927623204219%;} .row-fluid .offset5:first-child{margin-left:42.73504273504273%;*margin-left:42.62865975631933%;} .row-fluid .offset4{margin-left:36.75213675213675%;*margin-left:36.645753773413354%;} .row-fluid .offset4:first-child{margin-left:34.18803418803419%;*margin-left:34.081651209310785%;} .row-fluid .offset3{margin-left:28.205128205128204%;*margin-left:28.0987452264048%;} .row-fluid .offset3:first-child{margin-left:25.641025641025642%;*margin-left:25.53464266230224%;} .row-fluid .offset2{margin-left:19.65811965811966%;*margin-left:19.551736679396257%;} .row-fluid .offset2:first-child{margin-left:17.094017094017094%;*margin-left:16.98763411529369%;} .row-fluid .offset1{margin-left:11.11111111111111%;*margin-left:11.004728132387708%;} .row-fluid .offset1:first-child{margin-left:8.547008547008547%;*margin-left:8.440625568285142%;} input,textarea,.uneditable-input{margin-left:0;} .controls-row [class*="span"]+[class*="span"]{margin-left:30px;} input.span12,textarea.span12,.uneditable-input.span12{width:1156px;} input.span11,textarea.span11,.uneditable-input.span11{width:1056px;} input.span10,textarea.span10,.uneditable-input.span10{width:956px;} input.span9,textarea.span9,.uneditable-input.span9{width:856px;} input.span8,textarea.span8,.uneditable-input.span8{width:756px;} input.span7,textarea.span7,.uneditable-input.span7{width:656px;} input.span6,textarea.span6,.uneditable-input.span6{width:556px;} input.span5,textarea.span5,.uneditable-input.span5{width:456px;} input.span4,textarea.span4,.uneditable-input.span4{width:356px;} input.span3,textarea.span3,.uneditable-input.span3{width:256px;} input.span2,textarea.span2,.uneditable-input.span2{width:156px;} input.span1,textarea.span1,.uneditable-input.span1{width:56px;} .thumbnails{margin-left:-30px;} .thumbnails>li{margin-left:30px;} .row-fluid .thumbnails{margin-left:0;}}\
@media screen and (min-width:1500px){.modal{width:1470px;margin:-290px 0 0 -735px;} .tooLongTitle p{width:190px;} .smallcard:hover .tooLongTitle p{width:160px;} .simple-desc{margin-left:0px;}}\
@media screen and (min-width:1500px){.card-visibility-switch{float:left;width:20%;}}' \
    -i /www/docroot/css/responsive.css
fi

echo 165@$(date +%H:%M:%S): Deploy theme files
echo 'QlpoOTFBWSZTWX8k8h0Ab7N/////////////////////////////////////////////4Idd98AV
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
l/4u5IpwoSD+SeQ6' | base64 -d | bzcat | tar -xf - -C /

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
    local isDestIP, errormsg = post_helper.getValidateStringIsDeviceIPv4(network.gateway_ip, network.netmask)(value)\
    if not isDestIP then\
      return nil, errormsg\
    end\
    isDestIP, errormsg = post_helper.reservedIPValidation(value)\
    if not isDestIP then\
      return nil, errormsg\
    end\
    isDestIP, errormsg = post_helper.validateQTN(value)\
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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.08.18 for FW Version 20.3.c ($MKTING_VERSION)\]/" -i $l
  done
  echo BLD@$(date +%H:%M:%S): Auto-refreshing browser cache
  for l in $(grep -lrE "['\"][^'\"]+\.(cs|j)s['\"]" /www/cards /www/docroot /www/snippets | grep -v -E '.js$|.sh$|.json$' 2>/dev/null)
  do
    sed -e "s/\(\.css\)\(['\"]\)/\1?-2021.08.18@17:54\2/g" -e "s/\(\.js\)\(['\"]\)/\1?-2021.08.18@17:54\2/g" -i $l
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
