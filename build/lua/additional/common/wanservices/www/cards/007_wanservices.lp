--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local wanservicescard_helper = require("wanservicescard_helper")
local html = wanservicescard_helper.getWANServicesCardHTML()

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"WAN Services","modals/wanservices-ddns-modal.lp"),'\
    <div id="wanservices-card-content" class="content card_bg" data-bg-text="&#xf0c2;">\
    ',html,'\
    </div>\
  </div>\
</div>\
<script src="/js/007_wanservices.js"></script>\
');
