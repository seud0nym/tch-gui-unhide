#!/bin/sh

if [ "/$1/" = "/show/" ]; then
  for CHAIN in BWSTATSRX BWSTATSTX; do
    for CMD in iptables ip6tables; do
      $CMD -t mangle -nvL $CHAIN
      $CMD -t mangle -nvL FORWARD | grep $CHAIN
      echo
    done
  done
else
  PID=$(pgrep -f bwstats.lua)
  [ -n "$PID" ] && kill -9 $PID

  if [ "/$1/" = "/disable/" -o "/$(uci -q get bwstats.config.enabled)/" != "/1/" ]; then
    PARAMETER_COUNT="6"
    for CMD in iptables ip6tables; do
      $CMD -t mangle -S FORWARD | sed -e '/BWSTATS/!d' -e 's/-A/-D/' | xargs -rn $PARAMETER_COUNT $CMD -t mangle
      $CMD -t mangle -nL BWSTATSRX >/dev/null 2>&1 && { $CMD -t mangle -F BWSTATSRX; $CMD -t mangle -X BWSTATSRX; }
      $CMD -t mangle -nL BWSTATSTX >/dev/null 2>&1 && { $CMD -t mangle -F BWSTATSTX; $CMD -t mangle -X BWSTATSTX; }
    done
  else
    for CMD in iptables ip6tables; do
      $CMD -t mangle -nL BWSTATSRX >/dev/null 2>&1 || $CMD -t mangle -N BWSTATSRX
      $CMD -t mangle -C FORWARD -i br-lan -j BWSTATSRX 2>/dev/null || $CMD -t mangle -I FORWARD 1 -i br-lan -j BWSTATSRX # Change PARAMETER_COUNT if parameters change!
      $CMD -t mangle -nL BWSTATSTX >/dev/null 2>&1 || $CMD -t mangle -N BWSTATSTX
      $CMD -t mangle -C FORWARD -o br-lan -j BWSTATSTX 2>/dev/null || $CMD -t mangle -I FORWARD 1 -o br-lan -j BWSTATSTX # Change PARAMETER_COUNT if parameters change!
    done
    /usr/share/tch-gui-unhide/bwstats.lua $(uci -q get bwstats.config.log_level) >/dev/null 2>&1 &
  fi
fi

exit 0