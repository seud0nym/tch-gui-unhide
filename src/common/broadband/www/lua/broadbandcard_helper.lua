local bridged = require("bridgedmode_helper")
local common = require("common_helper")
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")

local floor,find,format,match,toupper = math.floor,string.find,string.format,string.match,string.upper
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local tonumber = tonumber

local M = {}

local function alignNumber(value,description,name)
  local html,text = common.bytes2string(value)
  return format('<span class="bytes" title="%s %s %s today">%s</span>',text,description,name,html)
end

local function getInterfaceStats(intf,icon)
  local content = {
    ip4addr = "rpc.network.interface.@"..intf..".ipaddr",
    ip6addr = "rpc.network.interface.@"..intf..".ip6addr",
  }
  content_helper.getExactContent(content)
  if content.ip4addr:len() ~= 0 or content.ip6addr:len() ~= 0 then
    local result = {
      name = intf,
      icon = icon,
      rx_bytes = 0,
      tx_bytes = 0,
      total_bytes = 0,
    }
    local rpc_ifname = proxy.get("rpc.network.interface.@"..intf..".ifname")
    if rpc_ifname then
      local path = format("%s_%s",os.date("%F"),rpc_ifname[1].value)
      local rx_bytes = proxy.get("rpc.gui.traffichistory.usage.@"..path..".rx_bytes")
      if rx_bytes then
        result.rx_bytes = tonumber(rx_bytes[1].value)
        result.tx_bytes = tonumber(proxy.get("rpc.gui.traffichistory.usage.@"..path..".tx_bytes")[1].value)
      else
        result.rx_bytes = tonumber(proxy.get("rpc.network.interface.@"..intf..".rx_bytes")[1].value)
        result.tx_bytes = tonumber(proxy.get("rpc.network.interface.@"..intf..".tx_bytes")[1].value)
      end
      result.total_bytes = result.rx_bytes + result.tx_bytes
      return result
    end
  end
  return nil
end

local function getActiveWANInterfaces(wansensing,mobile,wired)
  local intf = { }
  local wan,wwan

  if wired or wansensing == "1" then
    wan = getInterfaceStats("wan","link")
  end
  if mobile or not wired then
    wwan = getInterfaceStats("wwan","signal")
  else
    local perm_wwan = proxy.get("uci.network.interface.@wwan.enabled")
    if perm_wwan and perm_wwan[1].value == "1" then
      wwan = getInterfaceStats("wwan","signal") or {
        name = "wwan",
        icon = "signal",
        rx_bytes = 0,
        tx_bytes = 0,
        total_bytes = 0,
      }
      wwan.permanent = true
    end
  end

  if wan and wwan and not (mobile or wwan.permanent) then
    wan.rx_bytes = wan.rx_bytes + wwan.rx_bytes
    wan.tx_bytes = wan.tx_bytes + wwan.tx_bytes
    wan.total_bytes = wan.total_bytes + wwan.total_bytes
    intf[#intf+1] = wan
  else
    if wan then
      intf[#intf+1] = wan
    end
    if wwan then
      intf[#intf+1] = wwan
    end
  end

  return intf
end

function M.getBroadbandCardHTML(wansensing)
  local html = {}
  local status = "down"
  if bridged.isBridgedMode() then
    local ifnames = {
      iface = "uci.network.interface.@lan.ifname",
    }
    content_helper.getExactContent(ifnames)

    local intf_state = "disabled"
    local intf_state_map = {
      disabled = "Bridge disabled",
      connected = "Bridge connected",
      disconnected = "Bridge not connected",
    }
    local intf_light_map = {
      disabled = "off",
      disconnected = "red",
      connected = "green",
    }

    local connected_iface = "<br>"
    for v in string.gmatch(ifnames.iface,"[^%s]+") do
      local iface = untaint(match(v,"([^%.]+)")) -- try to remove the potential vlan id from the interface name
      local stats = {
        operstate = "sys.class.net.@"..iface..".operstate",
        carrier = "sys.class.net.@"..iface..".carrier",
      }
      content_helper.getExactContent(stats)

      if stats.operstate == "up" then
        if stats.carrier ~= "0" then
          intf_state = "connected"
          connected_iface = connected_iface.." "..iface
        elseif intf_state ~= "connected" then
          intf_state = "disconnected"
        end
      end
    end
    if connected_iface == "<br>" then
      connected_iface = "NONE"
    end

    html[#html+1] = ui_helper.createSimpleLight(nil,intf_state_map[intf_state],{light = {class = intf_light_map[intf_state]}})
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format(T'Connected Interfaces: <strong>%s</strong>',connected_iface)
    html[#html+1] = '</p>'

    status = "bridged"
  else -- not bridge mode
    -- wan status data
    local wan_data = {
      wan_ifname        = "uci.network.interface.@wan.ifname",
      wan_uptime        = "rpc.network.interface.@wan.uptime",
      dsl_status        = "sys.class.xdsl.@line0.Status",
      dsl_linerate_up   = "sys.class.xdsl.@line0.UpstreamCurrRate",
      dsl_linerate_down = "sys.class.xdsl.@line0.DownstreamCurrRate",
      ethwan_status     = "sys.eth.port.@eth4.status",
      dsl0_enabled      = "sys.class.xdsl.@line0.Enable",
    }
    content_helper.getExactContent(wan_data)

    local mobile = false
    local primarywanmode = proxy.get("uci.wansensing.global.primarywanmode")
    if primarywanmode and primarywanmode[1].value == "MOBILE" then
      mobile = true
    end
    local wired = true
    if wansensing == "1" and wan_data["dsl_status"] ~= "Up" and wan_data["ethwan_status"] ~= "up" then
      wired = false
    end

    local mobiled_state = {
      mob_session_state = "rpc.mobiled.device.@1.network.sessions.@1.session_state"
    }
    content_helper.getExactContent(mobiled_state)

    local wan_ifname = wan_data["wan_ifname"]
    local up = false
    if (mobile and wired) or not mobile then
      if not wired or (wan_ifname and (find(wan_ifname,"ptm") or find(wan_ifname,"atm"))) then
        if wan_data["dsl_status"] == "Up" then
          up = true
          html[#html+1] = ui_helper.createSimpleLight("1","Fixed DSL up "..common.secondsToTime(untaint(wan_data.wan_uptime)))
          -- After disabling broadband the page immediately refreshes. At this time the state is still up but the line
          -- rate is already cleared.
          local rate_up = tonumber(wan_data["dsl_linerate_up"])
          local rate_down = tonumber(wan_data["dsl_linerate_down"])
          if rate_up and rate_down then
            rate_up = floor(rate_up / 10) / 100
            rate_down = floor(rate_down / 10) / 100
            html[#html+1] = format('<p class="bbstats"><i class="icon-upload icon-small gray"></i> %.2f<small>Mbps</small> <i class="icon-download icon-small gray"></i> %.2f<small>Mbps</small></p>',rate_up,rate_down)
          end
          status = "up"
        elseif wan_data["dsl_status"] == "NoSignal" then
          html[#html+1] = ui_helper.createSimpleLight("4","Fixed DSL disconnected")
          status = "down"
        elseif wan_data["dsl0_enabled"] == "0" then
          html[#html+1] = ui_helper.createSimpleLight("0","Fixed DSL disabled")
          status = "disabled"
        else
          html[#html+1] = ui_helper.createSimpleLight("2","Fixed DSL connecting")
          status = "connecting"
        end
      end
      if not wired or (wan_ifname and find(wan_ifname,"eth")) then
        if wan_data["ethwan_status"] == "up" then
          up = true
          html[#html+1] = ui_helper.createSimpleLight("1","Fixed Ethernet up "..common.secondsToTime(untaint(wan_data.wan_uptime)))
          status = "up"
        else
          html[#html+1] = ui_helper.createSimpleLight("4","Fixed Ethernet disconnected")
          status = "down"
        end
      end
    end
    if wired then
      local vlanid = string.match(wan_ifname,".*%.(%d+)")
      if not vlanid then
        local vid = proxy.get("uci.network.device.@"..wan_ifname..".vid")
        if vid and vid[1].value ~= "" then
          vlanid = untaint(vid[1].value)
        end
      end
      if vlanid then
        local vlanifname = proxy.get("uci.network.device.@"..wan_ifname..".ifname")
        if vlanifname and vlanifname[1].value == "" then
          html[#html+1] = ui_helper.createSimpleLight("2","VLAN "..vlanid.." disabled")
        elseif up then
          html[#html+1] = ui_helper.createSimpleLight("1","VLAN "..vlanid.." active")
        else
          html[#html+1] = ui_helper.createSimpleLight("0","VLAN "..vlanid.." inactive")
        end
      end
    end
    if mobiled_state["mob_session_state"] == "connected" then
      html[#html+1] = ui_helper.createSimpleLight("1","Mobile SIM up "..common.secondsToTime(untaint(proxy.get("rpc.network.interface.@wwan.uptime")[1].value)))
      status = "up"
    elseif mobile then
      html[#html+1] = ui_helper.createSimpleLight("4","Mobile SIM disconnected")
      status = "down"
    end

    html[#html+1] = '<style>span.bytes{display:inline-block;text-align:right;width:50px;margin-top:-3px;margin-bottom:-6px;}</style>'
    for _,wan_intf in ipairs(getActiveWANInterfaces(wansensing,mobile,wired)) do
      local intf = toupper(wan_intf.name)
      html[#html+1] = format('<span class="simple-desc modal-link" data-toggle="modal" data-remote="/modals/broadband-usage-modal.lp?wan_intf=%s" data-id="bb-usage-modal"><i class="icon-%s status-icon" title="%s"></i><span class="icon-small status-icon">&udarr;</span>%s&ensp;<i class="icon-cloud-upload status-icon"></i> %s&ensp;<i class="icon-cloud-download status-icon"></i> %s</span>',
        wan_intf.name,
        wan_intf.icon,
        intf,
        alignNumber(wan_intf.total_bytes,"Total Uploaded + Downloaded over",intf),
        alignNumber(wan_intf.tx_bytes,"Uploaded to",intf),
        alignNumber(wan_intf.rx_bytes,"Downloaded from",intf))
    end
  end

  return html,status
end

return M
