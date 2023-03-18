local bridged = require("bridgedmode_helper")
local common = require("common_helper")
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")

local floor,find,format,match = math.floor,string.find,string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local tonumber = tonumber

local M = {}

local function getActiveWANInterfaces()
  local wan_intf = {}
  local content_wan = {
    wwan_ipaddr = "rpc.network.interface.@wwan.ipaddr",
    wwan_ip6addr = "rpc.network.interface.@wwan.ip6addr",
  }
  content_helper.getExactContent(content_wan)
  if content_wan.wwan_ipaddr:len() ~= 0 or content_wan.wwan_ip6addr:len() ~= 0 then
    wan_intf[#wan_intf+1] = "wwan"
  end
  wan_intf[#wan_intf+1] = "wan"
  return wan_intf
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

    local content_rpc = {
      rx_bytes = 0,
      tx_bytes = 0,
      total_bytes = 0,
    }
    local wan_intf
    for _,v in pairs(getActiveWANInterfaces()) do
      local rpc_ifname = proxy.get("rpc.network.interface.@"..v..".ifname")
      if rpc_ifname then
        wan_intf = v
        local path = format("%s_%s",os.date("%F"),rpc_ifname[1].value)
        local rx_bytes = proxy.get("rpc.gui.traffichistory.usage.@"..path..".rx_bytes")
        if rx_bytes then
          content_rpc.rx_bytes = content_rpc.rx_bytes + rx_bytes[1].value
          content_rpc.tx_bytes = content_rpc.tx_bytes + proxy.get("rpc.gui.traffichistory.usage.@"..path..".tx_bytes")[1].value
        else
          content_rpc.rx_bytes = content_rpc.rx_bytes + tonumber(proxy.get("rpc.network.interface.@"..v..".rx_bytes")[1].value)
          content_rpc.tx_bytes = content_rpc.tx_bytes + tonumber(proxy.get("rpc.network.interface.@"..v..".tx_bytes")[1].value)
        end
      end
    end
    content_rpc.total_bytes = content_rpc.rx_bytes + content_rpc.tx_bytes

    local wan_ifname = wan_data["wan_ifname"]
    local up = false
    if (mobile and wired) or not mobile then
      if not wired or (wan_ifname and (find(wan_ifname,"ptm") or find(wan_ifname,"atm"))) then
        if wan_data["dsl_status"] == "Up" then
          up = true
          html[#html+1] = ui_helper.createSimpleLight("1","Fixed DSL connected")
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
          html[#html+1] = ui_helper.createSimpleLight("1","Fixed Ethernet connected")
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
          vlanid = vid[1].value
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
      html[#html+1] = ui_helper.createSimpleLight("1","Mobile SIM connected")
      status = "up"
    elseif mobile then
      html[#html+1] = ui_helper.createSimpleLight("4","Mobile SIM disconnected")
      status = "down"
    end
    if wan_intf then
      html[#html+1] = format('<span class="simple-desc modal-link" data-toggle="modal" data-remote="/modals/broadband-usage-modal.lp?wan_intf='..wan_intf..'" data-id="bb-usage-modal" style="padding-top:5px"><span class="icon-small status-icon">&udarr;</span>%s&ensp;<i class="icon-cloud-upload status-icon"></i> %s&ensp;<i class="icon-cloud-download status-icon"></i> %s</span>',
        common.bytes2string(content_rpc.total_bytes),common.bytes2string(content_rpc.tx_bytes),common.bytes2string(content_rpc.rx_bytes))
    end
  end

  return html,status
end

return M
