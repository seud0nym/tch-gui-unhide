#!/bin/sh
# Make sure that we are running on Telstra firmware
if [ "$(uci -q get env.var._provisioning_code)" != "Telstra" ]
then
  echo "ERROR! This script is intended for devices with Telstra firmware. Exiting"
  exit 1
fi
cat /proc/cpuinfo | grep -q 'ARMv7 Processor rev 1 (v7l)'
if [ $? -ne 0 ]
then
  echo "ERROR! This script is intended for ARMv7 devices. Exiting"
  exit 1
fi

# Inspired by https://gist.github.com/t413/3e616611299b22b17b08baa517d2d02c

ACME_DIR="$(cd $(dirname $0) && pwd)"
SCRIPT="$(basename $0)"

usage() {
cat <<EOH
Issues or renews a Let's Encrypt certificate for this device using acme.sh. (Let's Encrypt is preferred as it does not require an email address to issue the certificate.)

Port 80 (or port 443 if the -s option is specified) will be temporarily opened in the firewall to allow the CA to verify ownership of the domain.

Pre-requisites:
 * The domain name must be configured as the IPv4 and/or IPv6 Dynamic DNS domain.
 * The IP address to which the domain name resolves must be assigned to this device.

Note:
 * The ca-certificates and socat packages are required to be installed, but if they are not, this script will configure opkg (if required) and install them.

Usage: $0 [options]

Options:
 -s       Validate domain ownership via HTTPS (port 443) rather than HTTP (port 80)
 -l       Write log messages to stderr as well as the system log
 -v       Verbose mode
 -y       Bypass confirmation prompt (answers 'y')
 -C       Adds or removes the scheduled daily cron job
            If the cron job is being added, then the script will continue on and
            request an initial issue or renewal. Otherwise it will exit after
            removing the cron job.
 --force  Force renewal of existing certificate
 --test   Use staging server for testing
 -U       Download the latest version of this $SCRIPT from GitHub

EOH
exit
}

upgrade() {
  RESPONSE_CODE=$(curl -kLsI -o /dev/null -w '%{http_code}' https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/build/lua/additional/wanservices/root/.acme.sh/$SCRIPT)
  if [ "$RESPONSE_CODE" = 200 ]
  then
    curl -kL -o $ACME_DIR/$SCRIPT https://raw.githubusercontent.com/seud0nym/tch-gui-unhide/master/build/lua/additional/wanservices/root/.acme.sh/$SCRIPT
    if [ $? -eq 0 ]
    then
      chmod +x $ACME_DIR/$SCRIPT
      echo "[$SCRIPT] Successfully downloaded $SCRIPT."
    else
      echo "[$SCRIPT] Failed to download $SCRIPT."
    fi
  elif [ "$RESPONSE_CODE" = 404 ]
  then
    echo "[$SCRIPT] ERROR! Not found on GitHub???"
  elif [ "$RESPONSE_CODE" = 000 ]
  then
    echo "ERROR! No Internet connection???"
  else
    echo "[$SCRIPT] ERROR! Unknown response code $RESPONSE_CODE"
  fi
  exit
}

SECONDS=$(date +%s)

CERTS_DIR="/etc/ipsec.d/certs"
CACERTS_DIR="/etc/ipsec.d/cacerts"
PRIVATE_DIR="/etc/ipsec.d/private"

CRON=N
DEBUG=""
YES=N

FORCE_RENEW=""
SERVER="letsencrypt"
TO_STDERR=""
WAN_PORT='80'

while getopts :c:i:k:lsvyCU-: option
do
 case "${option}" in
  c)  CERTS_DIR="$OPTARG";;
  i)  CACERTS_DIR="$OPTARG";;
  k)  PRIVATE_DIR="$OPTARG";;
  l)  TO_STDERR="-s";;
  s)  WAN_PORT='443';;
  v)  DEBUG="--debug";;
  y)  YES=Y;;
  C)  CRON=Y;;
  U)  upgrade;;
  -)  case "$OPTARG" in
        force)  FORCE_RENEW="--force";;
        test)   SERVER="letsencrypt_test";;
        *)      usage;
      esac;;
  *)  usage;;
 esac
done
shift $((OPTIND-1))

LOGGER="/usr/bin/logger $TO_STDERR -t $SCRIPT -p"
_log() { priority="$1"; shift; $LOGGER $priority "[$(date)] $@"; }
_dbg() { [ "!$DEBUG" = "!--debug" ] && { _log user.debug $@; } }

if [ ! -f "$ACME_DIR/$SCRIPT" ]; then
  _log user.err "Error determing execution directory ($ACME_DIR) - Script $ACME_DIR/$SCRIPT not found???"
  exit 2
fi

for d in "$CERTS_DIR" "$CACERTS_DIR" "$PRIVATE_DIR"
do
  if [ ! -d  "$d" ]; then
    _log user.warn "Requested directory $d does not exist! Creating..."
    mkdir -p "$d"
  fi
done

OK=0

_dbg "Determining domain name..."

DOMAIN=$(uci -q get ddns.myddns_ipv4.lookup_host)
if [ -n "$DOMAIN" -a "$DOMAIN" != "yourhost.example.com" ]; then
  _dbg ">> Found IPv4 Dynamic DNS domain: $DOMAIN"
  _dbg "Attempting to resolve IP address of $DOMAIN..."
  WAN_IP="$(dig +short -t A $DOMAIN @1.1.1.1 2>/dev/null)"
  if [ -z "$WAN_IP" ]; then
    _log user.warn "WARNING: Could not resolve the IP address of IPv4 Dynamic DNS domain: $DOMAIN"
  else
    _dbg ">> $DOMAIN IP address is $WAN_IP"
    _dbg "Checking that IP address $WAN_IP is assigned to an interface"
    IFNAME=$(ip -o address | grep "$WAN_IP" | cut -d' ' -f2)
    if [ -n "$IFNAME" ]; then
      _dbg ">> $DOMAIN IP address found on interface $IFNAME"
      [ "!$DEBUG" = "!--debug" ] && ip -o address | grep "$WAN_IP" | $LOGGER user.info
    else
      _log user.warn "WARNING: IP address $WAN_IP for domain $DOMAIN is not in use on this device"
      ip -o address | tr -s " " | cut -d" " -f2,4 | grep -v -E '^lo|^wl|lan' | $LOGGER user.info
      WAN_IP=""
      DOMAIN=""
    fi
  fi
else
  DOMAIN=""
fi

DOMAINv6=$(uci -q get ddns.myddns_ipv6.lookup_host)
if [ -n "$DOMAINv6" -a "$DOMAINv6" != "yourhost.example.com" ]; then
  _dbg ">> Found IPv6 Dynamic DNS domain: $DOMAINv6"
  _dbg "Attempting to resolve IP address of $DOMAINv6..."
  WAN_IPv6="$(dig +short -t AAAA $DOMAINv6 @2606:4700:4700::1111 2>/dev/null)"
  if [ -z "$WAN_IPv6" ]; then
    _log user.warn "WARNING: Could not resolve the IP address of IPv6 Dynamic DNS domain: $DOMAINv6"
  else
    _dbg ">> $DOMAINv6 IP address is $WAN_IPv6"
    _dbg "Checking that IP address $WAN_IPv6 is assigned to an interface"
    IFNAMEv6=$(ip -o address | grep "$WAN_IPv6" | cut -d' ' -f2)
    if [ -n "$IFNAMEv6" ]; then
      _dbg ">> $DOMAINv6 IP address found on interface $IFNAMEv6"
      [ "!$DEBUG" = "!--debug" ] && ip -o address | grep "$WAN_IPv6" | $LOGGER user.info
    else
      _log user.warn "WARNING: IP address $WAN_IPv6 for domain $DOMAINv6 is not in use on this device"
      ip -o address | tr -s " " | cut -d" " -f2,4 | grep -v -E '^lo|^wl|lan' | $LOGGER user.info
      WAN_IPv6=""
      DOMAINv6=""
    fi
  fi
else
  DOMAINv6=""
fi

if [ -z "$DOMAIN" -a -z "$WAN_IP" -a -z "$DOMAINv6" -a -z "$WAN_IPv6" ]; then
  _log user.err "ABORTING!"
  exit 2
fi

PROMPT=""
if [ $CRON = Y ]; then
  grep -q "$SCRIPT" /etc/crontabs/root
  if [ $? = 0 ]; then
    CRON_ACTION=remove
  else
    CRON_ACTION=add
  fi
  PROMPT="$CRON_ACTION the cron schedule"
else
  if [ -f "$ACME_DIR/$DOMAIN/$DOMAIN.key" -o -f "$ACME_DIR/$DOMAIN/$DOMAINv6.key" ]; then
    if [ -z "$FORCE_RENEW" ]; then
      PROMPT="commence the certificate renewal check"
    else
      PROMPT="force certificate renewal"
    fi
  else
    if [ -n "$DOMAIN" -a -n "$DOMAINv6" -a "$DOMAIN" != "$DOMAINv6" ]; then
      PROMPT="issue the certificates"
    else
      PROMPT="issue the certificate"
    fi
  fi
fi
if [ $YES = N ]; then
  echo "If you wish to $PROMPT now, enter y otherwise just press [Enter] to exit."
  read
else
  REPLY=y
fi
if [ "$REPLY" != "y" -a "$REPLY" != "Y" ]; then
  exit
fi

_dbg "Changing to $ACME_DIR"
cd $ACME_DIR
if [ "$(pwd)" != "$ACME_DIR" ]; then
  _log user.err "ERROR: Failed to change to $ACME_DIR??? pwd=$(pwd)"
  exit 2
fi

_log user.info "Attempting to acquire pre-requisites check lock..."
lock /var/lock/$SCRIPT.pre
_dbg "Acquired lock on /var/lock/$SCRIPT.pre"

_dbg "Checking internet accecss..."
ping -c 1 -W 2 -q 1.1.1.1 >/dev/null 2>&1
if [ $? -ne 0 ]
then
  _log user.err "ERROR: Ping test failed! Do you have internet access?"
  OK=1
else
  _dbg "Checking for packages ca-certificates and socat..."
  PKG_CNT=$(opkg list-installed | grep -c -E '^ca-certificates -|^socat -')
  if [ $PKG_CNT -eq 2 ]; then
    _dbg ">> Found packages ca-certificates and socat"
  else
    if [$(grep -c "^arch " /etc/opkg.conf) -eq 0 -o $(sed -e '/^#/d' /etc/opkg/customfeeds.conf | wc -l) -eq 0 ]
    then
      HOMEWARE=$(uci get version.@version[0].version | cut -d. -f1)
      if [ "$HOMEWARE" -eq 17 -o "$HOMEWARE" -eq 18 -o "$HOMEWARE" -eq 20 ]
      then
        _log user.info "Configuring opkg for Homeware $HOMEWARE..."
        echo "dest root">/etc/opkg.conf
        echo "dest ram /tmp">>/etc/opkg.conf
        echo "lists_dir ext /var/opkg-lists">>/etc/opkg.conf
        echo "option overlay_root /overlay">>/etc/opkg.conf
        echo "arch all 1">>/etc/opkg.conf
        echo "arch noarch 1">>/etc/opkg.conf
        echo "arch arm_cortex-a9 10">>/etc/opkg.conf
        echo "arch arm_cortex-a9_neon 20">>/etc/opkg.conf
        echo "arch brcm63xx-tch 30">>/etc/opkg.conf
        echo 'arch bcm53xx 40'>>/etc/opkg.conf
        case "$HOMEWARE" in
          17) cat <<-HW17 | sed -e 's/^[ \t]*//g' > /etc/opkg/customfeeds.conf
            src/gz base https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/base
            src/gz luci https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/luci
            src/gz management https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/management
            src/gz packages https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/packages
            src/gz routing https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/routing
            src/gz telephony https://raw.githubusercontent.com/BoLaMN/brcm63xx-tch/master/packages/telephony
HW17
;;
          18|20) cat <<-HW18 | sed -e 's/^[ \t]*//g' > /etc/opkg/customfeeds.conf
            src/gz chaos_calmer_base_macoers https://repository.macoers.com/homeware/18/brcm63xx-tch/VANTW/base
            src/gz chaos_calmer_packages_macoers https://repository.macoers.com/homeware/18/brcm63xx-tch/VANTW/packages
            src/gz chaos_calmer_luci_macoers https://repository.macoers.com/homeware/18/brcm63xx-tch/VANTW/luci
            src/gz chaos_calmer_routing_macoers https://repository.macoers.com/homeware/18/brcm63xx-tch/VANTW/routing
            src/gz chaos_calmer_telephony_macoers https://repository.macoers.com/homeware/18/brcm63xx-tch/VANTW/telephony
            src/gz chaos_calmer_core_macoers https://repository.macoers.com/homeware/18/brcm63xx-tch/VANTW/target/packages
HW18
            if [ "$HOMEWARE" ] == "20" then
              echo 'src/gz wg_go https://raw.githubusercontent.com/seud0nym/openwrt-wireguard-go/master/repository/arm_cortex-a9/base' >> /etc/opkg/customfeeds.conf
            end
;;
        esac
        sed -e 's/^src/#src/' -i /etc/opkg/distfeeds.conf
      else
        _log user.err "ERROR: Unable to configure opkg for Firmware Version $(uci get version.@version[0].marketing_version)"
        OK=1
      fi
    fi
    if [ $OK = 0 ]; then
      _log user.info "Installing System CA certificates..."
      SRC='https://downloads.openwrt.org/releases/packages-21.02/arm_cortex-a9/base/'
      for IPK in $(curl -sk $SRC/Packages.gz | gunzip | grep '^Filename: ca-' | cut -d' ' -f2)
      do
        curl -skL $SRC/$IPK -o /tmp/$IPK && opkg --force-overwrite install /tmp/$IPK 2>&1 | logger -s -t rpc.gui.opkg -p user.info && rm /tmp/$IPK
      done
      _log user.info "Updating local package list..."
      opkg update | logger -s -t rpc.gui.opkg -p user.info
      _log user.info "Installing socat..."
      opkg install socat | logger -s -t rpc.gui.opkg -p user.info
      PKG_CNT=$(opkg list-installed | grep -c -E '^ca-certificates -|^socat -')
      if [ $PKG_CNT -ne 2 ]; then
        _log user.err "ERROR: Failed to install required packages"
        OK=1
      fi
    fi
  fi
  _dbg "Checking CA bundle..."
  curl -skLz /etc/ssl/certs/ca-certificates.crt -o /etc/ssl/certs/ca-certificates.crt https://curl.se/ca/cacert.pem
  export SSL_CERT_FILE='/etc/ssl/cert.pem'
  _dbg "Checking acme.sh..."
  if [ -s acme.sh ]; then
    _dbg ">> acme.sh found. Checking for updates..."
    chmod +x acme.sh
    ./acme.sh --upgrade
  else
    _log user.info "acme.sh not found - Downloading..."
    curl https://raw.githubusercontent.com/acmesh-official/acme.sh/master/acme.sh > acme.sh
    if [ $? -eq 0 ]; then
      chmod +x acme.sh
    else
      OK=1
    fi
  fi
fi
lock -u /var/lock/$SCRIPT.pre
_dbg "Released pre-requisites check lock"
if [ $OK -ne 0 ]; then
  _log user.err "ABORTING: Pre-requisites failure!"
  exit 2
fi

if [ $CRON = Y ]; then
  if [ $CRON_ACTION = remove ]; then
    sed -e "/$SCRIPT/d" -i /etc/crontabs/root
    _log user.info "Scheduled certificate renewal check has been removed"
    /etc/init.d/cron reload
    exit
  else
    mm=$(awk 'BEGIN{srand();print int(rand()*59);}')
    entry="$mm 0 * * * $ACME_DIR/$SCRIPT -y"
    if [ $WAN_PORT = 443 ]; then
      entry="$entry -s"
    fi
    if [ "!$FORCE_RENEW" = "!--force" ]; then
      entry="$entry -F"
    fi
    _log user.info "Creating scheduled cron job to check certificate renewal every day at 00:$(printf '%02d' $mm)am"
    lock /var/lock/$SCRIPT.cron
    _dbg "Acquired lock on /var/lock/$SCRIPT.cron"
    sed -e "/$SCRIPT/d" -i /etc/crontabs/root
    echo "$entry" >> /etc/crontabs/root
    lock -u /var/lock/$SCRIPT.cron
    _dbg "Released cron lock"
    /etc/init.d/cron reload
  fi
fi

_log user.info "Attempting to acquire execution lock..."
lock /var/lock/$SCRIPT.run
_dbg "Acquired execution lock on /var/lock/$SCRIPT.run"

# Additional variables required for /etc/ra_forward.sh
_dbg "Configuring environment variables for /etc/ra_forward.sh"
RA_NAME='acme'
ENABLED='1'
LAN_PORT='61734'
_log user.info "Opening WAN port $WAN_PORT..."
. /etc/ra_forward.sh

if [ $WAN_PORT = 80 ]; then
  SOCAT="--standalone --httpport"
else
  SOCAT="--alpn --tlsport"
fi

if [ -n "$DOMAIN" ]; then
  _dbg "Running acme.sh for domain $DOMAIN against CA server $SERVER..."
  ./acme.sh --issue $SOCAT $LAN_PORT --listen-v4 --server $SERVER -d $DOMAIN --key-file $PRIVATE_DIR/$DOMAIN.key --fullchain-file $CERTS_DIR/$DOMAIN.cer --renew-hook "/etc/init.d/ipsec restart" --syslog 6 $FORCE_RENEW $DEBUG $STAGING
fi
if [ -n "$DOMAINv6" -a "$DOMAINv6" != "$DOMAIN" ]; then
  _dbg "Stopping nginx server to free up port $WAN_PORT..."
  /etc/init.d/nginx stop
  _dbg "Running acme.sh for domain $DOMAINv6 against CA server $SERVER..."
  ./acme.sh --issue $SOCAT $WAN_PORT --listen-v6 --server $SERVER -d $DOMAINv6 --key-file $PRIVATE_DIR/$DOMAINv6.key --fullchain-file $CERTS_DIR/$DOMAINv6.cer --renew-hook "/etc/init.d/ipsec restart" --syslog 6 $FORCE_RENEW $DEBUG $STAGING
  _dbg "Restarting nginx server..."
  /etc/init.d/nginx start
fi

_log user.info "Closing WAN port $WAN_PORT..."
ENABLED='0'
. /etc/ra_forward.sh

lock -u /var/lock/$SCRIPT.run
_dbg "Released execution lock"

_log user.info "Finished. Elapsed time was $(( $(date +%s) - $SECONDS )) seconds"
