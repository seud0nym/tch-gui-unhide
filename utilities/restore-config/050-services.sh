#!/bin/sh

log I "Stopping services..."
services="autoreset \
          bulkdata \
          cron cupsd cwmpd cwmpdboot \
          ddns dlnad dosprotect dumaos dumaos_qos_tweaks \
          hotspotd \
          intercept iperf ipset \
          led ledfw lotagent lte-doctor-logger \
          miniupnpd-tch mmpbxbrcmdect mmpbxd mmpbxd_lite mmpbxfwctl mosquitto mqttjson-services mud multiap_agent multiap_controller multiap_vendorextensions mwan \
          ndhttpd ndproxy nfcd nginx nqe \
          pinholehelper \
          qos \
          ra \
          samba samba-nmbd socat supervision sysntpd \
          telemetry-daemon tod trafficmon transformer \
          wansensing watchdog-tch wfa-testsuite-daemon wifi-doctor-agent wol \
          xl2tpd"

for service in $services; do
  if [ -e /etc/init.d/$service ]; then
    log D " -- Stopping $service service"
    /etc/init.d/$service stop >/dev/null 2>&1
  fi
done

log I "Killing any remaining service processes..."
sleep 5
for service in $services; do
  pgrep -f $service | xargs -r -n 1 kill
done
sleep 5
for service in $services; do
  pgrep -f $service | xargs -r -n 1 kill -9
done

unset services service
