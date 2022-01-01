--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")

-- Should be in wireless-modal.lp
if ngx.var.request_method == "POST" then
  local radio_state = ngx.req.get_post_args().set_wifi_radio_state
  if radio_state then
     proxy.set("uci.wireless.wifi-device.@radio_2G.state",radio_state:untaint())
     proxy.set("uci.wireless.wifi-device.@radio_5G.state",radio_state:untaint())
     proxy.apply()
  end
end

local state2G = proxy.get("rpc.wireless.radio.@radio_2G.oper_state")
local state5G = proxy.get("rpc.wireless.radio.@radio_5G.oper_state")
local wifiState
if (state2G and state2G[1].value == "1") or (state5G and state5G[1].value == "1") then
  wifiState = "1"
else
  wifiState = "0"
end

local session = ngx.ctx.session
local modalPath = nil
local switchName = nil

if session:hasAccess("/modals/wireless-modal.lp") then
  modalPath = "/modals/wireless-modal.lp"
  switchName = "set_wifi_radio_state"
end
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print(ui_helper.createCardHeader(T"Wi-Fi",modalPath,switchName,wifiState) ); ngx.print('\
    <div id="wifi-card-content" class="content card_bg" data-bg-text="&#xf09e;">\
      ');  ngx.print(ui_helper.createSimpleLight("3","Getting SSID status...")); ngx.print('\
    </div>\
  </div>\
</div>\
<script src="/js/004_wireless.js"></script>\
');