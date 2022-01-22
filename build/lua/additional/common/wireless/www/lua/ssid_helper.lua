local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local ipairs,string = ipairs,string
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

function M.getSSIDList()
  local ssid_list = {}
  local fon_count = 0
  local fon_state = 0

  for _,v in ipairs(proxy.getPN("rpc.wireless.ssid.",true)) do
    local path = v.path
    local values = proxy.get(path.."radio",path.."ssid",path.."oper_state",path.."network")
    if values then
      local ssid = values[2].value
      if ssid:sub(1,3) ~= "BH-" then
        if ssid == "Fon WiFi" or ssid == "Fon WiFi-5G" or ssid == "Telstra Air" or ssid == "Telstra Air-5G" then
          fon_count = fon_count + 1
          if values[3].value == 1 and fon_state == 0 then
            fon_state = 1
          end
        else
          local ap_display_name = proxy.get(path.."ap_display_name")[1].value
          local radio_name = values[1].value
          local tx_power_adjust = proxy.get("rpc.wireless.radio.@"..radio_name..".tx_power_adjust")
          local radio_suffix
          local display_ssid
          local isguest
          local sortby
          if radio_name == "radio_2G" then
            radio_suffix = format(" %s","(2.4G)")
          else
            radio_suffix = format(" %s","(5G)")
          end
          if ap_display_name ~= "" then
            display_ssid = ap_display_name..radio_suffix
          else
            local stb = proxy.get(path.."stb")
            if stb and stb[1].value == "1" then
              display_ssid = "IPTV"..radio_suffix
            else
              display_ssid = ssid..radio_suffix
            end
          end
          if values[4].value:sub(1,5) == "Guest" then
            sortby = "yyyyy"
            isguest = "1"
          else
            sortby = display_ssid:lower()
            isguest = "0"
          end
          ssid_list[#ssid_list+1] = {
            id = format("SSID%s",#ssid_list),
            ssid = display_ssid,
            isguest = isguest,
            state = untaint(values[3].value),
            sort = sortby,
            network = untaint(values[4].value),
            tx_power_adjust = tx_power_adjust and tx_power_adjust[1].value or "0",
            iface = match(path,".*@([^.]*)"),
          }
        end
      end
    end
  end

  if fon_count > 0 then
    ssid_list[#ssid_list+1] = {
      id = format("SSID%s",#ssid_list),
      ssid = T"Telstra Air ("..format(N("%s SSID","%s SSIDs"),fon_count,fon_count)..")",
      state = fon_state,
      sort = "zzzzz"
    }
  end

  table.sort(ssid_list,function(a,b) return a.sort < b.sort end)

  return ssid_list
end

function M.getWiFiCardHTML()
  local ssid_list = M.getSSIDList()
  local bs_lan = "disabled"
  local html = {}
  local bs = {}
  local ap = {}
  local hidden = {}
  local wps = {}

  for _,v in ipairs(proxy.getPN("uci.wireless.wifi-ap.",true)) do
    local path = v.path
    local values = proxy.get(path.."iface",path.."bandsteer_id",path.."public",path.."wps_state")
    local iface = untaint(values[1].value)
    local bs_id = values[2].value
    if bs_id == "" or bs_id == "off" then
      bs[iface] = "0"
    else
      local state = proxy.get("uci.wireless.wifi-bandsteer.@"..bs_id..".state")
      if state and state[1].value == "0" then
        bs[iface] = "0"
      else
        bs[iface] = "1"
      end
    end
    ap[iface] = match(path,".*@([^.]*)")
    if values[3].value == "0" then
      hidden[iface] = " style='color:gray'"
    else
      hidden[iface] = ""
    end
    if values[4].value == "1" then
      wps[iface] = "<img src='/img/Pair_green.png' title='WPS Enabled' style='height:12px;width:20px;object-position:top;object-fit:cover;padding-left:4px;'>"
    else
      wps[iface] = ""
    end
  end

  local ssid_attr = {
    span = {
      style = "height:20px;overflow:hidden;",
    },
  }

  for i,v in ipairs(ssid_list) do
    if bs_lan == "disabled" and v.network == "lan" and bs[v.iface] == "1" then
      bs_lan = "enabled"
    end
    if i <= 5 then
      local state
      if v.iface and ap[v.iface] then
        state = format("<span class='modal-link' data-toggle='modal' data-remote='/modals/wireless-qrcode-modal.lp?iface=%s&ap=%s' data-id='wireless-qrcode-modal' title='Click to display QR Code'%s>%s</span>%s",v.iface,ap[v.iface],hidden[v.iface],v.ssid,wps[v.iface])
      else
        state = v.ssid
      end
      if v.tx_power_adjust and v.tx_power_adjust ~= "" and v.tx_power_adjust ~= "0" then
        state = format("%s&nbsp;<span style='color:gray;font-size:xx-small;'>%s dBm</span>",state,v.tx_power_adjust)
      end
      html[#html+1] = ui_helper.createSimpleLight(v.state or "0",state,ssid_attr)
    end
  end

  if bs_lan == "disabled" then
    local multiap_agent = proxy.get("uci.multiap.agent.enabled")
    local multiap_controller = proxy.get("uci.multiap.controller.enabled")
    local multiap_enabled = multiap_agent and multiap_controller and multiap_agent[1].value == "1" and multiap_controller[1].value == "1"
    if multiap_enabled then
      local multiap_cred_path = "uci.multiap.controller_credentials."
      local multiap_cred_data = content_helper.convertResultToObject(multiap_cred_path.."@.",proxy.get(multiap_cred_path))
      local multiap_cred = {}
      for _,v in ipairs(multiap_cred_data) do
        if v.fronthaul == '1' then
          if match(v.frequency_bands,"radio_2G") then
            multiap_cred.primary = v.paramindex
          else
            multiap_cred.secondary = v.paramindex
          end
        end
      end
      local multiap_cred_secondary_path = multiap_cred.secondary and multiap_cred_path.."@"..multiap_cred.secondary
      if multiap_cred_secondary_path then
        local multiap_cred_secondary_state = proxy.get(multiap_cred_secondary_path..".state")
        if multiap_cred_secondary_state and multiap_cred_secondary_state[1].value == "0" then
          bs_lan = "enabled"
        end
      end
    end
  end

  html[#html+1] = ui_helper.createSimpleLight(bs_lan == "enabled" and "1" or "0","Band Steering "..bs_lan)

  return html
end

return M
