local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local floor = math.floor
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
    tx_bytes = "rpc.network.interface.@" .. wan_intf .. ".tx_bytes",
    rx_bytes = "rpc.network.interface.@" .. wan_intf .. ".rx_bytes",
  }
  content_helper.getExactContent(content_rpc)
  local uptime = content_helper.readfile("/proc/uptime","number",floor)

  local html = {}
  if wan_data["wan_ifname"] and (wan_data["wan_ifname"] == "ptm0" or wan_data["wan_ifname"] == "atmwan") then
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
  if wan_data["wan_ifname"] and string.find(wan_data["wan_ifname"],"eth") then
    if wan_data["ethwan_status"] == "up" then
      html[#html+1] = ui_helper.createSimpleLight("1", "Ethernet connected")
    else
      html[#html+1] = ui_helper.createSimpleLight("4", "Ethernet disconnected")
    end
  end
  if mobiled_state["mob_session_state"] == "connected" then
    html[#html+1] = ui_helper.createSimpleLight("1", "Mobile Internet connected")
  end
  if tonumber(content_rpc.rx_bytes) and tonumber(content_rpc.tx_bytes) and tonumber(uptime) then
    html[#html+1] = format('<span class="simple-desc" style="padding-top:10px"><i class="icon-cloud-upload icon-small status-icon"></i> <span id="broadband-card-upload">%s</span> <i class="icon-cloud-download icon-small status-icon"></i> <span id="broadband-card-download">%s</span> <span id="broadband-card-daily-average">%s</span>/<i>d</i></span>', 
      bytes2string(content_rpc.tx_bytes), bytes2string(content_rpc.rx_bytes), bytes2string((content_rpc.rx_bytes+content_rpc.tx_bytes)/(uptime/86400)))
  end

  return html
end

return M
