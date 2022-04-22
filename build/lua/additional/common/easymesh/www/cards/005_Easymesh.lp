--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local format = string.format

local function state_map(state)
  if state == "0" then
    return "disabled"
  end
  return "enabled"
end

local content = {
  broker = "uci.mesh_broker.global.@mesh_broker.enable",
  agent = "uci.wireless.map-agent.@agent.state",
  controller = "uci.mesh_broker.meshapp.@mesh_common.controller_enabled",
}

for _,v in ipairs(proxy.getPN("uci.mesh_broker.controller_credentials.", true)) do
  local backhaul = proxy.get(v.path.."backhaul")
  if backhaul and backhaul[1].value == "1" then
    content.backhaul_ssid = v.path.."ssid"
    content.backhaul_state = v.path.."state"
    break;
  end
end

content_helper.getExactContent(content)

local easymesh_role
local easymesh_state = "1"
if content.agent == "1" and content.controller == "0" then
  easymesh_role = "Agent"
elseif content.agent == "1" and content.controller == "1" then
  easymesh_role = "Controller"
elseif content.agent == "0" and content.controller == "1" then
  easymesh_role = "Controller-only"
else
  easymesh_role = "Disabled"
  easymesh_state = "0"
end

local deviceCount = proxy.get("Device.WiFi.MultiAP.APDeviceNumberOfEntries")
deviceCount = tonumber(deviceCount and deviceCount[1] and deviceCount[1].value or "0")
local nExt = deviceCount and deviceCount > 1 and deviceCount-1 or 0
local modal_link = ""
if nExt > 0 then
  modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/easymesh-topology.lp" data-id="device-modal"'
end

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
   ',ui_helper.createCardHeaderNoIcon(T"Wi-Fi Boosters","/modals/easymesh-modal.lp"),'\
    <div class="content card_bg" data-bg-text="&#xf0e8;">',
      ui_helper.createSimpleLight(easymesh_state,T(format("EasyMesh Role: %s",easymesh_role))),
      ui_helper.createSimpleLight(content.broker,T(format("EasyMesh Broker %s",state_map(content.broker)))),
      ui_helper.createSimpleLight(content.controller,T(format("EasyMesh Controller %s",state_map(content.controller)))))
      if content.controller == "1" then
        ngx.print(
        '<p class="subinfos">',
          format(N('<strong id="booster_count" %1$s>%2$d Booster</strong> connected','<strong id="booster_count" %1$s>%2$d Boosters</strong> connected',nExt),modal_link,nExt),
        '</p>'
        )
      end
      ngx.print(
      ui_helper.createSimpleLight(content.controller,T(format("EasyMesh Agent %s",state_map(content.agent)))),
      ui_helper.createSimpleLight(content.backhaul_state,T(format("SSID %s %s",content.backhaul_ssid,state_map(content.backhaul_state)))),'\
    </div>\
  </div>\
</div>\
');