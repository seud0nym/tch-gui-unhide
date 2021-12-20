--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local ajax_helper = require("web.ajax_helper")
local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")

local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local gOV = post_helper.getOptionalValidation
local vB = post_helper.validateBoolean
local vSII = post_helper.validateStringIsIP
local vSIDN = post_helper.validateStringIsDomainName

local dns_list = {
  { -- IPv4 DNS servers
    {"8.8.8.8",T"Google (8.8.8.8)"},
    {"8.8.4.4",T"Google (8.8.4.4)"},
    {"1.1.1.1",T"Cloudflare (1.1.1.1)"},
    {"1.0.0.1",T"Cloudflare (1.0.0.1)"},
    {"1.1.1.2",T"Cloudflare (1.1.1.2)"},
    {"1.0.0.2",T"Cloudflare (1.0.0.2)"},
    {"1.1.1.3",T"Cloudflare (1.1.1.3)"},
    {"1.0.0.3",T"Cloudflare (1.0.0.3)"},
    {"208.67.222.222",T"OpenDNS (208.67.222.222)"},
    {"208.67.220.220",T"OpenDNS (208.67.220.220)"},
    {"208.67.222.123",T"OpenDNS FamilyShield (208.67.222.123)"},
    {"208.67.220.123",T"OpenDNS FamilyShield (208.67.220.123)"},
    {"9.9.9.9",T"Quad9 (9.9.9.9)"},
    {"149.112.112.112",T"Quad9 (149.112.112.112)"},
    {"64.6.64.6",T"Verisign (64.6.64.6)"},
    {"64.6.65.6",T"Verisign (64.6.65.6)"},
    {"8.26.56.26",T"Comodo (8.26.56.26)"},
    {"8.20.247.20",T"Comodo (8.20.247.20)"},
    {"81.218.119.11",T"GreenTeam (81.218.119.11)"},
    {"209.88.198.133",T"GreenTeam (209.88.198.133)"},
    {"195.46.39.39",T"SafeDNS (195.46.39.39)"},
    {"195.46.39.40",T"SafeDNS (195.46.39.40)"},
    {"216.146.35.35",T"Dyn (216.146.35.35)"},
    {"216.146.36.36",T"Dyn (216.146.36.36)"},
    {"198.101.242.72",T"Alternate DNS (198.101.242.72)"},
    {"23.253.163.53",T"Alternate DNS (23.253.163.53)"},
    {"77.88.8.8",T"Yandex.DNS (77.88.8.8)"},
    {"77.88.8.1",T"Yandex.DNS (77.88.8.1)"},
    {"91.239.100.100",T"UncensoredDNS (91.239.100.100)"},
    {"89.233.43.71",T"UncensoredDNS (89.233.43.71)"},
    {"156.154.70.1",T"Neustar (156.154.70.1)"},
    {"156.154.71.1",T"Neustar (156.154.71.1)"},
    {"45.77.165.194",T"Fourth Estate (45.77.165.194)"},
    {"45.32.36.36",T"Fourth Estate (45.32.36.36)"},
    {"185.228.168.9",T"CleanBrowsing (185.228.168.9)"},
    {"185.228.169.9",T"CleanBrowsing (185.228.169.9)"},
    {"54.252.183.4",T"GetFlix (54.252.183.4)"},
    {"54.252.183.5",T"GetFlix (54.252.183.5)"},
  },
  { -- IPv6 DNS servers
    {"2001-4860-4860--8888",T"Google (2001:4860:4860::8888)"},
    {"2001-4860-4860--8844",T"Google (2001:4860:4860::8844)"},
    {"2606-4700-4700--1111",T"Cloudflare (2606:4700:4700::1111)"},
    {"2606-4700-4700--1001",T"Cloudflare (2606:4700:4700::1001)"},
    {"2606-4700-4700--1112",T"Cloudflare (2606:4700:4700::1112)"},
    {"2606-4700-4700--1002",T"Cloudflare (2606:4700:4700::1002)"},
    {"2606-4700-4700--1113",T"Cloudflare (2606:4700:4700::1113)"},
    {"2606-4700-4700--1003",T"Cloudflare (2606:4700:4700::1003)"},
    {"2620-119-35--35",T"OpenDNS (2620:119:35::35)"},
    {"2620-119-53--53",T"OpenDNS (2620:119:53::53)"},
    {"2620-74-1b--1-1",T"Verisign (2620:74:1b::1:1)"},
    {"2620-74-1c--2-2",T"Verisign (2620:74:1c::2:2)"},
    {"2a02-6b8--feed-0ff",T"Yandex.DNS (2a02:6b8::feed:0ff)"},
    {"2a02-6b8-0-1--feed-0ff",T"Yandex.DNS (2a02:6b8:0:1::feed:0ff)"},
    {"2001-67c-28a4--",T"UncensoredDNS (2001:67c:28a4::)"},
    {"2a01-3a0-53-53--",T"UncensoredDNS (2a01:3a0:53:53::)"},
    {"2610-a1-1018--1",T"Neustar (2610:a1:1018::1)"},
    {"2610-a1-1019--1",T"Neustar (2610:a1:1019::1)"},
    {"2a0d-2a00-1--2",T"CleanBrowsing (2a0d:2a00:1::2)"},
    {"2a0d-2a00-2--2",T"CleanBrowsing (2a0d:2a00:2::2)"},
  }
}

local add_options = {
  { text = "Manually enter a custom DNS Server IP Address", values = {} },
  { text = "Select from known IPv4 DNS Servers", values = {} },
  { text = "Select from known IPv6 DNS Servers", values = {} },
}

local dnsmasq_path
for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.",true)) do
  for _,dnsmif in pairs(proxy.get(dnsmidx.path.."interface.")) do
    if dnsmif.value == "lan" then
      dnsmasq_path = dnsmidx.path
      break
    end
  end
end

local function split(x,s)
  local sep,fields = s or ":",{}
  local pattern = format("([^%s]+)",sep)
  for field in x:gmatch(pattern) do
    fields[#fields+1] = field
  end
  return fields
end

local function handleDNSTableQuery(columns,options,filter,defaultObject,mapValidation)
  local data,helpmsg = post_helper.handleTableQuery(columns,options,filter,defaultObject,mapValidation)
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
  uci_wan_peerdns = "uci.network.interface.@wan.peerdns",
  uci_wan6_peerdns = "uci.network.interface.@wan6.peerdns",
}
local dns_valid = {
  logqueries = vB,
  noresolv = vB,
  uci_wan_peerdns = vB,
  uci_wan6_peerdns = vB,
}
local dns_config,dns_config_helpmsg = post_helper.handleQuery(dns_params,dns_valid)
if dns_config["uci_wan_peerdns"] == "" then
  dns_config["uci_wan_peerdns"] = "1"
end
if dns_config["uci_wan6_peerdns"] == "" then
  dns_config["uci_wan6_peerdns"] = "1"
end

local custom_dns_columns = {
  {
    header = T"Domain <span class='icon-question-sign' title='Specify a domain name to resolved against a specific server, or leave blank for all domains. The last server listed for duplicated domains will be used unless it does not respond,in which case the previous server will be queried.'></span>",
    name = "domain",
    type = "text",
    attr = { input = { class="span3",placeholder="Leave blank for all domains" } },
  },
  {
    header = T"DNS Server IPv4/IPv6 Address",
    name = "value",
    param = "value",
    type = "text",
    attr = { input = { class="span3" } },
    unique = true,
  },
}
local custom_dns_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "custom_dns",
  basepath = dnsmasq_path.."server.@.",
  createMsg = T"Add Custom DNS Server",
  newList = add_options,
  sorted = function(a,b)
    return tonumber(a.paramindex) < tonumber(b.paramindex)
  end
}
local custom_dns_valid = {
  domain = gOV(vSIDN),
  value = function(value,object)
    local ip = value:gsub("-",":")
    local ret,err = vSII(ip)
    if ret then
      local domain = match(untaint(object.domain),"^%s*(.-)%s*$")
      if domain ~= "" then
        object.value = format("/%s/%s",domain,ip)
      else
        object.value = ip
      end
      return true
    end
    return ret,err
  end
}

if ngx.var.request_method == "POST" then
  local content = ngx.req.get_post_args()
  local action = content.action
  if action == "TABLE-NEW-LIST" then
    local listid = content.listid
    if listid == "2" then
      custom_dns_columns[2] = {
        header = T"Known IPv4 DNS Server",
        name = "value",
        param = "value",
        type = "select",
        values = dns_list[1],
      }
    elseif listid == "3" then
      custom_dns_columns[2] = {
        header = T"Known IPv6 DNS Server",
        name = "value",
        param = "value",
        type = "select",
        values = dns_list[2],
      }
    end
  end
end

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
  value = function(value,object)
    local ret,err = vSII(value)
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

local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"
local ipv4DNScolumns = {
  {
    header = T"DNS Server IPv4 Address",
    name = "wanDnsParam",
    param = "value",
    type = "text",
    attr = { input = { class="span2",maxlength="15",pattern = ipv4pattern } },
  },
}
local ipv4DNSoptions = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "dns4server",
  basepath = "uci.network.interface.@wan.dns.@.",
  createMsg = T"Add IPv4 DNS Server",
  minEntries = 0,
  maxEntries = 4,
  sorted = function(a,b)
    return tonumber(a.paramindex) < tonumber(b.paramindex)
  end
}
local ipv4DNSvalid = {
  wanDnsParam =  post_helper.advancedIPValidation,
}
local ipv4DNSdata,ipv4DNShelpmsg = post_helper.handleTableQuery(ipv4DNScolumns,ipv4DNSoptions,nil,nil,ipv4DNSvalid)

local ipv6DNScolumns = {
  {
    header = T"DNS Server IPv6 Address",
    name = "wanDnsParam",
    param = "value",
    type = "text",
    attr = { input = { class="span2",maxlength="39" } },
  },
}
local ipv6DNSoptions = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "dns6server",
  basepath = "uci.network.interface.@wan6.dns.@.",
  createMsg = T"Add IPv6 DNS Server",
  minEntries = 0,
  maxEntries = 4,
  sorted = function(a,b)
    return tonumber(a.paramindex) < tonumber(b.paramindex)
  end
}
local ipv6DNSvalid = {
  wanDnsParam =  post_helper.validateStringIsIPv6,
}
local ipv6DNSdata,ipv6DNShelpmsg = post_helper.handleTableQuery(ipv6DNScolumns,ipv6DNSoptions,nil,nil,ipv6DNSvalid)

local ajax_attr = {
  span = {
    ["data-ajaxrefresh"] = "5",
  }
}
local cs = {
  dns = "rpc.network.interface.@wan.dnsservers",
  dnsv6 = "rpc.network.interface.@wan6.dnsservers",
}
local function dns_filter(tab)
  local ipv4_dns = ""
  local ipv6_dns = ""
  for _,v in ipairs(tab) do
    if match(v,"^(%d+)%.(%d+)%.(%d+)%.(%d+)$") then
      ipv4_dns=ipv4_dns..format("%s,",v)
    else
      ipv6_dns=ipv6_dns..format("%s,",v)
    end
  end
  if #ipv4_dns > 1 then
    ipv4_dns = string.sub(ipv4_dns,1,#ipv4_dns-1)
  end
  if #ipv6_dns > 1 then
    ipv6_dns = string.sub(ipv6_dns,1,#ipv6_dns-1)
  end
  return ipv4_dns,ipv6_dns
end
ajax_helper.handleAjaxQuery(cs,function(c)
  local dnstab = split(proxy.get("rpc.network.interface.@loopback.dnsservers")[1].value,"[^%,]+")
  local dnsv4,dnsv6 = dns_filter(dnstab)
  if proxy.get("uci.network.interface.@wan.auto")[1].value ~= "0" then
    c.dns = content_helper.getMergedList(proxy.get("rpc.network.interface.@wan.dnsservers")[1].value,dnsv4)
  else
    c.dns = dnsv4
  end
  local ipv6_enabled = proxy.get("uci.network.interface.@wan.ipv6")
  if ipv6_enabled and ipv6_enabled[1].value == "1" then
    c.dnsv6 = content_helper.getMergedList(proxy.get("rpc.network.interface.@wan6.dnsservers")[1].value,dnsv6)
  end
end)
-- will stop in handleAjaxQuery if it is an actual AJAX query
content_helper.getExactContent(cs)
if cs["dns"] and cs["dns"]:len() > 0 then
  cs["dns"] = cs["dns"]:gsub("%s+","")
end
if cs["dnsv6"] and cs["dnsv6"]:len() > 0 then
  cs["dnsv6"] = cs["dnsv6"]:gsub("%s+","")
end

ngx.print(ui_helper.createHeader(T"DNS",true,true))   ngx.print('\
<div class="modal-body update">\
  <form class="form-horizontal" method="post" action="modals/dns-modal.lp">\
    <style>\
    #custom_dns>thead>tr>th:nth-child(-n+2),#rewrites>thead>tr>th:nth-child(-n+2){width:46%;}\
    #dns-modal .interface-dns .control-label{width: 170px;color: #999999;}\
    </style>\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    if dnsmasq_path then
      ngx.print('\
      <fieldset>\
        <legend>'); ngx.print(T"Configuration"); ngx.print('</legend>\
        ');
        ngx.print(ui_helper.createSwitch(T"Ignore Interface DNS<span class='icon-question-sign' title='When ON,do NOT fallback to the Interface DNS Servers if the Custom DNS Servers fail to respond.'></span>","noresolv",dns_config["noresolv"],nil,dns_config_helpmsg["noresolv"]))
        ngx.print(ui_helper.createSwitch(T"Log Queries","logqueries",dns_config["logqueries"],nil,dns_config_helpmsg["logqueries"]))
        ngx.print('\
      </fieldset>\
      <fieldset>\
        <legend>'); ngx.print(T"Custom DNS Servers"); ngx.print('</legend>\
        ');
        ngx.print(ui_helper.createAlertBlock(T"Custom IPv6 DNS Servers defined here will be sent to LAN clients via Router Advertisement (if SLAAC + RA enabled)",{alert={class="alert alert-info"}}))
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
    <fieldset class="interface-dns">\
      <legend>'); ngx.print(T"Interface DNS Servers"); ngx.print('</legend>\
      ');
      ngx.print(ui_helper.createAlertBlock(T"If you turn off Auto-Discovery, you <b>MUST</b> add at least one Custom DNS Server, otherwise your setting will be ignored!",{alert={class="alert alert-warn"}}))
      ngx.print('\
        <legend><small>'); ngx.print(T"WAN Interface DNS Servers"); ngx.print('</small></legend>\
      ');
      ngx.print(ui_helper.createSwitch(T"Auto-Discover WAN DNS","uci_wan_peerdns",dns_config["uci_wan_peerdns"],nil,dns_config_helpmsg["uci_wan_peerdns"]))
      ajax_attr.span.id = "dns"
      ngx.print(ui_helper.createLabel(T"WAN DNS Servers",cs["dns"],ajax_attr))
      ngx.print(ui_helper.createTable(ipv4DNScolumns,ipv4DNSdata,ipv4DNSoptions,nil,ipv4DNShelpmsg))
      ngx.print('\
        <legend><small>'); ngx.print(T"WAN6 Interface DNS Servers"); ngx.print('</small></legend>\
      ');
      ngx.print(ui_helper.createSwitch(T"Auto-Discover WAN6 DNS","uci_wan6_peerdns",dns_config["uci_wan6_peerdns"],nil,dns_config_helpmsg["uci_wan6_peerdns"]))
      ajax_attr.span.id = "dnsv6"
      ngx.print(ui_helper.createLabel(T"WAN6 DNS Servers",cs["dnsv6"],ajax_attr))
      ngx.print(ui_helper.createTable(ipv6DNScolumns,ipv6DNSdata,ipv6DNSoptions,nil,ipv6DNShelpmsg))
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
');