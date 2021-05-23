--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local gmatch, format, lower, match = string.gmatch, string.format, string.lower, string.match

local wan_services_data = {
  dmz_enable = "rpc.network.firewall.dmz.enable",
  dmz_blocked = "rpc.network.firewall.dmz.blocked",
  upnp_status = "uci.upnpd.config.enable_upnp",
  upnp_rules = "sys.upnp.RedirectNumberOfEntries",
  ddns_IPv4_enabled = "uci.ddns.service.@myddns_ipv4.enabled",
  ddns_IPv6_enabled = "uci.ddns.service.@myddns_ipv6.enabled",
  ddns_status = "rpc.ddns.status",
}
content_helper.getExactContent(wan_services_data)

local session = ngx.ctx.session
if session:hasAccess("/modals/wanservices-modal.lp") then
    local dmzstate
    if wan_services_data["dmz_blocked"] == "1" then
        dmzstate = ui_helper.createSimpleLight("2", T"DMZ blocked")
    else
      if wan_services_data["dmz_enable"] == "1" then
        dmzstate = ui_helper.createSimpleLight("1", T"DMZ enabled")
      else
        dmzstate = ui_helper.createSimpleLight("0", T"DMZ disabled")
      end
    end

    local status = wan_services_data["ddns_status"]
    local function get_ddns_status(family)
      local service_name = "myddns_" .. lower(family)
      local state_prefix = family .. " Dynamic DNS "
      local ddnsstate

      if wan_services_data["ddns_"..family.."_enabled"] ~= "1" then
        ddnsstate = ui_helper.createSimpleLight("0", T(state_prefix .. "disabled"))
      else
        local service_status
        if status then
          for x in gmatch(status, '([^%]]+)') do
            service_status = match(x, service_name .. "%[(.+)")
            if service_status then
              break
            end
          end
        end
        if service_status then
          if service_status == "Domain's IP updated" then
            ddnsstate = ui_helper.createSimpleLight("1", T(state_prefix .. "- IP updated"))
          elseif service_status == "No error received from server" then
            ddnsstate = ui_helper.createSimpleLight("2", T(state_prefix .. "- IP updating"))
          else
            ddnsstate = ui_helper.createSimpleLight("4", T(state_prefix .. "update error"))
          end
        else
          ddnsstate = ui_helper.createSimpleLight("4", T(state_prefix .. "state unknown"))
        end
      end

      return ddnsstate
    end

    local n_upnp_rules = tonumber(wan_services_data["upnp_rules"])
    local upnpstate
    if wan_services_data["upnp_status"] == "1" then
      upnpstate = ui_helper.createSimpleLight("1", T"UPnP enabled")
    else
      upnpstate = ui_helper.createSimpleLight("0", T"UPnP disabled")
    end

    local wol = io.open("/lib/functions/firewall-wol.sh", "r") and proxy.get("uci.wol.config.")
    local wolstate = ""
    if wol then
      wolenabled = proxy.get("uci.wol.config.enabled")
      if wolenabled then
        if wolenabled[1].value == "1" then
          wolstate = ui_helper.createSimpleLight("1", T"WoL over Internet enabled")
        else
          wolstate = ui_helper.createSimpleLight("0", T"WoL over Internet disabled")
        end
      end
    end
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print( ui_helper.createCardHeader(T"WAN Services", "modals/wanservices-modal.lp") ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf0c2;">\
    ');  ngx.print(
      get_ddns_status("IPv4"),
      get_ddns_status("IPv6"),
	    dmzstate,
      wolstate,
      upnpstate,
      '<p class="subinfos">',
      format( N("<strong %s>%d UPnP rule</strong> is active",
                "<strong %s>%d UPnP rules</strong> are active", n_upnp_rules),
                'class="modal-link" data-toggle="modal" data-remote="modals/wanservices-modal.lp" data-id="wanservices-modal"', n_upnp_rules),
      '</p>'
      )
      ngx.print('\
    </div>\
  </div>\
</div>\
');  
end
  ngx.print('\
'); 