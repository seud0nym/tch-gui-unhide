--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local ssid_helper = require("ssid_helper")
local ipairs, ngx, string = ipairs, ngx, string
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

local state2G = proxy.get("rpc.wireless.radio.@radio_2G.oper_state")[1].value
local state5G = proxy.get("rpc.wireless.radio.@radio_5G.oper_state")[1].value
local wifiState
if state2G == "1" or state5G == "1" then
  wifiState = "1"
else
  wifiState = "0"
end

local ssid_list = ssid_helper.getSSIDList()

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
    ');  ngx.print(ui_helper.createCardHeader(T"Wi-Fi", modalPath, switchName, wifiState) ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf09e;">\
    ');  
      for i,v in ipairs(ssid_list) do
        if i <= 5 then
          local attributes = {
            light = {
              id = v.id
            }
          }
          ngx.print(ui_helper.createSimpleLight(v.state or "0", v.ssid, attributes))
        end
      end
      ngx.print('\
    </div>\
  </div>\
</div>\
<script>\
$(".header input#set_wifi_radio_state").on("change",function(){$("#waiting").removeClass("hide")});\
var ssidFuncID;\
function updateWirelessCard(){\
  $.post("/ajax/ssid-status.lua", [tch.elementCSRFtoken()], function(data){\
    for (var ssid in data) {\
      var light=$("#"+ssid);\
      light.removeClass("green off");\
      light.addClass(data[ssid] == "0" ? "off" : "green");\
    }\
  }, "json")\
  .fail(function(response){\
    if(response.status==403||response.status==404){clearInterval(ssidFuncID);}\
  });\
}\
$().ready(function(){ssidFuncID=setInterval(updateWirelessCard,20000);window.intervalIDs.push(ssidFuncID);});\
</script>\
'); 