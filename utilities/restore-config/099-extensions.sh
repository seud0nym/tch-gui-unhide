#!/bin/sh

LOCK="/var/run/${SCRIPT}.lck"
if [ -e $LOCK ]; then
  if ps | grep -qE "\b$(cat $LOCK)\b.*$SCRIPT"; then
    log E "Restore already running?"
    exit 2
  else
    lock -u $LOCK
    rm $LOCK
  fi
fi
log I "Attempting to acquire lock on $LOCK..."
lock $LOCK
trap "unlock" 2

STARTED=$(date +%s)
log I "Restore commenced at $(date +%H:%M:%S)"
find_scripts
restore_overlay_to_tmp
for EXTENSION in $(ls $SOURCE_DIR/[1-9]*.sh | sort); do
  log D "Importing $EXTENSION"
  source "$EXTENSION"
done
unlock normally
