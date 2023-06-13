#!/bin/sh

log I "Stopping services..."
services="watchdog-tch \
          adblock autoreset \
          bqos bulkdata \
          cron cupsd cwmpd cwmpdboot \
          ddns dlnad dosprotect dumaos dumaos_qos_tweaks \
          hotspotd \
          intercept iperf ipset iqos \
          led ledfw log lotagent lsyslogd lte-doctor-logger \
          mesh-broker minidlna miniupnpd-tch mmpbxbrcmdect mmpbxd mmpbxd_lite mmpbxfwctl mosquitto mqttjson-services mud multiap_agent multiap_controller multiap_vendorextensions mwan \
          ndhttpd ndproxy nfcd nginx nqe \
          pinholehelper \
          qos qos_tch \
          ra redirecthelper rsyncd \
          samba samba-nmbd socat supervision sync-adguard-home-data sysntpd \
          telemetry-daemon tls-thor tod trafficmon transformer tproxy \
          uhttpd \
          wansensing wfa-testsuite-daemon wifi-doctor-agent wol \
          xl2tpd"

for service in $services; do
  if [ -x /etc/init.d/$service ]; then
    log D " -- Stopping $service service"
    /etc/init.d/$service stop >/dev/null 2>&1
  fi
done

unset services service
