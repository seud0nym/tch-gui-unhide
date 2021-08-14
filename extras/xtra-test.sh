#!/bin/sh

while getopts :rx option
do
 case "${option}" in
  r)  XTRAS_REMOVE=Y;;
  x)  set -x;;
  *)  echo "Usage: $0 [-x] [-r] tch-gui-unhide-xtra-<script>"; exit;;
 esac
done
shift $((OPTIND-1))


if [ -z "$1" ]; then
  echo ERROR: xtra script name required
  exit
fi

FW_BASE=$(uci get version.@version[0].marketing_version)
SRV_transformer=0

. ./$1

if [ $SRV_transformer -gt 0 ]; then
  /etc/init.d/watchdog-tch stop
  /etc/init.d/transformer restart
  /etc/init.d/watchdog-tch start
fi
