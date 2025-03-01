#!/bin/sh

log I "Restoring firewall configuration..."
uci -q revert firewall
uci_copy firewall fwconfig
uci_set firewall.@defaults[0].accept_redirects
uci_set firewall.@defaults[0].accept_source_route
uci_set firewall.@defaults[0].auto_helper
uci_set firewall.@defaults[0].disable_ipv6
uci_set firewall.@defaults[0].drop_invalid
uci_set firewall.@defaults[0].forward
uci_set firewall.@defaults[0].input
uci_set firewall.@defaults[0].output
uci_set firewall.@defaults[0].syn_flood
uci_set firewall.@defaults[0].synflood_burst
uci_set firewall.@defaults[0].synflood_rate
uci_set firewall.@defaults[0].tcp_ecn
uci_set firewall.@defaults[0].tcp_window_scaling
log D " ++ System Rules"
uci_copy_by_match firewall rule name enabled
log D " ++ User Rules"
uci_copy firewall userrule "" "proto"
uci_copy firewall userrule_v6 "" "proto"
log D " ++ Includes"
uci_copy_by_match firewall include path enabled reload
log D " ++ DNS Hijacking"
restore_file /etc/firewall.ipset.dns6_xcptn /etc/firewall.ipset.dns_xcptn /etc/firewall.ipset.doh /etc/firewall.ipset.doh6 /etc/config/tproxy /etc/firewall.ipset.hijack_xcptn /etc/config/dns_hijacking
for cfg in dns_xcptn dns_int dns_masq dns6_xcptn tproxy dot_fwd_xcptn dot_fwd dot6_fwd_xcptn dot_fwd doh doh6 ipsets_restore doh_fwd_xcptn doh_fwd doh6_fwd doh6_fwd_xcptn doh6_fwd hijack_xcptn hijackv4 hijackv6; do
  uci_set firewall.${cfg}.enabled
done
uci_set dns_hijacking.config.enabled
uci_set firewall.dns_int.dest_ip
uci_set firewall.dns_masq.dest_ip
log D " ++ IPv4 Port Forwarding"
uci_copy firewall userredirect "" "proto"
log D " ++ IPv6 Port Forwarding"
uci_copy firewall pinholerule "" "proto"
log D " ++ DMZ"
uci_set firewall.dmzredirects.enabled
uci_set firewall.dmzredirect.dest_ip
uci -q commit firewall
if [ -e /etc/config/dosprotect -a -e $BANK2/etc/config/dosprotect ]; then
  log D " ++ DoS Protection"
  uci -q revert dosprotect
  uci_set dosprotect.globals.enabled
  uci_set dosprotect.globals.rpfilter
  uci -q commit dosprotect
fi
log D " ++ Intrusion Protection"
uci -q revert intrusion_protect
restore_file /etc/config/intrusion_protect
uci_set firewall.intrusion_protect.enabled

unset cfg