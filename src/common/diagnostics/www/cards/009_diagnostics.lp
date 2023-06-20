--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"Diagnostics","modals/diagnostics-traffic-modal.lp",nil,nil,{icon={class="icon-bug"}}),'\
    <div style="padding-left:36px;" class="content card_bg" data-bg-text="&#xf188;">\
      <div class="diag-icon" data-toggle="modal" data-remote="modals/diagnostics-traffic-modal.lp" data-id="diagnostics-traffic-modal"><img data-original-title="TRAFFIC" alt="traffic" class="diag-traffic"></div>\
      <div class="diag-icon" data-toggle="modal" data-remote="modals/diagnostics-ping-modal.lp" data-id="diagnostics-ping-modal"><img data-original-title="PING" alt="ping/trace" class="diag-ping"></div>\
      <div class="diag-icon" data-toggle="modal" data-remote="modals/logviewer-modal.lp" data-id="logviewer-modal"><img data-original-title="LOGVIEWER" alt="logviewer" class="diag-logviewer"></div></tr>\
      <div class="diag-icon" data-toggle="modal" data-remote="modals/log-connections-modal.lp" data-id="log-connections-modal"><img data-original-title="CONNECTIONS" alt="connections" class="diag-connections"></div>\
      <div class="diag-icon" data-toggle="modal" data-remote="modals/diagnostics-connection-modal.lp" data-id="diagnostics-connection-modal"><img data-original-title="CONNECTION" alt="connection" class="diag-connection"></div>\
      <div class="diag-icon" data-toggle="modal" data-remote="modals/diagnostics-network-modal.lp" data-id="diagnostics-network-modal"><img data-original-title="NETWORK" alt="network" class="diag-network"></div>\
      <div class="diag-icon" data-toggle="modal" data-remote="modals/diagnostics-tcpdump-modal.lp" data-id="diagnostics-tcpdump-modal"><img data-original-title="TCPDUMP" alt="tcpdump" class="diag-tcpdump"></div>\
    </div>\
  </div>\
</div>\
')
