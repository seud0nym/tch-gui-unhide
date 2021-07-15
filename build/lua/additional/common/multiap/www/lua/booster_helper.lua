local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local find, format, untaint = string.find, string.format, string.untaint 
local proxy = require("datamodel")

local M = {}

function M.findBackhaulAPPath()
  for _,v in ipairs(proxy.getPN("uci.wireless.wifi-ap.", true)) do
    local path = v.path
    local iface = proxy.get(path .. "iface")
    if iface and iface[1].value == "wl1_2" then
      return path
    end
  end
  return nil
end

function M.getSSIDList()
  local synced = 0
  local credentials = {}
  local v
  
  for _,v in ipairs(proxy.getPN("uci.multiap.controller_credentials.", true)) do
    local path = v.path
    local values = proxy.get(path .. "ssid", path .. "frequency_bands", path .. "backhaul", path .. "wpa_psk_key")
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

  for _,v in ipairs(proxy.getPN("uci.wireless.wifi-iface.", true)) do
    local path = v.path
    local values = proxy.get(path .. "ssid", path .. "device", path .. "network", path .. "backhaul")
    if values then
      local network = untaint(values[3].value)
      if network == "lan" then
        local ssid = untaint(values[1].value)
        local device = untaint(values[2].value)
        local backhaul = untaint(values[4].value)
        local credential
        for _,credential in pairs(credentials) do
          if credential.synced == false then
            local twoG = find(credential.bands, "radio_2G")
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

  return credentials, synced
end

function M.getBoosterCardHTML(agent_enabled, controller_enabled, modalPath)
  local content = {
    state5g = "uci.wireless.wifi-device.@radio_5G.state",
  }

  local appath = M.findBackhaulAPPath()
  if appath then
    content["apiface"] = appath .. "iface"
    content["apstate"] = appath .. "state"
    content["wl1_2ssid"] = "uci.wireless.wifi-iface.@wl1_2.ssid"
  end

  content_helper.getExactContent(content)

  local agentStatus, controllerStatus
  local backhaulStatus = nil
  local boosters = 0
  local modalPath
  
  if agent_enabled == "1" then
    agentStatus = "Multi-AP Agent enabled"
  else
    agentStatus = "Multi-AP Agent disabled"
  end
  if controller_enabled == "1" then
    local path = "Device.Services.X_TELSTRA_MultiAP.Agent."
    local data = content_helper.convertResultToObject(path, proxy.get(path))
    for key, agent in ipairs(data) do
      if agent.MACAddress ~= "" and agent.MACAddress ~= "00:00:00:00:00:00" then
        boosters = boosters + 1
      end
    end
    controllerStatus = "Multi-AP Controller enabled"
    modalPath = "/modals/wireless-boosters-status-modal.lp"
  else
    controllerStatus = "Multi-AP Controller disabled"
    modalPath = "/modals/wireless-boosters-modal.lp"
  end
  if appath and content["apstate"] and content["apiface"] == "wl1_2" and content["wl1_2ssid"] ~= "" then
    if content["apstate"] == "0" then
      backhaulStatus = T"Backhaul " .. content["wl1_2ssid"] .. " (5G) disabled"
    elseif content["state5g"] == "0" then
      content["apstate"] = "2"
      backhaulStatus = T"Backhaul " .. content["wl1_2ssid"] .. " (5G) radio off"
    else
      backhaulStatus = T"Backhaul " .. content["wl1_2ssid"] .. " (5G) enabled"
    end
  end

  local ssids, synced = M.getSSIDList()
  local html = {}
  
  html[#html+1] = ui_helper.createSimpleLight(controller_enabled, controllerStatus)
  if controller_enabled == "1" then
    local modalLink = format('class="modal-link" data-toggle="modal" data-remote="%s" data-id="booster-modal"',modalPath)
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format(N("<strong %1$s>%2$d booster</strong> found","<strong %1$s>%2$d boosters</strong> found",boosters),modalLink,boosters)
    html[#html+1] = '</p>'
  end
  html[#html+1] = ui_helper.createSimpleLight(agent_enabled, agentStatus)
  if controller_enabled ~= "1" and synced > 0 then
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format("%d of %d SSIDs synced from controller",synced,#ssids)
    html[#html+1] = '</p>'
  end
  if backhaulStatus then
    html[#html+1] = ui_helper.createSimpleLight(content["apstate"], backhaulStatus)
  end

  return html, modalPath
end

return M
  