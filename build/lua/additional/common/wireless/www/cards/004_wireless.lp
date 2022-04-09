--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")

local session = ngx.ctx.session
local modalPath = nil

if session:hasAccess("/modals/wireless-modal.lp") then
  modalPath = "/modals/wireless-modal.lp"
end
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print(ui_helper.createCardHeader(T"Wi-Fi",modalPath)); ngx.print('\
    <div id="wifi-card-content" class="content card_bg" data-bg-text="&#xf09e;">\
      ');  ngx.print(ui_helper.createSimpleLight("3","Getting Wi-Fi status...")); ngx.print('\
    </div>\
  </div>\
</div>\
<script src="/js/004_wireless.js"></script>\
');