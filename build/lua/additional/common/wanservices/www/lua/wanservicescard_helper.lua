local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local gmatch,format,lower,match,untaint = string.gmatch,string.format,string.lower,string.match,string.untaint

local M = {}

local function get_ddns_status(wan_services_data,family)
  local ddns_state

  if wan_services_data["ddns_"..family.."_enabled"] ~= "1" then
    ddns_state = ui_helper.createSimpleLight("0",T(format("%s Dynamic DNS disabled",family)))
  else
    local service_name = format("myddns_%s",lower(family))
    local name = untaint(wan_services_data["ddns_"..family.."_domain"])
    local attr = { span = { title = name }}
    local cert = ""
    if wan_services_data["cert_"..family.."_enabled"] and wan_services_data["cert_"..family.."_enabled"] == "1" then
      cert="&nbsp;<span class='icon-lock' title='Server Certificate auto-renewal enabled for "..name.."'></span>"
    else
      cert="&nbsp;<span class='icon-unlock' title='Server Certificate auto-renewal disabled for "..name.."'></span>"
    end

    local status = wan_services_data["ddns_status"]
    local service_status
    if status then
      for x in gmatch(status,'([^%]]+)') do
        service_status = match(x,service_name.."%[(.+)")
        if service_status then
          break
        end
      end
    end
    
    if service_status then
      local state_prefix = format("%s Dynamic DNS ",family)
      if service_status == "Domain's IP updated" then
        ddns_state = ui_helper.createSimpleLight("1",T(state_prefix.."- IP updated"..cert),attr)
      elseif service_status == "No error received from server" then
        ddns_state = ui_helper.createSimpleLight("2",T(state_prefix.."- IP updating"..cert),attr)
      else
        ddns_state = ui_helper.createSimpleLight("4",T(state_prefix.."update error"..cert),attr)
      end
    else
      ddns_state = ui_helper.createSimpleLight("4",T(state_prefix.."state unknown"..cert),attr)
    end
  end

  return ddns_state
end

function M.getWANServicesCardHTML()
  local html = {}

  local wan_services_data = {
    dmz_enable = "rpc.network.firewall.dmz.enable",
    dmz_blocked = "rpc.network.firewall.dmz.blocked",
    upnp_status = "uci.upnpd.config.enable_upnp",
    upnp_rules = "sys.upnp.RedirectNumberOfEntries",
    ddns_IPv4_enabled = "uci.ddns.service.@myddns_ipv4.enabled",
    ddns_IPv4_domain = "uci.ddns.service.@myddns_ipv4.domain",
    ddns_IPv6_enabled = "uci.ddns.service.@myddns_ipv6.enabled",
    ddns_IPv6_domain = "uci.ddns.service.@myddns_ipv6.domain",
    ddns_status = "rpc.ddns.status",
    cert_IPv4_enabled = "rpc.gui.acme.enabled",
  }
  content_helper.getExactContent(wan_services_data)
  wan_services_data["cert_IPv6_enabled"] = wan_services_data["cert_IPv4_enabled"]

  local ddns4 = get_ddns_status(wan_services_data,"IPv4")
  if ddns4 then
    html[#html+1] = ddns4
  end
  local ddns6 = get_ddns_status(wan_services_data,"IPv6")
  if ddns6 then
    html[#html+1] = ddns6
  end
  
  if wan_services_data["dmz_blocked"] == "1" then
      html[#html+1] = ui_helper.createSimpleLight("2",T"DMZ blocked")
  else
    if wan_services_data["dmz_enable"] == "1" then
      html[#html+1] = ui_helper.createSimpleLight("1",T"DMZ enabled")
    else
      html[#html+1] = ui_helper.createSimpleLight("0",T"DMZ disabled")
    end
  end

  local wol = io.open("/lib/functions/firewall-wol.sh", "r") and proxy.get("uci.wol.config.")
  if wol then
    wolenabled = proxy.get("uci.wol.config.enabled")
    if wolenabled then
      if wolenabled[1].value == "1" then
        html[#html+1] = ui_helper.createSimpleLight("1",T"WoL over Internet enabled")
      else
        html[#html+1] = ui_helper.createSimpleLight("0",T"WoL over Internet disabled")
      end
    end
  end

  local n_upnp_rules = tonumber(wan_services_data["upnp_rules"])
  if wan_services_data["upnp_status"] == "1" then
    html[#html+1] = ui_helper.createSimpleLight("1",T"UPnP enabled")
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format(N("<strong %s>%d UPnP rule</strong> is active","<strong %s>%d UPnP rules</strong> are active",n_upnp_rules),
              'class="modal-link" data-toggle="modal" data-remote="modals/wanservices-modal.lp" data-id="wanservices-modal"',n_upnp_rules)
    html[#html+1] = '</p>'
  else
    html[#html+1] = ui_helper.createSimpleLight("0",T"UPnP disabled")
  end

  return html
end

return M
  