--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
if session:hasAccess("/modals/wanservices-modal.lp") then
  local ui_helper = require("web.ui_helper")
  ngx.print('\
  <div class="span3">\
    <div class="smallcard">\
      ');  ngx.print(ui_helper.createCardHeader(T"WAN Services","modals/wanservices-modal.lp")); ngx.print('\
      <div id="wanservices-card-content" class="content card_bg" data-bg-text="&#xf0c2;">\
      ');
      ngx.print(ui_helper.createSimpleLight("3","Getting Dynamic DNS status..."));
      ngx.print(ui_helper.createSimpleLight("3","Getting DMZ status..."));
      ngx.print(ui_helper.createSimpleLight("3","Getting WoL over Internet status..."));
      ngx.print(ui_helper.createSimpleLight("3","Getting UPnP status..."));
      ngx.print('\
      </div>\
    </div>\
  </div>\
  <script src="/js/007_wanservices.js"></script>\
  ');
end
ngx.print('\
');