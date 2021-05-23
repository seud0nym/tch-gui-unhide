--pretranslated: do not change this file
 
-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local portslist = require("portslist_helper")
local uinetwork = require("web.uinetwork_helper")
local hosts_ac, hosts_ac_v6 = uinetwork.getAutocompleteHostsList()
local io, pairs, string = io, pairs, string
local table, ipairs, tonumber, format, match = table, ipairs, tonumber, string.format, string.match
local currentuserrole = ngx.ctx.session:getrole()
--local tprint=require("tch.tableprint")

local igd_select = {
  { "igdv1", T'1'},
  { "igdv2", T'2'},
}

local function generate_ddns_supported_services(name)
  -- open the supported services file that come with the ddns package
  local ddns_supported_services , valid_services = {}, {}
  local path = format("/etc/ddns/%s", name)
  local f = io.open(path, "r")
  if f then
    for line in f:lines() do
      --a service in this file is  indicated as a url between quotes, we want a list with urls and name of service in capitals
      local service = line:match('^%b""')
      if service then
        service = service:gsub('"','')
        ddns_supported_services[service] = service
        valid_services[#valid_services + 1] ={ service }
      end
    end
    f:close()
  end
  return ddns_supported_services, valid_services
end

local ddns_ipv4_supported_services , valid_ipv4_services = generate_ddns_supported_services("services")
local ddns_ipv6_supported_services , valid_ipv6_services = generate_ddns_supported_services("services_ipv6")

-- Retrieve GW IP + netmask for use by validation function
local ipdetails = {
  gw = "uci.network.interface.@lan.ipaddr",
  netmask = "uci.network.interface.@lan.netmask"
}
content_helper.getExactContent(ipdetails)

local ddns_ipv4_path = "uci.ddns.service.@myddns_ipv4."
local ddns_ipv6_path = "uci.ddns.service.@myddns_ipv6."
local ddns_ipv4 = proxy.get(ddns_ipv4_path)
local ddns_ipv6 = proxy.get(ddns_ipv6_path)

local ddns_status_data = {
  ddns_status = "rpc.ddns.status",
}

-- DMZ / DynDNS / UPnP switches Only one handleQuery in a page
local qry_params = {
  DMZ_enable = "rpc.network.firewall.dmz.enable",
  DMZ_destinationip = "rpc.network.firewall.dmz.redirect.dest_ip",
  upnp_status = "uci.upnpd.config.enable_upnp",
  upnp_natpmp = "uci.upnpd.config.enable_natpmp",
  upnp_secure_mode = "uci.upnpd.config.secure_mode",
  upnp_igd_version = "uci.upnpd.config.datamodel_version",
}

local ddns_ipv4_params = {
  ddnsv4_enabled = "uci.ddns.service.@myddns_ipv4.enabled",
  ddnsv4_service_name = "uci.ddns.service.@myddns_ipv4.service_name",
  ddnsv4_domain = "uci.ddns.service.@myddns_ipv4.domain",
  ddnsv4_lookup_host = "uci.ddns.service.@myddns_ipv4.lookup_host",
  ddnsv4_username = "uci.ddns.service.@myddns_ipv4.username",
  ddnsv4_password = "uci.ddns.service.@myddns_ipv4.password",
  ddnsv4_usehttps = "uci.ddns.service.@myddns_ipv4.use_https",
  ddnsv4_cacert = "uci.ddns.service.@myddns_ipv4.cacert",
}

local ddns_ipv6_params = {
  ddnsv6_enabled = "uci.ddns.service.@myddns_ipv6.enabled",
  ddnsv6_service_name = "uci.ddns.service.@myddns_ipv6.service_name",
  ddnsv6_domain = "uci.ddns.service.@myddns_ipv6.domain",
  ddnsv6_lookup_host = "uci.ddns.service.@myddns_ipv6.lookup_host",
  ddnsv6_username = "uci.ddns.service.@myddns_ipv6.username",
  ddnsv6_password = "uci.ddns.service.@myddns_ipv6.password",
  ddnsv6_usehttps = "uci.ddns.service.@myddns_ipv6.use_https",
  ddnsv6_cacert = "uci.ddns.service.@myddns_ipv6.cacert",
}

-- Shortcuts to validation helpers to make lines shorter
local gVIPIL = post_helper.getValidationIfPropInList
local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local vSIP = post_helper.validateStringIsPort
local vSIPR = post_helper.validateStringIsPortRange
local gVP = post_helper.getValidationPassword
local gVSIDIP = post_helper.getValidateStringIsDeviceIPv4
local vSIIP6 = post_helper.validateStringIsIPv6
local vSIDIP = gVSIDIP(ipdetails.gw, ipdetails.netmask)
local gAV = post_helper.getAndValidation
local gOrV = post_helper.getOrValidation
local vQTN = post_helper.validateQTN
local vSIDN = post_helper.validateStringIsDomainName
local vSIIP = post_helper.validateStringIsIP
local validateDMZ = post_helper.validateDMZ

local function set_lookup_hostv4(value, object, key)
  object["ddnsv4_lookup_host"] = object["ddnsv4_domain"]
  return true
end

local function set_lookup_hostv6(value, object, key)
  object["ddnsv6_lookup_host"] = object["ddnsv6_domain"]
  return true
end

local function set_cacertv4(_, object)
  object["ddnsv4_cacert"] = "IGNORE"
  return true
end

local function set_cacertv6(_, object)
  object["ddnsv6_cacert"] = "IGNORE"
  return true
end

local qry_valid = {
  DMZ_enable = vB,
  DMZ_destinationip = validateDMZ,
  upnp_status = vB,
  upnp_natpmp = vB,
  upnp_secure_mode = vB,
  upnp_igd_version = gVIES(igd_select),
}

local ddns_ipv4_valid = {
  ddnsv4_enabled = vB,
  ddnsv4_service_name = gAV(gVIPIL(gOrV(vSIDN, vSIIP), "ddnsv4_enabled", {"1"}), gVIPIL(gVIES(valid_ipv4_services), "ddnsv4_enabled", {"1"})),
  ddnsv4_username = gVIPIL(vNES, "ddnsv4_enabled", {"1"}),
  ddnsv4_password = gVP(gVIPIL(vNES, "ddnsv4_enabled", {"1"})),
  ddnsv4_domain = gAV(gOrV(vSIDN, vSIIP), gVIPIL(vNES, "ddnsv4_enabled", {"1"})),
  ddnsv4_lookup_host = set_lookup_hostv4,
  ddnsv4_usehttps = vB,
  ddnsv4_cacert = set_cacertv4,
}

local ddns_ipv6_valid = {
  ddnsv6_enabled = vB,
  ddnsv6_service_name = gAV(gVIPIL(gOrV(vSIDN, vSIIP6), "ddnsv6_enabled", {"1"}), gVIPIL(gVIES(valid_ipv6_services), "ddnsv6_enabled", {"1"})),
  ddnsv6_username = gVIPIL(vNES, "ddnsv6_enabled", {"1"}),
  ddnsv6_password = gVP(gVIPIL(vNES, "ddnsv6_enabled", {"1"})),
  ddnsv6_domain = gAV(gOrV(vSIDN, vSIIP6), gVIPIL(vNES, "ddnsv6_enabled", {"1"})),
  ddnsv6_lookup_host = set_lookup_hostv6,
  ddnsv6_usehttps = vB,
  ddnsv6_cacert = set_cacertv6,
}

local ddns_state_map = {
  disabled = T"Disabled",
  updating = T"Updating",
  updated = T"Updated",
  error = T"Error",
}

local ddns_light_map = {
  disabled = "off",
  updating = "orange",
  updated = "green",
  error = "red",
}

local del_pfw_index = nil

local wol = io.open("/lib/functions/firewall-wol.sh", "r") and proxy.get("uci.wol.config.")
if wol then
  qry_params.WOL_enabled = "uci.wol.config.enabled"
  qry_params.WOL_port = "uci.wol.config.src_dport"
  qry_params.WOL_dest_ip = "uci.wol.config.dest_ip"
  qry_valid.WOL_enabled = vB
  qry_valid.WOL_port = vSIP
  qry_valid.WOL_dest_ip = vSIIP
end

local qry_data, qry_helpmsg = post_helper.handleQuery(qry_params, qry_valid)

local ddns_ipv4_data, ddns_ipv4_helpmsg
local ddns_ipv4_status, ddns_ipv4_update_info

if ddns_ipv4 then
  ddns_ipv4_data, ddns_ipv4_helpmsg = post_helper.handleQuery(ddns_ipv4_params, ddns_ipv4_valid)
end

local ddns_ipv6_data, ddns_ipv6_helpmsg
local ddns_ipv6_status, ddns_ipv6_update_info

if ddns_ipv6 then
  ddns_ipv6_data, ddns_ipv6_helpmsg = post_helper.handleQuery(ddns_ipv6_params, ddns_ipv6_valid)
end

content_helper.getExactContent(ddns_status_data)
local status = ddns_status_data.ddns_status

local function get_ddns_status(service_name, ddns_enabled)
  local ddns_status = "error"
  local ddns_update_info = "No error received from server"
  local service_status

  if status then
    for x in string.gmatch(status, '([^%]]+)') do
      service_status = match(x, service_name .. "%[(.+)")
      if service_status then
        break
      end
    end
  end
  if ddns_enabled ~= "1" then
    ddns_status = "disabled"
  else
    if action == "SAVE" then
      ddns_status = "updating"
    elseif service_status then
      if service_status == "Domain's IP updated" then
        ddns_status = "updated"
      elseif service_status == "No error received from server" then
        ddns_status = "updating"
      else
        ddns_status = "error"
      end
      ddns_update_info = format("%s", service_status)
    end
  end
  return ddns_status, ddns_update_info
end

if ddns_ipv4 then
  ddns_ipv4_status, ddns_ipv4_update_info = get_ddns_status("myddns_ipv4", ddns_ipv4_data.ddnsv4_enabled)
end

if ddns_ipv6 then
  ddns_ipv6_status, ddns_ipv6_update_info = get_ddns_status("myddns_ipv6", ddns_ipv6_data.ddnsv6_enabled)
end

local action
if ngx.var.request_method == "POST" then
  action = ngx.req.get_post_args().action
  local content = ngx.req.get_post_args()

  if content.action == "TABLE-DELETE" and content.tableid == "portforwarding" then
  del_pfw_index = tonumber(content.index)
  end
end


-- Ipv6 data retrieval - Start
-- Useful pieces of transformer data for ipv6
-- Ipv6 data retrieval - End
-- DNS rules
local policy_select = {{"any", "any"}}
local outpolicy_select = {{"copy", "copy"}, {"default", "default"}}
local policy_pn = proxy.getPN("uci.mwan.policy.",true)
if policy_pn then
  for _,v in ipairs(policy_pn) do
    local path = v["path"]
    local policy_name = match(path,"@([^@%.]-)%.")
    if policy_name then
      policy_select[#policy_select + 1] = {policy_name, policy_name}
      outpolicy_select[#outpolicy_select + 1] = {policy_name, policy_name}
    end
  end
end
local intfs_select = {{"default", "default"}}
local intfs_pn = proxy.getPN("uci.network.interface.",true)
if intfs_pn then
  for _,v in ipairs(intfs_pn) do
    local path = v["path"]
    local intf_name = match(path,"@([^@%.]-)%.")
    if intf_name and intf_name ~= "loopback" and intf_name ~= "lan" then
      intfs_select[#intfs_select + 1] = {intf_name,intf_name}
    end
  end
end
local dnsrule_columns = {
  {
    header = "",
    name = "enable",
    param = "enable",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }}
  },
  {
    header = T"Domain",
    name = "domain",
    param = "domain",
    type = "text",
    unique = true,
    attr = { input = { class="span2", maxlength="30"} },
  },
  {
    header = T"DNS Set",
    name = "dnsset",
    param = "dnsset",
    type = "select",
    values = intfs_select,
    attr = { select = { class="span1" } },
  },
  {
    header = T"Policy",
    name = "policy",
    param = "policy",
    type = "select",
    values = policy_select,
    attr = { select = { class="span1" } },
  },
  {
    header = T"Out Policy",
    name = "outpolicy",
    param = "outpolicy",
    type = "select",
    values = outpolicy_select,
    attr = { select = { class="span1" } },
  },
}
local function dns_sort(rule1, rule2)
  return rule1.paramindex < rule2.paramindex
end

local domain_valid = {
   domain = gOrV(vSIDN, vSIIP),
}
local dnsrule_options = {
  tableid = "dnsrules",
  basepath = "uci.dhcp.dnsrule.@.",
  createMsg = T"Add new DNS Rule",
  sorted = dns_sort,
  objectName  = post_helper.getRandomKey(),
  addNamedObject = true
}

local dnsrule_data, dnsrule_helpmsg = post_helper.handleTableQuery(dnsrule_columns, dnsrule_options, nil, nil, domain_valid)
for k, v in pairs(dnsrule_data) do
  if(v[1] == nil or v[1] == "") then
    v[1] = "1"
  end
  if(v[4] == nil or v[4] == "") then
    v[4] = "any"
  end
  if(v[5] == nil or v[5] == "") then
    v[5] = "copy"
  end
end

-- UPnP forwarding rules
local upnp_columns = {
  {
    header = T"Protocol",
    name = "protocol",
    param = "proto",
    default = "tcp",
    type = "select",
    values = {
      { "tcp", "TCP"},
      { "udp", "UDP"},
      { "tcpudp", "TCP+UDP"}
    },
    attr = { select = { class="span2" } },
  },
  {
    header = T"WAN port",
    name = "wanport",
    param = "src_dport",
    type = "text",
    attr = { input = { class="span1", maxlength="5" } },
  },
  {
    header = T"LAN port",
    name = "lanport",
    param = "dest_port",
    type = "text",
    attr = { input = { class="span1", maxlength="5" } },
  },
  {
    header = T"Destination",
    name = "destinationip",
    param = "dest_ip",
    type = "text",
    attr = { input = { class="span2", maxlength="15"} },
  },
  {
    header = T"Description",
    name = "description",
    param = "description",
    type = "text",
    attr = { input = { class="span2", maxlength="15"} },
  },
}

local upnp_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "upnpportforwarding",
  basepath = "sys.upnp.redirect.",
}

local upnp_data, upnp_helpmsg = post_helper.handleTableQuery(upnp_columns, upnp_options, nil, nil, nil)

local help_link = nil
  ngx.print('\
');  ngx.print(ui_helper.createHeader(T"WAN Services", true, true, nil, help_link) ); ngx.print('\
<div class="modal-body update">\
  <form class="form-horizontal" method="post" action="modals/wanservices-modal.lp">\
  ');  
  ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  if ddns_ipv4 ~= nil then
    ngx.print('\
  <fieldset>\
    <legend>');  ngx.print( T"IPv4 Dynamic DNS" ); ngx.print('</legend>\
    ');  
     ngx.print(
      ui_helper.createLight(T"Status", nil, ddns_state_map[ddns_ipv4_status], { light = { class = ddns_light_map[ddns_ipv4_status] } }),
      ui_helper.createSwitch(T"Enabled", "ddnsv4_enabled", ddns_ipv4_data["ddnsv4_enabled"], nil, ddns_ipv4_helpmsg["ddnsv4_enabled"]),
      ui_helper.createInputText(T"Service Name", "ddnsv4_service_name", ddns_ipv4_data["ddnsv4_service_name"], {autocomplete = ddns_ipv4_supported_services}, ddns_ipv4_helpmsg["ddnsv4_service_name"]),
      ui_helper.createSwitch(T"HTTPS", "ddnsv4_usehttps", ddns_ipv4_data["ddnsv4_usehttps"], nil, ddns_ipv4_helpmsg["ddnsv4_usehttps"]),
      ui_helper.createLabel(T"", T"Note: HTTPS mode will enable encryption but not certificate-based authentication of DynDNS service", { span = {class = "span12"},}),
      ui_helper.createInputText(T"Domain", "ddnsv4_domain", ddns_ipv4_data["ddnsv4_domain"], nil, ddns_ipv4_helpmsg["ddnsv4_domain"]),
      ui_helper.createInputText(T"User Name", "ddnsv4_username", ddns_ipv4_data["ddnsv4_username"], nil, ddns_ipv4_helpmsg["ddnsv4_username"]),
      ui_helper.createInputPassword(T"Password", "ddnsv4_password", ddns_ipv4_data["ddnsv4_password"], nil, ddns_ipv4_helpmsg["ddnsv4_password"])
    )
    if ddns_ipv4_data["ddnsv4_enabled"] == "1" then
      local basic = {
        span = {
          class = "span12"
        },
      }
      ngx.print(
        ui_helper.createLabel(T"DynDNS Information", ddns_ipv4_update_info, basic)
      )
    end
    ngx.print('\
  </fieldset>\
  ');  end   ngx.print('\
  ');  
  if ddns_ipv6 ~= nil then
    ngx.print('\
  <fieldset>\
    <legend>');  ngx.print( T"IPv6 Dynamic DNS" ); ngx.print('</legend>\
    ');  
     ngx.print(
      ui_helper.createLight(T"Status", nil, ddns_state_map[ddns_ipv6_status], { light = { class = ddns_light_map[ddns_ipv6_status] } }),
      ui_helper.createSwitch(T"Enabled", "ddnsv6_enabled", ddns_ipv6_data["ddnsv6_enabled"], nil, ddns_ipv6_helpmsg["ddnsv6_enabled"]),
      ui_helper.createInputText(T"Service Name", "ddnsv6_service_name", ddns_ipv6_data["ddnsv6_service_name"], {autocomplete = ddns_ipv6_supported_services}, ddns_ipv6_helpmsg["ddnsv6_service_name"]),
      ui_helper.createSwitch(T"HTTPS", "ddnsv6_usehttps", ddns_ipv6_data["ddnsv6_usehttps"], nil, ddns_ipv6_helpmsg["ddnsv6_usehttps"]),
      ui_helper.createLabel(T"", T"Note: HTTPS mode will enable encryption but not certificate-based authentication of DynDNS service", { span = {class = "span12"},}),
      ui_helper.createInputText(T"Domain", "ddnsv6_domain", ddns_ipv6_data["ddnsv6_domain"], nil, ddns_ipv6_helpmsg["ddnsv6_domain"]),
      ui_helper.createInputText(T"User Name", "ddnsv6_username", ddns_ipv6_data["ddnsv6_username"], nil, ddns_ipv6_helpmsg["ddnsv6_username"]),
      ui_helper.createInputPassword(T"Password", "ddnsv6_password", ddns_ipv6_data["ddnsv6_password"], nil, ddns_ipv6_helpmsg["ddnsv6_password"])
    )
    if ddns_ipv6_data["ddnsv6_enabled"] == "1" then
      local basic = {
        span = {
          class = "span12"
        },
      }
      ngx.print(
        ui_helper.createLabel(T"DynDNS Information", ddns_ipv6_update_info, basic)
      )
    end
    ngx.print('\
  </fieldset>\
  ');  end   ngx.print('\
  <fieldset>\
    <legend>');  ngx.print( T"DMZ" ); ngx.print('</legend>\
    ');  
    -- Switch for toggling DMZ state
    local dmzipattr = {
      autocomplete = hosts_ac
    }
    local DMZ_destinationmac = {
      "rpc.network.firewall.dmz.redirect.dest_mac",
    }
    content_helper.getExactContent(DMZ_destinationmac)
    ngx.print(
      ui_helper.createSwitch(T"Enabled", "DMZ_enable", qry_data["DMZ_enable"], nil, qry_helpmsg["DMZ_enable"]),
      ui_helper.createInputText(T"Destination IP", "DMZ_destinationip", qry_data["DMZ_destinationip"], dmzipattr, qry_helpmsg["DMZ_destinationip"]),
      ui_helper.createLabel(T"Destination MAC", DMZ_destinationmac[1])
    )
    ngx.print('\
  </fieldset>\
  ');  if wol then  ngx.print('\
    <fieldset>\
    <legend>');  ngx.print( T"Wake on LAN over the Internet" ); ngx.print('</legend>\
    ');  
      ngx.print(
        ui_helper.createSwitch(T"Enabled", "WOL_enabled", qry_data["WOL_enabled"], nil, qry_helpmsg["WOL_enabled"]),
        ui_helper.createInputText(T"Destination IP", "WOL_dest_ip", qry_data["WOL_dest_ip"], nil, qry_helpmsg["WOL_dest_ip"]),
        ui_helper.createInputText(T"WAN port", "WOL_port", qry_data["WOL_port"], nil, qry_helpmsg["WOL_port"])
      )
      ngx.print('\
    </fieldset>\
  ');  end  ngx.print('\
  <fieldset>\
    <legend>');  ngx.print( T"UPnP" ); ngx.print('</legend>\
    ');  
    local advanced = { group = { class = "advanced hide" }}
    ngx.print(
      ui_helper.createSwitch(T"UPnP Enabled", "upnp_status", qry_data["upnp_status"], advanced, qry_helpmsg["upnp_status"]),
      ui_helper.createSwitch(T"NAT-PMP Enabled", "upnp_natpmp", qry_data["upnp_natpmp"], advanced, qry_helpmsg["upnp_natpmp"]),
      ui_helper.createSwitch(T"Secure Mode Enabled", "upnp_secure_mode", qry_data["upnp_secure_mode"], advanced, qry_helpmsg["upnp_secure_mode"]),
      ui_helper.createInputSelect(T"Datamodel Version", "upnp_igd_version", igd_select, qry_data.upnp_igd_version, advanced),
      ui_helper.createLabel(T"", T"Note: Datamodel version <b>1</b> may be required for older applications.", {span = {class = "span12 advanced hide"},}),
      ui_helper.createTable(upnp_columns, upnp_data, upnp_options, nil, upnp_helpmsg)
    )
    ngx.print('\
  </fieldset>\
  <fieldset>\
    <legend>');  ngx.print( T"MWAN DNS Rules" ); ngx.print('</legend>\
    ');  
    ngx.print(ui_helper.createTable(dnsrule_columns, dnsrule_data, dnsrule_options, nil, dnsrule_helpmsg))
    ngx.print('\
  </fieldset>\
  </form>\
</div>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
'); 