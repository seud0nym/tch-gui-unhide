--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local format = string.format
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local bbch = require("broadbandcard_helper")

-- wan status data
local wan_data = {
  wans_enable = "uci.wansensing.global.enable",
  wans_l2type = "uci.wansensing.global.l2type",
}
content_helper.getExactContent(wan_data)

local WS_en = wan_data.wans_enable
local WS_mode = wan_data.wans_l2type
if WS_mode == "" then
  WS_mode = "NONE"
end
local WS_State
if WS_en == "1" then
   if WS_mode == "NONE" then
      WS_State = "None"
      WS_state_map = T"WAN Sensing L2: None"
   else
      WS_State = "Enabled"
      WS_state_map = format(T"WAN Sensing L2: %s",WS_mode)
   end
else
   WS_State = "Disabled"
   WS_state_map = T"WAN Sensing: Disabled"
end

local WS_light_map = {
  Disabled = "off",
  None = "orange",
  Enabled = "green",
}

local session = ngx.ctx.session
local modalPath
if session:hasAccess("/modals/broadband-modal.lp") then
    modalPath = "modals/broadband-modal.lp"
end

local html = bbch.getBroadbandCardHTML()

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"Broadband", modalPath, nil, nil, nil) ); ngx.print('\
    <div class="content card_bg mirror" data-bg-text="&#xf0c1;">\
    ');  ngx.print( ui_helper.createSimpleLight(nil, WS_state_map, { light = { class = WS_light_map[WS_State] } }) ); ngx.print('\
      <div id="broadband-card-content">\
      ');  ngx.print(html); ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
<script>\
var bbFuncID;\
function updateBroadbandCard(){\
  $.post("/ajax/broadband-status.lua", [tch.elementCSRFtoken()], function(data){\
    $("#broadband-card-content").html(data["html"]);\
  }, "json")\
  .fail(function(response){\
    if(response.status==403||response.status==404){clearInterval(bbFuncID);}\
  });\
}\
$().ready(function(){bbFuncID=setInterval(updateBroadbandCard,15000);});\
</script>\
'); 