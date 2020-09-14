--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local session = ngx.ctx.session
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local format, untaint, match, upper, gsub = string.format, string.untaint, string.match, string.upper, string.gsub
local proxy = require("datamodel")

local modalPath = "/modals/wireless-boosters-modal.lp"

local content = {
  agent_enabled = "uci.multiap.agent.enabled",
  controller_enabled = "uci.multiap.controller.enabled",
  state5g = "uci.wireless.wifi-device.@radio_5G.state",
  ap6iface = "uci.wireless.wifi-ap.@ap6.iface",
  ap6state = "uci.wireless.wifi-ap.@ap6.state",
  wl1_2ssid = "uci.wireless.wifi-iface.@wl1_2.ssid",
}

content_helper.getExactContent(content)

if content.agent_enabled and content.agent_enabled ~= "" and content.controller_enabled and content.controller_enabled ~= ""  then

  local agentStatus, controllerStatus
  local backhaulStatus = nil
  local boosters = 0
  
  if content.agent_enabled == "1" then
    agentStatus = "Multi-AP Agent enabled"
  else
    agentStatus = "Multi-AP Agent disabled"
  end
  if content.controller_enabled == "1" then
    local path = "Device.Services.X_TELSTRA_MultiAP.Agent."
    local data = content_helper.convertResultToObject(path, proxy.get(path))
    for key, agent in ipairs(data) do
      if agent.MACAddress ~= "" and agent.MACAddress ~= "00:00:00:00:00:00" then
        boosters = boosters + 1
      end
    end
    controllerStatus = "Multi-AP Controller enabled"
  else
    controllerStatus = "Multi-AP Controller disabled"
  end
  if content["ap6state"] and content["ap6iface"] == "wl1_2" and content["wl1_2ssid"] ~= "" then
    if content["ap6state"] == "0" then
      backhaulStatus = T"Backhaul " .. content["wl1_2ssid"] .. " (5G) disabled"
    elseif content["state5g"] == "0" then
      content["ap6state"] = "2"
      backhaulStatus = T"Backhaul " .. content["wl1_2ssid"] .. " (5G) radio off"
    else
      backhaulStatus = T"Backhaul " .. content["wl1_2ssid"] .. " (5G) enabled"
    end
  end
 
ngx.print('\
<div class="span3">\
    <div class="smallcard">\
        '); ngx.print( ui_helper.createCardHeader(T"Wi-Fi Boosters", modalPath) ); ngx.print('\
      <div class="content card_bg" data-bg-text="&#xf0e8;">\
		    <div class="divtable">\
            ');
            ngx.print(
              ui_helper.createSimpleLight(content.controller_enabled, controllerStatus),
              ui_helper.createSimpleLight(content.agent_enabled, agentStatus)
            )
            if backhaulStatus then
              ngx.print(ui_helper.createSimpleLight(content["ap6state"], backhaulStatus))
            end
            if content.controller_enabled == "1" then
              local modalLink = format('class="modal-link" data-toggle="modal" data-remote="%s" data-id="device-modal"',modalPath)
              ngx.print(
              '<p class="subinfos">',
                format(N("<strong %1$s>%2$d booster</strong> found","<strong %1$s>%2$d boosters</strong> found",boosters),modalLink,boosters),
              '</p>'
              )
            end
            ngx.print('\
        </div>\
		  </div>\
    </div>\
</div>\
');
end
  ngx.print('\
');
