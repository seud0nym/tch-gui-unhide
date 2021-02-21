local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local floor = math.floor
local find = string.find
local format = string.format
local tonumber = tonumber

local M = {}

local function bytes2string(s_bytes)
  if s_bytes=="" then
    return "0<small>B</small>"
  else
    local bytes = tonumber(s_bytes)
    local kb = bytes/1024
    local mb = kb/1024
    local gb = mb/1024
    if gb >= 1 then
      return format("%.2f", gb) .. "<small>GB</small>"
    elseif mb >= 1 then
      return format("%.2f", mb) .. "<small>MB</small>"
    elseif kb >= 1 then
      return format("%.2f", kb) .. "<small>KB</small>"
    else
      return format("%d", s_bytes) .. "<small>B</small>"
    end
  end
end

function M.getBroadbandCardHTML() 
  -- wan status data
  local wan_data = {
    wan_ifname        = "uci.network.interface.@wan.ifname",
    dsl_status        = "sys.class.xdsl.@line0.Status",
    dsl_linerate_up   = "sys.class.xdsl.@line0.UpstreamCurrRate",
    dsl_linerate_down = "sys.class.xdsl.@line0.DownstreamCurrRate",
    ethwan_status     = "sys.eth.port.@eth4.status",
    dsl0_enabled      = "uci.xdsl.xdsl.@dsl0.enabled",
  }
  content_helper.getExactContent(wan_data)

  local mobiled_state = {
    mob_session_state = "rpc.mobiled.device.@1.network.sessions.@1.session_state"
  }
  content_helper.getExactContent(mobiled_state)

  local wan_intf ="wan"
  local content_wan = {
    wwan_ipaddr = "rpc.network.interface.@wwan.ipaddr",
    wwan_ip6addr = "rpc.network.interface.@wwan.ip6addr",
  }
  content_helper.getExactContent(content_wan)
  if content_wan.wwan_ipaddr:len() ~= 0 or content_wan.wwan_ip6addr:len() ~= 0 then
    wan_intf = "wwan"
  end
  local content_rpc = {
    rx_bytes = 0,
    tx_bytes = 0,
    total_bytes = 0,
  }
  local rpc_ifname = proxy.get("rpc.network.interface.@"..wan_intf..".ifname")
  local path = format("%s_%s", os.date("%F"), rpc_ifname[1].value)
  local rx_bytes = proxy.get("rpc.gui.traffichistory.usage.@"..path..".rx_bytes")
  if rx_bytes then
    content_rpc.rx_bytes = rx_bytes[1].value
    content_rpc.tx_bytes = proxy.get("rpc.gui.traffichistory.usage.@"..path..".tx_bytes")[1].value
    content_rpc.total_bytes = proxy.get("rpc.gui.traffichistory.usage.@"..path..".total_bytes")[1].value
  end

  local html = {}
  if wan_data["wan_ifname"] and (find(wan_data["wan_ifname"],"ptm0") or find(wan_data["wan_ifname"],"atm")) then
    if wan_data["dsl_status"] == "Up" then
      html[#html+1] = ui_helper.createSimpleLight("1", "DSL connected")
      -- After disabling broadband the page immediately refreshes. At this time the state is still up but the line
      -- rate is already cleared.
      local rate_up = tonumber(wan_data["dsl_linerate_up"])
      local rate_down = tonumber(wan_data["dsl_linerate_down"])
      if rate_up and rate_down then
        rate_up = floor(rate_up / 10) / 100
        rate_down = floor(rate_down / 10) / 100
        html[#html+1] = format('<p class="bbstats"><i class="icon-upload icon-small gray"></i> %.2f<small>Mbps</small> <i class="icon-download icon-small gray"></i> %.2f<small>Mbps</small></p>', rate_up, rate_down)
      end
    elseif wan_data["dsl_status"] == "NoSignal" then
      html[#html+1] = ui_helper.createSimpleLight("4", "DSL disconnected")
    elseif wan_data["dsl0_enabled"] == "0" then
      html[#html+1] = ui_helper.createSimpleLight("0", "DSL disabled")
    else
      html[#html+1] = ui_helper.createSimpleLight("2", "DSL connecting")
    end
  end
  if wan_data["wan_ifname"] and find(wan_data["wan_ifname"],"eth") then
    if wan_data["ethwan_status"] == "up" then
      html[#html+1] = ui_helper.createSimpleLight("1", "Ethernet connected")
    else
      html[#html+1] = ui_helper.createSimpleLight("4", "Ethernet disconnected")
    end
  end
  if mobiled_state["mob_session_state"] == "connected" then
    html[#html+1] = ui_helper.createSimpleLight("1", "Mobile Internet connected")
  end
  html[#html+1] = format('<span class="simple-desc modal-link" data-toggle="modal" data-remote="/modals/broadband-usage-modal.lp" data-id="bb-usage-modal" style="padding-top:10px"><span class="icon-small status-icon">&udarr;</span> %s&ensp;<i class="icon-cloud-upload status-icon"></i> %s&ensp;<i class="icon-cloud-download status-icon"></i> %s</span>', 
    bytes2string(content_rpc.total_bytes), bytes2string(content_rpc.tx_bytes), bytes2string(content_rpc.rx_bytes))

  return html
end

return M
