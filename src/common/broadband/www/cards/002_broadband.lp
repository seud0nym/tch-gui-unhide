--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local format = string.format
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")

-- could be in bridge mode and fail if included in wan_data
local isBridgedMode = require("bridgedmode_helper").isBridgedMode()

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
else
   WS_Text = T"WAN Sensing disabled"
   WS_State = "Disabled"
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

ngx.print('\
<div class="span3" id="broadband-card">\
  <div class="smallcard">',
    ui_helper.createCardHeader(T"Broadband",modalPath,nil,nil,{icon={class="icon-link"}}),'\
    <div id="broadband-card-throughput"><div class="throughput"><span title="Raw throughput including protocol overhead"></span></div></div>\
    <div class="content card_bg mirror" data-bg-text="&#xf0c1;">\
      ',ui_helper.createSimpleLight(nil,WS_Text,{ light = { class = WS_light_map[WS_State] } }))
      if WS_en == "1" then
        ngx.print(format('<p class="subinfos">L2: <strong>%s</strong> L3: <strong>%s</strong></p>',WS_L2,WS_L3))
      end
      ngx.print('\
      <div id="broadband-card-content">\
        <span class="simple-desc">Getting Broadband Status...</span>\
      </div>\
    </div>\
  </div>\
</div>')
if isBridgedMode then
  ngx.print('<script>$("div.throughput").hide();</script>')
end
ngx.print('<script src="/js/002_broadband.js"></script>');