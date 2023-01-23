--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

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

local modal_link = ""

local nExt = 0
local synced = 0

if easymesh_role == "Controller" or easymesh_role == "Controller-only" then
  local deviceCount = proxy.get("rpc.wifi.multiap.deviceNumberOfEntries")
  deviceCount = tonumber(deviceCount and deviceCount[1] and deviceCount[1].value or "0")
  nExt = deviceCount and deviceCount > 1 and deviceCount-1 or 0
  if nExt > 0 then
    modal_link = 'class="modal-link" data-toggle="modal" data-remote="/modals/easymesh-topology.lp" data-id="device-modal"'
  end
end

if easymesh_role == "Agent" then
  local credentials = {}
  local radios = {}

  for _,v in ipairs(proxy.getPN("uci.wireless.wifi-device.",true)) do
    local path = v.path
    local index = match(path,"@([^%.]*)")
    local values = proxy.get(path.."standard")
    if values then
      local standard = values[1].value
      if standard == "bgn" or standard == "bgnax" then
        radios["2"] = index
      else
        radios["5H"] = index
        radios["5L"] = index
      end
    end
  end

  for _,v in ipairs(proxy.getPN("uci.mesh_broker.controller_credentials.",true)) do
    local path = v.path
    local values = proxy.get(path.."ssid",path.."frequency_bands",path.."wpa_psk_key",path.."type")
    if values then
      if values[4].value == "home" then
        credentials[#credentials+1] = {
          ssid = untaint(values[1].value),
          radio = radios[untaint(values[2].value)],
          password = untaint(values[3].value),
        }
      end
    end
  end

  for _,v in ipairs(proxy.getPN("uci.wireless.wifi-iface.",true)) do
    local path = v.path
    local values = proxy.get(path.."ssid",path.."device",path.."network",path.."backhaul")
    if values then
      if values[3].value == "lan" then
        local ssid = untaint(values[1].value)
        local radio = untaint(values[2].value)
        for _,credential in pairs(credentials) do
          if credential.radio == radio and credential.ssid ~= ssid then
            synced = synced + 1
          end
        end
      end
    end
  end
end

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
   ',ui_helper.createCardHeaderNoIcon(T"Wi-Fi Boosters","/modals/easymesh-modal.lp",nil,nil,{icon={class="icon-sitemap"}}),'\
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
      ngx.print(ui_helper.createSimpleLight(content.agent,T(format("EasyMesh Agent %s",state_map(content.agent)))))
      if content.agent == "1" and content.controller == "0" then
        ngx.print(
        '<p class="subinfos">',
          format(N('<strong id="synced_count" %1$s>%2$d SSID</strong> connected','<strong id="synced_count" %1$s>%2$d SSIDs</strong> synced',synced),modal_link,synced),
        '</p>'
        )
      end
      ngx.print(ui_helper.createSimpleLight(content.backhaul_state,T(format("Backhaul %s (5G) %s",content.backhaul_ssid,state_map(content.backhaul_state)))),'\
    </div>\
  </div>\
</div>\
');