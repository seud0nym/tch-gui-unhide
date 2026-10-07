#!/bin/sh
# Band steering blocks a steered client on the 2.4GHz access point. Turning band steering off does not
# remove those entries, so clear them from every access point whose band steering group is now off.
for ap in $(uci -q show wireless | sed -n 's/^wireless\.\([^.=]*\)=wifi-ap$/\1/p'); do
  bs=$(uci -q get wireless.$ap.bandsteer_id)
  [ -n "$bs" ] && [ "$bs" != "off" ] && [ "$(uci -q get wireless.$bs.state)" = "0" ] || continue
  for mac in $(ubus call wireless.accesspoint.acl get "{\"name\":\"$ap\"}" 2>/dev/null | jsonfilter -e "@.$ap.dynamic_deny_list"); do
    ubus call wireless.accesspoint.acl delete "{\"name\":\"$ap\",\"macaddr\":\"$mac\"}"
  done
done
