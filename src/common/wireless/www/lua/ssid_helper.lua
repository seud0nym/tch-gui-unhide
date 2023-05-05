local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local ipairs,string = ipairs,string
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

function M.isMultiAPEnabled()
  local multiap_state = {
    agent = "uci.multiap.agent.enabled",
    controller = "uci.multiap.controller.enabled",
  }
  content_helper.getExactContent(multiap_state)
  return multiap_state and multiap_state.controller == "1" and multiap_state.agent == "1"
end

function M.getRadios()
  local radios = {}
  for _,v in ipairs(proxy.getPN("rpc.wireless.radio.", true)) do
    local radio = match(v.path, "rpc%.wireless%.radio%.@([^%.]+)%.")
    if radio then
      local values = proxy.get(v.path.."band",v.path.."channel_width",v.path.."channel",v.path.."tx_power_adjust",v.path.."admin_state")
      if values then
        local enabled = untaint(values[5].value)
        local channel = values[3].value
        local info
        if enabled == "0" or channel == "0" then
          info = format("%s disabled",values[1].value)
        else
          local dBm = values[4].value
          info = format("%s Channel %s <sup class='wifi-width'>%s</sup>",values[1].value,channel,values[2].value)
          if dBm ~= "" and dBm ~= "0" then
            info = format("%s <span class='wifi-dBm'>(%s dBm)</span>",info,dBm)
          end
        end
        radios[radio] = {
          admin_state = enabled,
          info = info,
          ssid = {},
        }
      end
    end
  end

  for _,v in ipairs(proxy.getPN("rpc.wireless.ssid.",true)) do
    local path = v.path
    local values = proxy.get(path.."radio",path.."ssid",path.."oper_state",path.."network")
    if values then
      local ssid = untaint(values[2].value)
      if not (ssid:sub(1,3) == "BH-" or ssid == "Fon WiFi" or ssid == "Fon WiFi-5G" or ssid == "Telstra Air" or ssid == "Telstra Air-5G") then
        local ap_display_name = untaint(proxy.get(path.."ap_display_name")[1].value)
        local radio = untaint(values[1].value)
        local display_ssid
        local isguest
        local sortby
        if ap_display_name ~= "" then
          display_ssid = ap_display_name
        else
          local stb = proxy.get(path.."stb")
          if stb and stb[1].value == "1" then
            display_ssid = "IPTV"
          else
            display_ssid = ssid
          end
        end
        if values[4].value:sub(1,5) == "Guest" then
          sortby = "yyyyy"
          isguest = "1"
        else
          sortby = display_ssid:lower()
          isguest = "0"
        end
        if not radios[radio] then
          radios[radio] = {
            admin_state = "0",
            info = "",
            ssid = {},
          }
        end
        radios[radio]["ssid"][#radios[radio]["ssid"]+1] = {
          id = format("SSID%s",#radios[radio]["ssid"]),
          ssid = display_ssid,
          isguest = isguest,
          state = untaint(values[3].value),
          sort = sortby,
          network = untaint(values[4].value),
          tx_power_adjust = radios[radio]["tx_power_adjust"],
          iface = match(path,".*@([^.]*)"),
        }
      end
    end
  end

  for _,radio in pairs(radios) do
    table.sort(radio.ssid,function(a,b) return a.sort < b.sort end)
  end

  return radios
end

function M.getAccessPoints()
  local ap = {}
  for _,v in ipairs(proxy.getPN("uci.wireless.wifi-ap.",true)) do
    local path = v.path
    local values = proxy.get(path.."iface")
    local iface = untaint(values[1].value)
    ap[iface] = match(path,".*@([^.]*)")
  end

  local aps = {}
  for _,radio in pairs(M.getRadios()) do
    local band
    if radio.frequency == "5" then
      band = " (5GHz)"
    else
      band = " (2.4GHz)"
    end
    for _,v in ipairs(radio.ssid) do
      aps[#aps+1] = { ap[v.iface], T(v.ssid..band) }
    end
  end
  table.sort(aps,function(a,b) return a[2] < b[2] end)
  return aps
end

function M.getWiFiCardHTML()
  local html = {}
  local ap = {}
  local hidden = {}
  local wps = {}
  local band_attr = {
    span = {
      class = "wifi-band",
    },
  }
  local ssid_attr = {
    span = {
      class = "ssid-status",
    },
  }

  for _,v in ipairs(proxy.getPN("uci.wireless.wifi-ap.",true)) do
    local path = v.path
    local values = proxy.get(path.."iface",path.."public",path.."wps_state")
    local iface = untaint(values[1].value)
    ap[iface] = match(path,".*@([^.]*)")
    if values[2].value == "0" then
      hidden[iface] = " style='color:gray'"
    else
      hidden[iface] = ""
    end
    if values[3].value == "1" then
      wps[iface] = "<img src='/img/Pair_green.png' title='WPS Enabled' style='height:12px;width:20px;object-position:top;object-fit:cover;padding-left:4px;'>"
    else
      wps[iface] = ""
    end
  end

  for _,radio in pairs(M.getRadios()) do
    html[#html+1] = ui_helper.createSimpleLight(radio.admin_state,radio.info,band_attr)

    for i,v in ipairs(radio["ssid"]) do
      if i <= 2 then
        local state
        if v.iface and ap[v.iface] then
          state = format("<span class='modal-link' data-toggle='modal' data-remote='/modals/wireless-qrcode-modal.lp?iface=%s&ap=%s' data-id='wireless-qrcode-modal' title='Click to display QR Code'%s>%s</span>%s",v.iface,ap[v.iface],hidden[v.iface],v.ssid,wps[v.iface])
        else
          state = v.ssid
        end
        html[#html+1] = ui_helper.createSimpleLight(v.state or "0",state,ssid_attr)
      end
    end
  end

  return html
end

return M
