local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local untaint_mt = require("web.taint").untaint_mt
local format = string.format

local M = {}

function M.getInternetCardHTML(mode_active) 
  local mobile_status = {
    wwan_up = "rpc.network.interface.@wwan.up",
  }
  content_helper.getExactContent(mobile_status)
  
  local mobile_ip
  if mobile_status.wwan_up == "1" then
    local content_mobile = {
      ipaddr = "rpc.network.interface.@wwan.ipaddr",
      ip6addr = "rpc.network.interface.@wwan.ip6addr",
    }
    content_helper.getExactContent(content_mobile)
    if content_mobile.ipaddr ~= "" then
      mobile_ip = content_mobile.ipaddr
    elseif content_mobile.ip6addr ~= "" then
      if #content_mobile.ip6addr <= 28 then
        mobile_ip = content_mobile.ip6addr
      else
        mobile_ip = format('<span style="font-size:12px">%s</span>', content_mobile.ip6addr)
      end
    end
  end
  
  local html = {}

  if mode_active == "bridge" then
    local cs = {
      variant = "env.var.variant_friendly_name",
    }
    content_helper.getExactContent(cs)

    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = T'Gateway is in bridge mode'
    html[#html+1] = '</p>'

  elseif mode_active == "dhcp" then
    local cs = {
      uci_wan_auto = "uci.network.interface.@wan.auto",
      ipaddr = "rpc.network.interface.@wan.ipaddr",
      ip6addr = "rpc.network.interface.@wan6.ip6addr",
    }
    content_helper.getExactContent(cs)
    local dhcp_state = "connecting"
    local dhcp_state_map = {
      disabled = T"DHCP disabled",
      connected = T"DHCP on",
      connecting = T"DHCP connecting",
    }
    local dhcp_light_map = {
      disabled = "off",
      connecting = "orange",
      connected = "green",
    }
    if cs["uci_wan_auto"] ~= "0" then
      cs["uci_wan_auto"] = "1"
      if cs["ipaddr"]:len() > 0 then
        dhcp_state = "connected"
      else
        dhcp_state = "connecting"
      end
    else
      dhcp_state = "disabled"
    end

    html[#html+1] = ui_helper.createSimpleLight(nil, dhcp_state_map[dhcp_state], { light = { class = dhcp_light_map[dhcp_state] } })
    if dhcp_state == "connected" then
      html[#html+1] = '<p class="subinfos">'
      html[#html+1] = format(T'WAN IP is <strong style="letter-spacing:-1px">%s</strong>', cs["ipaddr"])
      html[#html+1] = format(T'<br><strong style="letter-spacing:-1px">%s</strong>', cs["ip6addr"])
      html[#html+1] = '</p>'
    end

  elseif mode_active == "pppoe" then
    local content_uci = {
      wan_proto = "uci.network.interface.@wan.proto",
      wan_auto = "uci.network.interface.@wan.auto",
    }
    content_helper.getExactContent(content_uci)
    local content_rpc = {
      wan_ppp_state = "rpc.network.interface.@wan.ppp.state",
      wan_ppp_error = "rpc.network.interface.@wan.ppp.error",
      ipaddr = "rpc.network.interface.@wan.ipaddr",
      ip6addr = "rpc.network.interface.@wan6.ip6addr",
    }
    content_helper.getExactContent(content_rpc)
    local ppp_state_map = {
      disabled = T"PPP disabled",
      disconnecting = T"PPP disconnecting",
      connected = T"PPP on",
      connecting = T"PPP connecting",
      disconnected = T"PPP disconnected",
      error = T"PPP error",
      AUTH_TOPEER_FAILED = T"PPP authentication failed",
      NEGOTIATION_FAILED = T"PPP negotiation failed",
    }
    setmetatable(ppp_state_map, untaint_mt)
    local ppp_light_map = {
      disabled = "off",
      disconnected = "red",
      disconnecting = "orange",
      connecting = "orange",
      connected = "green",
      error = "red",
      AUTH_TOPEER_FAILED = "red",
      NEGOTIATION_FAILED = "red",
    }
    setmetatable(ppp_light_map, untaint_mt)
    local ppp_status
    if content_uci.wan_auto ~= "0" then
      content_uci.wan_auto = "1"
      ppp_status = format("%s", content_rpc.wan_ppp_state) -- untaint
      if ppp_status == "" or ppp_status == "authenticating" then
        ppp_status = "connecting"
      end
      if not (content_rpc.wan_ppp_error == "" or content_rpc.wan_ppp_error == "USER_REQUEST") then
        if ppp_state_map[content_rpc.wan_ppp_error] then
          ppp_status = content_rpc.wan_ppp_error
        else
          ppp_status = "error"
        end
      end
    else
      ppp_status = "disabled"
    end
    html[#html+1] = ui_helper.createSimpleLight(nil, ppp_state_map[ppp_status], { light = { class = ppp_light_map[ppp_status] } })
    if ppp_status == "connected" then
      html[#html+1] = '<p class="subinfos">'
      html[#html+1] = format(T'WAN IP is <strong style="letter-spacing:-1px">%s</strong>', content_rpc["ipaddr"])
      html[#html+1] = format(T'<br><strong style="letter-spacing:-1px">%s</strong>', content_rpc["ip6addr"])
      html[#html+1] = '</p>'
    end

  elseif mode_active == "static" then
    local cs = {
      uci_wan_auto = "uci.network.interface.@wan.auto",
      ipaddr = "rpc.network.interface.@wan.ipaddr",
      ip6addr = "rpc.network.interface.@wan6.ip6addr",
    }
    content_helper.getExactContent(cs)
    if cs["uci_wan_auto"] ~= "0" then
      local wan_data = {
        wan_ifname        = "uci.network.interface.@wan.ifname",
        dsl0_enabled      = "uci.xdsl.xdsl.@dsl0.enabled",
        dsl_status        = "sys.class.xdsl.@line0.Status",
        ethwan_status     = "sys.eth.port.@eth4.status",
      }
      content_helper.getExactContent(wan_data)
      if wan_data["wan_ifname"] and (wan_data["wan_ifname"] == "ptm0" or wan_data["wan_ifname"] == "atmwan") then
        if wan_data["dsl_status"] == "Up" then
          html[#html+1] = ui_helper.createSimpleLight("1", "Static on")
        elseif wan_data["dsl_status"] == "NoSignal" then
          html[#html+1] = ui_helper.createSimpleLight("4", "Static disconnected")
        elseif wan_data["dsl0_enabled"] == "0" then
          html[#html+1] = ui_helper.createSimpleLight("0", "Static disabled")
        else
          html[#html+1] = ui_helper.createSimpleLight("2", "Static connecting")
        end
      end
      if wan_data["wan_ifname"] and string.find(wan_data["wan_ifname"],"eth") then
        if wan_data["ethwan_status"] == "up" then
          html[#html+1] = ui_helper.createSimpleLight("1", "Static on")
        else
          html[#html+1] = ui_helper.createSimpleLight("4", "Static disconnected")
        end
      end
    else
      html[#html+1] = ui_helper.createSimpleLight("0", "Static disabled")
    end
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format(T'WAN IP is <strong style="letter-spacing:-1px">%s</strong>', cs["ipaddr"])
    html[#html+1] = format(T'<br><strong style="letter-spacing:-1px">%s</strong>', cs["ip6addr"])
    html[#html+1] = '</p>'
  end

  if mobile_ip then
    html[#html+1] = ui_helper.createSimpleLight(mobile_status["wwan_up"], "Mobile Internet connected")
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format(T'WAN IP is <strong style="letter-spacing:-1px">%s</strong>', mobile_ip)
    html[#html+1] = '</p>'
  end

  return html
end

return M
