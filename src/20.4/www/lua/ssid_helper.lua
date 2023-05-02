local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local ipairs,pairs,string = ipairs,pairs,string
local format,match,sub = string.format,string.match,string.sub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

local function fetch_by_cred(paramindex,radios)
  local radio,ssid,ishidden,isguest,state,sortby,wps_state
  ssid = untaint(proxy.get(format("uci.mesh_broker.controller_credentials.@%s.ssid",paramindex))[1].value)
  state = untaint(proxy.get(format("uci.mesh_broker.controller_credentials.@%s.state",paramindex))[1].value)

  local frequency = sub(untaint(proxy.get(format("uci.mesh_broker.controller_credentials.@%s.frequency_bands",paramindex))[1].value),1,1)
  for r,v in pairs(radios) do
    if v.frequency == frequency then
      radio = r
      break
    end
  end

  local password = untaint(proxy.get(format("uci.mesh_broker.controller_credentials.@%s.wpa_psk_key",paramindex))[1].value)
  for _,profile in ipairs(proxy.getPN("rpc.X_AIRTIES_Obj.MultiAPController.SSIDProfile.",true)) do
    local path = profile.path
    local profile_ssid = proxy.get(path.."SSID")
    if profile_ssid and profile_ssid[1].value == ssid and password == proxy.get(path.."KeyPassphrase")[1].value then
      ishidden = proxy.get(path.."Hide")[1].value == "true" and "0" or "1"
      wps_state = proxy.get(path.."WPS")[1].value == "true" and "1" or "0"
      break
    end
  end

  if proxy.get(format("uci.mesh_broker.controller_credentials.@%s.type",paramindex))[1].value == "guest" then
    sortby = "zzzz"
    isguest = "1"
  else
    sortby = ssid:lower()
    isguest = "0"
  end
  return radio,ssid,ishidden,isguest,state,sortby,wps_state
end

local function fetch_by_intf(paramindex)
  local radio,ssid,ishidden,isguest,state,sortby,wps_state
  local ap_display_name = untaint(proxy.get(format("rpc.wireless.ssid.@%s.ap_display_name",paramindex))[1].value)
  if ap_display_name ~= "" then
    ssid = ap_display_name
  else
    ssid = untaint(proxy.get(format("uci.wireless.wifi-iface.@%s.ssid",paramindex))[1].value)
  end
  radio = untaint(proxy.get(format("rpc.wireless.ssid.@%s.radio",paramindex))[1].value)
  state = untaint(proxy.get(format("rpc.wireless.ap.@%s.oper_state",paramindex))[1].value)
  ishidden = untaint(proxy.get(format("uci.wireless.wifi-iface.@%s.public",paramindex))[1].value)
  wps_state = untaint(proxy.get(format("rpc.wireless.ap.@%s.wps.admin_state",paramindex))[1].value)
  if proxy.get(format("uci.wireless.wifi-iface.@%s.network",paramindex))[1].value == "lan" then
    sortby = ssid:lower()
    isguest = "0"
  else
    sortby = "zzzz"
    isguest = "1"
  end
  return radio,ssid,ishidden,isguest,state,sortby,wps_state,paramindex
end

function M.isMultiAPEnabled()
  local multiap_state = {
    agent = "uci.wireless.map-agent.@agent.state",
    controller = "uci.mesh_broker.meshapp.@mesh_common.controller_enabled",
    meshbrokerState = "uci.mesh_broker.global.@mesh_broker.enable"
  }
  content_helper.getExactContent(multiap_state)
  return multiap_state and multiap_state.controller == "1" and multiap_state.meshbrokerState == "1" and (multiap_state.agent == "1" or multiap_state.agent == "0")
end

function M.getRadios()
  local radios = {}
  local multiap_enabled = M.isMultiAPEnabled()

  for _,v in ipairs(proxy.getPN("rpc.wireless.radio.", true)) do
    local radio = match(v.path, "rpc%.wireless%.radio%.@([^%.]+)%.")
    if radio then
      local values = proxy.get(v.path.."band",v.path.."bandwidth",v.path.."channel",v.path.."tx_power_adjust",v.path.."admin_state")
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
          frequency = sub(untaint(values[1].value),1,1),
          info = info,
          ssid = {},
        }
      end
    end
  end

  for _,network in ipairs(proxy.getPN("uci.web.network.",true)) do
    local path = multiap_enabled and network.path.."cred." or network.path.."intf."
    for _,paramindex in ipairs(proxy.get(path)) do
      local radio,ssid,ishidden,isguest,state,sortby,wps_state,iface
      if multiap_enabled then
        radio,ssid,ishidden,isguest,state,sortby,wps_state = fetch_by_cred(paramindex.value,radios)
        iface = untaint(proxy.get(paramindex.path:gsub("%.cred%.",".intf.").."value")[1].value)
      else
        radio,ssid,ishidden,isguest,state,sortby,wps_state,iface = fetch_by_intf(paramindex.value)
      end
      if radio then
        if not radios[radio] then
          radios[radio] = {
            admin_state = "0",
            frequency = "Unknown",
            info = "",
            ssid = {},
          }
        end
        radios[radio]["ssid"][#radios[radio]["ssid"]+1] = {
          id = format("SSID%s",#radios[radio]["ssid"]),
          ssid = ssid,
          ishidden = ishidden,
          isguest = isguest,
          state = state,
          sort = sortby,
          wps_state = wps_state,
          iface = iface,
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
  local aps = {}
  for _,radio in pairs(M.getRadios()) do
    local band
    if radio.frequency == "5" then
      band = " (5GHz)"
    else
      band = " (2.4GHz)"
    end
    for _,v in ipairs(radio.ssid) do
      aps[#aps+1] = { v.iface, T(v.ssid..band) }
    end
  end
  table.sort(aps,function(a,b) return a[2] < b[2] end)
  return aps
end

function M.getWiFiCardHTML()
  local html = {}
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

  for _,radio in pairs(M.getRadios()) do
    html[#html+1] = ui_helper.createSimpleLight(radio.admin_state,radio.info,band_attr)

    for i,v in ipairs(radio.ssid) do
      if i <= 2 then
        local hidden
        if v.ishidden == "0" or v.ishidden == "true" then
          hidden = " style='color:gray'"
        else
          hidden = ""
        end
        local state
        if v.iface then
          state = format("<span class='modal-link' data-toggle='modal' data-remote='/modals/wireless-qrcode-modal.lp?iface=%s&ap=%s' data-id='wireless-qrcode-modal' title='Click to display QR Code'%s>%s</span>",v.iface,v.iface,hidden,v.ssid)
        else
          state = format("<span%s>%s</span>",hidden,v.ssid)
        end
        if v.wps_state == "1" or v.wps_state == "true" then
          state = state.."<img src='/img/Pair_green.png' title='WPS enabled' style='height:12px;width:18px;object-position:top;object-fit:cover;padding-left:4px;'>"
        end
        html[#html+1] = ui_helper.createSimpleLight(v.state or "0",state,ssid_attr)
      end
    end
  end

  return html
end

return M
