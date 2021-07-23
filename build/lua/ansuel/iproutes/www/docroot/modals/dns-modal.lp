--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")

local format, match, untaint = string.format, string.match, string.untaint
local vSIIP = post_helper.validateStringIsIP
local vSIDN = post_helper.validateStringIsDomainName

local dns_columns = {
  {
    header = T"Interface",
    name = "Ifname",
    param = "Ifname",
    type = "text",
    readonly = true,
    attr = { input = { class="span2 centered" } },
  },
  {
    header = T"DNS Used",
    name = "dnsservers",
    param = "dnsservers",
    type = "text",
    readonly = true,
    attr = { input = { class="span2 centered" } },
  },
  {
    header = T"Custom DNS",
    name = "static_dns_servers",
    param = "static_dns_servers",
    type = "text",
    attr = { input = { class="span2 centered" } },
  },
}

local dns_options = {
  canEdit = true,
  canAdd = false,
  canDelete = false,
  tableid = "dnsinfo",
  basepath = "rpc.network.interface.",
}

local function validateCustomDNS(value, object, key)
  local servers = split(value,",")
  local ret, err, server
  for _,server in pairs(servers) do
    ret, err = vSII(server)
    if not ret then
      return nil, err
    end
  end
  return true
end

local dns_valid = {
  static_dns_servers = validateCustomDNS
}

local dns_filter = function(data)
  if data.type == "wan" or data.type == "lan" then
    data.Ifname = data.paramindex:gsub("^@","")
    return true
  end
  return false
end

local dns_data, dns_helpmsg = post_helper.handleTableQuery(dns_columns, dns_options, dns_filter, nil, dns_valid)

local address_columns = {
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

local function validateCustomAddress(value, object, key) 
  local ret, err
  ret, err = vSIDN(object.domain)
  if ret then
    ret, err = vSIIP(object.value)
    if ret then
      object.value = format("/%s/%s", object.domain, object.value)
      return true
    end
  end
  return ret, err
end

local address_valid = {
  domain = vSIDN,
  value = validateCustomAddress
}

local function address_path()
  local dnsmidx, dnsmif
  for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.", true)) do
    for _,dnsmif in pairs(proxy.get(dnsmidx.path.."interface.")) do
      if dnsmif.value == "lan" then
        return dnsmidx.path .. "address.@."
      end
    end
  end
end

local address_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add new custom DNS entry",
  tableid = "addresses",
  basepath = address_path(),
}

local function address_filter(data)
  if data.value then
    local domain, ip = match(untaint(data.value), "/([^/]+)/(.+)")
    if domain and ip then
      data.domain = domain
      data.ip = ip
      return true
    end
  end
  return false
end

local address_data, address_helpmsg = post_helper.handleTableQuery(address_columns, address_options, address_filter, nil, address_valid)
local k,v
for k,v in pairs (address_data) do
  local domain, ip = match(untaint(v[2]), "/([^/]+)/(.+)")
  address_data[k][1] = domain
  address_data[k][2] = ip
end

ngx.print(ui_helper.createHeader(T"IP Routing", false, true))   ngx.print('\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-internet.lp")
  ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dns-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('\
    <fieldset>\
      <legend>'); ngx.print(T"DNS Servers"); ngx.print('</legend>\
      ');
      ngx.print(ui_helper.createTable(dns_columns, dns_data, dns_options, nil, dns_helpmsg))
      ngx.print('\
    </fieldset>\
    <fieldset>\
      <legend>'); ngx.print(T"Custom DNS Entries"); ngx.print('</legend>\
      ');
      ngx.print(ui_helper.createTable(address_columns, address_data, address_options, nil, address_helpmsg))
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
<script>\
$("domain,ip").on("input", function(){\
  $("address").val("/"+$("domain")+"/"+$("ip"));\
});\
</script>\
');