--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local format,match = string.format,string.match

local gVIPIL = post_helper.getValidationIfPropInList
local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local gVP = post_helper.getValidationPassword
local vSIIP6 = post_helper.validateStringIsIPv6
local gAV = post_helper.getAndValidation
local gOrV = post_helper.getOrValidation
local vSIDN = post_helper.validateStringIsDomainName
local vSIIP = post_helper.validateStringIsIP

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local function generate_ddns_supported_services(name)
  -- open the supported services file that come with the ddns package
  local ddns_supported_services,valid_services = {},{}
  local path = format("/etc/ddns/%s",name)
  local f = io.open(path,"r")
  if f then
    for line in f:lines() do
      --a service in this file is  indicated as a url between quotes,we want a list with urls and name of service in capitals
      local service = line:match('^%b""')
      if service then
        service = service:gsub('"','')
        ddns_supported_services[service] = service
        valid_services[#valid_services + 1] ={ service }
      end
    end
    f:close()
  end
  return ddns_supported_services,valid_services
end

local ddns_ipv4_supported_services,valid_ipv4_services = generate_ddns_supported_services("services")
local ddns_ipv6_supported_services,valid_ipv6_services = generate_ddns_supported_services("services_ipv6")

local ddns_status_data = {
  ddns_status = "rpc.ddns.status",
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

local acme_params = {
  acme_enabled = "rpc.gui.acme.enabled",
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

local function set_lookup_hostv4(_,object,_)
  object["ddnsv4_lookup_host"] = object["ddnsv4_domain"]
  return true
end

local function set_lookup_hostv6(_,object,_)
  object["ddnsv6_lookup_host"] = object["ddnsv6_domain"]
  return true
end

local function set_cacertv4(_,object)
  object["ddnsv4_cacert"] = "IGNORE"
  return true
end

local function set_cacertv6(_,object)
  object["ddnsv6_cacert"] = "IGNORE"
  return true
end

local ddns_ipv4_valid = {
  ddnsv4_enabled = vB,
  ddnsv4_service_name = gAV(gVIPIL(gOrV(vSIDN,vSIIP),"ddnsv4_enabled",{"1"}),gVIPIL(gVIES(valid_ipv4_services),"ddnsv4_enabled",{"1"})),
  ddnsv4_username = gVIPIL(vNES,"ddnsv4_enabled",{"1"}),
  ddnsv4_password = gVP(gVIPIL(vNES,"ddnsv4_enabled",{"1"})),
  ddnsv4_domain = gOrV(vSIDN,vSIIP),
  ddnsv4_lookup_host = set_lookup_hostv4,
  ddnsv4_usehttps = vB,
  ddnsv4_cacert = set_cacertv4,
}

local acme_valid = {
  acme_enabled = vB
}

local ddns_ipv6_valid = {
  ddnsv6_enabled = vB,
  ddnsv6_service_name = gAV(gVIPIL(gOrV(vSIDN,vSIIP6),"ddnsv6_enabled",{"1"}),gVIPIL(gVIES(valid_ipv6_services),"ddnsv6_enabled",{"1"})),
  ddnsv6_username = gVIPIL(vNES,"ddnsv6_enabled",{"1"}),
  ddnsv6_password = gVP(gVIPIL(vNES,"ddnsv6_enabled",{"1"})),
  ddnsv6_domain = gOrV(vSIDN,vSIIP6),
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

local ddns_ipv4_data,ddns_ipv4_helpmsg
local ddns_ipv4_status,ddns_ipv4_update_info
local acme_domain,acme_data,acme_helpmsg

ddns_ipv4_data,ddns_ipv4_helpmsg = post_helper.handleQuery(ddns_ipv4_params,ddns_ipv4_valid)
if ddns_ipv4_data["ddnsv4_domain"] ~= "yourhost.example.com" then
  acme_domain = vSIDN(ddns_ipv4_data["ddnsv4_domain"])
end

local ddns_ipv6_data,ddns_ipv6_helpmsg
local ddns_ipv6_status,ddns_ipv6_update_info

ddns_ipv6_data,ddns_ipv6_helpmsg = post_helper.handleQuery(ddns_ipv6_params,ddns_ipv6_valid)
if not acme_domain and ddns_ipv6_data["ddnsv6_domain"] ~= "yourhost.example.com" then
  acme_domain = vSIDN(ddns_ipv6_data["ddnsv6_domain"])
end

if acme_domain then
  acme_data,acme_helpmsg = post_helper.handleQuery(acme_params,acme_valid)
else
  proxy.set("rpc.gui.acme.enabled","0")
end

content_helper.getExactContent(ddns_status_data)
local status = ddns_status_data.ddns_status

local action
if ngx.var.request_method == "POST" then
  action = ngx.req.get_post_args().action
end

local function get_ddns_status(service_name,ddns_enabled)
  local ddns_status = "error"
  local ddns_update_info = "No error received from server"
  local service_status

  if status then
    for x in string.gmatch(status,'([^%]]+)') do
      service_status = match(x,service_name.."%[(.+)")
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
      ddns_update_info = format("%s",service_status)
    end
  end
  return ddns_status,ddns_update_info
end

ddns_ipv4_status,ddns_ipv4_update_info = get_ddns_status("myddns_ipv4",ddns_ipv4_data.ddnsv4_enabled)

ddns_ipv6_status,ddns_ipv6_update_info = get_ddns_status("myddns_ipv6",ddns_ipv6_data.ddnsv6_enabled)

ngx.print(ui_helper.createHeader(T"WAN Services",false,true)); ngx.print('\
<div class="modal-body update">\
  '); lp.include("tabs-wanservices.lp");ngx.print('\
  <form class="form-horizontal" method="post" action="modals/wanservices-ddns-modal.lp">\
  ');
  local basic = {
    span = {
      class = "span12"
    },
  }
  ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.print('\
  <fieldset>\
    <legend>');  ngx.print( T"IPv4 Dynamic DNS" ); ngx.print('</legend>\
    ');
    ngx.print(
      ui_helper.createLight(T"Status",nil,ddns_state_map[ddns_ipv4_status],{ light = { class = ddns_light_map[ddns_ipv4_status] } }),
      ui_helper.createSwitch(T"Enabled","ddnsv4_enabled",ddns_ipv4_data["ddnsv4_enabled"],nil,ddns_ipv4_helpmsg["ddnsv4_enabled"]),
      ui_helper.createInputText(T"Service Name","ddnsv4_service_name",ddns_ipv4_data["ddnsv4_service_name"],{autocomplete = ddns_ipv4_supported_services},ddns_ipv4_helpmsg["ddnsv4_service_name"]),
      ui_helper.createSwitch(T"HTTPS","ddnsv4_usehttps",ddns_ipv4_data["ddnsv4_usehttps"],nil,ddns_ipv4_helpmsg["ddnsv4_usehttps"]),
      ui_helper.createLabel(T"",T"Note: HTTPS mode will enable encryption but not certificate-based authentication of DynDNS service",basic),
      ui_helper.createInputText(T"Domain","ddnsv4_domain",ddns_ipv4_data["ddnsv4_domain"],nil,ddns_ipv4_helpmsg["ddnsv4_domain"]),
      ui_helper.createInputText(T"User Name","ddnsv4_username",ddns_ipv4_data["ddnsv4_username"],nil,ddns_ipv4_helpmsg["ddnsv4_username"]),
      ui_helper.createInputPassword(T"Password","ddnsv4_password",ddns_ipv4_data["ddnsv4_password"],nil,ddns_ipv4_helpmsg["ddnsv4_password"])
    )
    if ddns_ipv4_data["ddnsv4_enabled"] == "1" then
      ngx.print(
        ui_helper.createLabel(T"DynDNS Information",ddns_ipv4_update_info,basic)
      )
    end
    ngx.print('\
  </fieldset>\
  <fieldset>\
    <legend>');  ngx.print( T"IPv6 Dynamic DNS" ); ngx.print('</legend>\
    ');
    ngx.print(
      ui_helper.createLight(T"Status",nil,ddns_state_map[ddns_ipv6_status],{ light = { class = ddns_light_map[ddns_ipv6_status] } }),
      ui_helper.createSwitch(T"Enabled","ddnsv6_enabled",ddns_ipv6_data["ddnsv6_enabled"],nil,ddns_ipv6_helpmsg["ddnsv6_enabled"]),
      ui_helper.createInputText(T"Service Name","ddnsv6_service_name",ddns_ipv6_data["ddnsv6_service_name"],{autocomplete = ddns_ipv6_supported_services},ddns_ipv6_helpmsg["ddnsv6_service_name"]),
      ui_helper.createSwitch(T"HTTPS","ddnsv6_usehttps",ddns_ipv6_data["ddnsv6_usehttps"],nil,ddns_ipv6_helpmsg["ddnsv6_usehttps"]),
      ui_helper.createLabel(T"",T"Note: HTTPS mode will enable encryption but not certificate-based authentication of DynDNS service",basic),
      ui_helper.createInputText(T"Domain","ddnsv6_domain",ddns_ipv6_data["ddnsv6_domain"],nil,ddns_ipv6_helpmsg["ddnsv6_domain"]),
      ui_helper.createInputText(T"User Name","ddnsv6_username",ddns_ipv6_data["ddnsv6_username"],nil,ddns_ipv6_helpmsg["ddnsv6_username"]),
      ui_helper.createInputPassword(T"Password","ddnsv6_password",ddns_ipv6_data["ddnsv6_password"],nil,ddns_ipv6_helpmsg["ddnsv6_password"])
    )
    if ddns_ipv6_data["ddnsv6_enabled"] == "1" then
      ngx.print(
        ui_helper.createLabel(T"DynDNS Information",ddns_ipv6_update_info,basic)
      )
    end
    ngx.print('\
  </fieldset>\
  ');
  if acme_domain then
    ngx.print('\
    <fieldset>\
      <legend>');  ngx.print( T"Server Certificates" ); ngx.print('</legend>\
      ');
      ngx.print(
        ui_helper.createAlertBlock(T"Enable to automatically issue and renew a Server Certificate for the specified Dynamic DNS IPv4 and/or IPv6 Domain.<br>\
          NOTE: You do <i>not</i> have to <i>enable</i> Dynamic DNS (e.g. if you have a static IP and your ISP provides DNS services) for a certificate to be generated,as long as you use a Domain that will resolve to the IP address of this device.",
          { alert = { class = "alert-info" } }),
        ui_helper.createSwitch(T"Server Certficates","acme_enabled",acme_data["acme_enabled"],nil,acme_helpmsg["acme_enabled"])
      )
      local acme_log_attr = {
        span = {
          id = "acme_log",
          style = "display:block;white-space:pre;",
        },
      }
      local acme_log = proxy.get("rpc.gui.acme.log")[1].value
      ngx.print(
        ui_helper.createLabel(T"Certificate Log",untaint(acme_log),acme_log_attr),'\
        <script>\
          var acmeLogFuncID;\
          function updateAcmeLog(){\
            $.post("/ajax/acme-log.lua",[tch.elementCSRFtoken()],function(data){\
              $("#acme_log").html(data["html"]);\
            },"json")\
            .fail(function(response){\
              if(response.status==403||response.status==404){clearInterval(acmeLogFuncID);}\
            });\
          }\
          $().ready(function(){acmeLogFuncID=setInterval(updateAcmeLog,2500);});\
        </script>\
      ')
    ngx.print('\
    </fieldset>\
    ');
  end
  ngx.print('\
  </form>\
</div>\
')
ngx.print(ui_helper.createFooter())
