--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")

local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"
local ipv4DNScolumns = {
  {
    header = T"DNS Server IPv4 Address",
    name = "wanDnsParam",
    param = "value",
    type = "text",
    attr = { input = { class="span2", maxlength="15", pattern = ipv4pattern } },
  },
}
local ipv4DNSoptions = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "dns4server",
  basepath = "uci.network.interface.@wan.dns.@.",
  createMsg = T"Add New IPv4 DNS Server",
  minEntries = 0,
  maxEntries = 4,
  sorted = function(a, b)
    return tonumber(a.paramindex) < tonumber(b.paramindex)
  end
}
local ipv4DNSvalid = {
  wanDnsParam =  post_helper.advancedIPValidation,
}

local ipv6DNScolumns = {
  {
    header = T"DNS Server IPv6 Address",
    name = "wanDnsParam",
    param = "value",
    type = "text",
    attr = { input = { class="span2", maxlength="39" } },
  },
}
local ipv6DNSoptions = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "dns6server",
  basepath = "uci.network.interface.@wan6.dns.@.",
  createMsg = T"Add New IPv6 DNS Server",
  minEntries = 0,
  maxEntries = 4,
  sorted = function(a, b)
    return tonumber(a.paramindex) < tonumber(b.paramindex)
  end
}
local ipv6DNSvalid = {
  wanDnsParam =  post_helper.validateStringIsIPv6,
}
local ipv6_input = {
  group = {
    class ="monitor-uci_wan6_auto monitor-1",
  },
}

local ipv6auto = proxy.get("uci.network.interface.@wan6.auto")[1].value

ngx.print('\
<fieldset>\
  <legend>');  ngx.print( T"Domain Name Server Configuration"  ); ngx.print('</legend>\
  ');  
  local ipv4DNSdata, ipv4DNShelpmsg = post_helper.handleTableQuery(ipv4DNScolumns, ipv4DNSoptions, nil, nil, ipv4DNSvalid)
  ngx.print(ui_helper.createTable(ipv4DNScolumns, ipv4DNSdata, ipv4DNSoptions, nil, ipv4DNShelpmsg))
  if ipv6auto ~= "0" then
    local ipv6DNSdata, ipv6DNShelpmsg = post_helper.handleTableQuery(ipv6DNScolumns, ipv6DNSoptions, nil, nil, ipv6DNSvalid)
    ngx.print(ui_helper.createTable(ipv6DNScolumns, ipv6DNSdata, ipv6DNSoptions, ipv6_input, ipv6DNShelpmsg))
  end
  ngx.print('\
</fieldset>\
');