--pretranslated: do not change this file
 
-- Localization
gettext.textdomain('webui-core')

local ddns_helper = require("wanservices-ddns_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local message_helper = require("web.uimessage_helper")
local ui_helper = require("web.ui_helper")

local format,sub = string.format,string.sub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local uniqueKey = post_helper.getRandomKey()

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local services_status = ddns_helper.get_services_status()

local ddns_columns = {
  { --[1]
    header = T"Enabled",
    name = "enabled",
    param = "enabled",
    type = "switch",
    readonly = true,
  },
  { --[2]
    header = T"State",
    param = "ddns_light",
    type = "text",
    readonly = true,
  },
  { --[3]
    header = T"Status",
    param = "ddns_status",
    type = "text",
    readonly = true,
  },
  { --[4]
    header = T"Service",
    name = "Service",
    param = "service_name",
    type = "select",
    values = ddns_helper.valid_services,
    readonly = true,
  },
  { --[5]
    header = T"Type",
    param = "interface",
    type = "select",
    values = ddns_helper.intf_map,
    readonly = true,
  },
  { --[6]
    header = T"Domain",
    name = "Domain",
    param = "domain",
    type = "text",
    readonly = true,
  },
  { --[7]
    header = T"Check",
    name = "CheckInterval",
    param = "check_interval",
    type = "text",
    readonly = true,
  },
  { --[8]
    header = T"",
    name = "CheckUnit",
    param = "check_unit",
    type = "hidden",
  },
  { --[9]
    header = T"Retry",
    name = "RetryInterval",
    param = "retry_interval",
    type = "text",
    readonly = true,
  },
  { --[10]
    header = T"",
    name = "RetryUnit",
    param = "retry_unit",
    type = "hidden",
  },
  { --[11]
    header = T"Force",
    name = "ForceInterval",
    param = "force_interval",
    type = "text",
    readonly = true,
  },
  { --[12]
    header = T"",
    name = "ForceUnit",
    param = "force_unit",
    type = "hidden",
  },
  { --[13]
    header = T"",
    legend = T"<small>Service Configuration</small>",
    name = "config",
    type = "aggregate",
    synthesis = nil,
    subcolumns = {
      { --[13][1]
        header = T"Enabled",
        name = "State",
        param = "enabled",
        type = "switch",
        default = "1",
      },
      { --[13][2]
        header = T"Service",
        name = "Service",
        param = "service_name",
        type = "select",
        values = ddns_helper.valid_ipv4_services,
        attr = { select = { class="span3" } },
      },
      { --[13][3]
        header = T"Type",
        name = "Interface",
        param = "interface",
        type = "select",
        values = { { "wan", T"IPv4"} },
        readonly = true,
      },
      { --[13][4]
        header = T"Domain",
        name = "Domain",
        param = "domain",
        type = "text",
        attr = { input = { class="span3"} },
      },
      { --[13][5]
        header = T"Username",
        name = "Username",
        param = "username",
        type = "text",
        attr = { input = { class="span2" } },
      },
      { --[13][6]
        header = T"Password/Token",
        name = "Password",
        param = "password",
        type = "password",
        attr = { input = { class="span2" } },
      },
      { --[13][7]
        header = T"HTTPS<span class='icon-question-sign' title='HTTPS mode will enable encryption but NOT certificate-based authentication of Dynamic DNS service'></span>",
        name = "HTTPS",
        param = "use_https",
        default = "1",
        type = "switch",
      },
      { --[13][8]
        header = T"Check Interval",
        name = "CheckInterval",
        param = "check_interval",
        type = "text",
        attr = { input = { type = "number",min = "1",class = "span1" } },
        default = "10",
      },
      { --[13][9]
        header = T"Retry Interval",
        name = "RetryInterval",
        param = "retry_interval",
        type = "text",
        attr = { input = { type = "number",min = "1",class = "span1" } },
        default = "60",
      },
      { --[13][10]
        header = T"Force Interval",
        name = "ForceInterval",
        param = "force_interval",
        type = "text",
        attr = { input = { type = "number",min = "1",class = "span1" } },
        default = "72",
      },
      { --[13][11]
        header = T"",
        name = "CheckUnit",
        param = "check_unit",
        type = "hidden",
        default = "minutes",
      },
      { --[13][12]
        header = T"",
        name = "RetryUnit",
        param = "retry_unit",
        type = "hidden",
        default = "seconds",
      },
      { --[13][13]
        header = T"",
        name = "ForceUnit",
        param = "force_unit",
        type = "hidden",
        default = "hours",
      },
    }
  },
}

local gVIES = post_helper.getValidateInEnumSelect
local vNES = post_helper.validateNonEmptyString
local gVNIR = post_helper.getValidateNumberInRange
local vSIDN = post_helper.validateStringIsDomainName
local vB = post_helper.validateBoolean

local ddns_valid = {
  State = vB,
  Service = gVIES(ddns_helper.valid_ipv4_services),
  Domain = post_helper.getAndValidation(vNES,post_helper.getOrValidation(vSIDN,post_helper.validateStringIsIP)),
  Username = ddns_helper.validate_username,
  Password = post_helper.getValidationPassword(vNES),
  HTTPS = vB,
  CheckInterval = gVNIR(1,99999999),
  CheckUnit = gVIES(ddns_helper.unit_map),
  ForceInterval = gVNIR(1,99999999),
  ForceUnit = gVIES(ddns_helper.unit_map),
  RetryInterval = gVNIR(1,99999999),
  RetryUnit = gVIES(ddns_helper.unit_map),
}

local function onAddDDns(index,content)
  local ddns_path = "uci.ddns.service.@"..index
  proxy.set(ddns_path..".cacert","IGNORE")
  proxy.set(ddns_path..".lookup_host",untaint(content.Domain))
  proxy.set(ddns_path..".interface",untaint(content.Interface))
  proxy.set(ddns_path..".ip_network",untaint(content.Interface))
  proxy.set(ddns_path..".ip_source","network")
  if content.Interface == "wan" then
    proxy.set(ddns_path..".use_ipv6","0")
  else
    proxy.set(ddns_path..".use_ipv6","1")
  end
  proxy.apply()
end

local function onUpdateDDns(index,content)
  local ddns_path = "uci.ddns.service.@"..index
  local domain = untaint(content.Domain)
  local interface = untaint(content.Domain)
  local function getValue(path)
    local result = proxy.get(path)
    if result then
      return result[1].value
    end
    return nil
  end
  if not domain then
    domain = getValue(ddns_path..".domain")
  end
  local ddns_lookup_host_path = ddns_path..".lookup_host"
  local ddns_lookup_host = getValue(ddns_lookup_host_path)
  if domain and ddns_lookup_host and domain ~= ddns_lookup_host then
    proxy.set(ddns_lookup_host_path,untaint(content.Domain))
  end
  if not interface then
    interface = getValue(ddns_path..".interface")
  end
  local ddns_ip_network_path = ddns_path..".ip_network"
  local ddns_ip_network = getValue(ddns_ip_network_path)
  if interface and ddns_ip_network and domain ~= ddns_ip_network then
    proxy.set(ddns_ip_network_path,untaint(content.Interface))
  end
end

local ddns_options = {
  tableid = "ddnsoptions",
  basepath = "uci.ddns.service.@.",
  createMsg = T"Add IPv4 DDNS Rule",
  objectName  = "myddns_"..uniqueKey,
  addNamedObject = true,
  canEdit = true,
  canAdd = true,
  canDelete = true,
  onAdd = onAddDDns,
  onModify = onUpdateDDns,
}
if ddns_helper.wan6 then
  ddns_options.newList = { { text = T"Add IPv6 DDNS Rule",values = {} }, }
end

local action,index,listid
if ngx.var.request_method == "POST" then
  local content = ngx.req.get_post_args()
  action = content.action
  index = tonumber(content.index)
  listid = content.listid
end

local dfltCheckInterval,dfltCheckUnit = "10","minutes"
local dfltRetryInterval,dfltRetryUnit = "60","seconds"
local dfltForceInterval,dfltForceUnit = "72","hours"

local function ddns_filter(data)
  local ddns_status,ddns_message = ddns_helper.to_status(services_status,data.paramindex)
  data.ddns_light = ui_helper.createSimpleLight(ddns_helper.ddns_light_map[ddns_status],ddns_helper.ddns_state_map[ddns_status])
  data.ddns_status = ddns_message
  data.check_interval = (not data.check_interval or data.check_interval == "") and dfltCheckInterval or data.check_interval
  data.check_unit = (not data.check_unit or data.check_unit == "") and dfltCheckUnit or data.check_unit
  data.retry_interval = (not data.retry_interval or data.retry_interval == "") and dfltRetryInterval or data.retry_interval
  data.retry_unit = (not data.retry_unit or data.retry_unit == "") and dfltRetryUnit or data.retry_unit
  data.force_interval = (not data.force_interval or data.force_interval == "") and dfltForceInterval or data.force_interval
  data.force_unit = (not data.force_unit or data.force_unit == "") and dfltForceUnit or data.force_unit
  if action ~= "TABLE-EDIT" then
    data.check_interval = format("%s%s",data.check_interval,sub(untaint(data.check_unit),1,1))
    data.retry_interval = format("%s%s",data.retry_interval,sub(untaint(data.retry_unit),1,1))
    data.force_interval = format("%s%s",data.force_interval,sub(untaint(data.force_unit),1,1))
  end
  return true
end

local ddns_data,ddns_helpmsg = post_helper.handleTableQuery(ddns_columns,ddns_options,ddns_filter,nil,ddns_valid)
if (action == "TABLE-NEW-LIST" and listid == "1") or ((action == "TABLE-EDIT" or action == "TABLE-MODIFY") and ddns_data[index][5] == "wan6") then
  ddns_columns[13].subcolumns[2].values = ddns_helper.valid_ipv6_services
  ddns_columns[13].subcolumns[3].values = { { "wan6", T"IPv6"} }
  ddns_valid.Service = gVIES(ddns_helper.valid_ipv6_services)
end

local acme_domain,acme_data,acme_helpmsg
local acme_params = {
  acme_enabled = "rpc.gui.acme.enabled",
}
local acme_valid = {
  acme_enabled = vB
}
for k=1,#ddns_data do
  if ddns_data[k][5] ~= "yourhost.example.com" then
    acme_domain = vSIDN(ddns_data[k][5])
    if acme_domain then
      break
    end
  end
end
if acme_domain then
  acme_data,acme_helpmsg = post_helper.handleQuery(acme_params,acme_valid)
else
  proxy.set("rpc.gui.acme.enabled","0")
end

local info_box = {
  alert = {
    class = "alert-info"
  },
}

ngx.print(ui_helper.createHeader(T"WAN Services",false,true),'\
<div class="modal-body update">\
  '); lp.include("tabs-wanservices.lp");ngx.print('\
  <form class="form-horizontal" method="post" action="modals/wanservices-ddns-modal.lp">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>',T"Dynamic DNS",'</legend>',
      ui_helper.createTable(ddns_columns,ddns_data,ddns_options,nil,ddns_helpmsg),'\
    </fieldset>\
    <script src="js/wanservices-ddns-modal.js"></script>\
    <script>\
      addUnitSelect("Check","',dfltCheckInterval,'","',dfltCheckUnit,'");\
      addUnitSelect("Retry","',dfltRetryInterval,'","',dfltRetryUnit,'");\
      addUnitSelect("Force","',dfltForceInterval,'","',dfltForceUnit,'");\
    </script>\
    ');
    if acme_domain then
      ngx.print('\
      <fieldset>\
        <legend>',T"SSL Certificates",'</legend>',
          ui_helper.createAlertBlock(T"Enable to automatically issue and renew SSL Certificates for the Dynamic DNS domains.<br>\
            NOTE: You do <i>not</i> have to <i>enable</i> Dynamic DNS (e.g. if you have a static IP and your ISP provides DNS services) for a certificate to be generated,as long as you use a Domain that will resolve to the IP address of this device.",
            info_box),
          ui_helper.createSwitch(T"Server Certficate","acme_enabled",acme_data["acme_enabled"],nil,acme_helpmsg["acme_enabled"]),'\
      </fieldset>\
      ');
    end
    ngx.print('\
  </form>\
</div>',ui_helper.createFooter());