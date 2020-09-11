--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local format = string.format

-- Should be in wireless-modal.lp
if ngx.var.request_method == "POST" then
  local radio_state = ngx.req.get_post_args().set_wifi_radio_state
  if radio_state then
     proxy.set("uci.wireless.wifi-device.@radio_2G.state", radio_state:untaint())
     proxy.set("uci.wireless.wifi-device.@radio_5G.state", radio_state:untaint())
     proxy.apply()
  end
end

local content = {
  state2g = "uci.wireless.wifi-device.@radio_2G.state",
  state5g = "uci.wireless.wifi-device.@radio_5G.state",
  ap0iface = "uci.wireless.wifi-ap.@ap0.iface",
  ap0state = "uci.wireless.wifi-ap.@ap0.state",
  ap1iface = "uci.wireless.wifi-ap.@ap1.iface",
  ap1state = "uci.wireless.wifi-ap.@ap1.state",
  ap2iface = "uci.wireless.wifi-ap.@ap2.iface",
  ap2state = "uci.wireless.wifi-ap.@ap2.state",
  ap3iface = "uci.wireless.wifi-ap.@ap3.iface",
  ap3state = "uci.wireless.wifi-ap.@ap3.state",
  wl0ssid = "uci.wireless.wifi-iface.@wl0.ssid",
  wl1ssid = "uci.wireless.wifi-iface.@wl1.ssid",
  wl0_1ssid = "uci.wireless.wifi-iface.@wl0_1.ssid",
  wl1_1ssid = "uci.wireless.wifi-iface.@wl1_1.ssid",
}
content_helper.getExactContent(content)

local session = ngx.ctx.session
local modalPath = nil
local switchName = nil
local wifiState
if content["state2g"] == "1" or content["state5g"] == "1" then
  wifiState = "1"
else
  wifiState = "0"
end
if content["state2g"] == "0" then
  if content["ap0state"] and content["ap0state"] == "1" then
    content["ap0state"] = "2"
  end
  if content["ap1state"] and content["ap1state"] == "1" then
    content["ap1state"] = "2"
  end
end
if content["state5g"] == "0" then
  if content["ap2state"] and content["ap2state"] == "1" then
    content["ap2state"] = "2"
  end
  if content["ap3state"] and content["ap3state"] == "1" then
    content["ap3state"] = "2"
  end
end

if session:hasAccess("/modals/wireless-modal.lp") then
  modalPath = "/modals/wireless-modal.lp"
  switchName = "set_wifi_radio_state"
end
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print(ui_helper.createCardHeaderNoIcon(T"Wi-Fi", modalPath, switchName, wifiState) ); ngx.print('\
    <div class="content">\
    ');  
      if content["ap0state"] and content["ap0iface"] == "wl0" and content["wl0ssid"] ~= "" then
        ngx.print(ui_helper.createSimpleLight(content["ap0state"], content["wl0ssid"].." (2.4G)"))
      end
      if content["ap2state"] and content["ap2iface"] == "wl1" and content["wl1ssid"] ~= "" then
        ngx.print(ui_helper.createSimpleLight(content["ap2state"], content["wl1ssid"].." (5G)"))
      end
      if content["ap1state"] and content["ap1iface"] == "wl0_1" and content["wl0_1ssid"] ~= "" then
        ngx.print(ui_helper.createSimpleLight(content["ap1state"], content["wl0_1ssid"].." (2.4G)"))
      end
      if content["ap3state"] and content["ap3iface"] == "wl1_1" and content["wl1_1ssid"] ~= "" then
        ngx.print(ui_helper.createSimpleLight(content["ap3state"], content["wl1_1ssid"].." (5G)"))
      end
      ngx.print('\
    </div>\
  </div>\
</div>\
'); 