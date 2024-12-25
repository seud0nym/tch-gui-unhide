--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local ajax_helper = require("web.ajax_helper")
local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local split = require("split").split

local dnsmasq_paths = {}
for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.",true)) do
  dnsmasq_paths[#dnsmasq_paths+1] = dnsmidx.path
end

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local format,match = string.format,string.match

---@diagnostic disable-next-line: undefined-field
local vB = post_helper.validateBoolean
local vSII = post_helper.validateStringIsIP

local result = require("web.lte-utils").getContent("rpc.mobiled.DeviceNumberOfEntries")
local devices = tonumber(result.DeviceNumberOfEntries) or 0

local isBridgedMode = require("bridgedmode_helper").isBridgedMode()

local dns_config,dns_config_helpmsg
if not isBridgedMode then
  local dns_params = {}
  local dns_valid = {}
  dns_params["uci_wan_peerdns"] = "uci.network.interface.@wan.peerdns"
  dns_params["uci_wan6_peerdns"] = "uci.network.interface.@wan6.peerdns"
  dns_valid["uci_wan_peerdns"] = vB
  dns_valid["uci_wan6_peerdns"] = vB
  if devices > 0 then
    dns_params["uci_wwan_peerdns"] = "uci.network.interface.@wwan.peerdns"
    dns_valid["uci_wwan_peerdns"] = vB
  end

  dns_config,dns_config_helpmsg = post_helper.handleQuery(dns_params,dns_valid)
  if dns_config["uci_wan_peerdns"] == "" then
    dns_config["uci_wan_peerdns"] = "1"
  end
  if dns_config["uci_wan6_peerdns"] == "" then
    dns_config["uci_wan6_peerdns"] = "1"
  end
  if devices > 0 and dns_config["uci_wwan_peerdns"] == "" then
    dns_config["uci_wwan_peerdns"] = "1"
  end
end
if not dns_config_helpmsg then
  dns_config_helpmsg = {}
end

local current_dns,ipv4DNScolumns,ipv4DNSoptions,ipv4DNSdata,ipv4DNShelpmsg,ipv6DNScolumns,ipv6DNSoptions,ipv6DNSdata,ipv6DNShelpmsg,wwanDNScolumns,wwanDNSoptions,wwanDNSdata,wwanDNShelpmsg

local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"
ipv4DNScolumns = {
  {
    header = T"DNS Server IPv4 Address",
    name = "wanDnsParam",
    param = "value",
    type = "text",
    attr = { input = { class="span2",maxlength="15",pattern=ipv4pattern } },
  },
}
ipv4DNSoptions = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "dns4server",
  basepath = "uci.network.interface.@wan.dns.@.",
  createMsg = T"Add WAN IPv4 DNS Server",
  minEntries = 0,
  maxEntries = 4,
  sorted = function(a,b)
    return tonumber(a.paramindex) < tonumber(b.paramindex)
  end
}
local ipv4DNSvalid = {
  wanDnsParam =  post_helper.advancedIPValidation,
}
ipv6DNScolumns = {
  {
    header = T"DNS Server IPv6 Address",
    name = "wanDnsParam",
    param = "value",
    type = "text",
    attr = { input = { class="span2",maxlength="39" } },
  },
}
ipv6DNSoptions = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "dns6server",
  basepath = "uci.network.interface.@wan6.dns.@.",
  createMsg = T"Add WAN IPv6 DNS Server",
  minEntries = 0,
  maxEntries = 4,
  sorted = function(a,b)
    return tonumber(a.paramindex) < tonumber(b.paramindex)
  end
}
local ipv6DNSvalid = {
  wanDnsParam =  post_helper.validateStringIsIPv6,
}

local dnstab = split(proxy.get("rpc.network.interface.@loopback.dnsservers")[1].value,"[^%,]+")

if isBridgedMode then
  ipv4DNSoptions.basepath = "uci.network.interface.@lan.dns.@."
  ipv4DNSoptions.createMsg = "Add LAN IPv4 DNS Server"
  ipv6DNSoptions.basepath = "uci.network.interface.@lan6.dns.@."
  ipv6DNSoptions.createMsg = "Add LAN IPv6 DNS Server"

  current_dns = {
    ipv4 = "rpc.network.interface.@lan.dnsservers",
    ipv6 = "rpc.network.interface.@lan6.dnsservers",
  }
else
  current_dns = {
    ipv4 = "rpc.network.interface.@wan.dnsservers",
    ipv6 = "rpc.network.interface.@wan6.dnsservers",
  }

  if devices > 0 then
    wwanDNScolumns = {
      {
        header = T"DNS Server Address",
        name = "wanDnsParam",
        param = "value",
        type = "text",
        attr = { input = { class="span2",maxlength="39" } },
      },
    }
    wwanDNSoptions = {
      canEdit = true,
      canAdd = true,
      canDelete = true,
      tableid = "dnsWWANserver",
      basepath = "uci.network.interface.@wwan.dns.@.",
      createMsg = T"Add WWAN DNS Server",
      minEntries = 0,
      maxEntries = 4,
      sorted = function(a,b)
        return tonumber(a.paramindex) < tonumber(b.paramindex)
      end
    }
    local wwanDNSvalid = {
      wanDnsParam =  vSII,
    }
    wwanDNSdata,wwanDNShelpmsg = post_helper.handleTableQuery(wwanDNScolumns,wwanDNSoptions,nil,nil,wwanDNSvalid)

    current_dns["wwan"] = "rpc.network.interface.@wwan.dnsservers"
  end

  ajax_helper.handleAjaxQuery(current_dns,function(c)
    local dnsv4,dnsv6,all = "","",""
    for _,v in ipairs(dnstab) do
      all = all..format("%s,",v)
      if match(v,"^%d+%.%d+%.%d+%.%d+$") then
        dnsv4 = dnsv4..format("%s,",v)
      else
        dnsv6 = dnsv6..format("%s,",v)
      end
    end
    if #dnsv4 > 1 then
      dnsv4 = string.sub(dnsv4,1,#dnsv4-1)
    end
    if #dnsv6 > 1 then
      dnsv6 = string.sub(dnsv6,1,#dnsv6-1)
    end
    if #all > 1 then
      all = string.sub(all,1,#all-1)
    end
    if isBridgedMode then
      c["ipv4"] = content_helper.getMergedList(proxy.get("rpc.network.interface.@lan.dnsservers")[1].value,dnsv4)
      local ipv6_enabled = proxy.get("uci.network.interface.@lan.ipv6")
      if ipv6_enabled and ipv6_enabled[1].value == "1" then
        c["ipv6"] = content_helper.getMergedList(proxy.get("rpc.network.interface.@lan6.dnsservers")[1].value,dnsv6)
      end
    else
      if proxy.get("uci.network.interface.@wan.auto")[1].value ~= "0" then
        c["ipv4"] = content_helper.getMergedList(proxy.get("rpc.network.interface.@wan.dnsservers")[1].value,dnsv4)
      else
        c["ipv4"] = dnsv4
      end
      local ipv6_enabled = proxy.get("uci.network.interface.@wan.ipv6")
      if ipv6_enabled and ipv6_enabled[1].value == "1" then
        c["ipv6"] = content_helper.getMergedList(proxy.get("rpc.network.interface.@wan6.dnsservers")[1].value,dnsv6)
      end
      if devices > 0 then
        local wwan_enabled = proxy.get("uci.network.interface.@wwan.enabled")
        if wwan_enabled and wwan_enabled[1].value == "1" then
          c["wwan"] = content_helper.getMergedList(proxy.get("rpc.network.interface.@wwan.dnsservers")[1].value,all)
        end
      end
    end
  end)
  -- will stop in handleAjaxQuery if it is an actual AJAX query
end
ipv4DNSdata,ipv4DNShelpmsg = post_helper.handleTableQuery(ipv4DNScolumns,ipv4DNSoptions,nil,nil,ipv4DNSvalid)
ipv6DNSdata,ipv6DNShelpmsg = post_helper.handleTableQuery(ipv6DNScolumns,ipv6DNSoptions,nil,nil,ipv6DNSvalid)

content_helper.getExactContent(current_dns)
if current_dns["ipv4"] and current_dns["ipv4"]:len() > 0 then
  current_dns["ipv4"] = current_dns["ipv4"]:gsub("%s+","")
end
if current_dns["ipv6"] and current_dns["ipv6"]:len() > 0 then
  current_dns["ipv6"] = current_dns["ipv6"]:gsub("%s+","")
end
if current_dns["wwan"] and current_dns["wwan"]:len() > 0 then
  current_dns["wwan"] = current_dns["wwan"]:gsub("%s+","")
end

ngx.print(ui_helper.createHeader(T"DNS",true,true))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dns-interfaces-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    if isBridgedMode then
      ngx.print("<legend>LAN Interface DNS Servers</legend>")
      ngx.print(ui_helper.createLabel(T"LAN IPv4 DNS Servers",current_dns["ipv4"],nil))
      ngx.print(ui_helper.createLabel(T"LAN IPv6 DNS Servers",current_dns["ipv6"],nil))
      ngx.print(ui_helper.createTable(ipv4DNScolumns,ipv4DNSdata,ipv4DNSoptions,nil,ipv4DNShelpmsg))
      ngx.print(ui_helper.createTable(ipv6DNScolumns,ipv6DNSdata,ipv6DNSoptions,nil,ipv6DNShelpmsg))
    else
      local ajax_attr = {
        span = {
          ["data-ajaxrefresh"] = "5",
        }
      }
      ngx.print('\
      <fieldset class="interface-dns">\
        <legend>',T"Interface Upstream DNS Servers",'</legend>\
        ');
        ngx.print(ui_helper.createAlertBlock(T"If you turn off Auto-Discovery, you <b>MUST</b> add at least one Custom Upstream DNS Server, otherwise your setting will be ignored!",{alert={class="alert alert-warn"}}))
        ngx.print('\
          <legend><small>',T"WAN Interface DNS Servers",'</small></legend>\
        ');
        ngx.print(ui_helper.createSwitch(T"Auto-Discovery","uci_wan_peerdns",dns_config["uci_wan_peerdns"],nil,dns_config_helpmsg["uci_wan_peerdns"]))
        ajax_attr.span.id = "ipv4"
        ngx.print(ui_helper.createLabel(T"WAN DNS Servers",current_dns["ipv4"],ajax_attr))
        ngx.print(ui_helper.createTable(ipv4DNScolumns,ipv4DNSdata,ipv4DNSoptions,nil,ipv4DNShelpmsg))
        ngx.print('\
          <legend><small>',T"WAN6 Interface DNS Servers",'</small></legend>\
        ');
        ngx.print(ui_helper.createSwitch(T"Auto-Discovery","uci_wan6_peerdns",dns_config["uci_wan6_peerdns"],nil,dns_config_helpmsg["uci_wan6_peerdns"]))
        ajax_attr.span.id = "ipv6"
        ngx.print(ui_helper.createLabel(T"WAN6 DNS Servers",current_dns["ipv6"],ajax_attr))
        ngx.print(ui_helper.createTable(ipv6DNScolumns,ipv6DNSdata,ipv6DNSoptions,nil,ipv6DNShelpmsg))
        if devices > 0 then
          ngx.print('\
            <legend><small>',T"WWAN Interface DNS Servers",'</small></legend>\
          ');
          ngx.print(ui_helper.createSwitch(T"Auto-Discovery","uci_wwan_peerdns",dns_config["uci_wwan_peerdns"],nil,dns_config_helpmsg["uci_wwan_peerdns"]))
          ajax_attr.span.id = "wwan"
          ngx.print(ui_helper.createLabel(T"WWAN DNS Servers",current_dns["wwan"],ajax_attr))
          ngx.print(ui_helper.createTable(wwanDNScolumns,wwanDNSdata,wwanDNSoptions,nil,wwanDNShelpmsg))
        end
        ngx.print('\
      </fieldset>\
      ');
    end
    ngx.print('\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>\
',ui_helper.createFooter(),'\
');