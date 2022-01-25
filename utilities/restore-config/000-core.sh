#!/bin/sh

SCRIPT="$(basename $0)"
SOURCE_DIR="$(cd $(dirname $0); pwd)"

usage() {
cat <<EOH
Restores device configuration from the overlay backup and configuration
dump created by mtd-backup using the -c and -o options.

Usage: restore-config.sh [options] <overlay-files-backup>

Where:
  <overlay-files-backup>  
      is the name of the overlay files backup tgz file from which the 
      configuration will be restored. 
        e.g. DJA0231-CP1234S6789-20.3.c.0329-overlay-files-backup.tgz
      If not specified, it will attempt to find a backup by matching
      on device variant, serial number and firmware version.

Options:
  -i n.n.n.n  Use LAN IP address n.n.n.n instead of the IP address in
                the backup.
  -n          Do NOT reboot after restoring the configuration.
                This is NOT recommended.
  -t          Enable test mode. In test mode:
                - Wi-Fi will be disabled;
                - Dynamic DNS will be disabled;
                - Telephony will be disabled; and
                - "-TEST" is appended to the hostname and browser tabs 
                    if the serial number in the backup does not match
                    the device.
  -v          Enable debug messages. 
                Use twice for verbose messages.
  -y          Bypass confirmation prompt (answers 'y')

EOH
exit
}

log() {
  local flag="$1"
  local colour level
  shift
  case "$flag" in
    D|V)  colour="\033[90m";   level="DEBUG:";;
    E)    colour="\033[0;31m"; level="ERROR:";;
    I)    colour="\033[1;32m"; level="INFO: ";;
    W)    colour="\033[1;32m"; level="WARN: ";;
  esac
  [ \( "$flag" != "D" -a "$flag" != "V" \) -o \( "$flag" = "D" -a $DEBUG = y \) -o \( "$flag" = "V" -a $VERBOSE = y \) ] && echo -e "${level}  ${colour}$*\033[0m"
}

config2yn() {
  local path="$1"
  case "$($UCI -q get $path)" in
    1) echo "y";;
    0) echo "n";;
    *) echo "u";;
  esac
}

download() {
  local SOURCE="$1"
  local TARGET="$(basename $1)"
  local FOLDER="$2"
  RESPONSE_CODE=$(curl -kLsI -o /dev/null -w '%{http_code}' $SOURCE)
  if [ "$RESPONSE_CODE" = 200 ]; then
    log D "Downloading ${SOURCE}"
    mkdir -p $FOLDER
    curl -kLs $SOURCE -o $FOLDER/$TARGET
  else
    case "$RESPONSE_CODE" in
      404)  log E "$SOURCE was not found???";;
      000)  log E "Failed to download $SOURCE - No Internet connection???";;
      *)    log E "Failed to download $SOURCE - Unknown response code $RESPONSE_CODE";;
    esac
    unlock normally
  fi
}

find_scripts() {
  local c f
  local MOUNT_PATH

  log D "Searching for de-telstra and tch-gui-unhide-${DEVICE_VERSION}"
  MOUNT_PATH=$(uci -q get mountd.mountd.path)
  log V " ++ Searching /tmp/ ${MOUNT_PATH}"
  f=$(find /tmp/ $MOUNT_PATH -follow -maxdepth 2 -name de-telstra -o -name tch-gui-unhide-$DEVICE_VERSION 2>/dev/null | grep -vE 'mvfs|^/tmp/run' | xargs)
  log V " == Found ${f}"
  [ -n "$f" ] && SCRIPT_DIR="$(dirname $(ls -t $f 2>/dev/null | head -n 1) 2>/dev/null)"
  [ -z "$SCRIPT_DIR" ] && SCRIPT_DIR=/tmp
  log V " >> Latest modified time is in ${SCRIPT_DIR}"
  if [ ! -e "$SCRIPT_DIR/de-telstra" -o $(grep no-service-restart "$SCRIPT_DIR/de-telstra" | wc -l) -eq 0 ]; then
    log W "Could not locate up-to-date de-telstra. Downloading..."
    download https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/utilities/de-telstra $SCRIPT_DIR
  fi
  if [ ! -e "$SCRIPT_DIR/tch-gui-unhide-$DEVICE_VERSION" -o $(grep no-service-restart "$SCRIPT_DIR/tch-gui-unhide-$DEVICE_VERSION" | wc -l) -eq 0 ]; then
    log W "Could not locate up-to-date tch-gui-unhide-$DEVICE_VERSION. Downloading..."
    download https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/tch-gui-unhide-$DEVICE_VERSION $SCRIPT_DIR
  fi

  [ ! -e $SCRIPT_DIR/de-telstra ] && { log E "$SCRIPT_DIR/de-telstra was not found???"; unlock normally; }
  [ ! -e $SCRIPT_DIR/tch-gui-unhide-$DEVICE_VERSION ] && { log E "$SCRIPT_DIR/tch-gui-unhide-$DEVICE_VERSION was not found???"; unlock normally; }
  log I "Found de-telstra and tch-gui-unhide-$DEVICE_VERSION in $SCRIPT_DIR"

  log D "Searching for tch-gui-unhide customisation files"
  for c in authorized_keys ipv4-DNS-Servers ipv6-DNS-Servers; do
    log V " ++ Searching for $c in /root/ $BANK2/root/ $MOUNT_PATH"
    f=$(find /root/ $BANK2/root/ $MOUNT_PATH -follow -maxdepth 2 -type f -name $c 2>/dev/null | xargs)
    if [ -n "$f" ]; then
      log V " == Found ${f}"
      f=$(ls -tr $f 2>/dev/null | head -n 1)
      log V " >> Latest modified time is ${f}"
      if [ "$(dirname $f)" != "$SCRIPT_DIR" ]; then
        if ! cmp -s "$f" "${SCRIPT_DIR}/$(basename $f)" 2>/dev/null; then
          log D " >> Copying $f to ${SCRIPT_DIR}"
          cp "$f" "${SCRIPT_DIR}/"
        else
          log D " -- $f skipped (identical to ${SCRIPT_DIR}/$(basename $f))"
        fi
      else
        log D " -- $f found"
      fi
    else
      log D " ?? $f NOT found"
    fi
  done
}

restore_file() {
  local target
  for target in $*; do
    local folder="$(dirname $target)"
    local source="$BANK2/$target"
    case "/$(ls -l "$source" 2>/dev/null | cut -c1)/" in
      /-/)  if ! cmp -s "$source" "$target"; then
              log V " ++ Restoring ${target}"
              mkdir -p "$folder"
              cp "$source" "$target"
            fi;;
      /c/)  if [ -e "$target" ]; then
              log V " -- Deleting  ${target}"
              rm "$target"
            fi;;
      *)    log V " ?? Skipping  ${target} (Not found in backup?)"
    esac
  done
}

restore_directory() {
  local target="$1"
  local f
  if [ -d "${BANK2}${target}" ]; then
    shift
    for f in $(find ${BANK2}${target} $* -type f -o -type c | cut -c ${PREFIX_LENGTH}-); do
      restore_file "$f"
    done
  fi
}

restore_overlay_to_tmp() {
  rm -rf $BASE
  mkdir $BASE

  echo './bank_2/etc/' >> $BASE/.include
  echo './bank_2/root/' >> $BASE/.include
  echo './bank_2/usr/lib/lua/wansensingfw/failoverhelper.lua' >> $BASE/.include
  echo './bank_2/usr/lib/opkg/info/' >> $BASE/.include
  echo './bank_2/usr/share/transformer/mappings/rpc/gui.map' >> $BASE/.include
  echo './bank_2/www/' >> $BASE/.include

  echo './bank_2/root/*.core.gz' >> $BASE/.exclude
  echo './bank_2/root/log/*' > $BASE/.exclude

  tar -tzvf $OVERLAY | grep '\./bank_2/usr/lib/opkg/info/' | grep '\.control$' > $BASE/.packages
  opkg list-installed > $BASE/.installed

  log D "Restoring required files from $OVERLAY to $BASE.."
  tar -xzf $OVERLAY -C $BASE -T $BASE/.include -X $BASE/.exclude

  rm -f $BASE/.include $BASE/.exclude

  BACKUP_VERSION=$($UCI -q get version.@version[0].marketing_version)
  BACKUP_VARIANT=$($UCI -q get env.var.variant_friendly_name | sed -e 's/TLS//')
  BACKUP_SERIAL=$($UCI get env.var.serial)
}

run_script() {
  log I "Executing: $* "
  cd $SCRIPT_DIR
  sh $*
  cd $BACKUP_DIR
}

uci_copy() {
  local config="$1"
  local section_type="$2"
  local exclude="$3"
  local lists="$4"
  local path cfg value c
  for path in $($UCI show $config | grep "=${section_type}$" | cut -d= -f1); do
    if [ -z "$exclude" -o "$(echo $path | grep -vE "$exclude")" != "" ]; then
      $UCI show $path | while read -r cfg; do
        if echo "$cfg" | grep -q "^${config}\.@${section_type}\["; then
          if echo "$cfg" | grep -q "=${section_type}$"; then
            log D " ++ uci add $config $section_type"
            uci add $config $section_type
          else
            value=$(echo "$cfg" | cut -d= -f2-)
            c=$(echo "$cfg" | cut -d= -f1 | cut -d. -f3)
            if echo "$lists" | grep -qE "\b${c}\b"; then
              uci_set "${config}.${section_type}[-1].$c=$value" n y
            else
              uci_set "${config}.${section_type}[-1].$c=$value" n n
            fi
          fi
        else
          c=$(echo "$cfg" | cut -d= -f1 | cut -d. -f3)
          if [ -z "$c" ]; then
            uci_set "$cfg" n n
          elif echo "$lists" | grep -qE "\b${c}\b"; then
            uci_set "$cfg" n y
          else
            uci_set "$cfg" n n
          fi
        fi
      done
    fi
  done
}

uci_copy_by_match() {
  local config="$1"
  local type="$2"
  local match="$3"
  shift; shift
  local cfg dest srce name value param
  $UCI show $config | grep "\.${match}=" | while read -r cfg; do
    srce="$(echo "$cfg" | cut -d. -f1-2)"
    name="$(echo "$cfg" | cut -d= -f2)"
    if [ "$($UCI -q get $srce)" = "$type" ]; then
      dest="$(uci show $config | grep "\.${match}=${name}" | cut -d. -f1-2)"
      if [ -n "$dest" ]; then
        for param in $*; do
          value="$($UCI -q get $srce.$param)"
          if [ -n "$value" -a "$(uci -q get $dest.$param)" != "$value" ]; then
            log D " ++ Setting $dest.$param='$value'"
            uci_set "$dest.$param='$value'"
          fi
        done
      else
        log D " ?? No $config $type matching ${match}='${name}' found. Skipped."
      fi
    fi
  done
}

uci_set() {
  local path="$1"
  local delete_if_empty="$2"
  local islist="$3"
  local config="$(echo $path | cut -d. -f1)"
  local cmd value v
  log V " >> uci_set $1 (delete_if_empty=$2 islist=$3)"
  [ "$islist" = y ] && cmd="add_list" || cmd="set"
  if echo "$path" | grep -q '='; then
    path="$(echo "$1" | cut -d= -f1)"
    value="$(echo "$1" | cut -d= -f2-)"
  else
    [ -n "$config" -a -f $BANK2/etc/config/$config ] && value="$($UCI -q get $path)"
  fi
  if [ -n "$value" ]; then
    if [ "$islist" = y ]; then
      log V " -- Deleting any pre-existing list items in ${path}"
      uci -q delete $path
      for v in $value; do
        v=$(echo "$v" | sed -e "s/^['\"]\(.*\)[\"']$/\1/")
        log V " ++ Adding ${path}='$v'"
        uci -q $cmd $path="$v"
      done
    else
      value=$(echo "$value" | sed -e "s/^['\"]\(.*\)[\"']$/\1/")
      log V " ++ Setting ${path}='$value'"
      uci -q $cmd $path="$value"
    fi
  elif [ "$delete_if_empty" = y ]; then
    log V " -- Deleted ${path} (No value found in configuration backup)"
    uci -q delete $path
  fi
}

unlock() { 
  if [ "$1" != "normally" ]; then
    echo
    log W "Ctrl-C caught...performing clean up"
  fi

  log I "Releasing lock on $LOCK..."
  lock -u $LOCK
  [ -f $LOCK ] && rm $LOCK

  if [ "$1" = "normally" ]; then
    log I "Restore completed at $(date +%H:%M:%S) and took $(( $(date +%s) - $STARTED )) seconds"
    if [ "$REBOOT" = "y" ]; then
      log W "Rebooting..."
      sync
      reboot
    fi
    exit
  else
    exit 2
  fi
}

DEBUG=n
IPADDR=""
REBOOT=y
TEST_MODE=n
VERBOSE=n
YES=n

while getopts :i:ntvy option; do
  case "${option}" in
    i)  IPADDR="${OPTARG}";;
    n)  REBOOT=n;;
    t)  TEST_MODE=y;;
    v)  [ $DEBUG = n ] && DEBUG=y || VERBOSE=y;;
    y)  YES=y; log W "Confirmation prompt disabled";;
    *)  usage;;
  esac
done
shift $((OPTIND-1))

DEVICE_VERSION=$(uci -q get version.@version[0].marketing_version)
DEVICE_VARIANT=$(uci -q get env.var.variant_friendly_name | sed -e 's/TLS//')
DEVICE_SERIAL=$(uci get env.var.serial)

if [ $# -gt 1 ]; then
    log E "Maximum of 1 parameter (excluding options) expected. Found $*"
    exit 2
elif [ -z "$1" ]; then
  DFLT_OVERLAY=$(uci -q get env.var.variant_friendly_name)-$DEVICE_SERIAL-$(uci get version.@version[0].version | cut -d- -f1)-overlay-files-backup.tgz
  if [ -f $DFLT_OVERLAY ]; then
    OVERLAY=$DFLT_OVERLAY
  else 
    PREV_OVERLAY=$(ls $(uci -q get env.var.variant_friendly_name)-$DEVICE_SERIAL-*-overlay-files-backup.tgz 2>/dev/null | sort -r | head -n 1)
    if [ -n "$PREV_OVERLAY" ]; then
      OVERLAY=$PREV_OVERLAY
    else
      log E "No overlay files backup specified!"
      exit 2
    fi
  fi
elif ! echo "$1" | grep -q 'overlay-files-backup.tgz$'; then
  if [ $(ls ${1}* 2>/dev/null | grep 'overlay-files-backup.tgz$' | wc -l) -eq 1 ]; then
    OVERLAY="$(ls ${1}* 2>/dev/null | grep 'overlay-files-backup.tgz$')"
  else
    log E "'$1' is not an overlay files backup!"
    exit 2
  fi
elif [ ! -e "$1" ]; then
  log E "'$1' not found?"
  exit 2
else
  OVERLAY="$1"
fi

CONFIG="${OVERLAY%-overlay-files-backup.tgz}-config.gz"
if [ ! -e $CONFIG ]; then
  log E "Config backup '$CONFIG' not found?"
  exit 2
fi

BACKUP_DIR="$(cd $(dirname $OVERLAY); pwd)"

log I "Restoring:  ${OVERLAY}"
log I "Config:     ${CONFIG}"
[ -n "$IPADDR" ] && log I "IP Address: ${IPADDR}"
log I "Test Mode:  ${TEST_MODE}"
log I "Reboot:     ${REBOOT}"

if [ $YES = n ]; then
  echo -n -e "ACTION: \033[1;32mEnter y to restore, or anything else to exit now:\033[0m "
  read
  if [ "$REPLY" != "y" -a "$REPLY" != "Y" ]; then
    exit
  fi
fi

BASE="/tmp/restore"
BANK2="$BASE/bank_2"
PREFIX_LENGTH=$(( ${#BANK2} + 1 ))
UCI="uci -c $BANK2/etc/config"

DEVICE_VERSION=$(uci -q get version.@version[0].marketing_version)
DEVICE_VARIANT=$(uci -q get env.var.variant_friendly_name | sed -e 's/TLS//')
DEVICE_SERIAL=$(uci get env.var.serial)

for EXTENSION in $(ls $SOURCE_DIR/[0]*.sh | grep -v '000-core.sh' | sort); do
  log D "Importing $EXTENSION"
  source "$EXTENSION"
done
