local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local untaint_mt = require("web.taint").untaint_mt
local find, format, gsub = string.find, string.format, string.gsub

local M = {}

function M.getInternetCardHTML(mode_active) 
  local mobile_status = {
    wwan_up = "rpc.network.interface.@wwan.up",
  }
  content_helper.getExactContent(mobile_status)
  
  local mobile_ip
  local mobile_dns
  if mobile_status.wwan_up == "1" then
    local content_mobile = {
      ipaddr = "rpc.network.interface.@wwan.ipaddr",
      ip6addr = "rpc.network.interface.@wwan.ip6addr",
      dns = "rpc.network.interface.@wwan.dnsservers",
      rx_bytes = "rpc.network.interface.@wwan.rx_bytes",
      tx_bytes = "rpc.network.interface.@wwan.tx_bytes",
    }
    content_helper.getExactContent(content_mobile)
    if content_mobile.ipaddr ~= "" then
      mobile_ip = content_mobile.ipaddr
      mobile_dns = content_mobile.dns
    elseif content_mobile.ip6addr ~= "" then
      if #content_mobile.ip6addr <= 28 then
        mobile_ip = content_mobile.ip6addr
        mobile_dns = gsub(content_mobile.dns,",",", ")
      else
        mobile_ip = format('<span style="font-size:12px">%s</span>', content_mobile.ip6addr)
        mobile_dns = format('<span style="font-size:12px">%s</span>', gsub(content_mobile.dns,",",", "))
      end
    end
    if tonumber(content_mobile.rx_bytes) * 100 < tonumber(content_mobile.tx_bytes) then
      mobile_status["wwan_up"] = "2"
      mobile_status["state"] = "Mobile Internet SIM disabled?"
    else
      mobile_status["state"] = "Mobile Internet connected"
    end
  end
  
  local html = {}

  local function addIPs(ipaddr, ip6addr, dnsv4, dnsv6)
    html[#html+1] = '<p class="subinfos" style="margin-bottom:4px;line-height:17px;">'
    html[#html+1] = format(T'WAN IP: <strong style="letter-spacing:-1px">%s</strong>', ipaddr)
    if ip6addr and ip6addr ~= "" then
      html[#html+1] = format(T'<br><strong style="letter-spacing:-1px">%s</strong>', ip6addr)
    end
    html[#html+1] = '</p>'
    html[#html+1] = '<p class="subinfos" style="line-height:17px;">'
    html[#html+1] = format(T'DNS: <strong style="letter-spacing:-1px">%s</strong>', gsub(gsub(dnsv4, "^%s*(.-)%s*$", "%1"),",",", "))
    if ip6addr and ip6addr ~= "" and dnsv6 and dnsv6 ~= "" then
      if dnsv4 and dnsv4 ~= "" then
        html[#html+1] = '<br>'
      end  
      html[#html+1] = format(T'<strong style="letter-spacing:-1px">%s</strong>', gsub(dnsv6,",",", "))
    end
    html[#html+1] = '</p>'
  end

  -- BRIDGE
  if mode_active == "bridge" then
    local cs = {
      variant = "env.var.variant_friendly_name",
    }
    content_helper.getExactContent(cs)

    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = T'Gateway is in bridge mode'
    html[#html+1] = '</p>'
  -- DHCP
  elseif mode_active == "dhcp" then
    local cs = {
      uci_wan_auto = "uci.network.interface.@wan.auto",
      ipaddr = "rpc.network.interface.@wan.ipaddr",
      ip6addr = "rpc.network.interface.@wan6.ip6addr",
      dnsv4 = "rpc.network.interface.@wan.dnsservers",
      dnsv6 = "rpc.network.interface.@wan6.dnsservers",
    }
    content_helper.getExactContent(cs)
    local dhcp_state = "connecting"
    local dhcp_state_map = {
      disabled = T"DHCP disabled",
      connected = T"DHCP connected",
      connecting = T"Trying to connect using DHCP...",
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
      addIPs(cs["ipaddr"], cs["ip6addr"], cs["dnsv4"], cs["dnsv6"])
    end
  -- PPOE
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
      dnsv4 = "rpc.network.interface.@wan.dnsservers",
      dnsv6 = "rpc.network.interface.@wan6.dnsservers",
    }
    content_helper.getExactContent(content_rpc)
    local ppp_state_map = {
      disabled = T"PPP disabled",
      disconnecting = T"PPP disconnecting",
      connected = T"PPP connected",
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
      addIPs(content_rpc["ipaddr"], content_rpc["ip6addr"], content_rpc["dnsv4"], content_rpc["dnsv6"])
    end
  -- STATIC
  elseif mode_active == "static" then
    local cs = {
      uci_wan_auto = "uci.network.interface.@wan.auto",
      ipaddr = "rpc.network.interface.@wan.ipaddr",
      ip6addr = "rpc.network.interface.@wan6.ip6addr",
      dnsv4 = "rpc.network.interface.@wan.dnsservers",
      dnsv6 = "rpc.network.interface.@wan6.dnsservers",
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
      if wan_data["wan_ifname"] and (find(wan_data["wan_ifname"],"ptm0") or find(wan_data["wan_ifname"],"atm")) then
        if wan_data["dsl_status"] == "Up" then
          html[#html+1] = ui_helper.createSimpleLight("1", "Static IP connected")
          addIPs(cs["ipaddr"], cs["ip6addr"], cs["dnsv4"], cs["dnsv6"])
        elseif wan_data["dsl_status"] == "NoSignal" then
          html[#html+1] = ui_helper.createSimpleLight("4", "Static IP disconnected")
          addIPs(cs["ipaddr"], cs["ip6addr"], cs["dnsv4"], cs["dnsv6"])
        elseif wan_data["dsl0_enabled"] == "0" then
          html[#html+1] = ui_helper.createSimpleLight("0", "Static IP disabled")
          addIPs(cs["ipaddr"], cs["ip6addr"], cs["dnsv4"], cs["dnsv6"])
        else
          html[#html+1] = ui_helper.createSimpleLight("2", "Trying to connect with Static IP...")
          addIPs(cs["ipaddr"], cs["ip6addr"], cs["dnsv4"], cs["dnsv6"])
        end
      end
      if wan_data["wan_ifname"] and find(wan_data["wan_ifname"],"eth") then
        if wan_data["ethwan_status"] == "up" then
          html[#html+1] = ui_helper.createSimpleLight("1", "Static connected")
          addIPs(cs["ipaddr"], cs["ip6addr"], cs["dnsv4"], cs["dnsv6"])
        else
          html[#html+1] = ui_helper.createSimpleLight("4", "Static IP disconnected")
          addIPs(cs["ipaddr"], cs["ip6addr"], cs["dnsv4"], cs["dnsv6"])
        end
      end
    else
      html[#html+1] = ui_helper.createSimpleLight("0", "Static IP disabled")
      addIPs(cs["ipaddr"], cs["ip6addr"], cs["dnsv4"], cs["dnsv6"])
    end
  end

  if mobile_ip then
    html[#html+1] = ui_helper.createSimpleLight(mobile_status["wwan_up"], mobile_status["state"])
    addIPs(mobile_ip, nil, mobile_dns, nil)
  end

  return html
end

return M
