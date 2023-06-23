#!/bin/sh

if [ "$(basename $0)" = "tch-gui-unhide-xtra.speedtest" -o -z "$FW_BASE" ]; then
  echo "ERROR: This script must NOT be executed!"
  echo "       Place it in the same directory as tch-gui-unhide and it will"
  echo "       be applied automatically when you run tch-gui-unhide."
  exit
fi

# The tch-gui-unhide-xtra scripts should output a single line to indicate success or failure
# as the calling script has left a hanging echo -n. Include a leading space for clarity.

if [ -f /root/ookla/speedtest -a -z "$XTRAS_REMOVE" ]; then
  if [ $(echo $RELEASE | tr -d '.@:') -lt 202306200000 ]; then
    echo " SKIPPED - Requires tch-gui-unhide 2023.06.20 or later"
  else
    echo " Adding Ookla SpeedTest support..."

    cat <<"RPC" > /usr/share/transformer/mappings/rpc/gui.speedtest.map
RPC
    cat <<"MOD" > /www/docroot/modals/diagnostics-speedtest-modal.lp
MOD

    chmod 644 /usr/share/transformer/mappings/rpc/gui.speedtest.map
    chmod 644 /www/docroot/modals/diagnostics-speedtest-modal.lp

    SRV_transformer=$(( $SRV_transformer + 1 ))

    sed \
      -e '/diagnostics-ping-modal\.lp/i\      <div class="diag-icon" data-toggle="modal" data-remote="modals/diagnostics-speedtest-modal.lp" data-id="diagnostics-speedtest-modal" title="SPEEDTEST®"><span data-original-title="SPEEDTEST" class="diag-speedtest"/></div>\\' \
      -i /www/cards/009_diagnostics.lp
    sed \
      -e '/diagnostics-ping-modal\.lp/i\    {"diagnostics-speedtest-modal.lp", T"Speed Test"},' \
      -i /www/snippets/tabs-diagnostics.lp
  fi
else
  echo " SKIPPED - /root/ookla/speedtest not found"
fi
