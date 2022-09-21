--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local ajax_helper = require("web.ajax_helper")
local content_helper = require("web.content_helper")
local dns_helper = require("dns_helper")
local ipv6_rpc_path = require("ipv6_rpc_path").getPath
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local split = require("split").split

local dns_list = require('dns_servers_helper').external

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local gOV = post_helper.getOptionalValidation
local vB = post_helper.validateBoolean
local vSII = post_helper.validateStringIsIP
local vSIDN = post_helper.validateStringIsDomainName

local add_options = {
  { text = "Manually enter a custom DNS Server IP Address", values = {} },
  { text = "Select from known IPv4 DNS Servers", values = {} },
  { text = "Select from known IPv6 DNS Servers", values = {} },
}

local result = require("web.lte-utils").getContent("rpc.mobiled.DeviceNumberOfEntries")
local devices = tonumber(result.DeviceNumberOfEntries) or 0

local isBridgedMode = require("bridgedmode_helper").isBridgedMode()
local dnsmasq_path = dns_helper.dnsmasq_path

local dns_params = {
  logqueries = dnsmasq_path.."logqueries",
  noresolv = dnsmasq_path.."noresolv",
}
local dns_valid = {
  logqueries = vB,
  noresolv = vB,
}
if not isBridgedMode then
  dns_params["uci_wan_peerdns"] = "uci.network.interface.@wan.peerdns"
  dns_params["uci_wan6_peerdns"] = "uci.network.interface.@wan6.peerdns"
  dns_valid["uci_wan_peerdns"] = vB
  dns_valid["uci_wan6_peerdns"] = vB
  if devices > 0 then
    dns_params["uci_wwan_peerdns"] = "uci.network.interface.@wwan.peerdns"
    dns_valid["uci_wwan_peerdns"] = vB
  end
end

local dns_config,dns_config_helpmsg = post_helper.handleQuery(dns_params,dns_valid)
if not isBridgedMode then
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

local custom_dns_data,custom_dns_helpmsg = dns_helper.handleDNSTableQuery(custom_dns_columns,custom_dns_options,nil,nil,custom_dns_valid)

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
  createMsg = T"Add IPv4 Interface DNS Server",
  minEntries = 0,
  maxEntries = 4,
  sorted = function(a,b)
    return tonumber(a.paramindex) < tonumber(b.paramindex)
  end
}
local ipv4DNSvalid = {
  wanDnsParam =  post_helper.advancedIPValidation,
}

if not isBridgedMode then
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
    createMsg = T"Add Interface IPv6 DNS Server",
    minEntries = 0,
    maxEntries = 4,
    sorted = function(a,b)
      return tonumber(a.paramindex) < tonumber(b.paramindex)
    end
  }
  local ipv6DNSvalid = {
    wanDnsParam =  post_helper.validateStringIsIPv6,
  }
  ipv6DNSdata,ipv6DNShelpmsg = post_helper.handleTableQuery(ipv6DNScolumns,ipv6DNSoptions,nil,nil,ipv6DNSvalid)
  ipv4DNSdata,ipv4DNShelpmsg = post_helper.handleTableQuery(ipv4DNScolumns,ipv4DNSoptions,nil,nil,ipv4DNSvalid)

  current_dns = {
    ipv4 = "rpc.network.interface.@wan.dnsservers",
    ipv6 = ipv6_rpc_path("dnsservers"),
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
      createMsg = T"Add WWAN Interface DNS Server",
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

  local dnstab = split(proxy.get("rpc.network.interface.@loopback.dnsservers")[1].value,"[^%,]+")
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
    if proxy.get("uci.network.interface.@wan.auto")[1].value ~= "0" then
      c["ipv4"] = content_helper.getMergedList(proxy.get("rpc.network.interface.@wan.dnsservers")[1].value,dnsv4)
    else
      c["ipv4"] = dnsv4
    end
    local ipv6_enabled = proxy.get("uci.network.interface.@wan.ipv6")
    if ipv6_enabled and ipv6_enabled[1].value == "1" then
      c["ipv6"] = content_helper.getMergedList(proxy.get(ipv6_rpc_path("dnsservers"))[1].value,dnsv6)
    end
    if devices > 0 then
      local wwan_enabled = proxy.get("uci.network.interface.@wwan.enabled")
      if wwan_enabled and wwan_enabled[1].value == "1" then
        c["wwan"] = content_helper.getMergedList(proxy.get("rpc.network.interface.@wwan.dnsservers")[1].value,all)
      end
    end
  end)
  -- will stop in handleAjaxQuery if it is an actual AJAX query
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
end

ngx.print(ui_helper.createHeader(T"DNS",true,true))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dns-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    if not isBridgedMode then
      ngx.print('\
      <fieldset>\
        <legend>'); ngx.print(T"Configuration"); ngx.print('</legend>\
        ');
        ngx.print(ui_helper.createSwitch(T"Ignore Interface DNS<span class='icon-question-sign' title='When ON, do NOT fallback to the Interface DNS Servers if the Custom DNS Servers fail to respond.'></span>","noresolv",dns_config["noresolv"],nil,dns_config_helpmsg["noresolv"]))
        ngx.print(ui_helper.createSwitch(T"Log Queries","logqueries",dns_config["logqueries"],nil,dns_config_helpmsg["logqueries"]))
        ngx.print('\
      </fieldset>\
      ');
    end
    ngx.print('\
    <fieldset>\
      <legend>'); ngx.print(T"Custom DNS Servers"); ngx.print('</legend>\
      ');
      ngx.print(ui_helper.createTable(custom_dns_columns,custom_dns_data,custom_dns_options,nil,custom_dns_helpmsg))
      ngx.print('\
    </fieldset>\
    ');
    if isBridgedMode then
      ipv4DNSoptions.basepath = "uci.network.interface.@lan.dns.@."
      ipv4DNSdata,ipv4DNShelpmsg = post_helper.handleTableQuery(ipv4DNScolumns,ipv4DNSoptions,nil,nil,ipv4DNSvalid)
      ngx.print("<legend>LAN Interface DNS Servers</legend>")
      ngx.print(ui_helper.createTable(ipv4DNScolumns,ipv4DNSdata,ipv4DNSoptions,nil,ipv4DNShelpmsg))
    else
      local ajax_attr = {
        span = {
          ["data-ajaxrefresh"] = "5",
        }
      }
      ngx.print('\
      <fieldset class="interface-dns">\
        <legend>'); ngx.print(T"Interface DNS Servers"); ngx.print('</legend>\
        ');
        ngx.print(ui_helper.createAlertBlock(T"If you turn off Auto-Discovery, you <b>MUST</b> add at least one Custom DNS Server, otherwise your setting will be ignored!",{alert={class="alert alert-warn"}}))
        ngx.print('\
          <legend><small>'); ngx.print(T"WAN Interface DNS Servers"); ngx.print('</small></legend>\
        ');
        ngx.print(ui_helper.createSwitch(T"Auto-Discover WAN DNS","uci_wan_peerdns",dns_config["uci_wan_peerdns"],nil,dns_config_helpmsg["uci_wan_peerdns"]))
        ajax_attr.span.id = "ipv4"
        ngx.print(ui_helper.createLabel(T"WAN DNS Servers",current_dns["ipv4"],ajax_attr))
        ngx.print(ui_helper.createTable(ipv4DNScolumns,ipv4DNSdata,ipv4DNSoptions,nil,ipv4DNShelpmsg))
        ngx.print('\
          <legend><small>'); ngx.print(T"WAN6 Interface DNS Servers"); ngx.print('</small></legend>\
        ');
        ngx.print(ui_helper.createSwitch(T"Auto-Discover WAN6 DNS","uci_wan6_peerdns",dns_config["uci_wan6_peerdns"],nil,dns_config_helpmsg["uci_wan6_peerdns"]))
        ajax_attr.span.id = "ipv6"
        ngx.print(ui_helper.createLabel(T"WAN6 DNS Servers",current_dns["ipv6"],ajax_attr))
        ngx.print(ui_helper.createTable(ipv6DNScolumns,ipv6DNSdata,ipv6DNSoptions,nil,ipv6DNShelpmsg))
        if devices > 0 then
          ngx.print('\
            <legend><small>'); ngx.print(T"WWAN Interface DNS Servers"); ngx.print('</small></legend>\
          ');
          ngx.print(ui_helper.createSwitch(T"Auto-Discover WWAN DNS","uci_wwan_peerdns",dns_config["uci_wwan_peerdns"],nil,dns_config_helpmsg["uci_wwan_peerdns"]))
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
</div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
');