#!/bin/sh

if [ "$(basename $0)" = "tch-gui-unhide-xtra.rsyncd" -o -z "$FW_BASE" ]; then
  echo "ERROR: This script must NOT be executed!"
  echo "       Place it in the same directory as tch-gui-unhide and it will"
  echo "       be applied automatically when you run tch-gui-unhide."
  exit
fi

# The tch-gui-unhide-xtra scripts should output a single line to indicate success or failure
# as the calling script has left a hanging echo -n. Include a leading space for clarity.

if [ -f /etc/init.d/rsyncd -a -z "$XTRAS_REMOVE" ]; then
  echo " Adding rsyncd support..."

  # Create the UCI transformer mapping
  cat <<"MAP" > /usr/share/transformer/mappings/rpc/gui.rsync.map
MAP
  chmod 644 /usr/share/transformer/mappings/rpc/gui.rsync.map
  SRV_transformer=$(( $SRV_transformer + 1 ))

  uci -q del_list system.@coredump[0].reboot_exceptions='rsync'
  uci -q add_list system.@coredump[0].reboot_exceptions='rsync'
  uci commit system
  SRV_system=$(( $SRV_system + 1 ))

  # Add the modules
  grep -q '\[home\]' /etc/rsyncd.conf
  if [ $? -ne 0 ]; then
    echo "">> /etc/rsyncd.conf
    echo "[home]" >> /etc/rsyncd.conf
    echo "path = /root" >> /etc/rsyncd.conf
    echo "read only = no" >> /etc/rsyncd.conf
  fi
  grep -q '\[tmp\]' /etc/rsyncd.conf
  if [ $? -ne 0 ]; then
    echo "">> /etc/rsyncd.conf
    echo "[tmp]" >> /etc/rsyncd.conf
    echo "path = /tmp" >> /etc/rsyncd.conf
    echo "read only = no" >> /etc/rsyncd.conf
  fi
  grep -q '\[usb\]' /etc/rsyncd.conf
  if [ $? -ne 0 ]; then
    echo "">> /etc/rsyncd.conf
    echo "[usb]" >> /etc/rsyncd.conf
    echo "path = /tmp/run/mountd/sda1" >> /etc/rsyncd.conf
    echo "read only = no" >> /etc/rsyncd.conf
  fi

  # Update the card
  uci -q set web.card_contentsharing.hide='0'
  sed \
    -e '/^local content/i \local rsync_enabled = "Disabled"' \
    -e '/^local content/i \local rsync = proxy.get("rpc.gui.rsync.enable")' \
    -e '/^local content/i \if rsync and (rsync[1].value == "1") then' \
    -e '/^local content/i \  rsync_enabled = "Enabled"' \
    -e '/^local content/i \end' \
    -e '/^local content/i \local rsync_light_map = {' \
    -e '/^local content/i \  Disabled = "0",' \
    -e '/^local content/i \  Enabled = "1",' \
    -e '/^local content/i \}' \
    -e '/^local content/i \local rsync_state_map = {' \
    -e '/^local content/i \  Disabled = T"Rsync Daemon disabled",' \
    -e '/^local content/i \  Enabled = T"Rsync Daemon enabled",' \
    -e '/^local content/i \}' \
    -e '/(content\["dlna/a                tinsert(html, ui_helper.createSimpleLight(rsync_light_map[rsync_enabled], rsync_state_map[rsync_enabled]))' \
    -i /www/cards/012_contentsharing.lp

  # Update the modal
  sed \
    -e '/getValidateCheckboxSwitch/a \local rsync_params = {' \
    -e '/getValidateCheckboxSwitch/a \  rsync_enable = "rpc.gui.rsync.enable",' \
    -e '/getValidateCheckboxSwitch/a \}' \
    -e '/getValidateCheckboxSwitch/a \local rsync_valid = {' \
    -e '/getValidateCheckboxSwitch/a \  rsync_enable = gVCS,' \
    -e '/getValidateCheckboxSwitch/a \}' \
    -e '/getValidateCheckboxSwitch/a \local rsync_params, rsync_helpmsg = post_helper.handleQuery(rsync_params, rsync_valid)' \
    -e '/^            tinsert(html, "<\/fieldset>")/a \            if rsync_params then' \
    -e '/^            tinsert(html, "<\/fieldset>")/a \               tinsert(html, "<fieldset><legend>" .. T"Rsync Status" .. "</legend>")' \
    -e '/^            tinsert(html, "<\/fieldset>")/a \               tinsert(html, ui_helper.createSwitch(T"Rsync Daemon", "rsync_enable", rsync_params["rsync_enable"]))' \
    -e '/^            tinsert(html, "<\/fieldset>")/a \               tinsert(html, "</fieldset>")' \
    -e '/^            tinsert(html, "<\/fieldset>")/a \            end' \
    -i /www/docroot/modals/contentsharing-modal.lp
else
  if [ -f /usr/share/transformer/mappings/rpc/gui.rsync.map ]; then
    echo " rsyncd removed - Cleaning up"
    rm /usr/share/transformer/mappings/rpc/gui.rsync.map
    SRV_transformer=$(( $SRV_transformer + 1 ))
    uci -q del_list system.@coredump[0].reboot_exceptions='rsync'
    uci commit system
    SRV_system=$(( $SRV_system + 1 ))
  else
    echo " SKIPPED because rsyncd not installed"
  fi
fi