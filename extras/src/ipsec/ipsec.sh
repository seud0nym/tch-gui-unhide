#!/bin/sh

if [ "$(basename $0)" = "tch-gui-unhide-xtra.ipsec" -o -z "$FW_BASE" ]; then
  echo "ERROR: This script must NOT be executed!"
  echo "       Place it in the same directory as tch-gui-unhide and it will"
  echo "       be applied automatically when you run tch-gui-unhide."
  exit
fi

# The tch-gui-unhide-xtra scripts should output a single line to indicate success or failure
# as the calling script has left a hanging echo -n. Include a leading space for clarity.

if [ -f /etc/ipsec.conf -a -z "$XTRAS_REMOVE" -a "$(opkg list-installed | grep strongswan-default | cut -d' ' -f3 | cut -d. -f1-2)" = "5.6" ]; then
  echo " Adding ipsec support..."

  grep -q '^conn tgu-responder-base' /etc/ipsec.conf
  if [ $? -eq 1 ]; then
    cat <<CONN-BASE >> /etc/ipsec.conf

conn tgu-responder-base
  compress = no
  type = tunnel
  dpdaction = clear
  dpddelay = 30s
  leftsubnet = 0.0.0.0/0
  rightsourceip = %dhcp
CONN-BASE
  fi

  grep -q '^conn tgu-responder-ikev1-psk-xauth' /etc/ipsec.conf
  if [ $? -eq 1 ]; then
    cat <<CONN-DROID >> /etc/ipsec.conf

conn tgu-responder-ikev1-psk-xauth
  also = tgu-responder-base
  auto = ignore
  keyexchange = ikev1
  leftauth = psk
  rightauth = psk
  rightauth2 = xauth
  rightsendcert = never
  ike = aes256-sha512-prfsha512-modp1024,aes256-sha384-prfsha384-modp1024,aes256-sha256-prfsha256-modp1024
  esp = aes128-sha1
CONN-DROID
  fi

  grep -q '^conn tgu-responder-ikev2-psk' /etc/ipsec.conf
  if [ $? -eq 1 ]; then
    cat <<CONN-iOS >> /etc/ipsec.conf

conn tgu-responder-ikev2-psk
  also = tgu-responder-base
  auto = ignore
  keyexchange = ikev2
  leftauth = psk
  rightauth = psk
  rightsendcert = never
  ike = aes256gcm16-prfsha384-ecp384
  esp = aes256gcm16-ecp384
CONN-iOS
  fi

  grep -q '^conn tgu-responder-ikev2-mschapv2' /etc/ipsec.conf
  if [ $? -eq 1 ]; then
    cat <<CONN-WIN >> /etc/ipsec.conf

conn tgu-responder-ikev2-mschapv2
  also = tgu-responder-base
  auto = ignore
  keyexchange = ikev2
  rightauth = eap-mschapv2
  rightsendcert = never
  ike = aes256gcm16-prfsha384-ecp384
  esp = aes256gcm16-ecp384
CONN-WIN
  fi

  if [ ! -f /etc/strongswan.d/libstrongswan.conf ]; then
    cat <<"LSS" > /etc/strongswan.d/libstrongswan.conf
LSS
    chmod 644 /etc/strongswan.d/libstrongswan.conf
  fi

  if [ ! -f /etc/firewall.ipsec.responder ]; then
    cat <<"FW" > /etc/firewall.ipsec.responder
FW
    chmod 755 /etc/firewall.ipsec.responder
    uci set firewall.ipsec_responder='include'
    uci set firewall.ipsec_responder.type='script'
    uci set firewall.ipsec_responder.path='/etc/firewall.ipsec.responder'
    uci set firewall.ipsec_responder.reload='1'
    uci set firewall.ipsec_responder.enabled='0'
    uci commit firewall
    SRV_firewall=$(( $SRV_firewall + 1 ))
  fi

  if [ ! -f /usr/share/transformer/mappings/rpc/gui.ipsec.map ]; then
    cat <<"RPC" > /usr/share/transformer/mappings/rpc/gui.ipsec.map
RPC
    chmod 644 /usr/share/transformer/mappings/rpc/gui.ipsec.map
    SRV_transformer=$(( $SRV_transformer + 1 ))
  fi

  if [ ! -f /www/cards/003_ipsec.lp ]
  then
    cat <<"CRD" > /www/cards/003_ipsec.lp
CRD
    chmod 644 /www/cards/003_ipsec.lp
  fi

  if [ ! -f /www/docroot/ajax/ipsec-status.lua ]
  then
    cat <<"AJX" > /www/docroot/ajax/ipsec-status.lua
AJX
    chmod 644 /www/docroot/ajax/ipsec-status.lua
  fi

  if [ ! -f /www/docroot/modals/ipsec-modal.lp ]
  then
    cat <<"MOD" > /www/docroot/modals/ipsec-modal.lp
MOD
    chmod 644 /www/docroot/modals/ipsec-modal.lp
  fi

  if [ ! -f /www/lua/ipsec_helper.lua ]
  then
    cat <<"HLP" > /www/lua/ipsec_helper.lua
HLP
    chmod 644 /www/lua/ipsec_helper.lua
  fi

  if [ ! -L /etc/ipsec.d/cacerts ]; then
    rm -rf /etc/ipsec.d/cacerts
    ln -s /etc/ssl/certs /etc/ipsec.d/cacerts
  fi

  /etc/init.d/ipsec restart 2>&1 >/dev/null
else
  if [ -e "/etc/firewall.ipsec.responder" ];then
    echo " ipsec removed - Cleaning up"
    uci -q delete firewall.ipsec_responder
    uci commit firewall
    /etc/init.d/firewall reload >/dev/null
    rm /etc/firewall.ipsec.responder
  else
    echo " SKIPPED because strongswan-default 5.6 not installed"
  fi
fi
