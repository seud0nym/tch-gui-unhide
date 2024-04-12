local ddns_helper = require("wanservices-ddns_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local format,match = string.format,string.match

local M = {}

function M.getWANServicesCardHTML()
  local html = {}

  local ddns_status = '<span class="modal-link" data-toggle="modal" data-remote="/modals/wanservices-ddns-modal.lp" data-id="wanservices-ddns-modal">Dynamic DNS %s</span>'
  local ddns_enabled = {}
  local ddns_service = proxy.getPN("uci.ddns.service.", true)
  for k=1,#ddns_service do
    local state = proxy.get(ddns_service[k].path.."enabled")[1].value
    if state ~= "0" then
      ddns_enabled[#ddns_enabled+1] = match(ddns_service[k].path,"@(.+)%.$")
    end
  end
  if #ddns_enabled == 0 then
    html[#html+1] = ui_helper.createSimpleLight("0",format(ddns_status,"disabled"))
  else
    local services_status = ddns_helper.get_services_status()
    local ddns = {
      updated = 0,
      updating = 0,
      error = 0,
      disabled = 0,
    }
    for k=1,#ddns_enabled do
      local service = ddns_enabled[k]
      local status = ddns_helper.to_status(services_status,service)
      ddns[status] = ddns[status] + 1
    end
    if ddns.error ~= 0 then
      html[#html+1] = ui_helper.createSimpleLight("4",format(ddns_status,"update error"))
    elseif ddns.updating ~= 0 then
      html[#html+1] = ui_helper.createSimpleLight("2",format(ddns_status,"updating"))
    elseif ddns.updated ~= 0 then
      html[#html+1] = ui_helper.createSimpleLight("1",format(ddns_status,"updated"))
    else
      html[#html+1] = ui_helper.createSimpleLight("0",format(ddns_status,"disabled"))
    end
  end

  local wan_services_data = {
    dmz_enable = "rpc.network.firewall.dmz.enable",
    dmz_blocked = "rpc.network.firewall.dmz.blocked",
    upnp_status = "uci.upnpd.config.enable_upnp",
    upnp_rules = "sys.upnp.RedirectNumberOfEntries",
    acme_enabled = "rpc.gui.acme.enabled",
  }
  content_helper.getExactContent(wan_services_data)

  local acme_status = '<span class="modal-link" data-toggle="modal" data-remote="/modals/wanservices-ddns-modal.lp" data-id="wanservices-ddns-modal">SSL Certificates %s</span>'
  if wan_services_data["acme_enabled"] == "1" then
    html[#html+1] = ui_helper.createSimpleLight("1",format(acme_status,"enabled"))
  else
    html[#html+1] = ui_helper.createSimpleLight("0",format(acme_status,"disabled"))
  end

  local dmz_status = '<span class="modal-link" data-toggle="modal" data-remote="/modals/wanservices-dmz-modal.lp" data-id="wanservices-dmz-modal">DMZ %s</span>'
  if wan_services_data["dmz_blocked"] == "1" then
    html[#html+1] = ui_helper.createSimpleLight("2",format(dmz_status,"blocked"))
  else
    if wan_services_data["dmz_enable"] == "1" then
      html[#html+1] = ui_helper.createSimpleLight("1",format(dmz_status,"enabled"))
    else
      html[#html+1] = ui_helper.createSimpleLight("0",format(dmz_status,"disabled"))
    end
  end

  local wol = io.open("/lib/functions/firewall-wol.sh","r") and proxy.get("uci.wol.config.")
  if wol then
    local wol_status = '<span class="modal-link" data-toggle="modal" data-remote="/modals/wanservices-wol-modal.lp" data-id="wanservices-wol-modal">WoL over Internet %s</span>'
    local wol_enabled = proxy.get("uci.wol.config.enabled")
    if wol_enabled then
      if wol_enabled[1].value == "1" then
        html[#html+1] = ui_helper.createSimpleLight("1",format(wol_status,"enabled"))
      else
        html[#html+1] = ui_helper.createSimpleLight("0",format(wol_status,"disabled"))
      end
    end
  end

  local n_upnp_rules = tonumber(wan_services_data["upnp_rules"])
  local upnp_status = '<span class="modal-link" data-toggle="modal" data-remote="/modals/wanservices-upnp-modal.lp" data-id="wanservices-upnp-modal">UPnP %s</span>'
  if wan_services_data["upnp_status"] == "1" then
    html[#html+1] = ui_helper.createSimpleLight("1",format(upnp_status,"enabled"))
    html[#html+1] = '<p class="subinfos">'
    html[#html+1] = format(N("<strong %s>%d UPnP rule</strong> is active","<strong %s>%d UPnP rules</strong> are active",n_upnp_rules),
              'class="modal-link" data-toggle="modal" data-remote="modals/wanservices-upnp-modal.lp" data-id="wanservices-upnp-modal"',n_upnp_rules)
    html[#html+1] = '</p>'
  else
    html[#html+1] = ui_helper.createSimpleLight("0",format(upnp_status,"disabled"))
  end

  return html
end

return M
