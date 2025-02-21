--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local dns_helper = require("dns_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")

local dns_list = require('dns_servers_helper').external

local format,gsub,lower,match,upper = string.format,string.gsub,string.lower,string.match,string.upper
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local getargs = ngx.req.get_uri_args()
local get_key = getargs.key
local cur_key = "Main"
local dnsmasq_paths = {}
for _,dnsmidx in ipairs(proxy.getPN("uci.dhcp.dnsmasq.",true)) do
  local dnsmasq_path = untaint(dnsmidx.path)
  local key = gsub(match(dnsmasq_path,"^uci%.dhcp%.dnsmasq%.@([^%.]+)%."),"^%l",upper)
  if key == get_key then
    cur_key = key
  end
  dnsmasq_paths[#dnsmasq_paths+1] = {
    path = dnsmasq_path,
    key = key
  }
end

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local gOV = post_helper.getOptionalValidation
local vB = post_helper.validateBoolean
local vSII = post_helper.validateStringIsIP
local vSIDN = post_helper.validateStringIsDomainName
local vSIP = post_helper.validateStringIsPort

local add_options = {
  { text = "Manually enter a custom DNS Server IP Address", values = {} },
  { text = "Select from known IPv4 DNS Servers", values = {} },
  { text = "Select from known IPv6 DNS Servers", values = {} },
}

local isBridgedMode = require("bridgedmode_helper").isBridgedMode()

--region LAN Interfaces
local interfaces_columns = {
  {
    header = T"Interface",
    name = "value",
    param = "value",
    type = "text",
  },
}
local interfaces_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "interfaces",
}
--endregion LAN Interfaces

--region Custom DNS Servers
local custom_dns_columns = {
  {
    header = T"Domain <span class='icon-question-sign' title='Specify a domain name to resolved against a specific server, or leave blank for all domains. The last server listed for duplicated domains will be used unless it does not respond,in which case the previous server will be queried.'></span>",
    name = "domain",
    type = "text",
    attr = { input = { class="span3 no-save",placeholder="Leave blank for all domains" } },
  },
  {
    header = T"DNS Server IPv4/IPv6 Address",
    name = "value",
    param = "value",
    type = "text",
    attr = { input = { class="span3 no-save" } },
    unique = true,
  },
}
local custom_dns_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
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
local adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")
local function custom_dns_filter(data)
  if adguard and adguard[1].value == "1" then
    if data.value == "127.0.0.1#53535" then
      return { canDelete = false, canEdit = false }
    end
  end
  return true
end
--endregion Custom DNS Servers

--region Rewrites
local rewrites_columns = {
  {
    header = T"Domain Address Query",
    name = "domain",
    type = "text",
    attr = { input = { class="span2 no-save" } },
  },
  {
    header = T"IPv4/IPv6 Address to Return",
    name = "value",
    param = "value",
    type = "text",
    attr = { input = { class="span2 no-save" } },
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
  tableid = "dns_rewrites",
  sorted = function(a,b)
    return lower(a.value) < lower(b.value)
  end
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
--endregion Rewrites

--region Rebind Protection
local domains_columns = {
  {
    header = T"Domain To Allow",
    name = "value",
    param = "value",
    type = "text",
    attr = { input = { class="span2 no-save" } },
  },
}
local domains_valid = {
  value = function(value,object)
    local ret,err = vSIDN(value)
    if ret then
      object.value = format("/%s/",object.value)
      return true
    end
    return ret,err
  end
}
local domains_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add Domain",
  tableid = "rebind_domains",
}
local function domains_filter(data)
  if data.value then
    local domain = match(untaint(data.value),"/([^/]+)/")
    if domain then
      return true
    end
  end
  return false
end
--endregion Rebind Protection

--region Attributes
local switch_class_enable = {
  input = {
    class = "monitor-changes",
  }
}
local rebind_protection_visible_attr = {
  group = {
    class = "monitor-rebind_protection monitor-1",
  }
}
local port_number_attr = {
  input = { 
    type = "number",
    maxlength = "5",
    min = "0",
    max = "65535",
    style = "width:100px",
  } 
}
--endregion Attributes

ngx.print(ui_helper.createHeader(T"DNS",true,true))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="',ngx.var.request_uri,'">\
    <div class="span2">\
      <ul class="nav nav-list">\
        <li class="nav-header">',T"Configuration",'</li>');
        local dnsmasq_path
        local html = {}
        for i=1,#dnsmasq_paths do
          local v = dnsmasq_paths[i]
          local url = "modals/dns-modal.lp?key="..v.key
          local active = ""
          if v.key == cur_key then
            active = "active"
            dnsmasq_path = v.path
            interfaces_options.basepath = dnsmasq_path.."interface.@."
            rewrites_options.basepath = dnsmasq_path.."address.@."
            domains_options.basepath = dnsmasq_path.."rebind_domain.@."
          end
          html[#html+1] = format('<li class="%s"><a id="%s" href="#" data-remote="%s">%s</a></li>',active,v.key,url,v.key)
        end
        ngx.print(html,'\
      </ul>\
    </div>\
    <div class="span9">\
      ',ui_helper.createMessages(message_helper.popMessages()),'\
      ')
      local dnsmasq_params = {
        logqueries = dnsmasq_path.."logqueries",
        port = dnsmasq_path.."port",
        rebind_protection = dnsmasq_path.."rebind_protection",
        rebind_localhost = dnsmasq_path.."rebind_localhost",
      }
      local dnsmasq_valid = {
        logqueries = vB,
        port = function(value,data,key)
          if value == "" or value == "0" then
            return true
          end
          return vSIP(value,data,key)
        end,
        rebind_protection = vB,
        rebind_localhost = vB,
      }
      if not isBridgedMode then
        dnsmasq_params["noresolv"] = dnsmasq_path.."noresolv"
        dnsmasq_valid["noresolv"] = vB
      end
      local dnsmasq_config,dnsmasq_config_helpmsg = post_helper.handleQuery(dnsmasq_params,dnsmasq_valid)
      if dnsmasq_config.port == "" then
        dnsmasq_config.port = "53"
      end
      local interfaces_data = post_helper.handleTableQuery(interfaces_columns,interfaces_options,nil,nil,nil)
      local rewrites_data,rewrites_helpmsg = dns_helper.handleDNSTableQuery(rewrites_columns,rewrites_options,rewrites_filter,nil,rewrites_valid)
      local domains_data,domains_helpmsg = dns_helper.handleRebindTableQuery(domains_columns,domains_options,domains_filter,nil,domains_valid)
      ngx.print('\
      <fieldset>\
        <legend>',T(cur_key),T" DNS Configuration"); ngx.print('</legend>');
        ngx.print(
          ui_helper.createInputText(T"Listen on Port<span class='icon-question-sign' title='Listening port for DNS queries (default=53). If set to 0, disables DNS server functionality.'></span>","port",dnsmasq_config["port"],port_number_attr,dnsmasq_config_helpmsg["port"]),'\
          <div class="control-group">\
            <label class="control-label">',T"Listen on Interfaces",'</label>\
            <div class="controls">',
              ui_helper.createTable(interfaces_columns,interfaces_data,interfaces_options,nil,nil),'\
            </div>\
          </div>')
        if not isBridgedMode then
          ngx.print(ui_helper.createSwitch(T"Ignore I/face DNS<span class='icon-question-sign' title='When ON, do NOT fallback to the Interface DNS Servers if the Custom DNS Servers fail to respond.'></span>","noresolv",dnsmasq_config["noresolv"],nil,dnsmasq_config_helpmsg["noresolv"]))
        end
        ngx.print(ui_helper.createSwitch(T"Log Queries","logqueries",dnsmasq_config["logqueries"],nil,dnsmasq_config_helpmsg["logqueries"]))
        custom_dns_options.tableid = cur_key.."_custom_dns"
        custom_dns_options.basepath = dnsmasq_path.."server.@."
        custom_dns_options.createMsg = T("Add "..cur_key.." Custom DNS Server")
        local custom_dns_data,custom_dns_helpmsg = dns_helper.handleDNSTableQuery(custom_dns_columns,custom_dns_options,custom_dns_filter,nil,custom_dns_valid)
        ngx.print('\
          <legend><small>',T"Custom Upstream DNS Servers",'</small></legend>',
            ui_helper.createTable(custom_dns_columns,custom_dns_data,custom_dns_options,nil,custom_dns_helpmsg),'\
          <legend><small>',T"DNS Rewrites",'</small></legend>',
            ui_helper.createTable(rewrites_columns,rewrites_data,rewrites_options,nil,rewrites_helpmsg),'\
          <legend><small>',T"DNS Rebind Protection",'</small></legend>',
            ui_helper.createSwitch(T"Enabled","rebind_protection",dnsmasq_config["rebind_protection"],switch_class_enable,dnsmasq_config_helpmsg["rebind_protection"]),
            ui_helper.createSwitch(T"Rebind localhost<span class='icon-question-sign' title='Allows upstream 127.0.0.0/8 responses (required for DNS based blacklist services).'></span>","rebind_localhost",dnsmasq_config["rebind_localhost"],rebind_protection_visible_attr,dnsmasq_config_helpmsg["rebind_localhost"]),
            ui_helper.createTable(domains_columns,domains_data,domains_options,rebind_protection_visible_attr,domains_helpmsg),'\
      </fieldset>\
    </div>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>\
',ui_helper.createFooter());