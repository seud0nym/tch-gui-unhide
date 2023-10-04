#!/bin/sh

if [ "$(basename $0)" = "tch-gui-unhide-xtra.minidlna" -o -z "$FW_BASE" ]; then
  echo "ERROR: This script must NOT be executed!"
  echo "       Place it in the same directory as tch-gui-unhide and it will"
  echo "       be applied automatically when you run tch-gui-unhide."
  exit
fi

# The tch-gui-unhide-xtra scripts should output a single line to indicate success or failure
# as the calling script has left a hanging echo -n. Include a leading space for clarity.

if [ -f /etc/init.d/minidlna -a -z "$XTRAS_REMOVE" ]; then
  echo " Replacing dlnad with minidlna..."
  if [ "$(uci get dlnad.config.enabled)" = "1" ]; then
    # Shut down dlnad
    uci set dlnad.config.enabled='0'
    uci commit dlnad
    /etc/init.d/dlnad restart
  fi

  # Create the commit/apply rules
  if [ ! -f /usr/share/transformer/commitapply/uci_minidlna.ca ]; then
    echo '^minidlna /etc/init.d/minidlna restart'>/usr/share/transformer/commitapply/uci_minidlna.ca
    chmod 644 /usr/share/transformer/commitapply/uci_minidlna.ca
    SRV_transformer=$(( $SRV_transformer + 1 ))
  fi

  # Create the UCI transformer mapping
  if [ ! -f /usr/share/transformer/mappings/uci/minidlna.map ]; then
    cat <<"MAP" > /usr/share/transformer/mappings/uci/minidlna.map
MAP
    chmod 644 /usr/share/transformer/mappings/uci/minidlna.map
    SRV_transformer=$(( $SRV_transformer + 1 ))
  fi

  # Create the mount hotplug to restart minidlna when USB added/removed
  if [ ! -f /etc/hotplug.d/mount/00-minidlna ]; then
    cat<<"PLUG" > /etc/hotplug.d/mount/00-minidlna
PLUG
    chmod 755 /etc/hotplug.d/mount/00-minidlna
  fi

  # Update /lib/functions/contentsharing.sh
  sed \
    -e 's|/etc/init.d/minidlna-procd|/etc/init.d/minidlna|' \
    -e 's|/var/run/minidlna_d.pid|/var/run/minidlna/minidlna.pid|' \
    -i /lib/functions/contentsharing.sh

  # Update the card
  sed \
    -e 's/dlnad/minidlna/' \
    -e 's/"DLNA/"MiniDLNA/' \
    -i /www/cards/012_contentsharing.lp

  # Update the modal
  sed \
    -e 's/dlnad/minidlna/' \
    -e 's/"DLNA/"MiniDLNA/' \
    -e '/^local gVCS/a local gVIES = post_helper.getValidateInEnumSelect' \
    -e '/^local dlna_available/i \local root_containers = {' \
    -e '/^local dlna_available/i \    {".", T"Use standard container"},' \
    -e '/^local dlna_available/i \    {"B", T"Browse Directory"},' \
    -e '/^local dlna_available/i \    {"M", T"Music"},' \
    -e '/^local dlna_available/i \    {"V", T"Video"},' \
    -e '/^local dlna_available/i \    {"P", T"Pictures"},' \
    -e '/^local dlna_available/i \}' \
    -e '/"uci.minidlna.config.friendly_name"/a\        dlna_inotify = "uci.minidlna.config.inotify",' \
    -e '/"uci.minidlna.config.friendly_name"/a\        dlna_root_container = "uci.minidlna.config.root_container",' \
    -e '/"uci.minidlna.config.friendly_name"/a\        dlna_interface = "uci.minidlna.config.interface",' \
    -e '/^local function valid_samba_dlna_string/i \local dir_options = {' \
    -e '/^local function valid_samba_dlna_string/i \  tableid = "media_dirs",' \
    -e '/^local function valid_samba_dlna_string/i \  basepath = "uci.minidlna.config.media_dir.@.",' \
    -e '/^local function valid_samba_dlna_string/i \  createMsg = T"Add Media Directory",' \
    -e '/^local function valid_samba_dlna_string/i \  minEntries = 0,' \
    -e '/^local function valid_samba_dlna_string/i \}' \
    -e '/^local function valid_samba_dlna_string/i' \
    -e '/^local function valid_samba_dlna_string/i \local dir_columns = {' \
    -e '/^local function valid_samba_dlna_string/i \  {' \
    -e '/^local function valid_samba_dlna_string/i \    header = T"Directory",' \
    -e '/^local function valid_samba_dlna_string/i \    name = "media_dir",' \
    -e '/^local function valid_samba_dlna_string/i \    param = "value",' \
    -e '/^local function valid_samba_dlna_string/i \    type = "text",' \
    -e '/^local function valid_samba_dlna_string/i \  }' \
    -e '/^local function valid_samba_dlna_string/i \}' \
    -e '/^local function valid_samba_dlna_string/i \local dir_valid = {' \
    -e '/^local function valid_samba_dlna_string/i \  dns_server = vNES' \
    -e '/^local function valid_samba_dlna_string/i \}' \
    -e '/^local function valid_samba_dlna_string/i \local dir_data, dir_helpmsg = post_helper.handleTableQuery(dir_columns,dir_options,nil,nil,dir_valid)' \
    -e '/^local function valid_samba_dlna_string/i' \
    -e '/^local function valid_samba_dlna_string/i \local network_rpc_path = "rpc.network.interface."' \
    -e '/^local function valid_samba_dlna_string/i \local network_rpc_content = content_helper.getMatchedContent(network_rpc_path)' \
    -e '/^local function valid_samba_dlna_string/i \local intfs = {}' \
    -e '/^local function valid_samba_dlna_string/i \local split = require("split").split' \
    -e '/^local function valid_samba_dlna_string/i \for _,v in ipairs(network_rpc_content) do' \
    -e '/^local function valid_samba_dlna_string/i \  local path = split(split(format("%s",v.path),"@")[2],"%.")[1]' \
    -e '/^local function valid_samba_dlna_string/i \  if path == "wan" or path == "lan" then' \
    -e '/^local function valid_samba_dlna_string/i \    intfs[#intfs+1] = { v.ifname, T(v.ifname) }' \
    -e '/^local function valid_samba_dlna_string/i \  end' \
    -e '/^local function valid_samba_dlna_string/i \end' \
    -e '/^local function valid_samba_dlna_string/i' \
    -e '/dlna_friendly_name = valid_empty_string,/a\    dlna_inotify = gVCS,' \
    -e '/dlna_friendly_name = valid_empty_string,/a\    dlna_root_container=gVIES(root_containers),' \
    -e '/dlna_friendly_name = valid_empty_string,/a\    dlna_interface=gVIES(intfs),' \
    -e 's/local advancedhide/local info_attr/' \
    -e '/class = "advanced show"/d' \
    -e 's/span = { class = "span4" }/alert = { class = "alert-info" }/' \
    -e '/DLNA name: ",/a\                tinsert(html, ui_helper.createInputSelect(T"Interface:", "dlna_interface", intfs, content["dlna_interface"], advanced))' \
    -e '/DLNA name: ",/a\                tinsert(html, ui_helper.createInputSelect(T"Root Container:", "dlna_root_container", root_containers, content["dlna_root_container"], advanced))' \
    -e '/DLNA name: ",/a\                tinsert(html, format("<label class=\\"control-label\\">%s</label><div class=\\"controls\\">", T"Media Directories: "))' \
    -e '/DLNA name: ",/a\                tinsert(html, ui_helper.createAlertBlock(T("If you want to restrict a Media Directory to a specific content type, you can prepend the type, followed by a comma, to the directory:<br>\\"A\\" for audio (eg. A,/tmp/run/mountd/sda1/Music)<br>\\"V\\" for video (eg. V,/tmp/run/mountd/sda1/Videos)<br>\\"P\\" for images (eg. P,/tmp/run/mountd/sda1/Pictures)"), info_attr))' \
    -e '/DLNA name: ",/a\                tinsert(html, ui_helper.createTable(dir_columns, dir_data, dir_options, nil, dir_helpmsg))' \
    -e '/DLNA name: ",/a\                tinsert(html, ui_helper.createSwitch(T"Auto-discover new files: ", "dlna_inotify", content["dlna_inotify"], attributes))' \
    -e '/DLNA name: ",/a\                tinsert(html, "<\/div>")' \
    -e '/DLNA name: ",/a\                tinsert(html, "<\/fieldset>")' \
    -i /www/docroot/modals/contentsharing-modal.lp
else
  grep -q '/var/run/minidlna_d.pid' /lib/functions/contentsharing.sh
  if [ $? -ne 0 ]; then
    echo " minidlna removed - Restoring default content sharing functions"
    sed \
      -e 's|/etc/init.d/minidlna |/etc/init.d/minidlna-procd |' \
      -e 's|/var/run/minidlna/minidlna.pid|/var/run/minidlna_d.pid|' \
      -i /lib/functions/contentsharing.sh
  else
    echo " SKIPPED because minidlna not installed"
  fi
  if [ -f /usr/share/transformer/commitapply/uci_minidlna.ca ]; then
    rm /usr/share/transformer/commitapply/uci_minidlna.ca
    SRV_transformer=$(( $SRV_transformer + 1 ))
  fi
  if [ -f /usr/share/transformer/mappings/uci/minidlna.map ]; then
    rm /usr/share/transformer/mappings/uci/minidlna.map
    SRV_transformer=$(( $SRV_transformer + 1 ))
  fi
  if [ -f /etc/hotplug.d/mount/00-minidlna ]; then
    rm /etc/hotplug.d/mount/00-minidlna
  fi
fi
