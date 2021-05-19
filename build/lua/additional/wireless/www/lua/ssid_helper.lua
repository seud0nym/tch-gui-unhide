local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local ipairs, string = ipairs, string
local format = string.format

local M = {}

function M.getSSIDList()
  local ssid_list = {}
  local fon_count = 0
  local fon_state = 0
  local v

  for _,v in ipairs(proxy.getPN("rpc.wireless.ssid.", true)) do
    local path = v.path
    local values = proxy.get(path .. "radio", path .. "ssid", path .. "oper_state", path .. "network")
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
          if values[4].value:sub(1,5) == "Guest" then
            sortby = "yyyyy"
          else
            sortby = display_ssid:lower()
          end
          ssid_list[#ssid_list+1] = {
            id = format("SSID%s", #ssid_list),
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
      id = format("SSID%s", #ssid_list),
      ssid = T"Telstra Air (" .. format(N("%s SSID","%s SSIDs"),fon_count,fon_count) .. ")",
      state = fon_state,
      sort = "zzzzz"
    }
  end
  
  table.sort(ssid_list, function(a,b) return a.sort < b.sort end)

  return ssid_list
end

function M.getWiFiCardHTML() 
  local ssid_list = M.getSSIDList()
  local html = {}

  for i,v in ipairs(ssid_list) do
    if i <= 5 then
      local attributes = {
        light = {
          id = v.id
        }
      }
      html[#html+1] = ui_helper.createSimpleLight(v.state or "0", v.ssid, attributes)
    end
  end

  return html
end

return M
