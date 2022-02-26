local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local find,format,match = string.find,string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

function M.findBackhaulPaths()
  local ap_path,bh_path,bh_iface
  for _,v in ipairs(proxy.getPN("uci.wireless.wifi-iface.",true)) do
    local backhaul = proxy.get(v.path.."backhaul")
    if backhaul and backhaul[1] and backhaul[1].value == "1" then
      bh_path = v.path
      bh_iface = match(bh_path,"^[^@]+@([^%.]+)%.")
      break
    end
  end

  if bh_path then
    for _,v in ipairs(proxy.getPN("uci.wireless.wifi-ap.",true)) do
      local path = v.path
      local iface = proxy.get(path.."iface")
      if iface and iface[1].value == bh_iface then
        ap_path = path
        break
      end
    end
  end

  return ap_path,bh_path,bh_iface
end

function M.getSSIDList()
  local synced = 0
  local credentials = {}

  for _,v in ipairs(proxy.getPN("uci.multiap.controller_credentials.",true)) do
    local path = v.path
    local values = proxy.get(path.."ssid",path.."frequency_bands",path.."backhaul",path.."wpa_psk_key")
    if values then
      credentials[#credentials+1] = {
        ssid = untaint(values[1].value),
        bands = untaint(values[2].value),
        backhaul = untaint(values[3].value),
        password = untaint(values[4].value),
        controller_ssid = "",
        synced = false,
        multiap_path = path,
        wireless_path = "",
        device = "",
      }
    end
  end

  for _,v in ipairs(proxy.getPN("uci.wireless.wifi-iface.",true)) do
    local path = v.path
    local values = proxy.get(path.."ssid",path.."device",path.."network",path.."backhaul")
    if values then
      local network = untaint(values[3].value)
      if network == "lan" then
        local ssid = untaint(values[1].value)
        local device = untaint(values[2].value)
        local backhaul = untaint(values[4].value)
        for _,credential in pairs(credentials) do
          if credential.synced == false then
            local twoG = find(credential.bands,"radio_2G")
            if credential.ssid ~= ssid and ((backhaul == "1" and credential.backhaul == "1") or (backhaul ~= "1" and credential.backhaul ~= "1" and ((device == "radio_2G" and twoG) or (device == "radio_5G" and not twoG)))) then
              credential.synced = true
              credential.controller_ssid = ssid
              credential.wireless_path = path
              credential.device = device
              synced = synced + 1
            end
          end
        end
      end
    end
  end

  return credentials,synced
end

function M.getBoosterCardHTML(agent_enabled,controller_enabled)
  local content = {
    state5g = "uci.wireless.wifi-device.@radio_5G.state",
  }

  local ap_path,bh_path,bh_iface = M.findBackhaulPaths()
  if ap_path then
    content["ap_iface"] = ap_path.."iface"
    content["ap_state"] = ap_path.."state"
    content["bh_ssid"] = bh_path.."ssid"
  end

  local bandlock = proxy.get("uci.multiap.bandlock.supported")
  if bandlock and bandlock[1].value == "1" then
    bandlock = "1"
    content["bandlock"] = "uci.multiap.bandlock.active"
  end

  content_helper.getExactContent(content)

  local agentStatus,controllerStatus
  local backhaulStatus = nil
  local bandlockStatus = nil
  local boosters = 0

  if agent_enabled == "1" then
    agentStatus = "EasyMesh Agent enabled"
  else
    agentStatus = "EasyMesh Agent disabled"
  end
  if controller_enabled == "1" then
    local path = "Device.Services.X_TELSTRA_MultiAP.Agent."
    local data = content_helper.convertResultToObject(path,proxy.get(path))
    for _,agent in ipairs(data) do
      if agent.MACAddress ~= "" and agent.MACAddress ~= "00:00:00:00:00:00" then
        boosters = boosters + 1
      end
    end
    controllerStatus = "EasyMesh Controller enabled"
  else
    controllerStatus = "EasyMesh Controller disabled"
  end
  if ap_path and content["ap_state"] and content["ap_iface"] == bh_iface and content["bh_ssid"] ~= "" then
    local pattern="Backhaul %s ".."%s %s"
    if content["ap_state"] == "0" then
      backhaulStatus = T(format(pattern,content["bh_ssid"],"(5G)","disabled"))
    elseif content["state5g"] == "0" then
      content["ap_state"] = "2"
      backhaulStatus = T(format(pattern,content["bh_ssid"],"(5G)","radio off"))
    else
      backhaulStatus = T(format(pattern,content["bh_ssid"],"(5G)","enabled"))
    end
  end
  if bandlock == "1" and content["bandlock"] then
    if content["bandlock"] == "1" then
      bandlockStatus = "Band Lock Active"
    else
      bandlockStatus = "Band Lock Inactive"
    end
  end

  local ssids,synced = M.getSSIDList()
  local html = {}

  html[#html+1] = ui_helper.createSimpleLight(controller_enabled,controllerStatus)
  if controller_enabled == "1" then
    local modalLink = 'class="modal-link" data-toggle="modal" data-remote="/modals/wireless-boosters-boosters-modal.lp" data-id="boosters-boosters-modal"'
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format(N("<strong %1$s>%2$d booster</strong> found","<strong %1$s>%2$d boosters</strong> found",boosters),modalLink,boosters)
    html[#html+1] = '</p>'
  end
  html[#html+1] = ui_helper.createSimpleLight(agent_enabled,agentStatus)
  if controller_enabled ~= "1" and synced > 0 then
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format("%d of %d SSIDs synced from controller",synced,#ssids)
    html[#html+1] = '</p>'
  end
  if backhaulStatus then
    html[#html+1] = ui_helper.createSimpleLight(content["ap_state"],backhaulStatus)
  end
  if bandlockStatus then
    html[#html+1] = ui_helper.createSimpleLight(content["bandlock"],bandlockStatus)
  end

  return html
end

return M
