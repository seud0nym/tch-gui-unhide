--pretranslated: do not change this file

local lfs = require("lfs")
if lfs.attributes("/etc/config/mmpbx","mode") ~= "file" then
  return
end

-- Enable localization
gettext.textdomain('webui-voice')

local ui_helper = require ("web.ui_helper")
local content_helper = require ("web.content_helper")
local tch = require("telephonycard_helper")

local session = ngx.ctx.session
if session:hasAccess("/modals/mmpbx-info-modal.lp") then
  local mmpbx = {
    state = "uci.mmpbx.mmpbx.@global.enabled"
  }
  content_helper.getExactContent(mmpbx)
  if not mmpbx.state or mmpbx.state == "" then
    mmpbx.state = "0"
  end

  local modalPath
  if mmpbx.state == "0" and session:hasAccess("/modals/mmpbx-global-modal.lp") then
    modalPath = "/modals/mmpbx-global-modal.lp"
  else
    modalPath = "/modals/mmpbx-info-modal.lp"
  end

  local html = tch.getTelephonyCardHTML(mmpbx.state)
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"Telephony",modalPath,nil,nil,{icon={class="icon-phone"}}),'\
    <div class="content card_bg mirror" data-bg-text="&#xf095;">\
      <div id="telephony-card-content">',html,'</div>\
    </div>\
  </div>\
</div>');
  if mmpbx.state == "1" then ngx.print('<script src="/js/008_telephony.js"></script>'); end;
end

