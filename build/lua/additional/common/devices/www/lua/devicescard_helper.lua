local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local format = string.format
local tonumber = tonumber

local modal_link='class="modal-link" data-toggle="modal" data-remote="modals/device-modal.lp" data-id="device-modal"'

local M = {}

function M.getDevicesCardHTML() 
  local devices_data = {
    numWireless = "sys.hosts.ActiveWirelessNumberOfEntries",
    numEthernet = "sys.hosts.ActiveEthernetNumberOfEntries",
  }
  content_helper.getExactContent(devices_data)

  local nAgtDevices = 0
  local multiap_controller_enabled = proxy.get("uci.multiap.controller.enabled")
  local multiap = (multiap_controller_enabled and multiap_controller_enabled[1].value == "1")
  if multiap then
    local agents = proxy.get("Device.Services.X_TELSTRA_MultiAP.Controller.MultiAPAgentNumberOfEntries")
    if agents and tonumber(agents[1].value) > 0 then
        for i = 1,tonumber(agents[1].value),1 do
        local devices = tonumber(format("%s",proxy.get("Device.Services.X_TELSTRA_MultiAP.Agent."..i..".AssociatedDeviceNumberOfEntries")[1].value) or 0)
        if devices then
          nAgtDevices = nAgtDevices + devices
        end
      end
    end
  end

  local nEth = tonumber(devices_data["numEthernet"]) or 0
  local nWiFi = tonumber(devices_data["numWireless"]) or 0
  if multiap and nAgtDevices > 0 and nEth > 0 then
    nEth = nEth - nAgtDevices
  end

  local html = {}

  html[#html+1] = '<span class="simple-desc">'
  html[#html+1] = '<i class="icon-link status-icon"></i>'
  html[#html+1] = format(N('<strong %1$s>%2$d ethernet device</strong> connected','<strong %1$s>%2$d ethernet devices</strong> connected',nEth),modal_link,nEth)
  html[#html+1] = '</span>'
  html[#html+1] = '<span class="simple-desc">'
  html[#html+1] = '<i class="icon-wifi status-icon"></i>'
  html[#html+1] = format(N('<strong %1$s>%2$d Wi-Fi device</strong> connected','<strong %1$s>%2$d Wi-Fi devices</strong> connected',nWiFi),modal_link,nWiFi)
  html[#html+1] = '</span>'
  if multiap then
    html[#html+1] = '<span class="simple-desc">'
    html[#html+1] = '<i class="icon-sitemap status-icon"></i>'
    html[#html+1] = format(N('<strong %1$s>%2$d Wi-Fi device</strong> booster connected','<strong %1$s>%2$d Wi-Fi devices</strong> booster connected',nAgtDevices),modal_link,nAgtDevices)
    html[#html+1] = '</span>'
  end

  return html
end

return M
