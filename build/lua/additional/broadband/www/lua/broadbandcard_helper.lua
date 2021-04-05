local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local socket = require("socket")
local ui_helper = require("web.ui_helper")

local floor, find, format, untaint = math.floor, string.find, string.format, string.untaint
local tonumber = tonumber
local TGU_MbPS = ngx.shared.TGU_MbPS

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
      return format("%.1f", gb) .. "<small>GB</small>"
    elseif mb >= 1 then
      return format("%.1f", mb) .. "<small>MB</small>"
    elseif kb >= 1 then
      return format("%.1f", kb) .. "<small>KB</small>"
    else
      return format("%d", s_bytes) .. "<small>B</small>"
    end
  end
end

local function getCurrentWANInterface()
  local wan_intf = "wan"
  local content_wan = {
    wwan_ipaddr = "rpc.network.interface.@wwan.ipaddr",
    wwan_ip6addr = "rpc.network.interface.@wwan.ip6addr",
  }
  content_helper.getExactContent(content_wan)
  if content_wan.wwan_ipaddr:len() ~= 0 or content_wan.wwan_ip6addr:len() ~= 0 then
    wan_intf = "wwan"
  end
  return wan_intf
end

function M.getThroughputHTML() 
  local wan_data = {
    lan_rx = "rpc.network.interface.@lan.rx_bytes",
    lan_tx = "rpc.network.interface.@lan.tx_bytes",
    wan_rx = "rpc.network.interface.@wan.rx_bytes",
    wan_tx = "rpc.network.interface.@wan.tx_bytes",
    wwan_rx = "rpc.network.interface.@wwan.rx_bytes",
    wwan_tx = "rpc.network.interface.@wwan.tx_bytes",
  }
  content_helper.getExactContent(wan_data)

  local key, value, ok, err
  for key, value in pairs(wan_data) do
    local is = tonumber(value)
    local now = socket.gettime()
    local from = TGU_MbPS:get(key.."_time")
    if from then
      local elapsed = now - from
      local was = TGU_MbPS:get(key.."_bytes")
      ok, err = TGU_MbPS:safe_set(key.."_mbps", (is - was) / elapsed * 0.000008)
      if not ok then
        ngx.log(ngx.ERR, "Failed to store current Mb/s into ", key, "_mbps: ", err)
      end
    end
    ok, err = TGU_MbPS:safe_set(key.."_time", now)
    if not ok then
      ngx.log(ngx.ERR, "Failed to store current time into ", key, "_time: ", err)
    end
    ok, err = TGU_MbPS:safe_set(key.."_bytes", is)
    if not ok then
      ngx.log(ngx.ERR, "Failed to store ", is, " into ", key, "_bytes: ", err)
    end
  end

  local wan_intf = getCurrentWANInterface()
  return 
    format("%.2f Mb/s <b>&uarr;</b><br>%.2f Mb/s <b>&darr;</b></span>", TGU_MbPS:get(wan_intf.."_tx_mbps") or 0, TGU_MbPS:get(wan_intf.."_rx_mbps") or 0),
    format("%.2f Mb/s <b>&uarr;</b><br>%.2f Mb/s <b>&darr;</b></span>", TGU_MbPS:get("lan_tx_mbps") or 0, TGU_MbPS:get("lan_rx_mbps") or 0)
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

  local content_rpc = {
    rx_bytes = 0,
    tx_bytes = 0,
    total_bytes = 0,
  }
  local wan_intf = getCurrentWANInterface()
  local wan_ifname = wan_data["wan_ifname"]
  local rpc_ifname = proxy.get("rpc.network.interface.@"..wan_intf..".ifname")
  local path = format("%s_%s", os.date("%F"), rpc_ifname[1].value)
  local rx_bytes = proxy.get("rpc.gui.traffichistory.usage.@"..path..".rx_bytes")
  if rx_bytes then
    content_rpc.rx_bytes = rx_bytes[1].value
    content_rpc.tx_bytes = proxy.get("rpc.gui.traffichistory.usage.@"..path..".tx_bytes")[1].value
    content_rpc.total_bytes = proxy.get("rpc.gui.traffichistory.usage.@"..path..".total_bytes")[1].value
  else
    content_rpc.rx_bytes = tonumber(proxy.get("rpc.network.interface.@" .. wan_intf .. ".rx_bytes")[1].value)
    content_rpc.tx_bytes = tonumber(proxy.get("rpc.network.interface.@" .. wan_intf .. ".tx_bytes")[1].value)
    content_rpc.total_bytes = content_rpc.rx_bytes + content_rpc.tx_bytes
  end

  local html = {}
  local up = false
  if wan_ifname and (find(wan_ifname,"ptm") or find(wan_ifname,"atm")) then
    if wan_data["dsl_status"] == "Up" then
      up = true
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
  if wan_ifname and find(wan_ifname,"eth") then
    up = true
    if wan_data["ethwan_status"] == "up" then
      html[#html+1] = ui_helper.createSimpleLight("1", "Ethernet connected")
    else
      html[#html+1] = ui_helper.createSimpleLight("4", "Ethernet disconnected")
    end
  end
  local vlanid = string.match(wan_ifname, ".*%.(%d+)")
  if not vlanid then
    local vid = proxy.get("uci.network.device.@".. wan_ifname .. ".vid")
    if vid and vid[1].value ~= "" then
      vlanid = vid[1].value
    end
  end
  if vlanid then
    local vlanifname = proxy.get("uci.network.device.@".. wan_ifname .. ".ifname")
    if vlanifname and vlanifname[1].value == "" then
      html[#html+1] = ui_helper.createSimpleLight("2", "VLAN "..vlanid.." disabled")
    elseif up then
      html[#html+1] = ui_helper.createSimpleLight("1", "VLAN "..vlanid.." active")
    else
      html[#html+1] = ui_helper.createSimpleLight("0", "VLAN "..vlanid.." inactive")
    end
  else
    html[#html+1] = ui_helper.createSimpleLight("0", "No VLAN defined")
  end
  if mobiled_state["mob_session_state"] == "connected" then
    html[#html+1] = ui_helper.createSimpleLight("1", "Mobile Internet connected")
  end
  html[#html+1] = format('<span class="simple-desc modal-link" data-toggle="modal" data-remote="/modals/broadband-usage-modal.lp" data-id="bb-usage-modal" style="padding-top:10px"><span class="icon-small status-icon">&udarr;</span>%s&ensp;<i class="icon-cloud-upload status-icon"></i> %s&ensp;<i class="icon-cloud-download status-icon"></i> %s</span>', 
    bytes2string(content_rpc.total_bytes), bytes2string(content_rpc.tx_bytes), bytes2string(content_rpc.rx_bytes))

  return html
end

return M
