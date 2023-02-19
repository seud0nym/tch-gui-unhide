#!/bin/sh

if [ "$(basename $0)" = "tch-gui-unhide-xtra.wlassoclist" -o -z "$FW_BASE" ]; then
  echo "ERROR: This script must NOT be executed!"
  echo "       Place it in the same directory as tch-gui-unhide and it will"
  echo "       be applied automatically when you run tch-gui-unhide."
  exit
fi

# The tch-gui-unhide-xtra scripts should output a single line to indicate success or failure
# as the calling script has left a hanging echo -n. Include a leading space for clarity.

if [ -z "$XTRAS_REMOVE" ]; then
  echo " Adding wlassoclist Access Point support..."
  cat <<MAP > /usr/share/transformer/mappings/rpc/gui.wlassoc.map
MAP
  chmod 644 /usr/share/transformer/mappings/rpc/gui.wlassoc.map
  SRV_transformer=$(( $SRV_transformer + 1 ))
  cat <<CONF > /etc/nginx/wlassoclist.conf
CONF
  chmod 644 /etc/nginx/wlassoclist.conf
  grep -q 'wlassoclist.conf' /etc/nginx/nginx.conf || sed -e '/include[[:blank:]]*mime.types;/a\    include /etc/nginx/wlassoclist.conf;' -i /etc/nginx/nginx.conf
  SRV_nginx=$(( $SRV_nginx + 2 ))
else
  echo " Removing wlassoclist Access Point support..."
  sed -e '/wlassoclist.conf/d' -i /etc/nginx/nginx.conf
  rm -f /etc/nginx/wlassoclist.conf /usr/share/transformer/mappings/rpc/gui.wlassoc.map
fi