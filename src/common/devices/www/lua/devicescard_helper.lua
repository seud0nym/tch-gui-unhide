local bridged = require("bridgedmode_helper")
local dkjson = require('dkjson')
local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local static_helper = require("ethernet-static-leases_helper")
local ui_helper = require("web.ui_helper")
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local tonumber = tonumber

local device_modal_link = 'class="modal-link" data-toggle="modal" data-remote="modals/device-modal.lp?dev=1" data-id="device-modal"'
local all_devices_modal_link = 'class="modal-link" data-toggle="modal" data-remote="modals/device-modal.lp?connected=All&dev=1" data-id="device-modal"'

local M = {}

function M.getDevicesCardHTML(all)
  local devices_data = {
    numWireless = "sys.hosts.WirelessNumberOfEntries",
    numEthernet = "sys.hosts.EthernetNumberOfEntries",
    activeWireless = "sys.hosts.ActiveWirelessNumberOfEntries",
    activeEthernet = "sys.hosts.ActiveEthernetNumberOfEntries",
  }
  content_helper.getExactContent(devices_data)

  local nAgtDevices = 0
  local multiap_controller_enabled = proxy.get("uci.multiap.controller.enabled")
  local multiap = (multiap_controller_enabled and multiap_controller_enabled[1].value == "1")
  if all and multiap then
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

  local nAPDevices = 0
  if all then
    local _,aps = static_helper.get_dhcp_tags()
    if aps > 0 then
      for _,p in ipairs(proxy.getPN("uci.dhcp.host.",true)) do
        local path = p.path
        local tag = proxy.get(path.."tag")[1].value
        if tag ~= "" then
          local ap = match(untaint(tag),"^AP_([%w_]+)$")
          if ap then
            local ipv4 = proxy.get(path.."ip")
            if ipv4 and ipv4[1].value ~= "" then
              local curl = io.popen(format("curl -qsklm1 --connect-timeout 1 http://%s:59595",ipv4[1].value))
              if curl then
                local json = curl:read("*a")
                local devices = dkjson.decode(json)
                curl:close()
                if devices and type(devices) == "table" then
                  nAPDevices = nAPDevices + #devices
                end
              else
                ngx.log(ngx.ERR,format("curl -qsklm1 --connect-timeout 1 http://%s:59595",ipv4[1].value))
              end
            end
          end
        end
      end
    end
  end

  local nEth = tonumber(devices_data["numEthernet"]) or 0
  local activeEth = tonumber(devices_data["activeEthernet"]) or 0
  local activeWiFi = tonumber(devices_data["activeWireless"]) or 0
  local inactive = ((tonumber(devices_data["numWireless"]) or 0) - activeWiFi) + (nEth - activeEth)
  local localWiFi = (nAgtDevices > 0 or nAPDevices > 0) and "Local" or ""
  if nAgtDevices > 0 and activeEth > nAgtDevices then
    activeEth = activeEth - nAgtDevices
  end
  if nAPDevices > 0 and activeEth > nAPDevices then
    activeEth = activeEth - nAPDevices
  end

  local html = {}

  html[#html+1] = '<span class="simple-desc">'
  html[#html+1] = '<i class="icon-link status-icon"></i>'
  html[#html+1] = format(N('<strong %s>%d Ethernet device</strong>','<strong %s>%d Ethernet devices</strong>',activeEth),device_modal_link,activeEth)
  html[#html+1] = '</span>'
  html[#html+1] = '<span class="simple-desc">'
  html[#html+1] = '<i class="icon-wifi status-icon"></i>'
  html[#html+1] = format(N('<strong %s>%d %s Wi-Fi device</strong>','<strong %s>%d %s Wi-Fi devices</strong>',activeWiFi),device_modal_link,activeWiFi,localWiFi)
  html[#html+1] = '</span>'
  if all then
    if multiap and nAgtDevices > 0 then
      html[#html+1] = '<span class="simple-desc">'
      html[#html+1] = '<i class="icon-sitemap status-icon"></i>'
      html[#html+1] = format(N('<strong %s>%d Booster Wi-Fi device</strong>','<strong %s>%d Booster Wi-Fi devices</strong>',nAgtDevices),device_modal_link,nAgtDevices)
      html[#html+1] = '</span>'
    end
    if nAPDevices > 0 then
      html[#html+1] = '<span class="simple-desc">'
      html[#html+1] = '<i class="icon-sitemap status-icon"></i>'
      html[#html+1] = format(N('<strong %s>%d AP Wi-Fi device</strong>','<strong %s>%d AP Wi-Fi devices</strong>',nAPDevices),device_modal_link,nAPDevices)
      html[#html+1] = '</span>'
    end
  end
  html[#html+1] = '<span class="simple-desc">'
  html[#html+1] = '<i class="icon-eye-close" style="color:grey"></i>'
  html[#html+1] = format(N('<strong %s>%d Inactive device</strong>','<strong %s>%d Inactive devices</strong>',inactive),all_devices_modal_link,inactive)
  html[#html+1] = '</span>'
  -- Do NOT remove this comment! Insert WireGuard peer count here
  if not bridged.isBridgedMode() then
    local bwstats_enabled = proxy.get("rpc.gui.bwstats.enabled")
    if bwstats_enabled then
      local bwstats_template = '<span class="modal-link" data-toggle="modal" data-remote="modals/device-bwstats-modal.lp" data-id="device-bwstats-modal">Bandwidth Monitor %s</span>'
      if bwstats_enabled[1].value == "1" then
        html[#html+1] = ui_helper.createSimpleLight("1",T(format(bwstats_template,"enabled")))
      else
        html[#html+1] = ui_helper.createSimpleLight("0",T(format(bwstats_template,"disabled")))
      end
    end
  end

  return html
end

return M
