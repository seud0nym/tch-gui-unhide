--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local ssid_helper = require("ssid_helper")

local html = ssid_helper.getWiFiCardHTML()

local session = ngx.ctx.session
local modalPath = nil

if session:hasAccess("/modals/wireless-modal.lp") then
  modalPath = "/modals/wireless-modal.lp"
end
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"Wi-Fi",modalPath,nil,nil,{icon={class="icon-rss"}}),'\
    <div id="wifi-card-content" class="content card_bg" data-bg-text="&#xf09e;">\
      ',html,'\
    </div>\
  </div>\
</div>\
<script src="/js/004_wireless.js"></script>\
');