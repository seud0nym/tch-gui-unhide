#!/bin/sh
echo 000@$(date +%H:%M:%S): Built for firmware version 18.1.c - Release 2021.08.18
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
echo 'QlpoOTFBWSZTWQIJE7oBr3j/////VWP///////////////8UVKyav6DUSxlWoKVJlDdYY2xe8evt
8vRjHilSHOq+zAdU1qh6OtvpezxSfOe916gF9NevrnXicl9bVtQb2dXb3w3r029nKr1e+7ewVktl
7O97726+uVUp83uGAK6MrqjPXd3zbT53svbj6g2t3u9eXe8e+X131vobd7hwPsmj2kttshVN6277
3d4PlKlAVQCcHqWjVG73B7rO6d6eBX3R0bfO5Y2773evtnu0+91FO3cOKlabTaX3PeZd77jwn1sy
PLU6yfQOAAAPr6AHgD7uCKr7PZigSvPbkm8+33jYD4nd7x6B57rztvfe9Pbet2fepx32g9Pb1nu+
3sbvax20++py5SvXat29Nex99y771V9tu7zX2tTyLM+91999647ONXY6O17dZN3j7vb7zq9UVJdt
J7twtfTg43vvm1rn0+fOXl53owl3qi+6xW+fN3eveAAKoHTWRpoy5d769tbe0AH29e8D6fTbapQ1
CXM+5ifWKKKDoAAAAe+e3j6oD3z3X3z77m8+AAAb3tN9agJ99999Pe9ucFUNmpeW9bt697Gm9gGg
+9ej3qvuw9e976LEGD6TucttvfHfW75zuan3NfWZve8PvfF73Dqu3vW3XOH13D6G7u7ucALYBoOg
oLrByW+u7LAKV63tq73dJ1pI+bQqIKUkF3mcNY7btuwOukVAQgAoAAKM5gO++7x6owB9OWw7y3K8
ykoAdU9t6+2AG9312ufV4sfe3XzC+43V8971vry7248+14ebXbU953L3u3a7Pey9DZ24VwKFAQus
J2AMdq0HXQ4kgDbUChQKC2GrzB74Z3On3dvAoKkHZ8ObNQ0KZjuvu3SOeLe9z7u+X2D5rPToGxuz
1vHgO0u3W26b21zGWrZe33sn3zqKC2+9vbu7bru99fPe+W5y2lVDPc0qut7cdbfIzgC0oH21ovdw
Hu8Nr0Pdh8nLt4I4PRK23zHPu3x5R2b5MpL2NE7W3vgvfffTQaIKp77ulegaVIVssJtWWs+71Y3k
eRXp8XnmNPu99t59Ap9r2rpLRjYDSqJUoCJCACid73nrfeWceSCJte93OzQ1u3vt58fB6A0UAAd5
XvvHjwC954PADQKqhIJVQBBdh3A316+vr77zd73xh4HoK3lnxllvtbdeW29tXT1oO89vt7329Pvr
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
KTba3CP6Xh/WWRy2wk+yNiz+q74/lRLMjjlONSfVAuqFIHmrqmMQPYr8OuGVOqVN2gXvaMR9F40e
o6R1zROnpkbxV5NfJDpS6ARnPphhKafKNOiLfsb92dmkur+WB3Z9P7NNzrZU6ft+HRLvWJ/Zm+5j
o6OXOq7Mh227Q7AUOXWzMlZOV1O53Tgo3Qw/2Lyc4fK6lm3B+1XhR8ci3sqK696d1dqzCtu/Jq7r
X3i0fD+mCDYfmuumQ1G3L94sIazqXBPZqhDKFuUPzPxLC5w4VdOzm6/of85UJdRPHweo4J05aUPW
qPAXWWVKLHOpkqWG5wku5+qEK1XbJwos09T2bvBAVAJ8jBTL8GqUkzQ/MwQyGCExg9qt+GTzqZJw
Yxm1BmSFHf+dULJQckysVrr/bkQx38b8j8iyjjuh554WVN1c4nnL+dLvoRDCXP96kbMJbSBxkf9h
P17UVqA+xBpeEl9iCENCaRCEkDJ2RQ36v1WY9DGcojBojrTYdGK+cRGWmZJ4nIoFpb9fj6zroH3S
ldwBD6GRB6NKsWdXE+dx9XzDjaOfJRcMO2UsnAmmgrdYlLUFGQPD4bOx2gO2LJ9DOh99xc6HVej4
0a/vTgiVHR90vWURjEus3JuN2baqSTefYD/wB2H3dky/lYurJlTZ7JRzCZyU5lnq9THPPBkwZKVd
vRvtusnRj9qpKvWbTrhSeiMDa9972SnXZu6s0H+nlXvqGvNiY3bLzDYdmekOvH2W3SzxrxAVjC2b
EEgwzNEVOXXwxRe0u0VdyJ9EkkeeyVW1vTL1fqiplOGLn0YB3EmTmHauxm4VG58K1V9M/qd757yW
iB51ZfxFHnK2T4J9s/L5Ou3WmawoeDt+ZP4Y5GZSf17v3pt6OUPn6/ubHe+UtvX/VXK+1p422GSK
ZsDJIYZy1+eCTQ8kIVnBVrri3zUhxkJ01eq9WZWa17SV38HE3bEojZBgifaLbLbbhNrEXHm2LYUq
pxJ5nJiCqeCHs7OEJc4PnQTih+qu28mPNVtsTBGFK8Xnneg8oitmJLH+PV31Z0W9DHffa5jEVuy+
smQSpU2Io0DSkKupD15kutkdTclpZHxDywm+BrwKNI6/Hs+G87afs+LZ59nHl8MN52O0iPefT2lp
SPvym8us6H9owoF4poSjK+THBzpcao1o5nLcGAXXQl3pfKMOGOC31Q8yP67crKv1fCGJ3TeHfEpg
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
@media screen and (min-width:1500px){.row{margin-left:-30px;*zoom:1;}.row:before,.row:after{display:table;content:"";line-height:0;} .row:after{clear:both;} [class*="span"]{float:left;min-height:1px;margin-left:30px;} .container,.navbar-static-top .container,.navbar-fixed-top .container,.navbar-fixed-bottom .container{width:1470px;} .span12{width:1470px;} .span11{width:1070px;} .span10{width:970px;} .span9{width:870px;} .span8{width:770px;} .span7{width:670px;} .span6{width:570px;} .span5{width:470px;} .span4{width:370px;} .span3{width:270px;} .span2{width:170px;} .span1{width:70px;} .offset12{margin-left:1230px;} .offset11{margin-left:1130px;} .offset10{margin-left:1030px;} .offset9{margin-left:930px;} .offset8{margin-left:830px;} .offset7{margin-left:730px;} .offset6{margin-left:630px;} .offset5{margin-left:530px;} .offset4{margin-left:430px;} .offset3{margin-left:330px;} .offset2{margin-left:230px;} .offset1{margin-left:130px;} .row-fluid{width:100%;*zoom:1;}.row-fluid:before,.row-fluid:after{display:table;content:"";line-height:0;} .row-fluid:after{clear:both;} .row-fluid [class*="span"]{display:block;width:100%;min-height:30px;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box;float:left;margin-left:2.564102564102564%;*margin-left:2.5109110747408616%;} .row-fluid [class*="span"]:first-child{margin-left:0;} .row-fluid .controls-row [class*="span"]+[class*="span"]{margin-left:2.564102564102564%;} .row-fluid .span12{width:100%;*width:99.94680851063829%;} .row-fluid .span11{width:91.45299145299145%;*width:91.39979996362975%;} .row-fluid .span10{width:82.90598290598291%;*width:82.8527914166212%;} .row-fluid .span9{width:74.35897435897436%;*width:74.30578286961266%;} .row-fluid .span8{width:65.81196581196582%;*width:65.75877432260411%;} .row-fluid .span7{width:57.26495726495726%;*width:57.21176577559556%;} .row-fluid .span6{width:48.717948717948715%;*width:48.664757228587014%;} .row-fluid .span5{width:40.17094017094017%;*width:40.11774868157847%;} .row-fluid .span4{width:31.623931623931625%;*width:31.570740134569924%;} .row-fluid .span3{width:23.076923076923077%;*width:23.023731587561375%;} .row-fluid .span2{width:14.52991452991453%;*width:14.476723040552828%;} .row-fluid .span1{width:5.982905982905983%;*width:5.929714493544281%;} .row-fluid .offset12{margin-left:105.12820512820512%;*margin-left:105.02182214948171%;} .row-fluid .offset12:first-child{margin-left:102.56410256410257%;*margin-left:102.45771958537915%;} .row-fluid .offset11{margin-left:96.58119658119658%;*margin-left:96.47481360247316%;} .row-fluid .offset11:first-child{margin-left:94.01709401709402%;*margin-left:93.91071103837061%;} .row-fluid .offset10{margin-left:88.03418803418803%;*margin-left:87.92780505546462%;} .row-fluid .offset10:first-child{margin-left:85.47008547008548%;*margin-left:85.36370249136206%;} .row-fluid .offset9{margin-left:79.48717948717949%;*margin-left:79.38079650845607%;} .row-fluid .offset9:first-child{margin-left:76.92307692307693%;*margin-left:76.81669394435352%;} .row-fluid .offset8{margin-left:70.94017094017094%;*margin-left:70.83378796144753%;} .row-fluid .offset8:first-child{margin-left:68.37606837606839%;*margin-left:68.26968539734497%;} .row-fluid .offset7{margin-left:62.393162393162385%;*margin-left:62.28677941443899%;} .row-fluid .offset7:first-child{margin-left:59.82905982905982%;*margin-left:59.72267685033642%;} .row-fluid .offset6{margin-left:53.84615384615384%;*margin-left:53.739770867430444%;} .row-fluid .offset6:first-child{margin-left:51.28205128205128%;*margin-left:51.175668303327875%;} .row-fluid .offset5{margin-left:45.299145299145295%;*margin-left:45.1927623204219%;} .row-fluid .offset5:first-child{margin-left:42.73504273504273%;*margin-left:42.62865975631933%;} .row-fluid .offset4{margin-left:36.75213675213675%;*margin-left:36.645753773413354%;} .row-fluid .offset4:first-child{margin-left:34.18803418803419%;*margin-left:34.081651209310785%;} .row-fluid .offset3{margin-left:28.205128205128204%;*margin-left:28.0987452264048%;} .row-fluid .offset3:first-child{margin-left:25.641025641025642%;*margin-left:25.53464266230224%;} .row-fluid .offset2{margin-left:19.65811965811966%;*margin-left:19.551736679396257%;} .row-fluid .offset2:first-child{margin-left:17.094017094017094%;*margin-left:16.98763411529369%;} .row-fluid .offset1{margin-left:11.11111111111111%;*margin-left:11.004728132387708%;} .row-fluid .offset1:first-child{margin-left:8.547008547008547%;*margin-left:8.440625568285142%;} input,textarea,.uneditable-input{margin-left:0;} .controls-row [class*="span"]+[class*="span"]{margin-left:30px;} input.span12,textarea.span12,.uneditable-input.span12{width:1156px;} input.span11,textarea.span11,.uneditable-input.span11{width:1056px;} input.span10,textarea.span10,.uneditable-input.span10{width:956px;} input.span9,textarea.span9,.uneditable-input.span9{width:856px;} input.span8,textarea.span8,.uneditable-input.span8{width:756px;} input.span7,textarea.span7,.uneditable-input.span7{width:656px;} input.span6,textarea.span6,.uneditable-input.span6{width:556px;} input.span5,textarea.span5,.uneditable-input.span5{width:456px;} input.span4,textarea.span4,.uneditable-input.span4{width:356px;} input.span3,textarea.span3,.uneditable-input.span3{width:256px;} input.span2,textarea.span2,.uneditable-input.span2{width:156px;} input.span1,textarea.span1,.uneditable-input.span1{width:56px;} .thumbnails{margin-left:-30px;} .thumbnails>li{margin-left:30px;} .row-fluid .thumbnails{margin-left:0;}}\
@media screen and (min-width:1500px){.modal{width:1470px;margin:-290px 0 0 -735px;} .tooLongTitle p{width:190px;} .smallcard:hover .tooLongTitle p{width:160px;} .simple-desc{margin-left:0px;}}\
@media screen and (min-width:1500px){.card-visibility-switch{float:left;width:20%;}}' \
    -i /www/docroot/css/responsive.css
fi

echo 165@$(date +%H:%M:%S): Deploy theme files
echo 'QlpoOTFBWSZTWX8k8h0Ab7N/////////////////////////////////////////////4Idd98AV
VAgF56vPu+73deq8ymPC8+j3vth83c7Xc3bvc4vY2s16Ud12ukJtZ58Ei897g29151767tF75o33
gO47zuoB93OA++7hAhV1hfe3E9Zd2c+7nd3nNvvvXd9yH3w6FuxwPvV97fTfb5ffM+2bdOe23sr2
963mem01e93Nva69Pd3t3u3s3h21KipUbud597de48C3d0ujC1lK7t1QC0Hrqbfce3HAOenPJ5K9
NeWkDNgHmNR98fe8qhvuOFzBkBZ9uzb3udb673ffPu92du+zz3vu+83r3c7SjttsVN27duXQmuu0
dV3YK6c7Uu1Fndl3W5NtDWGSTWVs1VpnXXXWirjcu773eFeOxzN27iugGjXQ7u6bbncarorQuy3X
O2o9c7GvZdx3Tcmut3cAcrt3blItLuk4jl26qzd27u1M2Y7p1urubta7bsaq3G94uuuIqmhMAAEw
ATCaYAAACnkyYCZMRiYExMJiYEwAmmmCYBGBMCYjTEwEwAEwBMAAAAAAASDVJAEwCMEwTVPxMmmA
mJhMmRgAAEwAmAJpkaZNGgwmEyYAaAJgJgACYEGTIwmTAJoTGkbJpoDJU/0NFQioI00TAJkYmAJt
E0yabSZMTJkwBkNNBkT0BoaMjTQTaAGhNNGTJMaT0MjKbJiaMTJhMmJphDQxNAGmJk00ZMRpppk1
T2qDVImhkAmGhoNGRpoCegGpjUyZqYJkano0xTyYTTTJPJphMmTQxU/JoaGRlNpkyYSemBGkzU0z
RNiGRkGCGienqmaZTyaZVP02gyYhlUESRBBNATICGhoj1GjTU2jQTT0EyntGk1P01E8E2k09MgwU
aekxNE/SaTG0mgnpPSPCnpMbRDJpqZqYJkzSepvVNPTakyaeRPJGnqHqekbU0yabTUwgklBCZNAB
MEaAAACMJgJpgZAGpgmDQATIZqaYEaYJphNA002hDaRgEMjTJk0aMJgmmJoTynhAGgZT0Bk1GQeO
El4vO2Rmaq/a8e5d8Ch0dx1WqWq0Fijeuw+y0t87VMBRfBwoL0+oOOpdeS1rqGr2Ovw4GkO2SxW5
RrBDTz90sn3Xes6eJoUklGEngHnV30tnZ959rO2aJ3siUVAqEYFUUVDet4O9xs9jKIW8afc2+5yt
zTlCMAymgB8+pAWybk8HMdl6lodlL2Z/f4lVvdlyb1Z2KgQgXmWSU3kKLoVmc8IZB5c/nELIFH8Q
DQ92BD7PsTrgGzpEZ6AJ1JBROpDMQ2hDvi714QYEBU2wh0v29BPbUqtovuLViITCSFokiYhBRekR
UHykDKzrIQyakQZeUpCRkYwioHtSdIeGKUaD44QE8AEsgp/5IgPOIvWmkhRx4AG5F/CIi/dkFfm/
t0gL5skyfO5mhjAHXjR1LtsiPO5EJzDlEOcoZABGA6OLVnpCiBY6osCljA+UYF1AR6M1BjJASyiS
b8mB3znBCeaAMCPfGRzcJvXQS/CD/bqwDdsY6OC1lgIeNo+rn68Efy4Bueh3el8VfDz0EfkRAfof
g0HIZUAwcnj780Q9JsBQ3h218nzIACDJTXGxLytd9sM6PCQxBD2XnfrD2VvZQtf0Elu9xLmExwvW
Et2lyWKxq2UyywrG2NWhhJMr4VKyzDZ9Cj989p5Y+4PaHvMV9v+3R9rA2cbkFjpj9Td8j5ITjKrA
RelybcicfkZkORDCtlrww5NYTGGOOMKwva2JYwtMsLXKlSqxvcvjMPAJe2WWdZVfDPG2WWOF6yxt
lXkc4XzoLmczhlbG2d8pkS9ExthhbPSnfcf3XX8HHxfk86ghDcVWYQTo+opPN+rP93ZXE/FobJev
hac6inMiLrXDRMHp1Bxb1j6mJNhCqbCyDCkmMwysVe4NB+slOQTds+Mkkv/Hx+1772AYX/ahlaWU
4YEP1GtULtLbDt3d4rG+OVEAAgCCeYgtVGL0xZIjh4MWA7no9whZrqyD3gfLROyj7hx2vpoaxaB5
Q22NcrTp+26ctFfcvzQWJj6IzQPdwTFcr73quOIwfJpVN1m+c57pmZaSSPM7NculS3AC/ORQlSqn
Rz9vf8vUZLvd1zO7N5CD7yNcWmrlRyzyEly2MhI2N093IRlevUjU+JNRgFUrGLuGD2MDc0FMdN70
pL7dNHHA8ipxh+TcCR7xzqPbXMUFQeAhS2Zfv9BAAFIskgQpxi39vwtJkI5z7KA0beC0NrbGuVhm
YSqzTDdN6RdpXrwpi9Pzj0F3Z1GPMM+v3+ffxWVad+vZ9DWEmm8sd5v+Bw3XTFZOVDRxrboDYkEB
Box8QVADyEqgIcqU5m5SPvqOGYuLtiFaG5ihALyLKA/NKicLm9P3pCxDffCbh60DH7o2ctkdt4/k
m8js+DCSdVH9bADHRG3+rmsNDolxqnAl+NCf7FOh7mf7HBPcYUgiASyORDXQZ3ZS+VMdiS/ySMXQ
aF4O4kVJaxN40fYD4+f0if0P1zoHAAEJopkQ1hLVQF7llUVDmG6e3EA3lbpIv6SBg0ZLuT/WkffH
a3KJYE35HfFAIC/rpRtkq1WgOvAGMZcjJ0Y1k4mc8glWpzyFJRQCmEkjjhQ8nP5uOw2wXGAAaEtA
QEujDywpMpZTCMjBr53rRihaBcCdPt0gWPHJ0y7rb4EWYqMxTBVVY/AvlGVzPOtcLEN5cqrfKXPF
FXs6wi3gqZGHiG7fkDJNYZ1hcTu7gVfDVxYNgJt6uDbI7fXSo7UONA8pRAZEkoTxJbEkGxij49Ud
dIAOdsi28UrKamKXAYjzNVcWMfGtanQyZHtB+ueO8+bRncwyyQf/PLtKGgCDrqHrbRdv4z6E5SGI
IahZxaoPxDWA6ZxJVaPgu5uVtntLbTv4FkHHHnL4QRJJZX0bLmLjGKu3Xr8Tkmy0//c7a9XWY2pZ
mpUri7aDtKsuYsbBaIIG2JAgFdDpfrtpeKFMznIbydmOl+ZsotWx69exVUGsqif8/zRh76mRS2pT
0nuOIgcblmE5qZNPxQlMR/vMIHvR4pKfpzxidV3Dgz7bI1NiqVZ38DCZaEnNYg4ssqd7STPZLBU/
OzdzvAUrbfQL/zxCyjtGY8s9F5fHhyH9Ev85d+mO5TErdzgvy3pNj6d2dnYRojkrAqmIes+4rfmZ
Hq5Pt2Ky4CbjRO1I7fb41NoUBrmQLiT7Wcu57h4eSknuuIRqobcfoq6ti0AoA77zjpwIBGlTly+o
OvqEF+kX9ZSkln7C9hF6bPuJQB7Aqo0g0Uie+KNxliosX0S01oflgnepK3PGd9blzT7pFqOvT/yD
qzTSBnH23izkuLwt49ZW4f0vJhs87mn80xYAAQ380Ihh4ckpkz0oG98Ykm3up3mR6FBuKZm33OXB
g85x+3C/NOQC4nnaoegWYtUkWlkVgX4VnLWG/7OK2DiJwBSOQMNtky23OrP6wcrbwF0maU23a/x3
dTUEpM4RUhaR/CyKPfQqr5w6WjYbE2k9OtkgbKVyY8NURU9vGhN7uVEYzwTy2RE3vGEPnoyYaHul
9LSxFaDH/8hLX3l9j0KLM9razbIKwkC/k2HjO98PdR1yQPKnsESCARBrBer2Lnqo5DT/4lu6BjCc
Sz6ydSDxPyL3zIE7id2e3N5Jqx+pPrI3mDlW+bHLOGjw/qQW7/2CpwqBVY5+0zowOAtNI2NCoUzz
GPB6qzRn7np2CYVKbMOlpxk7YXJo8i+t/Qf9/fCcPfP6bswCIMrqoTNrHB4L0Dn3LOzy8eM/DHYw
tzBeU58wtuDKxtoQB/A644QJhMIIDbafltj5dv/idyzuPfcV91MwzU0x4FgTbcp64thJpkLMtOfO
AlYXhav1bH/Voqjv1Da8Up6pfj7mozXPzzqSDny8F80VLZYSLc6J8SgdHp4YINHzaLAb6O2m3T6f
RfVLGdTv1QVd2xdlQ3rqBCgRtEq6wh8jnaf95oNziCjq0Eer1v3JLPj+VkoHFMvB/cqajxt/HaNU
LnmIAgRZgFAAdiQKaW/prBfWFZJa0DWL2ABtAnQQBk0883qj4mhN2rQ7a4G/Bv5WjaQbUIVLX1J+
fPovTeMcLH8LMZc0YRDVGGIFa04olcvECHRSql+d0NuZx755jN6VznNkCby9eB13NfmCRPQKvdnx
PwfIj2EDwbRDPaXGcBesmmjjNlN+zLWgTdel4y9XUby3xlRyGPfHBYs7it9+f4mZDG4+lEfLTXdG
GVNrajIq6iGJ4VhPH5ILe9U6tXRsr+X8sAq9QopQtruzsRcnVQuo9i7GIv3LiAPTUW1CSQpObfkD
5fhsXuONvG/v8UaWvXjUANRyIkNUE6VVu0aLRP6bjot7p3v926hgJAJe4/IgWvu1UIPjwV0mJnXO
KXjShJ/src1O6vb9Ine1j7ymqnNY9X9VEoxvsBOROCtnjNN+B/fK2kXahZHuRYPiLbzvbT2nrs7H
w42M/jV8HXne4j/ZDqZKrGo4xS+6NTcCq4PBskfvXBCuOBwVptXMIRCIxkXEccyX+3T2MnUM7U9g
flVnNRUt57FWr2MgiFyfKtW471H5+AnBMiY8qcTEOwFZAZIU+MSjEwYxxOiKOhYFWqELBRgI6/oQ
/I6GIpkEQMyQIPQrZlfR4eZ/nfP2m8zWv1HWI4v/waUpeTxlPbe53n4j9NLF87o5nIPE63+sSFXZ
hhsMMMOMODO9nXarGPJ9drcQh0r7FxGIkHRNV7m/iKK39ylBZDWUtuz+PqU637Swc79edSfZ1kSG
bN0BGUZLtKjdZicItOLbMAcpAG1mI4OtyjuWhjFES2qMurkzziQYKtLY8v7VPUSL5gaRgLtx36CA
FaZB7891Wza0vyl/QGQWIW/OhIZZRE1rm3lny2Bw/RuXphkZ8iwgWL8C4bYcJ+1UTsoN1WMPPD+G
pa1fKa933tpqIW6bf2MVFH1xtmmYh2mx6BqlFWeeGU9+eP1pubwvw+ipjRf3bRDE68gauRyi4Y+6
ft2QqvToQrKSdvUjep902JYGeU9+2d4I+40KKgl6fHVjD5wD/b1zHEXycd7S7wsysWnBlaLdHSj6
kZDZQZzQAAkTxBmb+ouhwnq8HoId0bu0ZsBL3KURaylLrU2C0RdRnjRdkjh3qkfPtKFRrlQc7ZCE
1EyGZOIENjMHDfPOSFAjv4bgDqVB88SNsh1+0fCjUUtcBd+AtL/JmAf/cBqzhCCQjpU5NidU10+a
919vbPxovI8I3yqhwrJ/mVmAyJEe9CMhcnjKIBc4JownYtCOHtFOEJEy+VP559V6JXsdHaS3VUtD
OriPN8WvnHQKzoEAgwyt5VB5OD80TA9OmrfCm38m8URaxoWUEsUA8Wx2phmtY0JHUPF3ihBBAIid
/ttooe5NNnR1bI2jZpN0rWQYmFQ+nxS3F+CcRavvOBDFpw8OyyHlVUznYhyXzOcI7GVNg1nWHQHJ
hK8FePFvgiCSsEog2lxwq8nh0x1n2+ZlXq/YUKe+W7De4h/gq7iC0pFmAl/07NW94sRvllemQmBg
JbkH8T5vIzIgq5jbT8Sf3MPHg82eCfOh99KVkMsCQeFL8O5cqa6pLzFyRlk2gt6h6djOw4AC5W3Z
tTnaBJiRnRU2G0jc2ZKsrdoAj21p+/XrXDFR3GFxRO3NE791z10pg+y1oasckxgDpEgAQIhWiqgw
ZCMjp4Pct2JPak6OMCU12E/b+l7kjnmI0rZc4vK5GRBa6YzzZcbQeMVAgr6MmT+ac9w+bd3qiUkH
A2LnvV6S0imY/Nron1e50UfBZILEVDkGWAraxPgnx4qMoj7ILiVo5UwlqM0L/dT6pTTN6h8PdzSp
6/FugSHovjhShfaOnjqiw7u6pCwWZ4FW3tsD3U+2G+Dt9pZDbKS0mHILcw7YQMCtBFt2pi/aQqzG
Nku85ofjD/H7hiA+0cbrG6JwixObZ81CAEIAfhNwLg2xlpacIXXwYt8241pe86X3Oov9zbKFwhw1
iAA1byJmi/+OjNL1ZD6THx5Lzsi3hs1F+DY+KciLijG3r1Vpi1nEzd9q4L3Zpazy5NtwjLoH/yST
+F5duiomxaih1FDOOTqQ4BJ09VIVnCXZrTqVmHaQknE+wRok9nYaQupLRvIwy/OhMxiknMk8lB2V
Xhe5QvPtyXHVG91/X2OHHyiMUWBTPFCXsfPeKMW7l+GZwIrAWRK/C8rkb0oYD5nPXFGKG9va6P66
VOfQxG0KklyJ97dO2OSOytMOz2NiRq94+p/ELjOoViLgz05cTqhBEeuOln1NTMEAu5MC7EKAFfY2
pAMd5Jcgw+8N7pFUimJGFTfLbQYh3/HnzqNgGrflz/1PW7YcR2Yy1v76g3kCzeB6jtK0VzlP73s3
lNvn+XRVeOhty4dbUti5l/fjL1HQQUhPaRI/OAX9B33Cr/LFMOBw2wmUWd+tS3ZelVTblSST5qUV
Ww63u8XwT2M6PGKK0VqauNsUa3t31vaJScllWEbU3KVZhoro2k5Q+ZZlq4byvH8emlIWbx606mrE
iZQXzag6spwEUb9rHZYHPGFL6CSwa8Cdh2qij0f4Zb5IQVhs1YFUex9YtDr2gIRyHVQHEIVHqteH
O/hnFDQ5Idy1grmtAxUi4MtH7Wrujv6qGP9EvGrhzZkBh8hvxk9sceEHj0VICxmLsfgP8ZsPt8Dc
BL1Jhr2Dv16ByOzmh6lJz49w3o7N4ctsM6Wutyn8Psr3z25kO+YYxQxvui6z6dqvYFb2noVp0Y5N
YgV78C6KofqGeu/6PXw562yGfC920rst0Kjpy4i66jE4iPoR2++/L0hZO7RZRc4z9IkrKLYDEVQG
TcXc9Ak4QKiEHYx5kSQ6gS20bglcaw8L3d12CtahiHuSZa2fi5LP1RGYmotj2iaKHwBpud3cSCz/
giq3CQPuFK+Qj0mUfEhiSbCoy4PyyBZD2+sr7U8D2FHIk6dAKhEb3rD0aJdyR7TxxHvG3UidD/tf
g2wgT5A3NGg9tRFwA0HTmHkoFnPemYj38KlenM/uqsYk+aJJ3b+psbx436s1rBg53Y3+JCedZ7/o
3naA4bUxBgOXqHl6Q8WxkfDKt3swgYZbBv4FZBZCTrmoZBtyw43hgV15Swa9OY2nCTUMD6UKWIcF
JigSHEkc8bH2Aq5bFFWlIKsmxJk5Ldv2upfaAdR55rxoVUTnVYwfqtRIlqPGgejaStsSuAOHRLFj
OY6LYheLEUvK6w6sCbPpgRcsp5JWLYcmOhW+KxiNYCcEYwBtSOjcDL9TvQh+f7Ll6fkKMIVYa0HK
xhv8WcHvauPBOAMt751pFc6EmVjXl0DZCOVkIOOtP/MsYOKZwgbPxPvjmu4+eKr2WVl7z9Mrh/Ka
EwngNDCYswvp9mmbwqDiKwzMnpAe+jwx1ZERXF6bAKBBoA+7aOH2UpLRtJoRnwtdFqYRELmoDN2B
wZ37HVxv8WNW6YOpi+L2hXtdP9d5mscQ4nDork+qI9S1HfBdE00n0xWq8LfdtuOym2GDocR26P05
85Y4wMBgZZ6Wv6uT/DjsdDnp8VdXkYGVeXkY8Av1DaB+O4U3CR//6w651Loe+fopYD19EFwK4Qpn
1D9JNeaTqwUVKGZcebQeMYmwrP42kSKCXVoFZIfOzaVWMF7Xvv+BFFJD6ryo3HblMHCcN4txgcD8
l9SKvjHLonnTTB9+HVbCiojdLfiBAgQIe4dektA/CxpBJ/kbOQcrRp9MFaTGUaVEbzS9aW/9kRb3
ynKEfosz00G+ttvv+P7E+8s1bpJwcdp6c47UF7QONEQ0fsteEz/IhZnwJZZ2+2x4D++dAwP5fKAM
ZMAnqdWkMulIn0v8SoJQdKV4okhsvOwXePKgjMrw9OPFx6V3JED25Zt62AQxiRpaud7YxrXA1YGJ
TyqCeGctizOwA3gxKf1dB+4Usv4uNDvZqLkUbJ8zYELVT3Qqut0I/ni6EnmdJBD5WGf+7FGgww6e
BKg9YyDVkgTKX7vashqIbLm7Ui2p3/j6VqO2MwqwmOO1SxDfPm/pnQREU/PdMq+Qi5ERCagzoKPD
YtnVjEiWn9DAYvehQY3lYb+6P8SjvlEyVUzbZ4I4yDHRweHd9fMuY/48U4AgBFVjMKmL6StAVije
vQ6ZKOb/A/9Qo8X2Hu/98bwKpXbCM1LXme2ut+K76vX71fElRsBuwf9nQVbqGqJOmPi0a4RRtv6J
H1YdzSf3zDafhZrEzlMwh2vSZV1gbU60rGCghbWHVN1/AdGcwqMpU/cA1++H8hn7MBDAp2V5caIH
HviqycV6R313f5exIb3Ve+GkizpZPsyz0fZ2EX2xKjRd7cRfYW/K/frdj03PyqN32mYv/vdr3JuL
E8jbe0X0kxA+Un4qMqlEk33QneNWF6/Oj3jDPymE5ppuAtZo1f7/+NG64BN3Nvz+7RW9FoXv8ctN
BcmuU7RY9DCiW1Sala20XTGiXxP+UohaynFNx8IOQ0wWjBWIZh+OS6CNxjH96xk+cMEGblKwGr/5
Mz9AJaPnDHfD8bbRYssKIU5PorgqCSVrbWxaz30MHtCzHh/UG7mTLW/aleruKF0QaQV/hYoQpAIg
EMET32Xe6AJ+1hsv4auzr8a4qlgs39pxc07xfUR235w9Ox1gbxk2ZV2HM/7qgqgACR0zkBpxI43y
RAf4oP02R8oYo1z7HCa1uRhLlA0GXdhq+AZTmdulvBBAjOgUbCOMbnNXTlX5tuK3sn5HWCzKTNrt
N8A84/FLqPhhSfRvMmPeJZmfPLSDPBCzkRcVdXDzH1Sh0QgSetNEeTxv8ObzuqGFfN+4qB2PHyQF
/BAQvth4oUV5V9ZxU/nVkvDY/y+13m1zYrtobbM4qdgO1HC6r5mIqvYXP8tojJIM8ReMe6DpxF2J
ab7+ovnZgSiu8VyeHH8YsVwmDrOTUi9EqyNt7vx/iRjuprcEykcmTs9obE1Iuaur4LJAgp/jMQyv
4dsVoF+tjqTP5G0+Jr6RkPC423Dnhcs9tWp+pCrBXO2C6WxL6b5wQoP6L3VMfrxQRd8963r1XF08
IPJ7H6A5WnX9w+OesWitos/MmojzNEaocUnL5h681/u5depV2+X74CQRxyK6nmJKYEf09nuWcRN2
VvXGT+Vpo/nWkj6s21U8hTgq3zhabjlVz+NaRYYkf68j4gYoUct7yoXJymQbhep9D8q6pNakTNOD
zgIHgek+Js3CSgX9QuB3fRo+L6dX43Q129lQFl2JYEI80Xoe5yI7G2fbSepIggo6t30nafee0kjR
yHPuXi1Er0X0Yd62dwgu/CP0W1otNq+59w8OogjBpJJhI9wBTZAQ7j1/e/d/ChXj7VBO0O2Ih1MA
gkIEQisCFKB3xrLjYCDGh5Pq8QrFPCuZ876btdelUdnvfoB6M8kcSe1hlsMMD6NmjAgFCkFP0rBl
xjW4CXAIGgwmQWCPVgj/ImzEYDpSI3Ug/AgGIwOOauwV+0PHHP9MX1n5pulkRCcT00YIfmxgptUl
tjnnDdytYmnie525dj4RbiQqg+bK1yhS6v3spQ+ZuZHUF4KYTjEvFKuPvjqT9THCAU2VGGqRKoHk
OnIPllHHRLn7WXfhgZqfTeU1niLr4I3QuRObRrOzZthPf5m50JfqR0gBmZn6Ok3J/XthqOqBuGkO
RqzOtDUm0OZntGkumRsDkO+kYlZbRGbANonCZBHwSDNiJkjxNCIoTYvWJoC4UldNJ7OcjgUub0kd
+6bYg74HrjSWlwCvPH9OfQjzGD5OH4QBSaTJzb0CEwzqmW7/H2V3QWMXwufxnsuo0OjBTfu382vU
UmAiGTVzN9SLeqmsgIRxhWzdH/sBEPnFUhXpN/995jdQeSGjfQYgjvl+Dbm3vdODkl6jS2UbNRsP
R2z0zmplN+d6mx0fP3oat94j+suOgkrkwkAEBk/qnQfbQ2kUvrCu34sth0+S94lKPG4OE+Da3IZX
NE92n4GUr70VhbjpAl8rQS8naYlKoHqKliDxbP0wMObpVe/WXxUht7Bc7jG8+PIFavZ/fV3WV5x4
ktwf2BhkywTD/2X6/dG+dHk17NT+xDR6w9A06oyjYo3DS/HQH50ux+j6kKNEUgxlRkYIuoKdK+4L
2CwnskWURBJEfgOCIowEuHLEH+T/u/rXMfxdVduDsIoFOLPl3TKyH5JOk04tt6R6v17mFAEtHTfh
8mVKWinTqHGKeOAVyotSq/tT6aaUoODUnEo3n+fu7/HQSCMU3DSrzrIWYfBwEHoJT+F60J0qG9Zm
f5ffinw0FwCPkFBN3H75NniyehY2nIV+F6L0aSrpdB/qDZxj0GDtSZEe+SHr+Vp+vwMxYD5rw3Qq
ub0Zr6hAtjc3ZTHbRDQl/hzVyX+v3Y3TaaAQB5kISQYxkJ/TVECD1VUpE+NE/Mjx/o/udPcaDtvQ
yMv4ulKV3GxwPYytDjfq98rHl+NJpayXJccYTqyzqxiZjzX32k+9IDSQgLfAkh/KV5gEPkXt/PH2
lmoT9nRYQJAIEmsVANVbz3bxP2x1w+xj89QCS9Gn+D5wtAnps9wtVWYze9Ph8uAwCFxrPAUh4ZVa
dtTYkqxeVutNcmwz6TdTOg+8aH4sk+eFaVkSDoehQ8v8Dc8obMA0YRarP2PKmIjrY6EIEPWj3s1x
3rXt+j9AW7IHHJmRaXCj0+FckWSF+ZYBge2CuHGRgfn+jf8/dFaJ4iRMeUbzfOlL1MMPOop740+V
wSlkS3gc7MbEkjdPoZZPaYbzMlvqhElwswzO+jnQqi3HT6PNweFTWa3wQGdwdPlw1G66M0MYHCn/
pkTxEQYvegChL+9D+pE90E/mObIK7x8vnxd3Vggh3jnrA9YmSSQDIJgmSJEleK91q4XoqJOsH0/E
ndajA3PlBgZHmCPMAzBO78tgfnSgdj2ltqtvJhrZb9Yq2/WQ1iv6oQqj4pgc/wHQpPVxQ9Pzyw3B
GlHhOWMxgxRLgoQKld5zpU8a0iYWnKeA/bb87v36qQA4kAoGnCf3ttpMXvsT5Swc39AZvn0BIWeq
OwOi6ggcsazZhQBjAn67I3lk/a3W/oM8Wf3tXwKqDWhPEy711TvZdG3812jmycNtwikie/eA3cFv
EHoFqr0UGaShnLp/XN2yvBqoD2lnRxgPgqjQivoUULEoFJCRMkcmrAD5sXy64A8DAZVo9D+rB9dj
mZfHKdgV+6PADO35IwHqp0MJczMT0NIc9s+YVBMw+HR8xZvx1biFWTqKpRwwks7Ato6X3F3r0x7L
j5bYy2SYj1H5NdPshH41I8fKtUahSHSIeo3MPHcVP8MWDvNWHdee+67lyHfaEtSyycqHIni2LZsu
SY2zlwvt/RZ3RSOIFBzPyHmOVixaq2vD/+Wl8EALgfwt6wRu0eeb4/dgD1nlk9Hr6nYIHkTpPK/Z
ma1QUkGCfDj4yCHUyU9sMJ/G//0L3lTV1zoEPQeZOcjP4vPyc0FCxiRlkES6PeHCXdCTfKcDDowg
4rIX9uWU8Mqu9vX30Px56/y1XsZfpnq8L4pH0ycMz8/Yu4Jgs9m76msM6eWaDd2qaFthHRlpE8Jr
w1CYM3cbzWXeX53S/E0Wx73/D045zd5jTyf8/5Y9+N5xyh0AuLMxTxj6sL2tL5cpmL8fYRgK79/+
ZjVjfa+OzYJ3pU+479GBSFpI8iluHJvhPFXMcMGGpg4F74sfJSEo5KqsOSr7rrr0BmcFDI0DN8+3
2C4/QRY6JHKKfkcL8RweJQ0Zuki26Aoo+i75ljyxzMToPdZGs4jRW1bVRq2Gkw5wJlE4DhCn0eaq
vOl0qLs0mRq/j7mJwwGfXyYgihWi38ajebVhdC4nMxJX1ZtGJTnoQuc+j4FU/EHhW+FeSq56OCFF
ms+DamBBiHkQhpIaeLL97b9NzpD+0HgLYpm6HgKJjX/r+cPThSb3BlU4sYUY+tR9mn1MiMtERXXv
bzXe5f7QuJcEIZZUKy9tAJDjUlUHC+D62noQvcDbIjHvMEb8PnTQgqGKECq0UJQeYyfdFj+uaD8Z
5Vf7S5Pr6ELs1PtOm7JSZ5X5ZI6gPI2i5NueS3S1RuDfm4mEH8ynzr6V+252fuNJyJn8rzES8X4V
MOaZodwqZrXNrO1q5PSz1m/rxKCxCi3uC7Bymk0mkgUfOgbUUgje5QCQ4ACQAIEguxAb0NfsODUt
JIdarCDVhH96qeGvrIS2xIvwa7E1eL+OGQECjBg7pQUGReE+HvRtD4nr4YXWMHZqY3moKoaocR6X
g0f+xhjDtQYKX3GG66FoVVQU9EWHVXN2I3zBxGqMuXnyG4zJT/gfwpcNZO7n2rIXR2t8hKmfnddN
fePYN1izm+lg8ARVyhhgGOSeopc8AfHJPS9r8bH9furfQNRwGnRmhYhX7kwNhoe+/RoMIyZB8T2n
/bnfzT+Pxg7D8jVuQsFijbLnfGAYGBc/A96VbHj1NwaMwzKNBkWPK0by/v/I+NCR+E6CjAfH/6/Q
AKEKfH1IHjAVQCKnRlP5TwQuaD5B5sGCuhdgiARHOXGKEWXN5mPnxTDreWqjI+LBvgQsYLfvOGPE
sAhaBSAHy8HQjg12f4q58n2nalb/CVn9wTm2PzU0iUFrQ6hVjfF7nqkQ50bMN5axNvEDXGEdLUTy
G9a9Pv0bmyruZy2m5sJbnflY3Uax9gMxvLj+9q9uwfrUM7wsXhscZEuxVKn7MawyZJgTJMJMkwmT
IRIsiQixge1PEpbJO/6qXSyV/Vl6Tqv6fZbaPNqnx9iUXCp3adBArKQUdjxDODAyYOX1PT5vx/AX
jN1GoLx50H9MYntKuX+1SIcq5if4iTt7uXekSKOBgSIBcAD2WXe14LrgF8eYl0jFCQSpIJRHTpGj
2hq2i13JPp9YZpkOhtTSOLLffa6f6CtQE9n/CfeFtiobxpTbI/NcX7v63LbDdSMgLHd1auGxRRu7
qbHNzNG7th9IP56MIjI9WHi0wM2YXcdhoNHTxI1VZvGTwhgJlyk66lN4dBBAz4B0DqkhJIQQwCRj
SAGwciBzN9zdBgubA1lyEIUynRnKTDTvG7Cbo4m0aZpBKDmA/tEg5P9KBCwea4J/J3SKt/YbiUB+
vlE3W3etLBshmLjfKc2aa5BQ1sR9oNvlQjsYeBsB1rS5tSuW0YUrGhXRasqnMW2vZ3YvRQEocg9i
SxwRES57qh5bpadfYEMHRfJr79fvieqQfoYo5+7I41tFhjKC5OGSMs9wL+UWhob8f9OZAMm3iFtO
lrnqFjofPkMZiRfUKdHBic8BTQM2LHE86JODjMQlCUQqk9hGiMiQ+qJRD6/ua70jaxc3EO5YGkm9
+gVJCQuFUdV3WkHNR3t0a3Jz/MBvb2J9chsy9UvYUmsAhBmBR8DfCjs8Dx24N0LHOJulkNw4drDj
TBJjMZXFKcZhaoQogQICUwYwB+TmZHrTwDqCBwHGPqCgyIIXLsG7i4OTk4bZ+T+Twe83EOkOd8zk
XANECt9rgAKGo79SUQN6ii8KaLndLYuXSmH/P1l9/0vi+i6Tt/eeZ7T+Iwaor/j1PwHFk+Mhks6a
0U3kAamA8iRqfVqvmD27zUi4pyD0WcoTQL9Ej8HvS4NyHHG8TJ7H4Mk4ayWmArpELUBOA1agCdJa
4ZbdhsUZUg/CewLF/+LfNtIFaLhUEsdDxLkuO8G81V5sJEJP58IWiwU5HFZG9ku52o9nmRZCv+P8
sZSzrdHmaDdiz122WLFN7Fgs0NEVPDKL9Rg8v9vm8s0GegY9qSEhISEhIEhISEKIQhUkkkkkjJJJ
CiEIQe3+DhoEkYxE/VIuKhalkIxWxFsnl8zfNt1hgmObIULgwmGAY93KH7YKw5TJ4WXoquZSGZrW
bapQjY+5ZengNhaHvyvWrMxO3HN8CzVWV7r179tNTSeaTu1KZqvv6NJbbts80CFi8L7xJakvdOd7
DgQWffdGYfSSV6wnck4PJWdkgQW0ZIW/yHLdSNH7Hge3y3yf8KqnI7PAyuOqMwNAxHP+Crd2Zv6o
zT1fFSiKqhqYxAKlUp3wOK7lCEMoQybejiYXDyz1aZAAEECAWp0QAGQrfrNvR7ARtf9FncU89FHY
Niv8TZDlKmiBnOJAIg/INHj7rOKOJB2D/MnXNS3ujtB99jjp5wgiJ6eCHAci4yDsOkN3C4l+YFAc
ks0mJezfCX+KnQGlS0JEXGI55GY2A0FY4WxGjhcMtVsC8NUv2NktfRZKmii1XCFqGXhES9xoIsC+
cDALHS4hcDtdWWMhITXM+wzQHGYuQ6nzyHuxtoy/WX5MH43N+jL4WZLNHB59n6d5s3DqOq5MtXJz
pfIW6caENvpTrDomgxZncLtBa70xXR2csvBGeoyrkCCQ4TjQYwGgj8NnZP4fh3fLnYq1lF5Rv/8p
dBw9yO8a6WLVYvT0e3+TF/J1JZHVwQLX3+HGDLRnkXx96ZcdF77y6+D5Jse3whgDdFxx8ARti+lN
4Ff9gDFLQQSbY46zWketLhbLU/BZEB1/HZ+tR+lul4MXYrL+/7zG6PV2k36fH7l+xmTNOulOkgWC
eneaQjOf5w6Q6KsuxSZHNC/NHpJ5wwwcM5zgiGMOdqL40OOGd8B+F5a/H7/HSeY2w0hs1aStpDaw
saNwbVuZFtytFVVD2SDnCQIYXvtVgg2i7uKYv62vLavh6bzxawaySB6vO9kKgMjtkcNFKwkPgOMu
tKhcI8KpjexV8F/f8Peh9Ow+Sa/aQuPSctFXBBS4Pw0c5heUOynyPVLbwBCmIX9/C8aVhQWHHyC/
J2JCUqLArGVXqKteDTGGZK7dqK8olGj6nbuVQ/N9zQIIHxRDmnxHk5O1+H8V1SNw4QXRmX6oFGvh
28geM93L/QfqxmFBDXqYWOp3U3j2wuBMC530ldcK9AT94T8FyC6N50/ss7T/n7yDLGT0lsu/UrOM
L+xxHCvG5ag5CQ+ppPKb31XruSOUJt8uwcy+kMMoPnYX6DoKzOmts8n0XK2tFsM9EzrTlXbpWGEy
sGKH4ZkG2EDxeWDa4PEOr683zC2q2zMtouOOIQ8TlbLd7gHd0AYzrd/MdYZnkMwXro+Nwf9rZU9w
aD2ZiUPe8Hp4Go5Pmb7cf7n/ROBMTNWSP5s5UkT+NtfrVDE1cIiQmTJCq9YbY/PaTHm7Guk1qbmV
HBfdxy5V3c/Vm9GNdofUYmHaEiH5lTYKXTCO9Rzm5OyGUQJzM8DpGhSVxEW9jsgYuLgSDi0gofos
4WNknWAfeaAzrhh+BtrXQfRkOBTPm8On9W9aq86DZW2g+jrFVf5GIFkSB/g5vN6D5R912B03TOB0
x6fEDOGReGPOwNKVlnlO6E6e/c6MAvqHRqFGaJp0ebOdtGGOqv4MrzVVd/bHDNDasav3jHWFkIda
vyIMSAJOMkDz7h2fF01bkulJhX5LOH2mtwtSoMWQ0m9pB+1qi2/YEZGOivdljy2QAXYt3deo7aPZ
C8z528akBoKQt9NJ4oX+dOMYrDP2xTZGaYNCQoWQZP4RaT+ljTPrBNlzv956Uec/CS5jECWM4Hgy
e3FTw8/UU4BDHWYzE3e6wN3F5Km9eUkk4nGfcfH7N6rkj7vTDccT7r1fkUX01/b3DU5iinYxifqD
fxMoNdMXLEk4ruiaSfQKupz/RkVAwgoTKBTmXXhpmVOFdJfDT2KHgaT2GerSZ0W0SwAFem3NeN8i
bMtgagiJfTYKIF87dPlcxmjUS4o956j97y2YdNzQZsLPlmDS32EBwlyJ6vY0zNeIq0kAhFuwQ3r4
XUAb4Et5qshdMmBiyTMN4zrzHT3Jnm0ClgkhPp3Tlo5nBklAt/x0bc8l0eNus1wvXU6iDVHekhI6
NleTEfe7/7T31skiRLnPtVXakT7PvPDjme/Oq3jVaqX7/5LT9t/qNrwRbHiTckK0DwpNOHSlhBxY
kZAjJeBQWYICtLP3uAQmZ1psB18m0GkLkQS/YGVSxatoQqadOrENT+bLZXRNeVvLjLExkkrJhBWP
QhEJh26Sb9kZ/LoxSXd5QYlyjHYIgO/EBABCCA3eWn23HGZ6BDsuoOOAkOh6iJDEiAUhXduUkObM
489zry1h37rhcPL0/cyYDQvqwPLTkyrgwgSsLL6kd35HrYiE420X1XlS3oGjeD4msFMzIdaMH6r4
2j3PtzkShCRPTRibWSciQEOkEtZzQ8Oo7uvsd8iqJRL+cgU9W99Uu5UsalimWFUcneAaJkyBTLSS
fPhHG7XUg3ApOIZrdtOlhiiUyV3JhceYmKho05Hd6Mgz8lbNx6Md5zsbaq50Qxy1XIl6JY9R59Nz
KbS5ONf/BAd4BJi2NKqkwxnLk+nPtieJ1RimYrw4FoxI0PsHIVfp7aGWA0wF5yuNsuNiR+k7pGfQ
zZ7oqzv3v1bzfiMNWFZq5xatKe+4u8xNpspeb5oP5SNWK04hvKRZYMu+hA5KFtv/CiploOLqFAVk
/V5nVs5r/dVy+NfmbiPT/xQ4RUHw/En4p9FY8Rd3mcKAUQst8ZHh2XxouF05aDJ/pl4sHpGOCYmk
piYTM00JVQfehD5NCgIOX7fFrLEgPZ2xe2wqRtWEC7cLokqd06S2nHEUQedeH4LrPOj7zpc4D7yE
QhVcONDcr3f1nwq1ldd7uwqvE/X+PNnoj7iwkvA/DWGQL1D7lbeW9bh3E5vU9dJDC8m6JNeLSa20
nB4ScviSzIM5zjJ6hMlsoGoFs6ou3KjyZSHrN6z/Gl1LLtue4uJei+HCd431q7lOhKMS5nx7loy3
1xA4a85F7ZxYD7C6CbKXPYG3XzbnqCMqPbaFKelMcEDUCd5ofwQWkg1CQJIHggwEfw4hu15ZCGcV
HeHS6w585P3WSBP2kirpzhIcaWkRQFf9rpZkNyDDCMqwZUSFGF5ytpM1vtveW3vr9Oj39ovUxXqL
Y53u8A3RudZ/OsxekNI3ykXT+Q/XBXiqc638G/YWe7UrhRqj6wtBC1Jn9XnGiEDFN7+hGn6+YEGi
BzdBVSLDws9T8rmagyhP7FRj4ufNGhLjQUs9PLNTOS8QNWObiNtzIRloqGCWPjgCp175cHcsJJyr
aCBOH+dA56TwqkeMTvVLU+54y4fB7gIShQRX2vwzZOIsiPggYGRynaZrSk8x8Sk5F1xtTf58+x+5
+g6mNJxPxQr7LiRkj+4tv46511+IyzkY7UvXNMS9Qozeia3+QZhPeZYtATx1shmDoTNr+9SEGAKB
9kiCrA+sLaXoly2iwnPJwsJvJJiPqS0tYKvUverT4eF73MLWqqqFqve+B+CdwcFjMJXirmpUJAgx
UP+/hv7P6Of/lm6j57vyh8KH5XnJrhkQ3tujIsHsRvaXLtpYwsFizCgqjEv9fbXhjAwvKabBCiS0
JlCyjZFjFGyj7+jtsnvbcByIFDxXZAT6cXc2x3kTGYnC4F702aE7rL5b7ZVt7qbBTUaeaP2PESqO
IuFAAdliHYeHnQ45j7Y99yscznGBlYyC1FEDmkCECECECECEYbMEMh/m8jiOmGl6ahsWxZ05KkrO
m0aMKrAb6omlOi34BIQhPltNRlUjRDBPcfu6izNED9MHSWZ4XzxSOeCPYFESTMJMyxshCBFcfsVR
p03pjnYP0ez+vq26PyCLZPQmLBxKX+uqWba6Fw1cvOkO8iKHrkudCmejczNeX4NWXaPxCdPIW/ep
8tBEpkdRLvuDUtP/i4/uEKRpjXaLOM/RsmD3A2qviB+m5GVR6aeaEVwf0n5lhnnA4xkEf5zcZbrH
+3nlkL1QPWnaE2fHP5N5/03HojjzO1h2JBwyguPwYUGIOnsvvQIbkO2xAjOdy7lo+HsSSy6jCJg0
6ONfpulhjsJtPz5ur7Rr6zhtB2egU7C9xmNcR21MP3fY4dwrNMSVoq1becGeHidQgamqSyz4+OvO
D4nirQAMzuxtowZlCjnGq18MNAfwVp9+Xo9Xrr4MUDtf+NDsGG2a6E8z4TqOfdF2ogGw4Oo/5G8O
oMToT6JsPgDhqnBxaC32H0doMjf8ZsLF6oANXVcG3s5eOUikjJplVtFirefrG5hgWdsNy2EothV/
n4u1YBybYzGacDO9+K9pLSvJWwl7zK84vH4WhJHoSGVt8uXNYXIGwyty9lyxiUZmvPXjNVUSWora
lsLmTM8ASSShImkI5Q0jJBQmQoZkNbGcvhyVPpczuZuVGqW428hyKfIpaE6mg107/lse66vQfP+c
4kt9CCOlUtGr5b8b35UclbA6EFAp8lfrk2++G0kHe8xuNFK01sCKolymc+tcxQQYtfqJFSgoSbTA
H4JwSmxNyP4Zttxy6kKH4ntVDjshUbE7Nc4gra8yTh/0dNcuAFx/TAAG10QxK9lt1nrHwPqydYT8
u9wgZTJBYdZiazxTR6rC1YsgucClW/CMwSYTWLbFflcq/UXjD4nIcTTPk+EutKSK1XboDrbfCrjQ
+MJIwvHgwr0wC91P66Q4ifRhPW2WT48kZWOxh4sT40lzK0MDgDDAgzfkBZP3GNAHtmm+t7MSAVJr
4+78kYTloP+fihSYSEngGx5tDVGsMoiZU8jrrW3vsJ6m7fSB1XBpNNU2t4M7RFkXjFJNHSx2aGiJ
d7m94czB8NgFXweyEWg2wHDlGZKgp7CLR3+M0Rt0eMJpUQDRz7xewNE0lFJYwBseH8AK0Krh6MUl
X2ce8KzBjUGRlgx9VeYIuOefQUUmIZg3gcYf7xd9ToiCX3vhSgKQcDW2alPIJXnY4PQnDvZ3UTSO
gAZq3OgkBmTdoIECBGCL6DuUrXvbXEeuZWiut/qHp645f3yH46stGWjJvqMhsPmuEYmbIE28uFOG
Ho+jr8b0/7hpywhRPL1aNoVVFVIfdajXcijRCXFhYVHhIAABgSbLUKFMpy4CycS3qd1uUo3Zwfw3
5r/iF1eUD/obDDEKqpX9l0xjG62+n3a476ef1oLxaSq18gWQcXCMreP1lCK7f8sbXXMTyZA54xAj
STFg6NLiFq/f6MZ+UxrTHts7fcHG1g7sUrECvLLx1TtJ35Ny0vfLC7cfPUj7TuzaNp1z423UehbF
gYPDAH66dLLh2/7VvmK31GIXXq0uXZ6WcU/5z4RmQFEvnoaUxX9vNTPkPguxShz1e8318Ukw7Twz
KZxuIUiUjMgfb/cyzMSd4cvoysiKbTSURpENKlvwueeKBO2s5SDQf4z1KP69KYM5pvMflCtHE+Wc
bRmIUop+CR4elM2FKwpeK0gIoDqY+ir64xFPrj323Nwtl8nJLe7cEyp9heo2m+sATBrZc8iuH9Y/
Qeqb9YJNQ79k+fxtMEm3RMEe/fx23C1NWzIe/4ZyHZVlx1TZX8Z3So8CXd/pi7te5HGetnqcb+mh
4GkV4owhY2UvofvWxibG+fcrW8NMtrmZ1ASASmTHI/jH2tCZGieC+SKhscBky9SILvpLufQasqry
GflDf3jnQ4/L8IjvR7p/Q/bu8HGnqFRy9aKCaweBvMYKiARievnOzwxCt1tHKSEk/YYgVoIJAggd
iIAIoc+X2afyVWQ3961aTZJQLHVcFmnLJtvhx1O8h5iCKFn42PZsu6XbQgFUl1P7ejCp/gtKWMYL
tNbaEPVYvpyF5QQldx2XhnCGWvuMEHseHOLAAEugDYO58u7jX+7E1tYnKSDQHWYVofjvdT9Kx6iA
UEWPj0uGK+VbIiZTXodn5YdbjvSgHatp2DaSe6vPC8Oj8aj++i1AjG/LYgQKwiF3aIXE+7N+Yr5v
ccCpCcv0lRLIWzXqckihjexochUwNeQ8mQpNKknpJYUoGeqTt/KF5kzhlrjsohYTW6dAXM40OH+A
QkAyJyCze963RPXIAuc4ARtzdY7/v+QyblssPgsKy2MopQe0WT3eGHBeGCkin1GVqemlgS4Jr8Kd
f1cdKw+3sbc4y6SUKHWmg7OBg0xvXlX4HsONIoQ/d8hyDjtXGiIsK0C1hdE0ACwggiZPx1VFuVrJ
adfINH9S7TlKc3rEDL2vZWg2WGpMlggNxQDfQdvEXsfxF43WdaPdVNpNjG/XQgLir6N7n3Ydofle
Z1vtUTYutKfxWX0LJEcd0ouT0+FNO4xzCxpHcJefnlWQmKnngpW+f5lApnfGjx8k9VAKTAad+vNW
5UtRG/QXTpyIVRnJ38IEpPv9W6SvC8Pjgs0A3i44DzsPxFAj6ESQIBDSwK0col2DiYF5b9NrBAIF
Cxm+zw1SFie1aGZ4ACASouSQJe+h6rFb8hCLLkemhTNKXDDN+1l25/Zcix/L801ImFEkjMUbsEio
1ALSpSe/mAJAJfmJu/YcamJ/lkinR53+E1xpnpiWmWIuY52Bs+0Vece817HAqoScrUrIitU0wniO
1bBzA3BgIIALUg9vEGu2uHg56XO+5nxQ8har3rChXN6k/rKGczxhkgvQha1dKOPLq9d3F3PXov9/
9FuOq1bOXwnmYHgevZ3CycwHDHIQbCKE+QyoKLvYFGrE1Ru6EXQxOh95paXbWYcRV1hYM31vrCGd
LqXlogMnn3qR6bWe20Sjix7UzHvz+lmLDh362vKE7OjOOFsv/zrcMf2ktxZtaTxqc33x2R3E7Db1
St8/04ND9e1dzeSuNdviQXkzWhqlwkOCBISj2aTAxQ+lANKk15cOe0JIRfWHS+y+CPcldk0fhdJW
tazhgJjLK1dklJMxrhIHimyGwqay74umz+jyATU2SsdP4YdpJTCijPvy3jR5dB0ExcHgIARC3a+0
sLo0Z1hvUelWeZ2NeydP9/tfQcnzy2Gjjr/5wJJfqarV+nmsBo7Z3Y6AiDbAyei4PrVhkQG9jhn1
uEnT5zRFou4CDKauvNdQ498oeLKJrJ3SPe6/Yq+515qm801q/jfmKrccDdFQlh1BL61tIN+CgfpB
MY2Tdix8mdPRGnQZ+7ZCXrj/MKxdVAQAme67K4L88BUHUdfKsUn67KfCCTrw1w84Lhqwa2gOmyEY
GJX0aZng+eGr+fgTOu2iDv/Vq1mPp68pYQ62Z9MQsSATqQcWOqboUf2TJn9hI4oQpmT+mp+d8lSN
MTGnle087lpziQp2xJoHI50Uf6GFkl8GWDgoDTsm/IW6kyhbkkbDl9F3WhKgurmYzTNXYT48GRMd
2boRF8df9GZmkEbckM40aDdwLGdquw76LIDrzNDAeEeU/SWvM1yy0uAV1JM5tQ0c7j8oHXBWfyIg
InU+VayngfARSg2HdEIQx04fnWCi25MvtlURe1mGblGnjA7z96Y6G/7BaCARuD2AgEc2b+JU+4X0
7eW4f688SEB0aUcdpbEyr6khzktfL9H5R0nvX9sLQoy/VLPCZDSh2wtGBVGzm+/vdZcKJK1jjPwz
phyMiv470yIxhpem7M7io5sOhhbq0KoqZ7r9iMQUJ7Zcs/JPtgs1NNv524d87uv2/sKlQnk6MNuo
cgRtwkz5m7TlOJ3/e7FMxh7xgii7LMeHFLHlOz8R2OTwy4KYym07wgFdLafNcoIdhHkGuSl/2R5t
sMU+KRZ5kJgcycsrOjtuMSGrq5GG+SH7PGqGWlq5yvYa+i3MtCl1hzhN0gMGbtRjkpKqKpDeawgj
r5Tehl6xjJZbYSLFCzSOTVNBZRebyQFuzNnNcWaSlq59h9hs0iiWbP1gcy/yqS/KVU7i1xAZKO8C
7UoR9ExKi1oLP1zgHpDeFitrwR4IvJ0FOiLDKv8mQQ444AzyGTXUo8v5Qapnv/6DF0SnoIupXb1d
+/jKeABk7sbvnlB16JoRj8Yz6Xl7yVNwmv8NJoqAwoVzi2Tl4FO+YRyUEAgoWp1pYMFU0LFeR66h
8+4i3bSUHswOAzeBDmUAPw+2Y6RD+66wTkACV0JfGOC8ScYL0LQENlIs6DfmyMTs0Ckl+gU3YW4z
M8Yjt4svH6LfpWIbedPxtgdftihnr+ym5Fk8n+n2MyjJtVC+N1Vc3ufk+g2E63S764vsTcCSXKYQ
ZH/iACO1vGFvUd+w7/ZreZJ4WFKd36KZPXgEY4zvI4VvefZmGGYYaSq13+K+CF6GfSC+wTMfLTcv
auZ0Nxs/ctu5ns0Xh9daCy+qTvS1hBpgSGlvPHmPz7FLn4YVJUCX0rXe2U/zpXHT4391da2Sqpey
JablHCcu8TNh/QeH/strAMkadNAb8tNZqqlgaDaJoLe4UhMrZIHz8ELMuUXZp/EtLUuRLrkoydGR
gd3fT1SWY+kW4HZTtclH85M3X+hyO3zVl7Ic0oTCwjIkqDe9dKEwIMCQDBvzr6vZdfbgsqIW1868
5kQocH7I1A8ZzC6E8D/C1YQlebl7f5OXYgJet+Dlo86o4UN5D+MNC7DvSZ628Gx7W5+i6NEQ/KFu
bodDGBo6TsGtQ5CYCrG+EMdze/+JTrUHJYu7XhBWYOOzXkjcm1ZzvXpHxQTdror0LJWuYZR+6QbI
GtPB5R9dy2Gd9vPUo/cxIl3NrvsxHXvYHg+3GJbbgaeFYIHu+F1gW7SD8XEhEiACGptDTDd976nW
SBXA4leDwA304teLJoT85GcfZa1d9DkjbNaqXVg86kF7ChbGtfDsB74LP4toyXodV2/eoyXRPcta
H4NoBXvVR60Risyxfu4R9kcWZq/d3ipdi8uN4RJadQwrv9zJmfAB+QiOo776LZRx8cFZ8xOGBXnn
HQWBMU7/6tfw0TA2YF2acsVZXJK75tcxTrcOeQ0fgEAjJGxO53d1g6f4EOI2UVqSds40JwEMIB46
JonBYKHGx+Qpcym7dB/gzz7JWNq3KF6vXysjRO6prPy/t5e7H2ygopsi/g10JWfvTKxhKwlpEsKm
BjaLuhU245Ko0HWrMkOe/bxIjolSfQuZACfKot6HOF/9PJS2B+TSHzxXqbaJhyfQy/O0WwMR4b+S
lB4SMgD0J346Uht/W8e9T2XPBDuWPpU/Jjcch9WUZ5w+cTpYtZsL0MMPI2I3r2S+jW3kBa0sCQnZ
iNsH7sHn2BCT4TaXFlDdKJPCkhzKQYIu2eqX+NEXEpNDigwXDDuNQCARvQx2fp3NOtmP507ev4XS
QnPOPOl0PSeEYas4yXFmzI4HrRX0tDn56oAI4l04+/77kaEBbUECx2RJhpIIAa56PHQru2ckdvj3
YkWUEHsimUndM2pqPk67RV3bG8jPdo/C4XykQNYRk547mRjuAKzZ0QnaIue18TxcvGoBGzkqToZs
l29n1D1sX5DKPkHhPKpaC9Syh+IqPruMrtuXVRiWgkBj5A6/+s+zR2xXC9FQzgIrdTRWj7e/0B02
RmetSKqCFb9ZTDfwu/sP77eJr1Y7H+7LI63LXQ3osd4WOHUG46tFZu5F1oy3YGRPS134ziPMyuwy
vXfnThqcmgHZLxl94L3MmEkmuchWxDHyQ8rc7DD4T2t10iQ+HrXastJ+SP2PFgkCnoYYfdUyA20D
JKh6s61fZtfaLKs1iErv+xFy+tUxtacLH0Pvp1W2i0/sgNbCrk4rABS+wBfr1N/yBe33W6y+ZpLt
9+H5695hwvSSuYbbhcH34YjEoR8OCic8hi4LCRAEdvPym3rM5/QZONDwIYaDzI6D80fmZ+mcGy2t
22y/jnKl+FSvwONnISHi97yF4VQTiWQccN4RWcrOl2QaZ63KZpmhJRAfnADbT8JxdubnsTWw2cWK
25hSnS3hb6m+y3oFvEXLOFEyVcppiWqPyepEugT4+aWNAuWWk/D3MR0Mx4DzrqAIYRs/jGHHtAE/
z/XZo68uKfNdL45SfDcBdek0NxclYWhW+6H5HnE1tp6LLTun/D6NvSdZQWwSumJxVf7yBttG/6GX
hUkKIr6S0kdRd4P4YtMMe9+sESppCiynTfuv/6MjypVrV4cPkU5Lp9e0QNFi7jLxaqzUwsl5BChc
5HIMS+st6i9vxNyj+K6l/g8spu1SnpE+AqXzPk2F4GbExiP4U1128xzuPXaF0wh7eBHdd3Bru9f5
u7k4k2wXtlWh2nLBF29z3yn5fZfIT2TKE8GIDlw0moEUva+HmYkqaiXhg0rcWsrrH1zenhmBLPXb
oafvMqe1HJsW1ixgtGRbyiCCMq6rLKFxR4YqKuJNL61sJobmiYE+iGtULjwKgyPsrYgMqyH+TlOm
qqRSYN5DCNEWLWTFn9BVCnf3VPfW44ZPrK2vZqFgKziV2/xBeaq4a4ELtTkk+xenHZ7jdarBp5xe
LH3UhLRkq9CHLTT1MwQ3bzjK/P7gqjpzY03Y44deNUkAQIpzbz1olO6ZXI4XCMAQJDRu9zz6oXeB
+ODznp3qfnzUkTHmyJUWzBb91tYXit8D9B0FjFccESfjjj60eF68Zj87+Dvi75qzoA2zayveToqW
tQTmN6dfPRuauA+14zABZ4+NSNeEGas+tP1mQ7XF3qoId6q/5vsfjB++Lg5YxADRqr9i9mt/6Q1l
tFqM/4ATANqiVjaUSAkAkygp8Wq80y1JWnkFf3ldDPdeGRtR51wnllM4Ad11MXlY06Rnv7GmJnFa
y3Amg7thqxc2OCXB0bsnx+t1hVSsBkHrKJrMIKuAPFk4OYUPmuhonmIXsfDHHcho1aP+U1uC7ax7
xcYPQNr1Nm/Ez5Ux9zfsN6VlHofqY/ahSd6gsPSUm+1Bon2QKJnFxjUcCzM/il30tHqHUM9UsfGL
811JxlLLeciL3R3Ghh6bVBDTGKMIpZ8IDG5GjqjsQBW/IBG6WmACxGxhq3kIsGk4uHgc159QUL5H
hrWTbiOG1RijoPmQJUaIqa2uuq85/EW4+rmK+8VuKMb848Rm/kHjfYsR5IUHS9Fc4Puf8Fs1Dqlg
rUfLfHSIACHTxAEAKOvojcmuRdYkZ9vLv9/1wocKJb3WsCQaJl8H/xPuYfAkaBoAMYgKfk5eEO6y
ylzztAoEh60sRzvAHWk3Mxdb9by+oowHHpgNOfoWbFwqDgyfy90JwA5HOwXlIMbhtnTteOhTsdTx
u74afuCLzZFwuKG/Decw1VJK2eufKvvA+PTmFi4UskYOZz6sPZU3gW1F2EkRFypWnQputx/+r9xo
JSXqYT+icZsNZ7Gw/g2E5rMHjsGcIIbvB3QSIOM8zG+HSeT4z9bq5PBey3MnS3H4KjPtR8I1KO3j
+9HUE6ClGaQ6JoRRw3DsJPZiplcKpIh+2yPpYa3Pck2yO0w3OQoSgZw2zCeBBLqyl0NTbDcPf7xi
7UK9WQwGH6tsqOLjmUR42OVk1apHYrjboi9zcCe+kXryCCKUkQCASIGmYQaMGt9D4+Q1TmSu3b55
GTfnEjirzf+bn+8dixvNbB76B1s94vwePNKULonqY2QuVBZ2mK/UuFi7u1bvFTWI396hNgeILJxg
4HX8QJIcXbNxx15OfVt85lBw6Se1gVGl/6Oz7raac6+ajnl4uFyM0P4yvQhaYow5mNdCgIcI0+3s
+AauT77EJ9mw6Z3pnedhXjlaWqrr2TZsf7tQKU3K+OEan6qYX5skWYEW9X/w8P2sNZC+4Uwfa+kU
H2FjL5ode2D+BqIECnpBQzTLOEGA+3e4DnioPobF13qjxOBRrAUoUTSNXP3RklqO40F/CF9avlFw
4bLqBsO7xsuEvMrU2m5yR91WpLt3Zd3WCDZIbxpd6S/V+ng5zbnAICB4yh4SwC+QH3P9n5dwe6/Y
pXNEh/hgST44dp7M+T1otdT7OLRvcPJf1DRROYkiA6EjFlS17f5KfSfF8FdrfR+L769r+rPa2CYj
7uwJrekwWJazD8SInVsO5V0A98Y3UYS39flh/HTi2DcdD6tNdrM6IE9Seww11VHJ/NXqXEVhtYvb
aqcI0wUtCsPHfBfe4LhX45QObvSd1dGu7OLddnhyzYdLLbLeFq1uXvOzhPDBWRIdJuCXBHbLdmwu
bee7EWtKZ8cw3eKAnSQrDnAkAUadgjhAgQIEWLAS64IBBQkdF9M7TSoudVLtE4yFUWOn29y9RTDm
70DcvISUblP1V5wuNpv13wRQa5+H5t+82oe/5mq/4dqw4i6+LnKTFU/sgLYyvxV/iNIcufwnz3dH
8CzARq/QZGkvxSrakr1GSlcWWxNAiILJjVUksCuqdiGSrEVeRpB/VGFtU3aa/SXzpWnYVvpf5M81
r8mFlRhd8mZheoJMJXpFXLeKCipiED4xh+FCEIQgHfB0Ah77NFC4XUDphC5BOuBPKHqZGYeg3DFH
UvwurUdfyREieXOCxtUJ6nWcTvRAj2IRMlm4qUSCtjiHb1r3gHUAcfkDE2zHgUim9ulEhEDWoNAc
cmg3L4KrodQAzdfrUppGAbAyAHHasWDNUzwwFsJfEdidxoHBB5NEggelTjEAj2IEQ2kySwkSwh8I
CA9+vSvcEdELrdOtdesSOyuu0+ySwa+JeltNbg6o4nxdcYUispPGqySvNh4h+b+voh3jErYCfUPs
PwJJ8ScFo+9YhLMdRsQcnTXrOzH2+YHlfdc0WnkUKgJvI7pa5jDYQjLyA3RSYpmiITBkYt4oeLXU
kJ3V7X6xE/OgjECEGCxCIQAPR9snr3tAUAdTSj9BdeMlVl/YtHiTGkVFUBX2KiECfGEgxIWU7w9L
JK7NAPS9eSHDxTMACFEDEIxpakA2DoWFxQ+8uUMMCABQANnGt7WBTZRCCRK64JbGLobpKTBpuw7t
3a5zJv9NZr8fZ/YteCz/xTw2/6B0OUPvib6e1KboYCMGz0rcPBwF6V72QOP58PRbKd+HVDMoNgSO
A98n0Ib7X0xvdrke0+ye6X11bdVXb57l/+3t54mijL9PeajvEahJtT8+z5/P/PHpGTFkrYgi/S7i
GYqMx+NL/b6zYQfEM18vfHeV+5G39zp5+5Z+mmPEmFjO/MOkDJkmcMUA6zDOsGInb3f6W0nZ+TG4
4xwnBwVbAcEY8Nc4B2ZuG9yzeQ8XGwC4ic84QMuLaSyPrsAtZIggdGeidv5n3n7+sNw2d3POlLCg
kRJCQGqJQQkkZRSEpSSDVzePHAGNFSQlkDGkg0G+tZTEsdcQcCG7gcAHjg31FxxRCGIUGTHwHBJw
ENDHUEI4EJRkDp7TqeJ/hqOFY5TWY+4B6nqE79Wf4I62+PZBVD1Z2CuyjDkr74ik4TVG8/8SZHIU
SGM8AgIfAC4/KPzYWl3u1C3bCp/24SW4GwMPL+5h999XhzcTv/t1lEWck0TSSVpGdE6vIB5OGHu+
Z58BcTDxQdfb1MC4GZqsgMLUqFeVstkI2iayPe/Or2Ph3uPQ3NCGeoQKADDUbz8o9Wizmo6Gz5vd
fN4POPozf7fzRyyUCW3VTpYn9NYaUIMKgmRE2O8z5Nw1t0BIqdl0zYB1ig4FKXNB4xNwiUlqOhPd
bgWUchDSiBIBgNWXtF88HZgyCCEEIAjFCwjF7eF9Ud6zEAoUhKXIwEZw1tDunvvFHC8kECJk/D0A
cSeqHR7o5aqcx2yYkylo5BNyRkjq7UjKG0wHo23NGtX73XhGCxs9lL1UldxVsj0sgFrxr5PwQVtS
kL1XPf90jVmupth/vCEB4fsvFpWJAjM2Ah55eb0dRnHXy+Gh6PrSru+JG4G5vQug4BD6IWD6lLcr
+XEMDvxYaUMyAcp0dcNwrBPBwoGGCI8cB14Z4rxvMCXWIL3dpdu8t9KwVaRnJCPQvRSRGjgEGxaw
hMMt/hAqUHFXFy6LQbBC5HrfOh1LwEATIXLtvn6vB5w/cs0IJOpOZCiOeAiqEmASQhDnyTzgBxe8
45mHg9bpBmOkKMYkhOgX911laE7nAxA6IPMh0Jx4wNCZokDEQHXZV3CI6h7DdMh7QyDaxDvM1/Ee
2OxDFHTHnCYoBuJxObmsOXUMjYbGiCyiVB2kjwCXETe8XRv6k5yQA5BaimgtCZxbxIipAbl0s2EL
0OVBySXHGinjDiUGwIrYIh/JoaMMc6tVURpzqiXJKb2LFprdI/h7CoxxIKclMtY74QiB2yUhZN8x
y3TY3MQUxBSeB9CMbxpUB0kECMpAooYSQ0iujpbHefyzGiyGwu9QJ7HVq3Po6QipMx4/Xzc0VvlK
L2//eRa7zLzOKMrwrugesx1WDkMS4LaAgQUUvlQzW3VB7k/z9mR0P2spwNh1tp+G8gWLP+fIoVIy
X6yGuyNk8LqT2pMrT0SwJhdujcWGEyetdu19NJNhmZhMyZkM19DDeRnc8UzAf1nbnor2bP1ZMV/M
MLFFzDNFCR+582cWYJgSKWJJxCwjokUh6hFGC+OBXB1EgqK4cX1SVVZsmOVwQPxWEMmQwwyhy6j4
9JEeQkKuHj1VKSZAglUmSWSl0pbG1hfBjsc+gzWR/yCiWGgejzhKVoGI3YxzjM6HOnmtCob7Pa/R
4TdGHolaeV6Edv5TKOmjdmMADUT04rTRRC6V33+oNH1NvarTp/SxHzYX/m5zEiIPnj424DyQvrvg
4hKlgmUCISMCN0rl+Q8FYf8cHIBT8j9iwFwgADVvoiF8+GqVk/5TND+EYeLwafg/RtOckLgkqeRp
KE2zCEd6qYfiFk6v+P1p2IKJ0mSGtUrmzUgYKlKfFjYfSOUQJ4dBAqwJTAH9a6/Nxfpvm+iyXvvw
+vsA/PLd2Zsl2sz6bnQxCt/9zLLFpi98Lpz21cS/OvZdJ83Qj2RXioVBz9xTQDewBArovNmriXE4
P/sT9FIUGaM5lAlDKdqU23B6Ep9pO4y1FOsO10E3qIGoZCSpIRfT2dF17n9SwAIAUXPv3d3sctMv
YCL5wQE9CD2yJekFlf755qxzwBATyCRJD2abvKeFUqLEuGAj4y3uZQgQf3g6KH45oRhAC/P7nsus
1gLiQT1ow/F2GQo7YQaDnmkMQMz5ebgCCYHYGoEPHq2CMO5CEJFAJArADmV8x/sNLBNDRB/eevec
+yuV8WSk4vCnz+5+1EC/YobbyDb63g8iirMKKqKtHADHKp0WXBRwKkjmN2pIi40gm0VI+dBmFrm1
u8Xf6vvYbSAvldxWbl5wfd9hDtZuGSl7lbJI0m5L92a5Vzb8bA285uSob51j6uvkDeetqEkpqlA7
lHBSxHin0I3wiOC0DpZioDgYV1wHXSYHDswlDAzhgfs6adFJIcGEYcEBBJQQRQIRhSPBk1Iw6uY8
D8rYWDfahKUlSEJJGSQkhGSEkJCEZJIQkJCEYRkkISQhJJCFZIBIjBbKnq9gC7Qd1A9601ujwnGV
DQJtnCAptnywT9DU7wppJqHSbaAYDuHo9tOI7a7JxxQN1YBzAAIPMBTBrSnboa14A5MO/iXHbNRD
oTJ3taDp4QyCwhQ8poC+Lk+p8ScXLNG6LmnQhSBSXEgUchPfA0cdwHW5rcMjZQhsFJ9Lwbut8xoQ
U+D0Xbfb9f+7ZS9AmD0R0F2GahK7+3YdO4r1KNk/lP4KzUc5TkK0NkTGSEzCHXb/K1YPNP9RbD77
+w8nPbuyDPN1d5kCtX5ibG+1eoVIC+eBxsN8qlN3jd86caD0MDJAGyUvyHR0sxjVc5Q4ytArzXz+
k7IOqsZOuVpgZAexm9ZdHHNWApaJIywnMIqC76ehE4Cu44aSKaBT03UfBRJo7zDJMrENFRFShdbJ
DwfbXzVUPJp/IYzvxbMztm39037fwM+eOEgjjRvIeVPVLvWMcxI80ocL73IwOCh5fc9RkwaGMC1b
uHC7lvplhOKlpqAhR5dmxKQj3ZBXSmZF6tPoXBtH00vu4HlkQ1ruroqv6DmioNRpu61GWqe63NGi
labXuCKNkCApFrwUtWJb/YYIXtXvhat5VeXpret/cS5W2Q5tH+xuzjb3+7bWKozq5h+6uQ0w7TzO
esCTeY7L9bWwnbbMea7ntvf9gKK9n8Ul90R3d1+CntVX5NedTqHNsQGaKdf7TACB7Moi3LqJxDJs
lRNx+Dsu5v8ubd2FkEOQQQh8eoAIBDUjj4S7gIIKDTMjEMGt9YreXzfX45hzxuraYiUUiBq3aHIY
fhnjHb+YrhtbPeKfdeXOAg9f9HM3COMWKhRRiyKOGTJkkB8OkpaE/gALgeFKDSBkQJD3w/pAXsDA
mIUULP1w2nzBmRgRhtg/CJCKvrULuAQIQxKhx0QV0CKhGTjvlwMuSBYgJIyzZkAVZSRDQSoG9UqS
wMwsYDTkpKIdRKhJny3eKgB66ZmbKeTYlEQ2N9IxDWPEBZDiEAMmuFJPyqTpdpf0zrIQ6JEm4RWR
1ZoeEwTdTUG4fxpvVLFcfh5WY9v9KPJHK8e837T186vKqKrwA3Mmqk++WPmQfBbwa+RWWkt061Jb
RJHJUS/QpqKdl4uUC8YygK5UtKf1ca7vJb7V+N4uz003albhVZi4YNxZTF4V0SB0giNAOV3yDxj1
ZHP1Gr8Xv/JPX0PAq361uJCvaBH5SxKJXsAKoCFjCOZMwMMMzRhkJgg+o9SYTlOgIpAj1Aek7JJG
z2h8DUIkQMMRBPihkKl+5vq0CQZ6PQWL3f+SUMBo2iQUIOoEEBTIG9BBAJEUNaVTHTt7k70dli29
sfstNKy2ss9+88DKYuxT5/suTdX9QMeZJYfTNlf8zBJZeGT2OMLsLMIIlNwva7MVcKUQZ+myF2pD
eeUm3fBbgKCyJ4lTo1Xr9b+dtfkLqeVN7vvevBz2d70hhe8I6zFcYz1vuSwSvSYYfBOigxDrSn0Q
lkIaiyag8jDynges+B6zTmn4oaiiAwwwwwwwwwMhwScTIpI0x9NdPEtOmrt29Kt3L8rTmOF/uhvs
xZJYJ0xYkZRE6BMAmAmxxgcjkCAddHVKMiyBFSAH3wMIwhITtB75NAI6W4BdUCAoRIghm5IFCD0x
wnvSWDX6f+EudwTaFD1zj1JmlRD+U1h3EBsvxdA+sIAFwN6wAbhcKH4mSP2GRvpIM8sFx2lupdtx
5GESEFpR+oq4SSAkAsOARWkwNAC2bPpIJd5PW+ax6r8in6ThM7WCTH/h9t7D6Gski0H8xvqH4aPL
oPxIocSpxDJ83Lafsuvc1XGNGUJy4Vdr6I5kIJVTi4/QJTNmn5ma0/ehd6ocruxq7d7gjuXKPSal
QSqlLPormPez6o6EorM0RotcIZwSBuI1i4Txe2BXoJ4BGMSNsb+yHmQTU6bIbbedT+MrSbTI/1bq
KT5345OWn54PTHEPEWF5IER8enRqLkBmhi5idiH6lfEVTFTrtK1PdxA3z/b5s+wmOQHPUR8wREsd
oXTbGxEE3SgMcRNkAwhSb/apTZKS4sW6DsQMgwBxAgYO+o0BmQX1V2jQZIgOP2NkQseSZgTSCJIq
eFt43hFByLFgV0BsWIORyATaMhLFl04J7/fFVzGAyArqH6sYqmIKBmodNqFHYeJoKVg3eMCUUBCj
SmRmAIfS8KCdI7YAO3QW4HVes21Uo2EUIxo2Gj8e2KMNb99wCBUi2uMC4L6ewTm6W2o0oNi+FLJD
vftjTtEEKi54JtkDc6Q1LdqQgR0nOncKna+WTUvJOsCPRm/RqvdLzQ+xlVrumJwYAGZAd1jKd5Q+
Yzd4YdBWxgoEX4qC5E8jAUj3e8XW0+IAZr1CWisFi5nFyNu7sQtlPc3v9fGhv+p7mEp9rFh0JZtk
AMyFhBOoT6VKKcqQNWIRFCdStKPryz84jGLS60Dpza1AaSCcMH79UDNPn98mYJwcCTR1kdDTDvw5
KwTpFPihAOCI88ICkiPpSsDXeDcwGZD+mpEjvB80ND/TBoPpQgfTjYcCy+2LTWlH5bBfj5qNI5GQ
Wb5mBdRh85wUcLBUuQwjLVfCmoEwAaMINQKMMqvfGVavzt09N0TwW6D4PoP6/1i4Q19E3RMhSQKt
DMQefd7oUXC36nRxk+SugUnlkX9m5WeL0FQYmIMp3XTYbnQNO9lDjdWpdd22Vaw3zFbAb9KSIema
1Gv1YPgjjjaOPeCYgzGKESIT1HsRXEvID1hmmv8vzUL6N4YImewTf3lVNBiExaHwtNuB6er0bod8
IJHBzCY7I+LC50rOd1v4mx1L6xnLRd1td6vU32huLXm4mMibBC1SXgU6N7OBvF52+Bn/94E1pRka
8++jzy2umIQR8G4YJSNM2J+PGqOy8BRNaDm5g7DsSxLBgaA7rSjfFsuaY7PYGf0zAyxl11yZBBBB
G9EMmANeMiniPCcXoO5ev3a3+ZFiACjAiFWddZkBDiIkpRCgCoBBoSe3ItC6yI/sf2eFPcfe/mex
+P8jVrEE1ISA60lMGAkSjr6aVK36Wt+X6Hn1PxO6zOyu/ExRZ7xca7adhMTMuLhS9KAPXQF/wIh0
SnP4qE6cOOmTMO2+f6I2vQ5gBjBIKDBge/P2SxF3ARu2DKhiCvkEgiEPF6HQgxURXwUJPpI5fZhW
0q5jkHnXAD4evSNx/MMBOJYRN8Fe36oEaBDUeu42QH4ykzf2jhAyD2Kq/bKrtg8hRfNnw5w2zOUI
gcD8xd/WEIrzIvAfL5VAkWELDE1BX3/ed0YiYiYFSQQONDP3nR+c8R+edYPybZ+xkLQueSa5m/pb
fIQFvDXksZubVOmvXUcPPQD+ZeahyzyZZP10z+r/dNQl04192r/1KB+eKVmwFSt4B2wEa2gECHBA
IWTrb5ZLldFZW8b4b74eLJcmLiL/EF+ezlyV5Jm8Dq80dQEVwfOq9e0nS+l3QyISZ72nEv6zsKCx
uWSQ4cqS9iJEERollJHD8HemLWYuxChiYbvP09KUulUqnh6CSqPumKtPcYS/gLkU/aelM62lAL9K
jBmRidEsicpG8E/4gNOhknusS2OPZz7IfBhrHIi92YHtTIAiUnrIFQYdWYvX6QTLdTcRPUFg+HcN
0eBufbieI9WulBTWbrxWyRa92/cZb5cfpE+uzG/8ehNQ7iBxj14Jxogb8EYF0snJVXj7WjWazITm
HMQLLcP9gSgOkyQ0BCCMRa2IX/eAaUnRBY9fXClDM5KE2i1b/2Wv7tRabWhxUCYTTIkuL0Or267V
Q21gMiXaNfeGMYCYzTNK6mJPFs4YOMjobl/voY74bbAm/u7YVdcvSgZwKMCCCCCIfkTWzDH30Crz
SnBfDGq8QUt+0KkE/iyM3FqQriPKjz2jUeaOrLGNBejExwBpyPB+TlF8wyEiBEdu6mVyUNGgLOhr
sf1daZJwoFDNI0NCUISMXJzIkCIQzLHQnnrhtQTKKlCLEB5XzOq/Ex1YyMJEYiwFkYvL4+82GYf+
JRMRgXI2S1qUbWhY3SkadcdRLphnHESR8HxYnJ9S5TJsMxDLLh/K8jY87A4CTtGmrtS0Rze1EhC6
dbwm/pyM/LYgRMgdXJCqCMUoEURTAQ8kkq8yiFgMYEUcF8E9smlsYOAyBISEjILRmWLYjx0xNjoO
b23PIayEZm7FfCdvS9XYfvxPYEixhGbS9sdpyD3G+cGoBe7BAosRkjHcMVpQhbzpxx0gpwpvJwwa
HfNIjQtyD914jxQKHqkQ2JvHEDyj0Ry3bFXfOJ6AwTRBspyJyYOAPRfofjmYm7ySoGJAGoNAGoTN
rTXRmI3U4eY25SYN4Km+qm+r8u9eiMAV5h9pqRLSRD4594jqigpYwwMDpDB2NfiM66p1iT/HOm7+
bbGg1ehtA3LwdueAzisXoiFbI3IggZfq6Wul1pUQZYM7+eiy4Brxeiz/WwY27DnehMretb5kvqy5
qeytac6FlxFy0ndPg30vEQ5SW96djNPBlGVyV8tvfY5fyPm0Z3MhOmoT2LNqLNbDkMBCuXcTDaKN
mZtzQa3S47ncj9SiS4NBnD+ymV3pf1E29UJTUoQaEAxSjTGhs8seEl0Cb6/hhZc2BPUkRAgMtJZW
+JOOLZY16Br2SBwHAYIHsPXIfT/ZCfCD36q3MO3opDlJ3ZyVeRcBE9pheIm/hqiovPX6QOs6p6eL
LS3kI+dBbqcvLK1XDZxCSrKKYRC1wWl/wZmA6vDQmkk+vrU3vHtIoslJ+076Wu9xa2v7E3BMQz+w
lAhAu3/8dQ19a0WSACAdwBdBzvAOhfez8Vo0/q9xZLvIZ+3dHocY/DfTvdTwFuCplEA4ODgDgdCE
YQZDs+YDyRTqB7SCgepevGh9z71EyCAVIGsIJVpJMJ70eGcfbCi2KtTIIUum9Q2S0skYhuW5IR3C
k8GvwxUceCOIItSZsKvVOwF7xIUCzuc+Lx5p0TlW2RSRyCVvUd943ODpAZOZ2qxqoPFM+gTPq/yP
V9MrXrT01Z4eHvtz1J0LkvDSSWF1E5s8SbQ0hqc9gEkbu7Zn38xWZnuMrUz2d1WPuNpo5oJusT5/
sqcfgmIBdYYeAkwIfMzuPMYHgIhJxo2KophBkUYFRkMz2b+f/9+Vc1Z84FfJ/OwSIsvY5muJJLSd
tSSKxWPZ4NzgVbRelf3ubkCRdvP3OWvvPJ/69F61k4dswE/t/OWfW6g0cyu8byouMiSVO3uTIYHx
KPOUWfCaDFpjdVvTh+Pryz/uhQ3mocTHK1mMH01nvOAeR5dsDkJrftfrbRZGr/30L89GSf8vXpvN
e/8Ya4/vq4hKDyU1JwsxFoN+wlzzvN+6KU4ad0kXhx4KDU7u38TOe5F3rWl9BPwCAB0WNHPau+56
cXEC4BIgZgoZfADnr/k7bsvsjNPixNAARIAERAZz8cEuOHzz/HV7aq/7Y2l4CasR9zv26eTnzTXb
PqwQAQFhxVXGyj1yT7PtfjYnT4McDg1lHpHm+nJ3F9MdASG1Dl7K2zg8M19b90tDMlosfi07SFe8
tyvxiIeYTwFSE5C87RvpKyjF3i5B1WUlMGXK3aiLNhOlA7OO7z0xshwILP7rm4uYqKbekh4taI3w
SYjb6yL0ujzseViYjC3VfaujoshqK55fQykmKFkgYZCh6CIID7bzzrxwPr2EZXzmUMA5FrNVI+BY
MYXaNQlIFg693Q8vwuBvoPv3zxd5Z9h2QW8AA8OAhgFIX1itJw+uKhU2xTfjp9/AXlqQgs2EZ9I/
cjJft7vvU64e9heGvkKypReKeGFHEkezpWGgLPqUBJOtnbDI5siMk1DazLqINxGLMXTWO3xgObKi
ahsJ0PWoeNyBKjzxdYTqT29f0FBVXj9q9FsoP1gjSL9xz+xaMT9/ZmBcED2MluBBAMIMpjQkbb8x
R6XwOLishkc/Jas9r3uRyfPv/C3FOQL6714zoAG6yHB0FGYeM6HDncTQEgA8ex91SH0XJxNYvggh
2/5w5sfX4i4+FKkGJcX+uXDvvDFgA7DrOvha33KGuNPb1pMQrrA5voLYnNYMUVflcCz6Q9KDPur7
LPzM7pdM4D9YCDWhOs1gCAPD9Mid7n+zSf9EteY7t5AIJEghch8i/acTnk6GlQ53sm1FjSedNnCK
SA84sRiCkXwEeZ0gkGDBgkGDBvw8YvVFUmZmsZnCT0tzPr/3L06h978v/cvovcx3Fxh6Ra9+xqAS
SYM7Vd6gKFKEHybUD4RyrAjlSwLDKKIxYE0glTlgmlg2oZAw58zsNQdRJR8TM1mmaMTMrLAxoOs0
SrkWEOkNwkvRjnYkWSdhC4FdgyFehOkGecfjpJySlXNfR4uZkov5cErUQlw89xxiBBc+tqB9xplv
6dlHWnO4+92BISLqpg5GcCw7jkF2z0Mx3E/tWYjoTMCCC9E3AuxByemU0IcRxFAWbpqJZAtdGACW
W/IpEe3hQh7CP8JS2C3ElanefbfCErijDmjEKMzxMtjKq3rMZAepGKa360tHnZrB2itYNyO9vRCl
HaUGEldLBeVxvgvaRerszrKnpgT4212b2q3AWhxfDIFk7f9xFZ3GY+rvSdhnoxgZSrDh5WEXLlku
ULjslXb3tLt1qTtD4fl4CtXsPQnpWCvBBIy4LbFMtv+YtIZ6/1fKTQM7pAIrX3tTpicz0BKDaXRF
Sbw3diOfSWA2ZTnw67cAwjPC3p0QSKP8vdBqHebwYprtfI4foDMsk45BfIn9a6mZbFhTJ4gUMV4y
t4bPAZHlj42bsZjzCXX4GnyOokUbRISSYn0sV4LAGJJI+aKFf8xOwKpAhEQYSoiVEbwgEgSBQSID
56bnhdhy/Q8IttpazzEAbevOJaKxI/PKmKPs4Js8UgKTqBMvwxUA37Q4g2Beiu0sY6ny6qjenN9F
O+n7zX/nYXH+1MB61qCMhtxK7I8k8eVpsGEwiaT7CFUDuB0iGCu3hcpr7Yp6vio1VG3DRp6KVnf0
ECLXIR0b+cMIcgfA/WEl72BiCFeWp67EOVPcKq5ExLwaSt78+11nE+0yIQ8Q+dWVBtF52TYFhbaU
/UaEiG61apxVgfelr9/OC4H0Kz9aamsvHfZwVunSa+DavtpHd6MnQSavgqCKS4c95UOGD4I3JhHj
rnuX403WAflpWmanr9hzPrkd9aMy6MBzeI2t6k13a5QwcBv6QpxDC4bP6Fyw+pmdQ5M9mvEfwee1
FdxXLbJNrFrADfbyx1IFHRwgIhVvMTp6vOVrn+wHnCOFqciDuz+kNc7KisbDQV525T7x2gqKnH/b
IIzdEcxD9Z9bGtYdqvZfo6U70/eDmWjs4b1RM9giYUahNf8qJwCht8/uMcFcBH3ISt4MYUiM5ckt
wCniBFzJ7vI0Ro2s2ysWb+3THsWd9ol3KaP0lkFNhCn+e9V7pxfCfzQ559NtfcGi8Y2e1ZoGr4MT
UkY2/V3tNSIT0uxdM20vi4pUA4Wxt1oK0yaIF1r+9TMrudmmOyEFc6yZVPi14kxq1Kq1rxBfsuCc
XZZ+BhCxUTp+dRJWg43CopDJCd4Qah4xLervJ1STvI4YBT/wsI4Ppd7nfIsCK5n97zdNW12sKoGG
O3msXcX0WxEq1/v6kEW9cX251fb4G917i+eYKeQuaO1f7iOqJmpJ53O1ZNnWYm+pYyLMUFtDGpwi
hoYjZe+Kh9sXqHSijV6iI11IKP8+riolcxyo85843ol9CF2d44r/ITAaN1g++fSMsl9pyhrZK6Np
B2Hv56zqVcUSz/2nO+J/0kAjBfgyM2z2Oja7jPYv9BX87vuP8GWuBznWvVbWk+GEjmC5xxjgEd0K
ArkoHeM+QBIBIRsaaqCoRWekpsbrySTHwWrau7Tb+fkBFJjpm7/JwtEpIbf3RwyrRvaWR1g3ct21
LdlhFtelA0kS8qAJulLjttZdg6OV5BhPYkLfeq2KtKAg8upIOsfSbpC5HxDAr9KO5GzS9vQBW2Rf
AaI2aCx4RnSIR1aQ0jDtmr/OGrny9oe4NfmM38kiMsGuYKEMOJo8enegh+G6jg/+Y0TNKiX5ZH3Z
1nqCbo18Bgp7H7mI/S+09/Iq4tCapNfmQ8eFyRTNZgPUD5a3Ffs4F4FsN2ZWtw5stpIFlvFBQT7B
hSQNzEMuTLi3ySFCrmiol0RArHy2UFkFrXfi2jij/hoj7eWf/r4ovgyPXv+r7g94IRGYyyDGQdiy
pXJQNgxf1FO8M40TYlLiRGW3Tm3LQ13/Gb8mQlTTa/Hzdp/nrakiXSZnm4qu9eQyerJY3BVZhof9
G88q2sdC26rOfa3lPiCfcqFQmlCFCF0voHsmTFtakbquC7YaHgRXsPc/3PuyeScVTqlo7oYoCfDQ
k7czEk+FKnO5rxo7bUdsY6cIDyZ6PERQ2/9B0Dh+4Rr4/5KPDshF8EfpDi4bK8ol2Bwp/DBDquxM
BjN9a6OAeD3x020Lgqu905KpWX2lQvhgbdudpH6dGGxh90VJm+CB7sPp2XdSr7nMaXj8L2ZuQNoJ
VmF2LqqVdHNWDpdlISqMsvtBivQeQ0316PBN9+XQ35upY/Mg1KP+LlpgNm2JyuGLLXrh+YMI4MH9
YM6NYmj1lm3djs0P0fr4IMkzcPZmEBiZj009FS3DoUUH+I20FjVHZpvQxIWJ82mywMKJv/LO0Z6+
TVVxX1eYlPpWk2eCzGnBH7ivlxCUjOBGSzFMl8aWFKBv3YEDLOFkBFbR2wSpgUhZ7QJ2/sSkb7i1
kWzOT5PPCDtsXxCkE/zota0o4wGWGsPyp3eM9NCYlOz8+B6W7BWSaZZN7ph0cBm/DKvRju6DjpjW
po9B6zGM4yF6s4nHBLLHIE5PC/iBW9fUMAWCmhoGJPElJdqbJc6FVt/otNAbxwT6bEfrTIl6HgYl
KZ3nZns31Hh8GWA+Koqbn+ROV5CGTLARpHJBipDmg+wBysqXyxMBHRJ60TimtSlOn4QiLLLnVQxD
0Oy4HB3bTZzBPesrOEcSHtym5ZYmyzu3Bd69qiDvWKq7DytDZoOtuAX3MI0NYOi4DQXU1YXMy8xf
DaScRC2HF/xHUwLKASc1uaA4kkagGbE+yq30mqmjb17ztQtPP16aRF2liGyC0lDVQP5Hrgrwtd/t
kme/kKz/pzk43Ov72HGSDHqzvp5NKXbHCHrCC9Lpwwf823K+2izB/yb6KUGfUNmGwSOJ9Et1gnGK
4mRTR7k8Jmz72hXZHChhx08KH501DeQk6P2zY7QmO5BTUedUrp9dSVjTRoRbEzHoLO3TcidanO+j
AQuer4P+tsHkx9tyxxyWRt3Kciro3Ks1lcSy574JK14PT/s+yrEvGMl3oLqE1de9LxLCvggEajvM
Yg0bHEjS430mLe5/IlXKdSvK9M6JnUVW5/X1amhIa9byzooafAkqSPaiRIDGHhNG583M5nmiLh2f
dJW8jcjR1WMvE5Wwktr9J30ZL+R45EoVJofG0wookRVWANCxtM7gicwFRvydd+E8wg06raIECCRv
peEhkCuHfwUP2ZX1j82MvXPCug6uRXwFerdTMjiJAXww9aytWapaRewP37Q2YcKKtRnkIjJPQxMn
9RnpAUFZIOeGm4XFFLQ9pguwPo/QPRqKYQ9TgTGkhj2BjSJEZD9PmsLwzDht81MXFIHLljPFf/9h
cZsa8mtWJC1R1f5/ZN3/6SWt7r7+ASgjFw7Vi9ejOCLSh42yaw67M+z0FJkRTtL6lwx2la4zEg5c
w3sx4lG2yt4mXTzfS/if21FsTk2+9E07Nou+8+hUbwkYKSi3WKvIS9M6ycenScdt5UZFJXcYdgb5
ItsGJSLxtddbP9fY/xqwTev2JK8KDjk9t1rNGe48s2RDA36fzt/oyZAZL0oYvCnATsqAdsVTWn1x
zOvf+ZWSme+nqfvyzsImie74OmEOrX4i1x9rf+gNsNZmiACQo8ge8oBHI9I9O2+0btf30cDEgUSt
YUM+oyC/TKmPLMjh4N693KnkBch8rl1t+5PO8FujwU/vM9XNpzAHbBnKtSduAckXLRsd2y5Wjinw
x0vhJn/zcA1O1ETa6DlyCdSLpg0/FZdCnbmDH+AsbxosfJqye1btJ56Dd8c0fBHd2Wp2Cw2QW/ls
53jGuvT9xfE5NGjKn+XFunhKyDjL7J0uNqiUxslYLQbYfid8qeah0Lf6LLyLvnsizHitgI7EQZDE
cTFQzrCnC3/x2t6tzCdRPyyDavvtn10BKmkeZe5S8Dt0fGft/bn+jzWrcqEHN2ayiPypNZxF5YA4
N7LjzvsLub2gfCu4TDYOiJWrEsFvTCzbwnfWPDnnYaXu6GuzGaMyyE619n4wZtPxVKOLUAEH53Rz
uk4EIfK59Jp2hbxCElB1yukG3SE/rRAUCO4WOS4Y01HwjuSFgFjLL23LD3WRzgNydOL49Z1HqzjA
gd6IUQfz9xDEnIPqiqO6j6xXHs/ih5gdWdrd3SSHJlyVy8p+XX2mqMnJNY7wyTHapEG/5vjA18s9
ePmXd2mKm4qymXvf7Q18M2kWLrNOFbrfTbwARR8OsRuGjbxJCSNlh6HSuVF01ebmXeP/xwh+Krvq
qzJ/ZXTOhVO0ZgmIopkj61wEP5UgJB1OWZQ2HYBxrELvtX4F+J7ivcbXBZdxYgyduVLYF/QF1h/d
ZOjShxe/m64SVYjS4MKVF/ta/35q+f6cxSoUmS7PTVvbcaQWcrWAjulwxr71491XQcvORF3vRAlD
MqytDsUxi0Go5yZ0wO9UKvuNajHuD0IJRpGEJCnmfy71jrLZhPM+q7WiTkZBvfQudltvUYtA7tOc
5nHq3DVjkOEsEhZNTJ+VXQTtHl1dLOSTWyDCFASHFqFbGN9GlU+RiR30WEQW6QF6Y/1zzdVf40lt
TiQaBkOn4dMLV5683LGM26UB82i7nlQH42mczObdQoMkvKBD0zzbln2hZMhYvJqGViG+INR/jPF9
izxZIVj7KGKlPAeXKNJnroqOlel4dBZku8nyRKsa03/o/Mk6EqdfYFk8Y5EBc5RzOPpEwsyE9qme
aIIFH/k5p4O9XzmVm580zWoCxUVT8ya3pVgMUF9BcC4O6L5w4SljfH5FUqR7NV3dqzcx1ToZF4Uk
vmr5rxfo2LoZ4lM80zC1zGoti+Y5A7qVMN5Gdv3hm65TfYVvJkjm0wDS8yZz358nCt8IVgU+wNgy
9If+YmcG/r3QTgQn8Gftjobvn6m19L8B4hW6Lqap9vfCmXCcxW++N7DPNSyt8BjPzq997MGDQP+1
h3QoprgxHmhMDIlkHs4hwNwy/RzOOCYova2w/TE1gJNR6roVGqm/qP7gHcZpD2pc2U0ieQErsRQN
+TpYDJPrdMP7FtBDot0o7VnHcZ7V8o/cLc3l3I4PvEYJi5TbBr64OWcWnfhpAiWq4zFvAav8lSMY
V+uJIp1Q4T2f9pKuFzS1k5hWjNm6W+YPx2m7NpzfRtMROwsLoPClFgnF3hWtKIJv96Hg+jo+Eyjs
Sr3n48Y4UGqa8z9fVmQDmhl3a1sv426zrOi/Nsd3LGZWTzbidYK+l9pck5hnz5XyFx84CKiPFgfq
j1fuyRLUpZ1akTo+GLtALn94QRkvZSlAmXDDgr/7e8IG2q13WIdKzvqS0vy4d4VUrEOG15lf6HkO
DP7IkHHoCaUJfHsM9eVvujlYIgVEg7qtbGWKsXwk9EclxS2+lKnkiA7GJHL6oHm6/P3LtSVtyXuU
Pgsjp/M+mhNRFfG/ll8L34vTZav4D6eZjROHrV0IEW83UFF4SjQ6HyBNv0oNttYzXo1XZpbwrg+X
Iy1X7CWCpygHEordWpPvSiKCq5pwLIRbNKk6gSo9V+sgcyjtlCmIHyRHt7rKrJO4YTZl2BBGz1ok
84FjbsWcX+UTtuOoIVRiGRkE5zgSpM1yFYxXJXCzJ45vCBVT1J0m8fQBv6QDw4pFk5o4z7MjDvP6
ENM48GYbC088O8W7f5RmnEJ+W5n45lcNlzPkOSomfDfupU1/oj31TIAzcMqVwuOvkFShw9UILrlc
SL228Z48wr83sukRS3y1+ldO0RScKSlN7RhYfwyUD3CewIsx5y1ewJdWE+L1vfiPsLDVmSX5ontc
6N+U72R0h0RT9unhyhxGab1Lue8J8540Nc2kM9aaGjiSH072YAaAMWAiJcO4lUmLEJmVzfQdPmBi
YaY9un2IvV6Q5ApGrLg14kZuUdHW9cGBnrAvg1mEN1Oy/AFzrbYgWIIY5NJZuvb1NJ+9Pd7Puxhv
L5y5ohU0YwZA4opYwCCwYBhL+PGH5NcV9T824tVBLVDhUPgoZZW/yHR829EtrQ3hyYNnWtWISn1k
pmWx7TzVwce0Hj38Ke6PMTagXx4J96YkcI9de4uqexzrDZqmTWqEYT+tq18KG+EgwfifMZKZqvad
d1pwtw8EEGCxcCjXF6hz0gwA048angBm28l7P0Q8aOSAgoDLGCPJS9eRftyIyUIqAYcUClaBxB7L
zvYWUl/w5izcLxVb967c1qYEd5p+kzZgVFYh+6qYN2rNZJB5weSY/3y8oKTAdKXZ1SuRcfRzWEBx
ZHu1W6k8+hfHOV4e6b/ZNyf/n5TiyeZ6JFkGkAhxLwFYZFrlOWk/dh+9S2SD+ECndVPint5P8YW+
NAr1Xwww2jh83ixkfIs08fCqQbTW78HXrHbZfCLIY2OgmH5LQ9ppjt1Z9L0N4gJpVaagQrL2BV29
a5naZhbP/BMdwWk1L28lC0jKRC1BffDDcrGAJ2b+t+BHjS+ViaQ1tkRPcFuqaDGWOWejZ/yVzoxJ
NYAOcY/kP6bRMwLEvMALmBQrGS/VXbevvZcpxHcsRpcjf6oxZ2chiUf7ncmS61OOvcOFz7nxmX1Y
vwl7me5zCvKNdlYuS9xGcU4ueNpeIJTSEklOff4veqcxNpyMhKSiKqFOYRq9yX8vwFZ2+TSNJdXI
Gqw9RLMU6NYxBKc8GJRmJcanrQQO3zEoodVoEcOToapvNHgLBy3brH2EeGvE0nbDZQuUigmW8CbJ
4ecx9vtHpu7OvKtWgY303OFgTKPnDCgKlwHEacbuL0dqbfcGDtR6vdAV8aKUTmeuBUAsg5LiZ88C
RmJiwi9D4Lw7jOlyj3oE+jOIx67r6UFATlIOGm/1HK1ZBbHO3uujzDIeE9Ixqn5KYjH+NbVk5x/O
+36dxuIMCBP0fXFCASRIH4kSId5cDGdTQI+ytcqQDLn4AHiTcZ1fVsnUt3b8Y+6aFfzycNJ6I8qb
w7M4wIy1fzHuZhSxApfGJpSTiKB80Qe5REA//cSwhptFXnkv26k39BGhmfnTDTDQqOi2CQn9prni
nMcTniCoPMLvPD+snH8cT2GXkXGyaUzqOlQyhi4XanvP+w4U5hvxt/7eQvB9epILP+RVWdiC+hQQ
InsFEkSIFX/2qP5yisgIJLkatgW7l/mtsEi7VUSh3UpDtdY9CkDER4jxPBuwquVLIS+T+gGvxvn8
wBSFWGuO1+/W/sBvZSmrOg3oZQV36AcaJnfkvyf4FwXx6rnEfkKY72kbRz7OmGF9u+tMnFLZ2lj2
1nu8YV0RvEO5hltLcgtDmpo1CfwHyptDRslEEiZnQyuyD3PrVZTU/zxmP0ey5vW0CQErdw8upS0F
rl/uC4+qaGIPpJOg/hTZyyd3d7j3mbkLx0WHYqwcKgHTy8+UGzq9yjkPNKS8jVYZjMdlro9Jj+bC
sLjbMCn2DX+E+suiL8vQbuvR2pl5bZhgb5QPVopP1owX2GToApGz9/rxNF/Oz8Q8jSCSMVqG44wG
OS3/7ICP7wTyBiRVKBNYMiwYBNzDt4Krx4AdCekvjnLUeMfrslikVLeg8q2l597n/0b7hN8QZAus
ZGbjhqSXouYxnE746zpiYmog3Mfqll1KvXnT90oV3jgqTl1kGu4Pya9zLWxzgg2Ic0/Jcr4/GGg0
g8MUjJx/Yk1JyKMYo+QQ2T4QMKGU23NfkQAu8sfMW76EutRJklSCPD24b5tK5l3uRt5U9lzhnBzt
Zh1kqptUto9axq/DujTUWEV6QIJlNmOzVVKBNScWIuGvZYhvF1Y6+HktBs64Oh9CUPG9MXyS0aiK
1lhhqIqLvbN2e7sFCjjoPtDhA33s9iAWchVUP8eHulFni2AG7L4oKqDmXpxM3XdrKXlWorelW/QP
jXubi8bwHiEEXiyciNkmtn2hOIHpwgzjPEqLDT99KaKY+QRY+5WuUDWC7FScdHpYcdDLUWfraxLt
FKfa5j/LCCN6cMe74qByzssPf56dHcNnfCWCUk1wpSc0yW3KbgM2kF9tZ+fGg3fW9rfoax9i4try
5OeY7XGoa6Sswx0mmaQPeoLi9zsgaifUqAjwb6P967sok7HuaxxHb+NUvxiH8NJXAuWT4DjLwsXf
NlEWxL8igg4mupw7VZ/hwq2ze1gjsDaSzITh+ftpFgPuQ47lfxC1z/1ciPRKfn+W54JOnPUrXTnZ
W3W/3mqjLJiPhAPVOwDYW98Z5/V8vHDU3vKinV1Hr+D0T4o7otVBoleB246nGuzLdYHuV1ILJwfH
VVnHrpiQYzbl1UfxLcXPiOVhrMC3+glBrlWevXKat6PJ2b5tbXQzAgV7vuK5m08oMbXefo1/SaLQ
LGKjapV9J/mWtjCrvpEzRLTvv+KhAS7xPTdmrTVgGyS/LqaPZ0cZDMtk3qD8XfgRCWat+rLsweaL
U08rLw49U8p0aMN0sGv1QVYLoYnB7usS9qDNlVXpYUOtTXRl9oN8nMEK7Ba+j8PDLta4O2ZKHKIc
tJcG+TkRtPwJQaZyXXb+q8x88MzjBpPF8xvQFjktE3pKWZBfTnbT3WdLFgH2FZRlKCkwk/FKyAyQ
8w0sxFRjx55qTInj88Hrd8rfP0dQTrc+Y9WhGaf3wkTUZjGolrW9GhHNtM35tRduLSdUh2ZHIyjj
Hayo73XZoyeIH7M/KdOWT8IxWd1fo3mir0wt3ZYxHFQ3+r7dLFp5sKkpsbbNLHZ59x2ywd3pQiXz
XSTurrBvKry4bWdqo7wXPfi7XVzW8c8/3L1EQ2VD5BfrZgxvHt/GXiZpekcYziF8wyNNuOqhctDF
doTL6hyRT1E+5l9lMVXSgjmLLfLN8JJ7HQlBdEST0tHH/L6hYL69wmVSFZKH+euQ5DU6U8J1qFM3
FJAvsIHtJLSY6fxPkppOKVyiWp6qmYnH3jEeiVTDmBqiBgyUZ+L0bB+l6NBo+e6unNgOSbD+o7AC
2Efq0gGjeH/1lU219xwGkeusBoMLefqiuU1KrE5QK/0AhAxbW3te7KjAqR4UhFBcPMW1UytZw0NT
nPd1Pabnvxsvreu33aortrWmx8EOZUWLs6z7PFc/59XePtjYqk4vktgSFLdSTU8zbF6a70PYRmGH
AVtUaZzQ9Kg6IXS24HPAIn+gMsalqxfuq10pObKLoAn9XXSaf42luW4P0+JckpvmikCBpl3fVMqc
tXhn6aQ5+BBJtJL7oXL9GGW+Gvr6yu8PCsff6t8F1gdK7q4T1ZQEhIbPMA61YTOQ0iwUGo1XR8sh
6qbc35CVyeCFqRQ+iTg9/3LtOCOBah4npePrLRoNBEJh2JkS60YJnHzxhtCz8qJQYeTtZQ4jsWjN
HOHpmevDDk9O85/D+/Ie6y2mQ96IPlQv4ijP2RyVT7dJCZPSgNAAIAaG3avEHL/F7Y3NODUPwsdX
KNXTwY9BaUr9qiaX1NYcrEGmesOm7oNz3O3e9qFdOwHewE/0EWIc3qwjH+YcBsfdhqUPstm8Ot6/
l3H1PF6N0t4IfJBGPIziVNr7h4rliBmJbK7vIfsR8h/KUOMG90UwhTZidiXRIDa6nDjv2PdCi0Qf
LqVUWNpTH5s6valV/TcBUO7VWCv+b2j+glwjFEc6zl9lpbQ4fqUDi84m+wWLQnRZ0iwQWrEHLcMF
bxFwNdR+wzPBm8Wqt7UruRpNSpH+LoD1PIFqn4kxHs2NHwt3slib/RuNm67x9u4f0nyeB885n8x6
6QsMOiSK4+FsEiSia4/fvWL/eqZ51fZl+WgDdJAc8hO7qqcxIuVKoGbVW71wKlf/FEO3BAfiuD9k
s+mwEc0KM4kaVKuv3zp2+OY20+N/Q19m0u5K5KBG/IpONACAWlvRU88/YhkpnithAuNnQqndPWd9
YilLw8XB4Iw9IHMzhBNKq0sZ/E09MTqkiC4B0WD4MMFHHaN0A2wDwdUGVfLigd4MW4KP9Px7JQ2h
Zc/3FVB3pPzDf3G+u7F7rkBkMvKH/PaS9hlquCXZbvj8p8Lb1D02r6CC1czKpwRJDVhMT9k9PGql
t1IiG74hvF7p39HcAprXWyIBtcMliRn7NnuaPoMhLCJb/s8UtMCFo6GH3TssC+bOW/IWMN1tE5Ep
z9r0zU+rVc4FwUtQD+XPEpjXwo5ebWmC6wv3l0atybvcr8GMQi/i5GtSh06QDy2uVEy136GzJsHe
9sqvT+8MPh948VINdzZVSOU2b7en1/mQ/Cu03wzuzXbg/WMIH0j8ktITdGUeZt+ZawLqB7sONcZg
Oh5jE/HfM7tcG1JAd5Wmd27ujQux+jcIIL0ddEMTSArBFP5Rh/9bYCZqxi8PLx6Q+/eLJ8t5KrV/
jCXZaSN5Ni+PADdsyvV25k/5/NsP+VTR1gMf2j5pe/YXaiQ0EMpUt5FrDJZWHYoBqDHrkXZ4pvhi
MHCqtjJTUJ3f8fkMkIlqfn9VEa5JekWNNLJ02Z9GmSsD2RLlowx3xgzPl/QtMKZk4ph/XXdWnzJq
5AF6JtMh5ZTzi1rDZ0EKg9dBJTkk78DFfH/rL2JflqErUOz46rtLkxc8E8K/UgQ01imf3i0RcfDN
TOpbI7OHtECzwAmKCtXGuQ7HYKo4qWfXkZR76bXZPJ1kVeGZtV4bAQCI6GYaW0zHwxTpPOtNlEXA
zHr4MOepKiJHAdvE0Dhxgn6feK0gxge+GHXDUQn04Hrhyl4xfE9JEEwyfLdd37bY2cS5z/zPI5fq
S2C1qBAAAQLQUNg1gInsn8q3ZdDCOsrFGGrTeHLdy+65x2i0sSM1/ZaL9LgY7kwRVFretghYs5d0
r0foR+MP404cW4uU5oun6B69+CTplsiGzpK5GnXve5rxM+/8i264jw5w6iaFBw0zyifsPbwzqbdY
JnmcDC0ZX+5wrqxgIDh/t2/0AandZGNiFY3bgvjfbdAoiYg3bRMWCdRLppquETsc1T9iIIlDFbbx
2qs/6WORrnoYMnibxVkNiydN0uuK6aqSixL4Wtk6U5AOZOMCQ9I1UUjRuSQJJCQkFEeaUhCxKCgu
o5aZmvXY5yL7Bc/L/2ME/XWIFhtExJtCO8Y5Wf57awyhqcc9dKFcqemg/hyB5/U4c3mW/6AHPym2
hOyIS1tuRrrr2Toaj3vV3BNHfif5qU3n32LqQh1KItAosc05oG3kXVDiXTp7xuCc2HWG8EioY+J0
S8pN5t5+k7Xw+eRhP+ptYywU33TsEYwPbQB+nWAca7jGjMfYrCt+1BwSTaUYKe1APQYL7ovN3OkO
8+OpXw6WAtmR8XEUxnHHEnWEJ1pq/4rc2Sn+FSb7usnXP/eJzqDnsjQ4e/MqinK1jnQIEFsiKsJ9
EavG3ivZ1r4oT7gcBrJQqozSWjwCt3y+ib95PnJz6G065EsC1JhOoS5rKOBr63apI39GJUOlIsGY
td67JxtixH28MQQCIo3ZlLhWkKcO+C8Bu9wRfi51P4t4eaL1n9m5TZb9acFEF3r7jkf0x9OKemKC
6J4so1U+JZUCOgBCEdKhq9jgGawwFkR3+QH3ohFI4er/sOZ33Aw9aSLyKQqO/mbMNUYZsl5gOlIE
XYSY4MweiFgaCPE/6afb9BQAnOUwpOZ49qIMLbAWiQl8AZQq8DB3ja5ZUZtTeDlInnJ9mhr1sufh
TrWHZvODUxG6DTiqHg+X7xbUM0aQWKmuk71A9GU3JkBVkZ4NirxUUCF1b8VXyu+Oh6LIOfehoUdF
FsOwKnO4KBYr9yeTms6vKfEmdBFqp3SZSmT4ZROdxXJjz3c6/7UyQabq6+yFLPSYc8YCY+gzUC8e
vfzgKt9ze4h0CDx+/Eq41dAZLoKNdNNBnQi9satmLmpcnYQ9Yr7TUw8Uo81gIkIfeiQ4rBpPJfh0
qkEksXnooeZo5IIU1dswQMwE2nPYavZOjvP6EC/a+pkaFaeerEpH08Qd8EED6nYPoi1qg+2eBdwY
rxco4u1A/RaGtcomMhwIp1NLVbJhrjT7L9JXNWen9bvLcCYvByZDdnVV4mLKIlWrfr4SuQ/u/peo
u82Q+Gc6fStElC+DBgpJat7n8DGzwl66hRR4S/9rAoYj7JXt0G18DIaOxL7d8BJHLvU+bIuHTi+J
yxOFucPJj60Znj+CDypleDOU2+wv8SUBs8EoGr9ENkKG3f67oy4ALRlbiAbET9E6tufk+taAXQrm
1BnGamOLufAEN2pOiBOIy+PyfezOY7362UQWRHDibRhgr/Pr8R8iZkCQd4Cyw/wuMZ4d2weTTHpx
MUTLbNTTZvQC2ZkR86HsChpeMj2SjzpEPbZer/jrWyQlp8+pbwz6PbRfWznPgOSm2laTj8R1TSbO
97ECC5svvJ6woePHUlbfE6c0fToG24WfAtg/LNIrn1uDJ9GJkvWHhin5r4otJBwE7H6qMGITs3KZ
HSr9/u+slIYJKoISzDIvtfu4tnGp6PaUL3oNCS7n+zWMOFrRHvbH6RGSvcLmqazRfLa64xiaBrNC
jCOLrBYdHjM87TIutCVPKT4hrV28euR2RSajsvhurfi3KB+L/FlY3bSgP7JY0lz4Loc9m14FzXXY
Tz8+q7dZ3rd9PfoXFT3PVC6WiYHv+kpHEJf24AYbRlU/Sb87LHvRlKyENT27KdmTgUQ54guLAOLo
hQF7/dftaenavLnQ0DT6sm4PeVL6vwU25EkF/1aaOskp4SsP/VAmXP3m29bTEi8CTnrXCPX1U4+y
fsx7UmfoDQ5wvTqUzeHR8+0WBpGXtG2nbi7xbAOmkaaxuYs7EvNR5axWmMlu5jNOFQ/bnzUrde2p
O/HUvuq4iNYAkYKxaXIOot1AyDG375mY3rPuAdxb/rBYkdg7bSyqZAKdHLl8vtrjzMNyEHehzcvF
NmtiNp+y7qcVeOToi0DQlNxB5PofRSfD9Z22WeZKH7iL4D8v7xfIuIoxXngLAeAK8htueEQH3HnQ
1gAQuh2iTkTL+d5+xL6c3Fe79stWK9w1sLsM6nVjtdvTuqUhQHVgT5vyBYFHmIPZXm6ounz69s6T
hECVdvIVbpI337zib+5MSyUCq0MT/20OmG+sWa1m0B6sYOMJOtJrXd6I0Ztcg92YO93DVaWOkP+V
8DXhtuDUnmkO61OpY9hXZ6A9HTgic7WF4Hz0STWyZz0UsIVvZwOjsK2Iqv1+QRozY6Kr6pC+f9yI
u/+4e22s5ZsWN/V4fdRjrxvFA9KXCi4MIdXFqfhjzh4sgaz+2zWJ75cVaE8nkFC6n25BV44zydTP
HwYJ97imMC3/cFDJBjuNRlGTy8oiIMwP4n/Av/USpTfzhcL9kFhU7vMy8NoufW+jmHALmAvySC+4
G6YrsCOUNn/eGqSCamM3dXkTECOuLjZ6IlGhUvkOnV5qMcc9C5nLdOZPU+f+MtZFezKYjEwXQf44
pinrgd8G9paVeY/5v2vsv3xWN4xdkoqIR/6aEvLJ+vwqm+qoI5of3kB1Q7ZFx6rRJWk2nDL1/Lzy
0WEibQU5E1Gkt7KhHagTxSQIywTyKhod65CbeHPrVYMu5I5cUcgd9N6MGv2IZ1QyuCGFHixLCfeq
MfRi/yrd9CcW8blWU97Yx7f6MmI1RkbITvWBv2u1cQO7yoscVq4GOA9CVY39I6kPfilZ0kNVVV8i
MzWw/HAOtE3r0NA0SAKGhWjg+sQ0G5pPT6vVDZ9rBU4O+vPG02arVEfh7Ag7LXCkAixUNOzAeYTx
gpmUjUGq41cFHEjN/AIETJVWTEPCUdDLm8SMGj7les021ixsYF5hYi5zpfa8VeF2ulb7KtUsD7+z
A3Ek+IftE01yZ7FUOhRCfyVLyW9fCqHqh7qmq19ThYCB4snmp07SY+lMVll2C5dmsIZlii6FckGI
Wb1KHx5yasAUySHxDpeOTNqKBFi+26Q2o9zH8u4eLx/MSESecVJL3mjNBLnUk5AphBPMfYzlBS5W
Z1qwnqALBBH71Xbid+EZJDhWBzA6Yu8gcAdh9PVbsKycvBSNv2Q1FHGcfLMMbQ7j44M/7F6zF7tZ
qqoE0fvgR6HRjS6EjBRpMApocEP3+szc9OfFDpr2Z4zkpKQy8DYpG/K8TwgzQ+gA9LybdGpb4vbM
WNsBfYWzgMsoPEy+KC+sz+iG4TghVF61A2knpzvYDU3OPnoHDwR9yxOyOWkPmJoPNgt6Yd3f/MgF
1IA02kn1okRc5es5PvIqQ6Isw+jUtGYi6GPW2uSfy3M0x9D/qaavAImy9rI0UkzOC0j1HvhhNRvZ
dGNMoPd1gvEvl8xl0KCp2PofCn5hN3LGyNNiSRVfbcuuEOlWHS6FmDzsplIht7jpSQrnidP9XUox
0ZX6yT6ez7urMGP5HKXFLApnXScnj03pLDfbCACfmLG3t4CU4G5qVCxujG5KA/A6Et8OhSufc0CG
Ax66hyePPO5jYDekBDz/BsT2mET3aYvThH5g53VO7UKVP4baCQfcmzfCJKGfzqKbbInAwzI68kR8
dewoZR9PcIKtu7vKPgvZWk3xl2oY764wTk515jrurHMipauotHgxRAbjki3A0AmfrSYl5XAGbYS3
TgLLW39BFSrIoaZrVWWWky2TwBRZSiwwTrNCYo1BlNxJM6rD8m7tdCp/zSPLtfxGSJZrVqnloRdZ
k/NsSOcwCzB234Zxa8Xb0vRMaAorARzK+BTSHbr+FG/TW8dICZLIZgfUqWW7/Htu3MMW77powCbx
1MKcZqPonvCjzUlvU2yvQ8GKHgOl2Ct/MvP7hm97g2N0NicaRQXwQxkena89j7p3aFyKx6Ixwa7N
S1O+3PnQGlAh7N8VF4LjL5qBYIlD4akvurKFSJKXYthS02JEPTcBXVinwukbjciJ4KUq7cV9+nEi
40fOlpYByBo/IythhbsvY+dWlbTY8ProwALSWVqLkUr2L+SWKbyu6b6wrEG4qPdhKK3DgYMEYr2B
fXEGRc7qWNhZvdgupjNw7pP7DvOpgkXHLe2Pn1ohuPslUHrbSf9T0kxoqcByrNKZbohkkVOk2Emc
c2xDBtwVhKbGtrRmlexovNOVo4/pL3P0FW8UK/pN3d9nx1411suoFBT7vpVyeDblX/GDPDjKXT+r
iGrxvgAID1tYHVMMsffzsXhHUBQkGalucjnS1i8NBpKILPtslRuSXmvK923LPIloyDq6MkalwT76
PLWsJo+RCpqu7a3xIXGPaXr3SZ0cyzkDyVyW5qi5ZvUzAFLpKXH+TWc0RqZTv0tnvFuw0tvpa0nw
PW9p7V5SBjoYJXR8DpXCd7LrRqMIELvmFN1RWcQBWr1rAo1uHBMbQHoKFIWON/vuPKZ27rDBwA/v
LfTAmCubxs6uVMRKx0XKNqEGR62v3W7K8QKN4svwSbr0n+/8ibEB0mCxWk924eU5G8nK8VnnD+m9
tFDhHB0X0EXPpD+PgzQqX5pI00Wt/P4PiyAveqjLbMzMAQrfcwGaFuz7emyF7WI8fE2UoV/7kkqS
UWjpyJr+Bf8EJgwMqEiIiw2RdHHk11pkdvu52m9QN1b+QL/Iz6r8tfHbzjBxetywzKPGb2/rnByh
anOQf+xcTK7H2Lf322h5bPvmUTiTWYOze7TaU/JK4Hdku4TkjRj/FTi9+YI7ACzuAsw38qukxYtK
FxYytdpD8qBPdf3cSLoD4G2K0hXYLXzP7/UKSC8BXmd80RMoX/OcZy2qvrxb2f9Z2GNdZdwedd+m
4lmxmMLA+Raml7cviH7OvDicq4ZHoNf5HB0ZWtCZDfpoge5ZOD7fsdwgwEpN3qzkxCyWetYen0H+
KQbXleis5AY32eFhNv6KWd1z//LRWp8Lj4YteZIGjVaN7OJVsuZR7H8a1M4Ew+3rDh3c/4XmNJbs
aP78w8Q7kByRdRX/HYohT2dZA3mnSAwLjYfaFeQay3l9aUrGrrm8PdamWpHxR5D5qLi+gjK8No3V
xH79r0XckLYSl3WNdEPPjv0UyqFcidSl/bUUaZ2j2aAIHer66yzBP64aAfBuUzlKm8kcTi8WAnu7
Xix9EGBTR9bslhY/IwapnhqLm8viBxXnfaUjzeaHAHUhl15ET/79HG+ZosySfI78nn/GO6sLxx3x
iPbnL7NggsNoJRmsEHNr3WT26m4bcayom7AnE9xS1cjePnuvF29BO9uWb19MmcsMp+8Vr71rwmI+
ha9Ytgf/0USTORzteVHoQ78asHFxEQkQtdvorn6JvC6UoIKyPB0QtYXwywp6dGAxg+wM41FzvsnO
YYgFUIYDukP3mQ7CyDgVNn/0pjBkt5R4UKjKKJsO86THcj3/iu+IUn0+J8ax2T60azkjoon+iX2K
b3oUICwouo39O7wFKlt/5am/ebhDKVM02K7bAiXuFU6JLQlBmbHtAGF6TbhuNrQDK7QTH/SyhKx/
59RdaeAfqVSarrVtCPwxlXM31tg7j/dQcxPn7WNP0VBRBhmx2k7JvP2aBjP2O/sM/iSm5/C/HoDN
lJFOKORD85vfG4ocv/NgN2WJttbtgdmx4sNOO7MIG/oJTnFvJrU67KhKjiaSl+N9s3HeFbeBR+Zi
zrMj74jMwbyGuR9H15AV2pcPNvSuQQzHuFjfAtkRDukqhKFIhJkmeUnpmLb8DgomRezMykyeaNyE
4/7x4xneUiuH447+AutQp22HnwrfoFyfs+G1PviRek3II/e7hWz4JgtY/2o+n5dG7QvnEQlVJx0T
aM5N55iLKYZh7vkdyX86pGsidomuecxPf2b0QKxeoFGJy9rd8gI24meZortT2+CZ/JeUXkyuN3cy
nc6aUkbvrMhkK+y4bPixZZYEu7mDqSiDdHkZFveY25dHLTSktiklFJM8J3CuUnpy98E6pBG+bJXm
g2PyfRsZykf+bAtXv/hVG8J24RJ+SOIDevddBqQdmzRJ30sOl6llC/0hgfL5pqddh3wTUN/2Lokj
fPkKaX2IYI/XZ0vD3maA8n9ewiAgvKQzAjRvYq88imN7Lb3zisoh1uMhgYcGmdRUexTYzLO6VCsz
Vc+de53DuPHw+YQn+FNxc0QzNnU49ZIIa4fGDkpdJPyJ9sRljY5sYX6bttO3/XEKCXM+6o5zwsMa
f5Z/hG+3pvGKQR/DJ04xkO2ncqlVy4yK0MfRyb1r7G+8CADDNkyVotpEsIj4NwocIokeFPcj0D90
A5AX6kq0z+RN7CYmvoXaB0KFxL6nbtDnLrnG/fvazGORNtJj4vt5RbJm0x0anfSeHgp5EliUSr/s
Xrd33qZKFx/FRzO9DchIs0UXKbQqt0ACIYwST6D3m72rY8QbnIR257F1x5GJC2GTPeEMG7ykGsV3
hRg0vCPXR2xG0kMPA0khuLPNCJizMmhil+sfO9DaideF2aVNIoipRwpTu30MkrtJtq+YwGExQCKa
ldHkg8ewndIdqbzvWHFA9wmdqmD6kXp0rc7O87OYmC9DocIDZH4580/R6huKKGniT09L+iQ+qoyh
8mB+rrJ3jok+4TYq6je2IG2Utj84VBMuKVvXR2HcR2JukIFTJBw8mQ1m7xtwwn5kIOphnEiXnotJ
e8R+EEQJSxJ7IdVwa9kO1xSbNMFFb1+tKJHD0wcrahSYnTKoxL0mzlvDuKLrfa1EyiRUNPiZfAI3
/a1259RnTrT7J4vDs/BvNeszW1W1NnZGC0/bcP9SLAgStJVmAU79biYI42DFRA8y5INA6O0gOjeO
k+TD1J4G1QadO2xa0he5uXLj1ddLc+kjuN5n7gRocyRE/kKOK3ydzYJMHulugsdRiuy/HPBC1aCk
fmmFzhwe70lQxLdmRnlA2WH+nwR2lxb1icZNHlPGS+Mb9+0xOzQOb8r7BQKm+wZwHLrxclZ6hz61
kkZ2ST/akxgBcIBDMU/yxnhVqR9LnUbnNXNXf+jah4bi46DTCl2m97mPivxSrvFJPut2R/teSyUs
2m/j/LmNhGnEzPg6sppUxFw77cojfk9mFtXx/Yc+M089YK/W5y8TI90ehqo/wSoEjtFL6Z6TFJnE
AM58wakDYWm0BGH2+vhcwdo6vN7Q/TNDFrvFG1jjYVcocjHSdbUCe1PfdrIJyHMnODyZ22WP1AAE
OXmTdNP3lcSTlTOyBki/imPcFIh6v0HKvDRZkC7mF+DK7PM9dwLlQPqDG8wMNB2VkVOQ8n42QcZV
reykqNTc4LmC3LLLnP5MnXripZeU2fwXBIJVS7l6vgTsHNTGrlLJLpyCxzQrPVMEvsznfNsP7u+q
9ZS7AToFZJYTSJ7AHmwo04BeTotJrC7JCvDzq0ARCjOMRKcvRQnnscsdTXTavvZ4wdx3xNua57gd
2rCdxjnnIvf3R4zFLEO120Nr3I3kaxTfek60W/NiWTifkFoez9cZjNUO5fjiQ6UPB/NnQaV/imUz
YOVzSREtd9OGCIU9IcllX8ewPu71g+80YPebjql7AM52zFc4X+5td7PETKU8M05Obc/LwMZt5/19
YKVe/HaX2mGRCVx5nHzjGwxDvfqcNnsiQPxvKF7s2zPWxiT3yy+4vtM9OB2Uxfv7h9Grkf1f9qRs
1p1jyKPPwQX3Pj1H1PaVysctvlaL4iGe3YJAUit8lWFDZC/iCAiLuJZ6uh09ujRvO0wkiEKbTDFc
7b/Ds83sSweeUZ7TsdG4fSFcMy6qq43OKxTnAREkkvXOH5NU4daHQDiaW26ADMyUweYyuWfBftUg
1i1GmigVp4L6vZCoP+Gp7BHNktf8Hz5/M7Tf32xizGcbxrN2rPjvet+YvIsQGU0ThqJ0KVZWB3OS
7Ks3BtaxvlBjTz9glt4XVPJlWO3AxW+w5WSFXgYalkMtE5MO7q0+ANLDr2ESAjwOnm/jqrkNdzvk
XlCEZYapmUH6FqMbe/fBkzEiGCYHuLPoRPE4Iy5CEPb2hXMu1oTYv9dJC/LecLw7HS6ckKILcHXB
A4HjljrPGoTFKgjDir9Z5qzc22r5OEL1xpxk8U0/5fBwS7GBulys0+leGikpuMxCqZOKz6lCYWN5
woWaZ+4bIm11YODjB38VPQFHli1Kbc3nhVJHj0aL9wWOPalwe267r8ZWctNYoiP2FZTpwvZoiz5x
I014bm0FByBqjWjVyi7rZhiMmpRJQPlf2ju0udQGaTHGY6xLmkvb6UA+rQlU9GZHT/Gl2sVwn36h
lA34ntSVlXwtDIcRvDfNxiTXtFisX84mgiLT3zupDW1b5A8y4g0MaMHzbRmU9SsUI0abDM1YNyRQ
v3acyV2580cjQtSKC0P8myUndW43rQMOyLri7yZF92GvcTRbtBEdCWgsw03CmB4cridLkB6rwHou
XZaRe7iZyHTs4430Sf3KmdD516ZS5ZhDiOFfb5A/N/ybCSNs4Zh3hi2SeQf1UgW0AYLsaHIFCyT9
MMeW3+9KUhw/tHpRqp37yM/Mz3MjNqmCReRri70POXvIGUiV6t9+i0K1UGjTBCx4zjn5E6tmsSBE
gb1slhf4y7zAgpn5CY5rEqtDtQhHwy8s2As5AbuPI17PBDsaDyfUXOK1Si93LbhZECYTqAfwxfXY
vl1coJZHIpDxf1nsFVdZFt9edSTF326NJrFJiX+7EXLrDn6LmLVp4UxFWeRcM7/Sq8TS9rrn8hLg
5WbZ18eEyApY4uZrVoVwX6bYzuvmQ7DWQVt3E410dpNn1pvBU2PFfJSqW/bSxchSW6Nk6/i6irb8
Zpr/z/G+cPe32+lOhS8KiuyX1DirmafvPPIHz/QH+aow7KNzlsH9YFuphrLQ/05wypd5jPK0KyCI
7W5F/GYcnrA9gOEgRaG52mzXVuLK9nzr4pPys48IvLhlbWxvs2ZdpghtkCvxsTnBcI3FoZQ0JhMy
jvgIaWT6I0yF+EMVPAkCpqgEGTf9rdvaA68Yffs1frFIjCSKnhuVTKHqk9htUDbv0CZpUPSwvPHG
nDiOeWw+rGYZEHVCjDVE+dNKjMm0ObyB/VY3hfdWrNd+3g+wNgjotPzFH+ys4GYe9MQ5AdLfq+J3
CVfbw0AyVF299XnS+FWsnEPqYTutTTyzH0jXg3Rt1OGOVGPpYwO+PhZaQTb4O7u9o3zDbFh1EKVp
EMz3G1W7hudRoAk+yjbnzOXNDC1o+7uU+A9tnpdpCE6QSXgT4bX++D4JvvNQ+QJyBj9xkfFfZF6p
XTHXn4hPydi01lbhIQqQr9WNDT7aFGg7DbU2bLtToeiXSFCNm3n2Ub45ultIenxvyf3jganvlEzk
Ffyz7ygO85B/HiGJ2sVTVVu9O1iChKIFsEYqBgylUW8xaljEKvZVzEEFEU/URnOA1KUL8h8iGUKY
sJBaEDVGPInf0ZSSLKxlOk/UHbwwZWp7up4GFdPcsiCy1RdcwpGDYrjTM/jE6Cm17iSmX7WkqN1W
lP3W76in1tjMnYQZOcWLID9WGOK071KqFSkNW54ToiAsGPt0z3X2gtyto1ZiRbeq8LOqeYOElPq8
6AhV2xOWzGN1ZzU/7ckqR/TwKfH48z/DES772skYQ33nyg/q1GUFfEJCg84I/ALWkNDaiWO7c0FE
L+va45tVCVFTEZZev3WfGBk8X4+HnhzwRByYcOkNVqcfOSDt7WoZ5ERiefq7SAFdsOUfiEu9mBM7
VWkSD5b5bfJJRqow2qqrHFMnW3AUc4g4aqZRyRhsEH+3/iRfo+PYGUQkEm5sHZj0XcotWvqHbYGv
gwx/yyY9a+glFbZ5SBLy6domsjeV9eTKBggVLPOBzdvBGy1Qdu+K7MuW4aacHxDZSyuBAuC/1my5
RQQwU2ymNbaQE/W9auocfoqcp6Tt+7sj8pAjkzK9Ak+4JM2jeA/Z6pgjKxk7UnTxC34S2CnbMW+b
gJNQM1aDvsatgnA242v7qPKnTxCSfZ2QnHwCss00sMVdlhfLUyK4VWpDr8dLNdpsGflfQ+d24rdK
C4uno/7jtlXaoy+NZaErJlVZtUALyr2RbQW2bxVh3C2mW/Mz3Ti+irDQWpeCxeXB/X3NbCk2AWyG
GWUalz/6zUAL6kmzmU2e6nd8Su3IHpCZFUiSpwOEMJEDvJkk4kYMlv9KN2N8K0ocy+n/9SE+fxJU
Gvkw6kfev+aTQjHhreiaMBkuvHBmWuIq07Twhy1Rshx4eDzPP+ezrfMTKPBDknDRKNcyKu80BHtn
6OlT8hsLmtVnKydjjr31ZhgRB91x7CzfizYqxlgIm3GjMKK6OiFoRB1adE0xQtokBL6g31oWPdpI
WPWBT6ECYbn6aGBhnUpgXOvMCq55YJSNU4NyzipN/uOGOLPTZNSM5baIBAb+Sb7lh9ov2kUDl/Eh
eRb7rhi05L/+iLA8DlRbYKT4QYYBnPhRs99Z/kc18Un8bc5BVavGeBWHy0puicbAQ+7E3s5AyQ5p
z5ReOC2Tmh9aOiTFNP5Gp9hoNpzlVe3vmpBtXRWTIn/7iBtaFQm3ce7CKKHq5bSbTT4UGjiD9Ifw
r3ya4JMN/6DJIshXR7+1+Z9y8e3RidMi9ADVLCycERouqe87Xa8/JTXGd66xnovw/2CpmjCS7j+F
FiyWC79o025YZUSADzV4PCZvHUiojvenht/xT+0sohaY1dQuIJIsvBXQexMNz0dc58Cr7bRY+PZy
py5MJBKxBPS/VapLiDY3Lsve3lv0XbUq5OP78FO6KLaHisWdgOa3TQbdAzOvSz207hZe4v+SPdef
2fUPo59JpKsn2ms+WCjZqG6sFMdKSUmVxWgTbIAea/nYD5yGAuDX0VFYj0nWr/kWqUI6uTPV7zYK
3d5/QE2hG3BrFWltX6AYEUkbAEPC+3kYflxpOmOIWsaCCp5w1ue4jMq5APgPpgCLwUiI3YAf1bwW
U07Goeb7FRDty+3Pm16moVJlMBDi+QsoMb1UHN4nxuEW4W4kVKQmM52xFZG7rpeFS/WNjya+L4ay
04G4ky6ur/VanmZ9sxxE1yhL+41Rgc0//NHTjJUuOVgF54L1lACwmjHH5mOBLDMiy+hbao15yl8I
a6M0/QaSp226REYLbwUGChs7nAeE46YAh2YfWWq6DD1JyEzd4CkPdhNnf/HC1YSYdXVFgWQOEWnv
suUjM2NmEeHv9eEr1fTnRn7BNAvGxxPktbIC2fD5iLTeZvoGtSduteqULDxhub2xKLUdMmUvpPqR
cxBLexuG5h4x7Cf1M6GjU+synP/BUOY3VUaI8qqt2nIyT7GQoFlFSLm97lKmLdlz4W1Lsz7yioYp
FQle1Za9/WzCjZKZ6wuEyrp9Rj/+pzwemGBW9un+2+j8CcqYoiakS9V5kbYiuSzK9hD3qHbQlzzP
njFjlYpU0Bvy/h/yJwEZsARkhXCe6wdk1aNsa2k+IOIeiq1PlMUDsMFCn/eYpRrgWlIWggDeeexS
ExpZJVErUlG8hRhxA9po2v0J1NrmGgjP4+3shiaS63dtft7QdSr1pEpM9QCkuAAJOcW4y74pE9b5
ZjrXf6+WJAGe8eGp3QpMPExQRh1HFT/RlOKL14cXTZYJOJkSe4XwaY8HHwi2I28Sd8njFNs5rktP
+nJPyPUEThxFkQRpprBQtqvtdYVvI7eDmiRppsnybogbcQjrHrgQQvFGK8J1A/tMaTZnBFhkxgo2
Jj+oKZTIfM+/u2rImSwHZQ7XNo+IQhCIHRB0wK313DfLAk+OOzh9oZ/OQ24WLWlbBcSrjY5qcRa8
3Af4TKFjaKauMx6fq4DuAtYK/ATjYrLng7zWsAgzWdYZT6pZNC89X/C0M9OJy0dTBl6kmxTsO553
GPjalMRSgeCJ/AkfRKKHEE5CxZUHprqmzGTPAff6NvTCllLD53BP9uEUXyyQL5eeKaTdK5td8LI5
UaSbtkQX8G+nfKw4Y/vX1aITpStf1+7WP+HnhzbPWOwtkLw/mBloYeqQEBQjYHvKN+GW4rpjx+Sl
MT/QCyxCyNHmoQy4vvEl/nwqwKjIxXbquvhCtcRy6IcnzLUGGjn3EFhfXfQtgLxD+SNfRWllIZqH
niFMgUgomyRBCMShSQCgJ7TKyC/cJD7VIoQUEiL9lwIyaF0zCfRxYXHRJ+2S7RazLTgUKV3BIzWd
qgiaMZN/A/o27eWWTqz1vr5yhgR3s+kXTLY/UvYQep0GsVsd6Qr4DmLKPsRhhtbSNMq52yuSZg2G
o4jwPu+fW5WTB4aUD2Q+SXSPO7RVvfJCrw6F1WmcxX2G+xxC+HUTvkaApjlNDqKOf6Htqr2BaevQ
Xq5JRea4fnxmfzNCqz2/JMVcyTExe798uHtONSzZ1ST3GrlqDstp30ZXPlLtjjD4j6+xnNAPuj1S
oMwPdLF3QTdsVM2B8Gj+H86sqwPANERQ7SLm4RdeuYIdHKFgE+JIfZ5Zg8HP04w9u/aCzPFWcm7E
rjUWH+atpoUwtXHzHm4EzryRj57hqrI8a9rx3VjeIgfiM2AM4kAOb4Ch3J1cNuOK5+g82He6y44i
I6Yz0ztZmYszbJy1Jt20fjJtRSKVJMJJ412VZAAWNEChglvFPfFim/i95a4q6OqA1kHrJBafIk+2
UHNs5ToW88+94wrNWns91jwVkqvy9eyrdUp0B/DkzWaLkR0jIzrpqcrnJ3sPQA5tQEKOB4Sz6zge
+lZv0cr2waWkoLEwXHnzYBweCtP5MsaSvTUepOUJ9Yq4C7jaDalQ2PD8ZhvY4apGmSmhESMdSrwU
1FU3Jg2zC0MJxWw9c4xHqDisB4JIKkN36nN4+OybxBCF0hKn2Ms3x2GS9d4xwrjYs/+o/7WvNJFS
fM0Hy0bcaaQlGorwgY6CTnn1H1VyN6kJH7Byis9n54L6nX0iYqVte+OovZbHshpcj+FGp5l7cuk6
lH1cK1IRK2llYWK66H/N9FiPM3/BOMVMGKzUMuhVmO/DRYUDXioXniNzu2Ps7bKHHfyqMldKFCm/
JHOjBqDQ3IVN6mnush41h4ynToU2ejvyiYGMYnOKRko/0yi5Jc/BNz+RiBf85RLPBahN2irWIkBR
7R0yau+SAmuHT9TQ9IMQE8bmAq7kbFVYQatHFWeqXBfOdtBrnnQ7iVR/7rS0NfFiaFDPSo7a1mr5
PT6Mzych0jYAqKM0tbp5vvDc4S+gL46YlxsYO8NlZiH9LoPcNN6eKqumxbq+USEMdwyHWfaKHt7k
19CvtEjc6MA5Vbqi2gkeVid1oT95PhyR3ZFBehTT+t7jvogli9w5lGDYZmx2+4nRIT67yy76M1XW
PANtpusBVzAtR38e9BT98PU82TvZ5Ln+SpoXTJ4hoGtipjbeYoKH9hRvdtEcIU5y631q1ovEFu/o
5DZn7Jvs3NfPGkbWSWSEIxP2weuhj0PUMl8vXn7/hm5FNVHc3gR18a3AlXwGCQhqGYztyQCcXlz5
EZFm2GnX8zWUNajw6YvNDD84iFIpxDfNzwUHWA0h7/7OcxB95ObigKo7064/d2xTVcqi+8OsX3AN
Nd3q743IFm0aRliEolklR0el6tQPoWK97x0+z4YhirXlpP2OJUV4QagsTDpm+w+K2vHumSwKPkn9
kkh7xW3SHPb8X9uXXLXBoa2Ag6KWuUy5V8QI1ou8j4LKtNdlptPKtzqH2ycLpOD/jLPUOhde06wk
AUbY32MSD5B53/0fPOphneCxUPXASOCu92AzrfuH73jCouncNoG4PbyoMBVeAgAU2yMrZQy4V0ch
n4Tn062acD4PJLBnP4yoYYgMQUCb5c8bKxmyJtLyt/b+nk4gDNe+mgKE+lEA0gtRQlRBAXavToDy
XvakTrFNr7anlGuWfxraO2OvOAriyUxu5DU/PUMDjM5ae9PrXYGCKVlkIdTRBU211J1Mh2FyaaTZ
DsOMVqJyuryl0Xw3m1TnE7h+QIsfPlgzwZ67TPoohuCjSM5FjrCFUEOS3mdsfd6dIIk5hMHKz4xe
TXLc2gxcyoTrtWa1f9GSAW551VE7MwVs8cPzbZzTh9G9jywygLsHwjMZZP3VHfA9brVj5Q1nlomk
eLc7Uwpv9uwCYqD77Dx5COdVhXXhhASKhfB9lF3tZbSW0s3aszr9MLWnrm8m+YjZ3x6UiTrB86rc
dB0XrqcOIYXu9KwU2v3vDfR9xeVDKKn6nb+uQP1xWYJ97AQBBt440d1wEYUg+eB9Mp2VT5sOms58
Ykdrs6r1xzl3Z2VExE7wMlD4VWAKQBlwdRK2CAVs5jtQP7nqbtVCfAU7jmdrBxzi5FXPy1mPFdew
qnJv60JVShpGM36idfMQpwZLmPyQsoHDvV2O7mWoMOh4wTgsgtTvKhzTOYDhHE6QVJTypUToYJp5
+KN85EFHQygqQrgyHoVRij+s6ZLANhsb/GSbK1fdZpM+guQjP+HfB/GRUB15Qw0ga/4sx2w4ygpm
9JlqUR9fvCtO7U5z8ROdeTHY5V2ke0Khwhl7F29CSNAaPoUikEe+HZ9QF399pZ64r9j0uBqCpOMl
GCCIlGg4xdMBfwARfLIv3CUJxHeGOmPvILz+Jcw5Y1r8ry4qn37uOxSR3nlPaExyxcEOckELHJKh
PH4LDDVqhl+RDxZ5yUaX4Wv2nbv24yBRr5VVBfncM4T3At/hwDry3R1x86ciituB8rjQVt+vp2s3
k7XOI35YrQE/0YdHbQ+AdRrqHYae1J3gug14N11CX42XjeGsz37G0QDo/bItskWnaRvaFqsTWUy1
DAr/ht8/IYSVkufvkifjIwkrnzxy0YNTec26Oy5ZQtFCl1II7xQYHooua6xRZh6EXD1ajQbSdmf8
hl9Wa4I4pcfZv8iusOjkVrLSVa2+jyMCcBhpD1yleXkmty46InM8+HCRCVsW4Gh6K89d7/CnZgOm
QtyzK6GCv/zBDlpyQr2wP5cFXtgnExn4fdqiuSjVszmEFaENFlGVjJp5O9Qy+CTY5MC2+t62vrFL
CUTkUej0kWikrZnFOz88BtSOXpKyNE+onK/cYTgpR0/bUYMe3qeE2tm+Z6bvUqaLdsjrAsbl9G6G
FkPxe80qnBlidnlzIEYcABISDHssz7VEGF0H3cgxtnseQp8DI7mPYggWJuK3X++PH0JYFdEDE4Yk
CSacSTtU7W3IzUGrwvyEkf0+7rcwyS/Z7BRMan+3RFpgf4HKMLu5ruBd3boCZ6Lyz3NTTDrnmlr0
s4lTw/z9D55H2FKaY9u7Jo9ZxY1XHmurlVJ0nltxJNqlwEGyQTBzC0OmXrBNjRW2stjkpKiEGnD/
xY9Qk9sWdkr6EqhvBKpQ+gDE6Ve4CLgiqzBrzBh0VjBQFmRlCvzuBmRXw5NYywBiU7Id4huRIAEt
39Xo0GazbbeQd/9DpuBXnGjllSijfCLpPSpbERb3MDRPvjFUe3rAUiRFan/lC6QcgsXBNm6HPIfj
hMIV8vqVHli1sFCeGxbzJatyfzJSW2qTA1UQTnKeFTe5ZxwjrDvNJ84y3PJCzFAxFEVi6WMkzxvr
pcDdyGPRw8fBY3jxdI+LTv7iYxJVwhRfTBhWqdm2jAGTXd5UlzZvhJVHDfAztfrwbHLgCjvLOsmD
cvA0DN1RagZmJy3LqbHUCtjBKHhQubEBWH4mFLv3RQbNt1LL4/CX2W/TU3yCAvuZBGtLSH+8VlWr
guKbJliZ6E+n8FA/K5MiQJDsEhzBJem+KC/XiYSXubLIoFUDcV6JKDnCeOSywSho/Y+gFKUkLh/d
MgWjimQ8yWKIJUmt2UMkr/1Dw+JceDvj/4J2rvFx02QOeOG3dPjePI+VVkmjEhHYh/VLy1sS6FZG
iFOaRi5Hn44czQo0mbAgjEJEAFKqpO1l7+ylcnVPFoo3KVULvxVGfPymx1h0WXzWecdJNIBC57iE
sgzpsXJ32HMcyWBp1Su7x2b45EMLa7C8hk8OD+nOghe5Pzcj3xjfT3jlvEQsIhl4T4x55gsBKFgK
MY2zxYYGg1Kxy2Rwc0mMqvx8LXgqY05CrVqL3DYMKafBqCMIRM9RoTB+Xyq5RgwKuoBiGgx/bFdL
eGSoXeTnuJ+xDVhzVx/2+lWcLIjkVL8K99y1jy4CycNgb2b6KM1J+jx/0zhL4KTsET3I0MYgOV8s
t1Kl1e2QrjTzOyX93TO9qg60AprJePeHEkfxFc0Xz2Gozv55KDn6+YukrFPyrpyvUoMj2TYFkmQK
PO23Y640a3PxlKVTe0mhPcM2uZLAz50kqQp+wmNzzIb11B87GFcfYkcjovelLdUOJdcTsohwc5Pu
uMkSrCt7IF2qhC5jyqjZioQDDM3hOnQIl5qMFqMx627QKWqNavp9/VazbDQ7zNSGbWpWF2a2asPY
b7OWTyPOwm4WEpuWdEuVfXA05IXUXTYBmEbztOPQLozsGev3n6KN2la/SFepLFZF2P61y8nwxKth
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
    sed -e "s/\(current_year); ngx.print('\)/\1 \[tch-gui-unhide 2021.08.18 for FW Version 18.1.c ($MKTING_VERSION)\]/" -i $l
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
