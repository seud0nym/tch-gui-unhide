--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')
local session = ngx.ctx.session
if session:hasAccess("/modals/device-modal.lp") then
  local ui_helper = require("web.ui_helper")
  local dch = require("devicescard_helper")
  ngx.print('\
<div class="span3">\
  <style>.icon-wifi:before{content:"\\f09e";}</style>\
  <div class="smallcard">\
    ',ui_helper.createCardHeaderNoIcon(T"Devices","modals/device-modal.lp",nil,nil,{icon={class="icon-laptop"}}),'\
    <div class="content card_bg" data-bg-text="&#xf109;">\
      <div id="devices-card-content">',
        dch.getDevicesCardHTML(false),'\
      </div>\
    </div>\
  </div>\
</div>\
<script src="/js/006_Devices.js"></script>\
');
end
