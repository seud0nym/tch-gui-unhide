local content_helper = require("web.content_helper")
local ipv6_rpc_path = require("ipv6_rpc_path").getPath()
local ui_helper = require("web.ui_helper")
local untaint_mt = require("web.taint").untaint_mt
local find,format,gsub = string.find,string.format,string.gsub


local items = {
  ["OK"] = {"1", T"Fixed Broadband online",},
  ["OK_LTE"] = {"1", T"Mobile Broadband online"},
  ["OK_BOTH"] = {"1", T"Fixed + Mobile Broadband online"},
  ["OFF"] = {"2", T"Broadband Service disabled",},
  ["E_NO_PRE"] = {"4", T"Broadband Service down", "<ol><li>Check that your Telephone or Ethernet cable is firmly connected to the correct port, the Filter on the Telephone socket or the Ethernet socket on the wall.</li><li>Check that your username is correct and re-enter your password <a aria-label='Page redirection for Internet Access Modal' href=/gateway.lp?openmodal=internet-modal.lp>here</a>.</li><li>Restart.</li></ol>",},
  ["E_PPP_DSL"] = {"4", T"Broadband Service down", "<ol><li>Check that your Telephone cable is firmly connected to the correct port or the Filter on the telephone socket on the wall.</li><li>Check that your username is correct and re-enter your password <a aria-label='Page redirection for Internet Access Modal' href=/gateway.lp?openmodal=internet-modal.lp>here</a>.</li><li>Restart.</li></ol>",},
  ["E_PPP_ETH"] = {"4", T"Broadband Service down", "<ol><li>Check that your Ethernet cable is firmly connected to the correct port or the Ethernet socket on the wall.</li><li>Check that your username is correct and re-enter your password <a aria-label='Page redirection for Internet Access Modal' href=/gateway.lp?openmodal=internet-modal.lp>here</a>.</li><li>Restart.</li></ol>",},
  ["E_DHCP_DSL"] = {"4", T"Broadband Service down", "<ol><li>Check that your Telephone cable is firmly connected to the correct port or the Filter on the telephone socket on the wall.</li><li>Restart.</li></ol>",},
  ["E_DHCP_ETH"] = {"4", T"Broadband Service down", "<ol><li>Check that your Ethernet cable is firmly connected to the correct port or the Ethernet socket on the wall.</li><li>Restart.</li></ol>",},
}

local M = {}

function M.getInternetCardHTML(mode_active)
  local msg_key = "E_NO_PRE"

  local ws_content = {
    primarywanmode = "uci.wansensing.global.primarywanmode",
    l2 = "uci.wansensing.global.l2type",
  }
  content_helper.getExactContent(ws_content)

  local L2 = "PRE"
  if ws_content.l2 == "ADSL" or ws_content.l2 == "VDSL" then
    L2 = "DSL"
  elseif ws_content.l2 == "ETH" then
    L2 = "ETH"
  end

  local mobile = false
  if ws_content.primarywanmode == "MOBILE" then
    mobile = true
  end

  local html = {""}

  local function addIPs(source,ipaddr,ip6addr,ipv6uniqueglobaladdr,ipv6uniquelocaladdr,ip6prefix)
    html[#html+1] = '<p class="subinfos" style="margin-bottom:1px;line-height:14px;">'
    html[#html+1] = format(T'%s IP: <strong style="letter-spacing:-1px"><span style="font-size:12px">%s</span></strong>',source,ipaddr)
    if ip6addr and ip6addr ~= "" then
      local addr = ip6addr
      if find(ip6addr," ") then
        if ipv6uniqueglobaladdr and ipv6uniqueglobaladdr ~= "" and find(ip6addr,gsub(ipv6uniqueglobaladdr,"/%d+","")) then
          addr = gsub(ipv6uniqueglobaladdr,"/%d+","")
        elseif ipv6uniquelocaladdr and ipv6uniquelocaladdr ~= "" and find(ip6addr,gsub(ipv6uniquelocaladdr,"/%d+","")) then
          addr = gsub(ipv6uniquelocaladdr,"/%d+","")
        end
      end
      html[#html+1] = format(T'<br><strong style="letter-spacing:-1px"><span style="font-size:12px">%s</span></strong>',addr)
    end
    html[#html+1] = '</p>'
    if ip6prefix and ip6prefix ~= "" then
      html[#html+1] = '<p class="subinfos" style="margin-bottom:1px;line-height:14px;">'
      html[#html+1] = format(T'Prefix: <strong style="letter-spacing:-1px"><span style="font-size:12px">%s</span></strong>',ip6prefix)
      html[#html+1] = '</p>'
    end
  end

  if not mobile then
    -- BRIDGE
    if mode_active == "bridge" then
      local cs = {
        variant = "env.var.variant_friendly_name",
      }
      content_helper.getExactContent(cs)

      html[#html+1] = '<p class="subinfos">'
      html[#html+1] = T'Gateway is in Bridged Mode'
      html[#html+1] = '</p>'
    -- DHCP
    elseif mode_active == "dhcp" then
      local cs = {
        uci_wan_auto = "uci.network.interface.@wan.auto",
        uci_wan6_auto = "uci.network.interface.@wan6.auto",
        wan6_prefix = ipv6_rpc_path.."ip6prefix",
        ipaddr = "rpc.network.interface.@wan.ipaddr",
        ip6addr = ipv6_rpc_path.."ip6addr",
        ipv6uniqueglobaladdr = ipv6_rpc_path.."ipv6uniqueglobaladdr",
        ipv6uniquelocaladdr = ipv6_rpc_path.."ipv6uniquelocaladdr",
      }
      content_helper.getExactContent(cs)
      local dhcp_state = "connecting"
      local dhcp_state_map = {
        disabled = T"IPv4 disabled on boot",
        connected = T"Address acquired from DHCP",
        connecting = T"Requesting address from DHCP...",
      }
      local dhcp_light_map = {
        disabled = "off",
        connecting = "orange",
        connected = "green",
      }
      if cs["uci_wan_auto"] ~= "0" then
        if cs["ipaddr"]:len() > 0 then
          dhcp_state = "connected"
          if cs["ipv6uniqueglobaladdr"]:len() > 0 or cs["ipv6uniquelocaladdr"]:len() > 0 then
            dhcp_state_map[dhcp_state] = T"Addresses acquired from DHCP"
          end
        else
          dhcp_state = "connecting"
        end
      elseif cs["uci_wan6_auto"] ~= "0" and (cs["ipv6uniqueglobaladdr"]:len() > 0 or cs["ipv6uniquelocaladdr"]:len() > 0) then
        dhcp_state = "connected"
      else
        dhcp_state = "disabled"
      end

      if dhcp_state == "connected" then
        msg_key = "OK"
        addIPs("DHCP WAN",cs["ipaddr"],cs["ip6addr"],cs["ipv6uniqueglobaladdr"],cs["ipv6uniquelocaladdr"],cs["wan6_prefix"])
      else
        msg_key = "E_DHCP_"..L2
        html[#html+1] = ui_helper.createSimpleLight(nil,dhcp_state_map[dhcp_state],{ light = { class = dhcp_light_map[dhcp_state] } })
      end
    -- PPOE
    elseif mode_active == "pppoe" then
      local content_uci = {
        wan_auto = "uci.network.interface.@wan.auto",
      }
      content_helper.getExactContent(content_uci)
      local cs = {
        wan_ppp_state = "rpc.network.interface.@wan.ppp.state",
        wan_ppp_error = "rpc.network.interface.@wan.ppp.error",
        wan6_prefix = ipv6_rpc_path.."ip6prefix",
        ipaddr = "rpc.network.interface.@wan.ipaddr",
        ip6addr = ipv6_rpc_path.."ip6addr",
        ipv6uniqueglobaladdr = ipv6_rpc_path.."ipv6uniqueglobaladdr",
        ipv6uniquelocaladdr = ipv6_rpc_path.."ipv6uniquelocaladdr",
      }
      content_helper.getExactContent(cs)
      local ppp_state_map = {
        disabled = T"PPP disabled",
        disconnecting = T"PPP session disconnecting",
        connected = T"PPP session connected",
        connecting = T"PPP session connecting",
        disconnected = T"PPP session disconnected",
        error = T"PPP error",
        AUTH_TOPEER_FAILED = T"PPP authentication failed",
        NEGOTIATION_FAILED = T"PPP negotiation failed",
      }
      setmetatable(ppp_state_map,untaint_mt)
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
      setmetatable(ppp_light_map,untaint_mt)
      local ppp_status
      if content_uci.wan_auto ~= "0" then
        content_uci.wan_auto = "1"
        ppp_status = format("%s",cs.wan_ppp_state) -- untaint
        if ppp_status == "" or ppp_status == "authenticating" then
          ppp_status = "connecting"
          msg_key = "E_PPP_"..L2
        end
        if not (cs.wan_ppp_error == "" or cs.wan_ppp_error == "USER_REQUEST") then
          if ppp_state_map[cs.wan_ppp_error] then
            ppp_status = cs.wan_ppp_error
          else
            ppp_status = "error"
          end
          msg_key = "E_PPP_"..L2
        end
      else
        ppp_status = "disabled"
        msg_key = "OFF"
      end
      if ppp_status == "connected" then
        addIPs("PPP WAN",cs["ipaddr"],cs["ip6addr"],cs["ipv6uniqueglobaladdr"],cs["ipv6uniquelocaladdr"],cs["wan6_prefix"])
        msg_key = "OK"
      else
        html[#html+1] = ui_helper.createSimpleLight(nil,ppp_state_map[ppp_status],{ light = { class = ppp_light_map[ppp_status] } })
      end
    -- STATIC
    elseif mode_active == "static" then
      local cs = {
        uci_wan_auto = "uci.network.interface.@wan.auto",
        wan6_prefix = ipv6_rpc_path.."ip6prefix",
        ipaddr = "rpc.network.interface.@wan.ipaddr",
        ip6addr = ipv6_rpc_path.."ip6addr",
        ipv6uniqueglobaladdr = ipv6_rpc_path.."ipv6uniqueglobaladdr",
        ipv6uniquelocaladdr = ipv6_rpc_path.."ipv6uniquelocaladdr",
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
            addIPs("Static WAN",cs["ipaddr"],cs["ip6addr"],cs["ipv6uniqueglobaladdr"],cs["ipv6uniquelocaladdr"],cs["wan6_prefix"])
            msg_key = "OK"
          elseif wan_data["dsl_status"] == "NoSignal" then
            html[#html+1] = ui_helper.createSimpleLight("4","Disconnected")
            addIPs("Static WAN",cs["ipaddr"],cs["ip6addr"],cs["ipv6uniqueglobaladdr"],cs["ipv6uniquelocaladdr"],cs["wan6_prefix"])
            msg_key = "E_NO_PRE"
          elseif wan_data["dsl0_enabled"] == "0" then
            html[#html+1] = ui_helper.createSimpleLight("0","Static IP connection disabled")
            addIPs("Static WAN",cs["ipaddr"],cs["ip6addr"],cs["ipv6uniqueglobaladdr"],cs["ipv6uniquelocaladdr"],cs["wan6_prefix"])
            msg_key = "OFF"
          else
            html[#html+1] = ui_helper.createSimpleLight("2","Trying to connect...")
            addIPs("Static WAN",cs["ipaddr"],cs["ip6addr"],cs["ipv6uniqueglobaladdr"],cs["ipv6uniquelocaladdr"],cs["wan6_prefix"])
            msg_key = "E_NO_PRE"
          end
        end
        if wan_data["wan_ifname"] and find(wan_data["wan_ifname"],"eth") then
          if wan_data["ethwan_status"] == "up" then
            addIPs("Static WAN",cs["ipaddr"],cs["ip6addr"],cs["ipv6uniqueglobaladdr"],cs["ipv6uniquelocaladdr"],cs["wan6_prefix"])
            msg_key = "OK"
          else
            html[#html+1] = ui_helper.createSimpleLight("4","Disconnected")
            addIPs("Static WAN",cs["ipaddr"],cs["ip6addr"],cs["ipv6uniqueglobaladdr"],cs["ipv6uniquelocaladdr"],cs["wan6_prefix"])
            msg_key = "E_NO_PRE"
          end
        end
      else
        html[#html+1] = ui_helper.createSimpleLight("0","Static IP connection disabled")
        addIPs("Static WAN",cs["ipaddr"],cs["ip6addr"],cs["ipv6uniqueglobaladdr"],cs["ipv6uniquelocaladdr"],cs["wan6_prefix"])
        msg_key = "OFF"
      end
    end
  end

  local mobile_status = {
    wwan_up = "rpc.network.interface.@wwan.up",
  }
  content_helper.getExactContent(mobile_status)
  if mobile_status.wwan_up == "1" then
    if msg_key == "OK" then
      msg_key = "OK_BOTH"
    else
      msg_key = "OK_LTE"
    end
    local content_mobile = {
      ipaddr = "rpc.network.interface.@wwan.ipaddr",
      ip6addr = "rpc.network.interface.@wwan.ip6addr",
      ipv6uniqueglobaladdr = ipv6_rpc_path.."ipv6uniqueglobaladdr",
      ipv6uniquelocaladdr = ipv6_rpc_path.."ipv6uniquelocaladdr",
      wan6_prefix = "rpc.network.interface.@wwan.ip6prefix",
      rx_bytes = "rpc.network.interface.@wwan.rx_bytes",
      tx_bytes = "rpc.network.interface.@wwan.tx_bytes",
    }
    content_helper.getExactContent(content_mobile)
    if tonumber(content_mobile.rx_bytes) * 100 < tonumber(content_mobile.tx_bytes) then
      html[#html+1] = ui_helper.createSimpleLight("2","Mobile SIM disabled?")
    end

    addIPs("SIM WWAN",content_mobile.ipaddr,content_mobile.ip6addr,content_mobile.ipv6uniqueglobaladdr,content_mobile.ipv6uniquelocaladdr,content_mobile.wan6_prefix)
  end

  local item = items[msg_key] or items["E_NO_PRE"]
  html[1] = ui_helper.createSimpleLight(item[1],item[2])
  if item[3] then
    html[#html+1] = format('<div>%s</div>',item[3])
  end

  return html
end

return M
