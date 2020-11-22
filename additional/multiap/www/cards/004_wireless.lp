--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
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

local ssid_list = {}
local fon_count = 0
local fon_state = 0
for _, v in ipairs(proxy.getPN("rpc.wireless.ssid.", true)) do
  local path = v.path
  local values = proxy.get(path .. "radio" , path .. "ssid", path .. "oper_state")
  if values then
    local ssid = values[2].value
    if ssid:sub(1,3) ~= "BH-" then
      if ssid == "Fon WiFi" or ssid == "Fon WiFi-5G" or ssid == "Telstra Air" or ssid == "Telstra Air-5G" then
        fon_count = fon_count + 1
        if values[3].value == 1 and fon_state == 0 then
          fon_state = 1
        end
      else
        local ap_display_name = proxy.get(path .. "ap_display_name")[1].value
        local radio_name = values[1].value
        local radio_suffix
        local display_ssid
        local sortby
        if radio_name == "radio_2G" then
          radio_suffix = " (2.4G)"
        else
          radio_suffix = " (5G)"
        end
        if ap_display_name ~= "" then
          display_ssid = ap_display_name .. radio_suffix
        elseif proxy.get(path .. "stb")[1].value == "1" then
          display_ssid = "IPTV" .. radio_suffix
        else
          display_ssid = ssid .. radio_suffix
        end
        if display_ssid:sub(1,5) == "Guest" then
          sortby = "yyyyy"
        else
          sortby = display_ssid:lower()
        end
        ssid_list[#ssid_list+1] = {
          ssid = display_ssid,
          state = values[3].value,
          sort = sortby
        }
      end
    end
  end
end
if fon_count > 0 then
  ssid_list[#ssid_list+1] = {
    ssid = T"Telstra Air (" .. string.format(N("%s SSID","%s SSIDs"),fon_count,fon_count) .. ")",
    state = fon_state,
    sort = "zzzzz"
  }
end

table.sort(ssid_list, function(a,b) return a.sort < b.sort end)

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
          ngx.print(ui_helper.createSimpleLight(v.state or "0", v.ssid))
        end
      end
      ngx.print('\
    </div>\
  </div>\
</div>\
<script>$(".header input#set_wifi_radio_state").on("change",function(){$("#waiting").removeClass("hide")});</script>\
'); 