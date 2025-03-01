#!/bin/sh

# IPv6 pin holes
ip6tables -S zone_wan_forward | grep zone_lan_dest_ACCEPT | while read -r action line
do
    parameters="$(echo "$line" | sed -e 's/zone_wan_forward/forwarding_Guest_rule/' -e 's/"!fw3:.*"/Fix-Guest-Access-To-Port-Forwards.sh/')"
    ip6tables -D $parameters 2>/dev/null
    ip6tables -A $parameters
done

exit 0