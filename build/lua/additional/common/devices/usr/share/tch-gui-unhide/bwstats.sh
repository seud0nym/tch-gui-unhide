#!/bin/sh

TABLE=mangle
PARENT_CHAIN=FORWARD

if [ "/$1/" = "/show/" ]; then
  for CHAIN in BWSTATSRX BWSTATSTX; do
    for CMD in iptables ip6tables; do
      $CMD -t $TABLE -nvL $CHAIN
      $CMD -t $TABLE -nvL $PARENT_CHAIN | grep $CHAIN
      echo
    done
  done
else
  PID=$(pgrep -f bwstats.lua)
  [ -n "$PID" ] && kill -9 $PID

  if [ "/$1/" = "/disable/" -o "/$(uci -q get bwstats.config.enabled)/" != "/1/" ]; then
    PARAMETER_COUNT="iptables -t $TABLE -S $PARENT_CHAIN | grep -m 1 BWSTATS | wc -w"
    for CMD in iptables ip6tables; do
      $CMD -t $TABLE -S $PARENT_CHAIN | sed -e '/BWSTATS/!d' -e 's/-A/-D/' | xargs -rn $PARAMETER_COUNT $CMD -t $TABLE
      $CMD -t $TABLE -nL BWSTATSRX >/dev/null 2>&1 && { $CMD -t $TABLE -F BWSTATSRX; $CMD -t $TABLE -X BWSTATSRX; }
      $CMD -t $TABLE -nL BWSTATSTX >/dev/null 2>&1 && { $CMD -t $TABLE -F BWSTATSTX; $CMD -t $TABLE -X BWSTATSTX; }
    done
  else
    for CHAIN in BWSTATSRX BWSTATSTX; do
      [ $CHAIN = BWSTATSRX ] && DIRECTION=i || DIRECTION=o
      for CMD in iptables ip6tables; do
        $CMD -t $TABLE -nL $CHAIN >/dev/null 2>&1 || $CMD -t $TABLE -N $CHAIN
        $CMD -t $TABLE -C $CHAIN -j LOG --log-prefix "$CHAIN IGNORED! " 2>/dev/null || $CMD -t $TABLE -A $CHAIN -j LOG --log-prefix "$CHAIN IGNORED! "
        $CMD -t $TABLE -C $PARENT_CHAIN -$DIRECTION br-lan -j $CHAIN 2>/dev/null || $CMD -t $TABLE -I $PARENT_CHAIN 1 -$DIRECTION br-lan -j $CHAIN
      done
    done
    /usr/share/tch-gui-unhide/bwstats.lua $(uci -q get bwstats.config.log_level) >/dev/null 2>&1 &
  fi
fi

exit 0