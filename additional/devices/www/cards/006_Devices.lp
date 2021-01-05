--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local dch = require("devicescard_helper")
local ui_helper = require("web.ui_helper")

local session = ngx.ctx.session
if session:hasAccess("/modals/device-modal.lp") then
  local html = dch.getDevicesCardHTML()
  ngx.print('\
<div class="span3">\
  <style>.icon-wifi:before{content:"\\f09e";}</style>\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeaderNoIcon(T"Devices", "modals/device-modal.lp")  ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf109;">\
      <div id="devices-card-content">\
      ');  ngx.print(html); ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
<script>\
var dFuncID;\
function updateDevicesCard(){\
  $.post("/ajax/devices-status.lua", [tch.elementCSRFtoken()], function(data){\
    $("#devices-card-content").html(data["html"]);\
  }, "json")\
  .fail(function(response){\
    if(response.status==403||response.status==404){clearInterval(dFuncID);}\
  });\
}\
$().ready(function(){dFuncID=setInterval(updateDevicesCard,3000);});\
</script>\
');  
end
  ngx.print('\
'); 