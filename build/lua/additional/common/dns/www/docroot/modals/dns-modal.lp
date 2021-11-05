--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")

local format,match,untaint = string.format,string.match,string.untaint
local gOV = post_helper.getOptionalValidation
local vB = post_helper.validateBoolean
local vSIIP = post_helper.validateStringIsIP
local vSIDN = post_helper.validateStringIsDomainName

local dnsmasq_path
local dnsmidx,dnsmif
for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.",true)) do
  for _,dnsmif in pairs(proxy.get(dnsmidx.path.."interface.")) do
    if dnsmif.value == "lan" then
      dnsmasq_path = dnsmidx.path
      break
    end
  end
end

local function split(x,sep)
  local sep,fields = sep or ":",{}
  local pattern = format("([^%s]+)",sep)
  for field in x:gmatch(pattern) do
    fields[#fields+1] = field
  end
  return fields
end

local function handleDNSTableQuery(columns,options,filter,defaultObject,mapValidation)
  local data,helpmsg = post_helper.handleTableQuery(columns,options,filter,defaultObject,mapValidation)
  local k,v
  for k,v in pairs (data) do
    local domain,ip = match(untaint(v[2]),"/([^/]+)/(.+)")
    if domain and ip then
      data[k][1] = domain
      data[k][2] = ip
    end
  end
  return data,helpmsg
end

local dns_params = { 
  logqueries = dnsmasq_path.."logqueries",
  noresolv = dnsmasq_path.."noresolv",
}
local dns_valid = { 
  logqueries = vB,
  noresolv = vB,
}
local dns_config,dns_config_helpmsg = post_helper.handleQuery(dns_params,dns_valid)

local custom_dns_columns = {
  {
    header = T"Domain <span class='icon-question-sign' title='Specify a domain name to resolved against a specific server, or leave blank for all domains. The last server listed for duplicated domains will be used unless it does not respond, in which case the previous server will be queried.'></span>",
    name = "domain",
    type = "text",
    attr = { input = { class="span2",placeholder="Leave blank for all domains" } },
  },
  {
    header = T"DNS Server IPv4/IPv6 Address",
    name = "value",
    param = "value",
    type = "text",
    attr = { input = { class="span2" } },
  },
}
local custom_dns_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "custom_dns",
  basepath = dnsmasq_path.."server.@.",
  createMsg = T"Add Custom DNS Server",
  sorted = function(a,b)
    return tonumber(a.paramindex) < tonumber(b.paramindex)
  end
}
local custom_dns_valid = {
  domain = gOV(vSIDN),
  value = function(value,object,key) 
    local ret,err = vSIIP(value)
    if ret then
      local domain = match(untaint(object.domain),"^%s*(.-)%s*$")
      if domain ~= "" then
        object.value = format("/%s/%s",domain,value)
      end
      return true
    end
    return ret,err
  end
}
local custom_dns_data,custom_dns_helpmsg = handleDNSTableQuery(custom_dns_columns,custom_dns_options,nil,nil,custom_dns_valid)

local rewrites_columns = {
  {
    header = T"Domain Address Query",
    name = "domain",
    type = "text",
    attr = { input = { class="span2" } },
  },
  {
    header = T"IPv4/IPv6 Address to Return",
    name = "value",
    param = "value",
    type = "text",
    attr = { input = { class="span2" } },
  },
}
local rewrites_valid = {
  domain = vSIDN,
  value = function(value,object,key) 
    local ret,err = vSIIP(value)
    if ret then
      object.value = format("/%s/%s",object.domain,value)
      return true
    end
    return ret,err
  end
}
local rewrites_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add DNS rewrite",
  tableid = "rewrites",
  basepath = dnsmasq_path.."address.@.",
}
local function rewrites_filter(data)
  if data.value then
    local domain,ip = match(untaint(data.value),"/([^/]+)/(.+)")
    if domain and ip then
      return true
    end
  end
  return false
end
local rewrites_data,rewrites_helpmsg = handleDNSTableQuery(rewrites_columns,rewrites_options,rewrites_filter,nil,rewrites_valid)

local interface_dns_columns = {
  {
    header = T"Interface",
    name = "Ifname",
    param = "Ifname",
    type = "text",
    readonly = true,
  },
  {
    header = "Auto-Discover<br>DNS Servers",
    name = "peerdns",
    param = "peerdns",
    type = "switch",
  },
  {
    header = T"Interface Custom<br>DNS Servers",
    name = "static_dns_servers",
    param = "static_dns_servers",
    type = "text",
    attr = { input = { class="span2" } },
  },
  {
    header = T"DNS Servers Used",
    name = "dnsservers",
    param = "dnsservers",
    type = "text",
    readonly = true,
  },
}
local interface_dns_options = {
  canEdit = true,
  canAdd = false,
  canDelete = false,
  tableid = "dnsinfo",
  basepath = "rpc.network.interface.",
  sorted = function(a,b)
    return a.paramindex < b.paramindex
  end,
}
local interface_dns_valid = {
  static_dns_servers = function(value,object,key)
    local servers = split(value,",")
    local ret,err,server
    for _,server in pairs(servers) do
      ret,err = vSIIP(server)
      if not ret then
        return nil,err
      end
    end
    return true
  end
}
local interface_dns_filter = function(data)
  if data.type == "wan" then
    data.Ifname = data.paramindex:gsub("^@","")
    return true
  end
  return false
end
local interface_dns_data,interface_dns_helpmsg = post_helper.handleTableQuery(interface_dns_columns,interface_dns_options,interface_dns_filter,nil,interface_dns_valid)

ngx.print(ui_helper.createHeader(T"DNS",false,true))   ngx.print('\
<div class="modal-body update">\
  <form class="form-horizontal" method="post" action="modals/dns-modal.lp">\
    <style>#custom_dns>thead>tr>th:nth-child(-n+2),#rewrites>thead>tr>th:nth-child(-n+2){width:46%;}</style>\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    if dnsmasq_path then
      ngx.print('\
      <fieldset>\
        <legend>'); ngx.print(T"Configuration"); ngx.print('</legend>\
        ');
        ngx.print(ui_helper.createSwitch(T"Ignore Interface DNS<span class='icon-question-sign' title='When ON, do NOT fallback to the Interface DNS Servers if the Custom DNS Servers fail to respond.'></span>","noresolv",dns_config["noresolv"],nil,dns_config_helpmsg["noresolv"]))
        ngx.print(ui_helper.createSwitch(T"Log Queries","logqueries",dns_config["logqueries"],nil,dns_config_helpmsg["logqueries"]))
        ngx.print('\
      </fieldset>\
      <fieldset>\
        <legend>'); ngx.print(T"Custom DNS Servers"); ngx.print('</legend>\
        ');
        ngx.print(ui_helper.createTable(custom_dns_columns,custom_dns_data,custom_dns_options,nil,custom_dns_helpmsg))
        ngx.print('\
      </fieldset>\
      <fieldset>\
        <legend>'); ngx.print(T"DNS Rewrites"); ngx.print('</legend>\
        ');
        ngx.print(ui_helper.createTable(rewrites_columns,rewrites_data,rewrites_options,nil,rewrites_helpmsg))
        ngx.print('\
      </fieldset>\
      ')
    end
    ngx.print('\
    <fieldset>\
      <legend>'); ngx.print(T"Interface DNS Servers"); ngx.print('</legend>\
      ');
      ngx.print(ui_helper.createTable(interface_dns_columns,interface_dns_data,interface_dns_options,nil,interface_dns_helpmsg))
      ngx.print(ui_helper.createAlertBlock(T"If you turn off Auto-Discovery for an interface, you <b>MUST</b> add at least one Custom DNS Server for that interface, otherwise your setting will be ignored!",{alert={class="alert alert-warn"}}))
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
');