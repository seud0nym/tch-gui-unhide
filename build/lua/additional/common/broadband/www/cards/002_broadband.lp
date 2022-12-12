--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local format = string.format
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")

-- could be in bridge mode and fail if included in wan_data
local proxy = require("datamodel")
local bridged = require("bridgedmode_helper")
local wan_proto = proxy.get("uci.network.interface.@wan.proto")
local static = wan_proto and wan_proto[1].value == "static"

-- wan status data
local wan_data = {
  wans_enable = "uci.wansensing.global.enable",
  wans_l2type = "uci.wansensing.global.l2type",
  wans_l3type = "uci.wansensing.global.l3type",
}
content_helper.getExactContent(wan_data)

local WS_en = wan_data.wans_enable
local WS_L2 = wan_data.wans_l2type
local WS_L3 = wan_data.wans_l3type
if WS_L2 == "" then
  WS_L2 = "NONE"
end
local WS_State
if WS_en == "1" then
  WS_Text = T"WAN Sensing enabled"
  if WS_L2 == "" then
    WS_L2 = "NONE"
    WS_State = "None"
  else
    WS_State = "Enabled"
  end
  if WS_L3 == "" then
    WS_L3 = "NONE"
  end
  if static then
    WS_State = "static"
    WS_Text = T"WAN Sensing enabled on STATIC IP"
  end
else
   WS_Text = T"WAN Sensing disabled"
   WS_State = "Disabled"
end

local WS_light_map = {
  Disabled = "off",
  None = "orange",
  Enabled = "green",
  static = "red",
}

local session = ngx.ctx.session
local modalPath
if session:hasAccess("/modals/broadband-modal.lp") then
  modalPath = "modals/broadband-modal.lp"
end

ngx.print('\
<div class="span3" id="broadband-card">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"Broadband",modalPath,nil,nil,nil) ); ngx.print('\
    <div id="broadband-card-throughput"><div class="throughput"><span title="Raw throughput including protocol overhead"></span></div></div>\
    <div class="content card_bg mirror" data-bg-text="&#xf0c1;">\
      ');
      if (WS_en == "1" or not static) and not bridged.isBridgedMode() then
        ngx.print( ui_helper.createSimpleLight(nil,WS_Text,{ light = { class = WS_light_map[WS_State] } }) )
      end
      if WS_en == "1" then
        ngx.print( format('<p class="subinfos">L2: <strong>%s</strong> L3: <strong>%s</strong></p>',WS_L2,WS_L3) )
      end
      ngx.print('\
      <div id="broadband-card-content">\
      </div>\
    </div>\
  </div>\
</div>\
<script src="/js/002_broadband.js"></script>\
');