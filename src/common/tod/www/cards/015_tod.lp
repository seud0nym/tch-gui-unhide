--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
if session:hasAccess("/modals/tod-modal.lp") then
  local format = string.format
  local ui_helper = require("web.ui_helper")
  local content_helper = require("web.content_helper")

  local tod = {
    host = "uci.tod.hostNumberOfEntries",
    wifi = "uci.tod.wifitodNumberOfEntries",
    dnd = "uci.tod.voicedndNumberOfEntries",
    dnd_enabled = "uci.tod.todvoicednd.enabled",
  }
  content_helper.getExactContent(tod)

  local rulecount_host = tonumber(tod.host) or 0
  local rulecount_wifi = tonumber(tod.wifi) or 0
  local rulecount_dnd = tonumber(tod.dnd) or 0

  ngx.print('\
  <div class="span3">\
    <div class="smallcard" >\
    ',ui_helper.createCardHeader(T"Time of Day","/modals/tod-modal.lp",nil,nil,{icon={class="icon-time"}}),'\
      <div class="content card_bg" data-bg-text="&#xf017;">\
        <div class="divtable">\
          <span class="simple-desc">\
            <i class="icon-laptop status-icon"></i>',
            format("<strong class='modal-link' data-toggle='modal' data-remote='/modals/tod-modal.lp' data-id='tod-modal'>%d Device Access</strong> %s defined",rulecount_host,N("rule","rules",rulecount_host)),'\
          </span>')
          if session:hasAccess("/modals/tod_wireless-modal.lp") then ngx.print('\
            <span class="simple-desc">\
              <i class="icon-rss status-icon"></i>',
              format("<strong class='modal-link' data-toggle='modal' data-remote='/modals/tod_wireless-modal.lp?tod' data-id='tod_wireless-modal'>%d Wireless Control</strong> %s defined",rulecount_wifi,N("rule","rules",rulecount_wifi)),'\
            </span>')
          end
          if session:hasAccess("/modals/tod_dnd-modal.lp") then
            if tod.dnd_enabled == "1" then
              ngx.print('\
              <span class="simple-desc">\
                <i class="icon-phone status-icon"></i>',
                format("<strong class='modal-link' data-toggle='modal' data-remote='/modals/tod_dnd-modal.lp?tod' data-id='tod_dnd-modal'>%d Do Not Disturb</strong> %s defined",rulecount_dnd,N("rule","rules",rulecount_dnd)),'\
              </span>')
            else
              ngx.print('\
              <span class="simple-desc">\
                <i class="icon-phone status-icon" style="color:grey"></i>\
                <strong class="modal-link" data-toggle="modal" data-remote="/modals/tod_dnd-modal.lp?tod" data-id="tod_dnd-modal">Do Not Disturb</strong> disabled\
              </span>')
            end
          end
          ngx.print('\
        </div>\
      </div>\
    </div>\
  </div>\
  ')
end
