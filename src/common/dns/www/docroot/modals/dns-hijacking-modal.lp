--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local proxy_helper = require("proxy_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local uinetwork = require("web.uinetwork_helper")
local splitter = require("split")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local find,gsub = string.find,string.gsub

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local ipdetails = {
  gw = "uci.network.interface.@lan.ipaddr",
  netmask = "uci.network.interface.@lan.netmask",
  ip6addr = "rpc.network.interface.@lan.ip6addr",
}
content_helper.getExactContent(ipdetails)

local vSIIP4 = post_helper.validateStringIsIPv4
local vSIIP6 = post_helper.validateStringIsIPv6
local gVSIDIP = post_helper.getValidateStringIsDeviceIPv4
local vSIDIP = gVSIDIP(ipdetails.gw,ipdetails.netmask)
local vB = post_helper.validateBoolean

local hosts_ac,hosts6_ac = uinetwork.getAutocompleteHostsList()
local hostname_map = {}
for hostname,ip in pairs(hosts_ac) do
  hostname_map[ip] = splitter.first_and_rest(hostname,"%(")
end
for hostname,ips in pairs(hosts6_ac) do
  for ip in splitter.each(ips) do
    hostname_map[ip] = splitter.first_and_rest(hostname,"%(")
  end
end
local function ip2hostnameFilter(data)
  data.hostname = hostname_map[untaint(data.ip)] or ""
  return true
end

local path = {}

local function findPath(basepath,option)
  for _,v in ipairs(proxy.getPN(basepath,true)) do
    local values = proxy.get(v.path..(option or "name"))
    if values then
      local name = untaint(values[1].value)
      if name and name ~= "" then
        path[name] = v.path
      end
    end
  end
end

findPath("uci.firewall.redirect.")
findPath("uci.firewall.nat.")
findPath("uci.firewall.rule.")
findPath("uci.firewall.include.","path")
findPath("uci.firewall.ipset.")
findPath("uci.tproxy.rule.")

local cronjob
local data = proxy.getPN("rpc.gui.cron.entries.",true)
for _,v in ipairs(data) do
  local cmd = proxy.get(v.path.."command")
  if cmd and find(untaint(cmd[1].value),"doh%-ipsets%-maintain") then
    cronjob = v.path
    break
  end
end

local helpmsg = {}

local enabledv4 = "0"
local enabledv6 = "0"
local dest_ipv4
local dest_ipv6
local dns_int = path["Intercept-DNS"] or path["Redirect-DNS"]
if dns_int then
  if cronjob and path["Masquerade-DNS"] and path["Allow-DoT"] and path["Allow-DoTv6"] and path["Deny-DoT"] and path["Allow-DoH"] and path["Allow-DoHv6"] and path["Deny-DoH"] and path["Deny-DoHv6"] and path["dns_xcptn"] and path["doh"] and path["doh6"] and path["/usr/sbin/ipsets-restore"] and path["dns6_xcptn"] and path["/usr/sbin/tproxy-firewall"] and path["Intercept-DNSv6"] and path["/usr/share/tch-gui-unhide/No-Hijack-DHCP-DNSv4.sh"] then
    local state = proxy.get(dns_int.."enabled",path["Masquerade-DNS"].."dest_ip",path["Intercept-DNSv6"].."enabled",path["Intercept-DNSv6"].."destIP")
    if state then
      if state[1].value and state[1].value ~= "0" then
        enabledv4 = "1"
      end
      dest_ipv4 = untaint(state[2].value)
      if state[3].value and state[3].value ~= "0" then
        enabledv6 = "1"
      end
      dest_ipv6 = untaint(state[4].value)
    end

    if enabledv6 == "1" and ipdetails.ip6addr == "" then
      enabledv6 = "0"
      dest_ipv6 = ""
      proxy_helper.set(path["Intercept-DNSv6"].."enabled",enabledv6)
      proxy_helper.set(path["Intercept-DNSv6"].."destIP",dest_ipv6)
    end

    if ngx.var.request_method == "POST" then
      local args = ngx.req.get_post_args()
      if args["action"] == "SAVE" then
        local new_enabledv4 = untaint(args["enabledv4"])
        local new_dest_ipv4 = gsub(untaint(args["dest_ipv4"]),"^%s*(.-)%s*$","%1")

        local validv4,errmsg = vB(new_enabledv4)
        if not validv4 then
          helpmsg["enabledv4"] = errmsg
        elseif new_dest_ipv4 ~= "" then
          validv4,errmsg = vSIIP4(new_dest_ipv4)
          if not validv4 then
            helpmsg["dest_ipv4"] = errmsg
          end
        end

        if validv4 then
          if new_enabledv4 == "0" then
            proxy_helper.set(path["Masquerade-DNS"].."enabled",new_enabledv4)
            dest_ipv4 = ""
          elseif new_enabledv4 == "1" then
            if new_dest_ipv4 then
              proxy_helper.set(dns_int.."dest_ip",new_dest_ipv4)
              proxy_helper.set(dns_int.."name","Redirect-DNS")
              proxy_helper.set(path["Masquerade-DNS"].."dest_ip",new_dest_ipv4)
              proxy_helper.set(path["Masquerade-DNS"].."enabled",new_enabledv4)
              if vSIDIP and vSIDIP(new_dest_ipv4) then
                proxy_helper.set("rpc.gui.firewall.dns_intercept.add_exception",new_dest_ipv4)
              end
              dest_ipv4 = new_dest_ipv4
            else
              proxy_helper.set(dns_int.."name","Intercept-DNS")
              proxy_helper.set(path["Masquerade-DNS"].."enabled","0")
              dest_ipv4 = ""
            end
          end
          proxy_helper.set(dns_int.."enabled",new_enabledv4)
          local paths = { "Allow-DoT","Allow-DoH","Deny-DoT","Deny-DoH","dns_xcptn","doh","/usr/sbin/ipsets-restore","/usr/share/tch-gui-unhide/No-Hijack-DHCP-DNSv4.sh" }
          for _,p in ipairs(paths) do
            proxy_helper.set(path[p].."enabled",new_enabledv4)
          end
          enabledv4 = new_enabledv4
        end

        local new_enabledv6 = untaint(args["enabledv6"])
        local new_dest_ipv6 = gsub(untaint(args["dest_ipv6"]),"^%s*(.-)%s*$","%1")

        local validv6,errmsg6 = vB(new_enabledv6)
        if not validv6 then
          helpmsg["enabledv6"] = errmsg6
        elseif new_dest_ipv6 ~= "" then
          validv6,errmsg6 = vSIIP6(new_dest_ipv6)
          if not validv6 then
            helpmsg["dest_ipv6"] = errmsg6
          end
        end

        if validv6 then
          if new_enabledv6 == "0" or (new_enabledv6 == "1" and ipdetails.ip6addr == "") then
            new_enabledv6 = "0"
            proxy_helper.set(path["Intercept-DNSv6"].."enabled",new_enabledv6)
          elseif new_enabledv6 == "1" then
            if new_dest_ipv6 then
              proxy_helper.set(path["Intercept-DNSv6"].."destIP",new_dest_ipv6)
              local hosts = proxy.getPN("rpc.hosts.host.",true)
              for _,v in ipairs(hosts) do
                local addresses = proxy.get(v.path..'IPv6')
                if addresses and addresses[1].value:find(new_dest_ipv6) then
                  for address in addresses[1].value:gmatch('%S+') do
                    proxy_helper.set("rpc.gui.firewall.dns6_intercept.add_exception",address)
                  end
                  break
                end
              end
              dest_ipv6 = new_dest_ipv6
            else
              local ula = proxy.get("rpc.network.interface.@lan.ipv6uniquelocaladdr")
              if ula and ula[1].value ~= "" then
                local v = untaint(ula[1].value)
                if v:find("/") then
                  v = string.match(v,"(.+)/%d+")
                end
              end
              dest_ipv6 = ""
            end
          end
          local paths = { "Intercept-DNSv6","dns6_xcptn","Allow-DoTv6","Allow-DoHv6","Deny-DoHv6","doh6" }
          for _,p in ipairs(paths) do
            proxy_helper.set(path[p].."enabled",new_enabledv6)
          end
          enabledv6 = new_enabledv6
        end

        local cron = "0"
        if new_enabledv4 == "1" or new_enabledv6 == "1" then
          proxy_helper.set("rpc.gui.firewall.dns_intercept.populate_ipsets","1")
          cron="1"
        end
        if proxy.get(cronjob.."enabled")[1].value ~= cron and proxy_helper.set(cronjob.."enabled",cron) then
          proxy_helper.set("rpc.gui.cron.writecrontab","1")
        end

        proxy.apply()
      end
    end
  else
    dns_int = nil
    message_helper.pushMessage(T"Firewall configuration for DNS hijacking is incomplete! Please re-run tch-gui-unhide to fix.","error")
  end
end

local mapParams = {
  verbose = "uci.tproxy.config.verbose",
}
local mapValid = {
  verbose = vB
}
local content,vhelpmsg = post_helper.handleQuery(mapParams,mapValid)
if not vhelpmsg then
  vhelpmsg = {}
end

local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"
local ipv4validation = {
  ip = vSIDIP,
}
local ipv4cols = {
  {
    header = T"IPv4 Addresses Excluded from Hijacking",
    name = "ip",
    param = "ip",
    type = "text",
    attr = { input = { class="span2",maxlength="15",pattern = ipv4pattern },autocomplete=hosts_ac },
    unique = true
  },
  {
    header = T"Host Name",
    param = "hostname",
    readonly = true,
  },
}
local ipv4opts = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "excludedIPv4",
  basepath = "rpc.gui.firewall.dns_intercept.exceptions.@.",
  createMsg = T"Add Excluded IPv4 Address",
  onAdd = function(index,record)
    proxy.set("rpc.gui.firewall.dns_intercept.exceptions."..index..".ip",record.ip)
  end
}
local ipv4excdata,ipv4exchelpmsg = post_helper.handleTableQuery(ipv4cols,ipv4opts,ip2hostnameFilter,nil,ipv4validation)

local ipv6validation = {
  ip = function(value)
    for ip in splitter.each(value) do
      local validv6,errmsg = vSIIP6(ip)
      if not validv6 then
        return nil,errmsg
      end
    end
    return true
  end,
}
local ipv6cols = {
  {
    header = T"IPv6 Addresses Excluded from Hijacking",
    name = "ip",
    param = "ip",
    type = "text",
    attr = { input = { class="span3",placeholder="Multiple separated by spaces allowed" },autocomplete=hosts6_ac },
    unique = true
  },
  {
    header = T"Host Name",
    param = "hostname",
    readonly = true,
  },
}
local ipv6opts = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "excludedIPv6",
  basepath = "rpc.gui.firewall.dns6_intercept.exceptions.@.",
  createMsg = T"Add Excluded IPv6 Address",
  onAdd = function(index,record)
    local first = true
    for ip in splitter.each(record.ip) do
      if first then
        proxy_helper.set("rpc.gui.firewall.dns6_intercept.exceptions."..index..".ip",ip)
        proxy.apply()
        first = false
      else
        local next = proxy.add("rpc.gui.firewall.dns6_intercept.exceptions.")
        proxy_helper.set("rpc.gui.firewall.dns6_intercept.exceptions."..next..".ip",ip)
        proxy.apply()
      end
    end
  end
}
local ipv6excdata,ipv6exchelpmsg = post_helper.handleTableQuery(ipv6cols,ipv6opts,ip2hostnameFilter,nil,ipv6validation)

local enabled_attr = {
  input = {
    class = "monitor-changes"
  }
}
local monitor_attr = {
  group = {
    class = "monitor-enabledv4 monitor-1"
  },
  input = {
    class = "span2",
    placeholder = "Leave blank for Device"
  }
}
local monitor6_attr = {
  group = {
    class = "monitor-enabledv6 monitor-1"
  },
  input = {
    class = "span3",
    placeholder = "Leave blank for Device"
  }
}
local warn_attr = {
  alert = {
    class = "alert-warn",
    style = "text-align:center",
  },
}

ngx.print(ui_helper.createHeader(T"DNS",true,false),'\
<div class="modal-body update">\
  '); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dns-hijacking-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <legend>',T"DNS Hijacking",'</legend>',
      ui_helper.createAlertBlock(T("DNS Requests to servers defined on the Local Network lan interface screen will <B>NOT</B> be hijacked!"),warn_attr),'\
    <legend><small>',T"IPv4",'</small></legend>',
      ui_helper.createSwitch(T"Enabled","enabledv4",enabledv4,enabled_attr),
      ui_helper.createInputText(T"DNS Server Address<span class='icon-question-sign' title='IPv4 address of the DNS server to which hijacked requests will be sent. Leave blank to have this device handle IPv4 DNS.'></span>","dest_ipv4",dest_ipv4,monitor_attr,helpmsg["dest_ipv4"]),'\
      <div class="control-group" style="display; block">\
      <label class="control-label">',T"Excluded LAN IPs",'</label><div class="controls">',
      ui_helper.createTable(ipv4cols,ipv4excdata,ipv4opts,nil,ipv4exchelpmsg),'\
      </div></div>\
    <legend><small>',T"IPv6",'</small></legend>');
    if ipdetails.ip6addr == "" then
      enabledv6 = "0"
      ngx.print(
        ui_helper.createLabel(T"Disabled","No IPv6 LAN address"),
        ui_helper.createSimpleInputHidden("enabledv6",enabledv6),
        ui_helper.createSimpleInputHidden("dest_ipv6",""))
    else
      ngx.print(
        ui_helper.createSwitch(T"Enabled","enabledv6",enabledv6,enabled_attr),
        ui_helper.createInputText(T"DNS Server Address<span class='icon-question-sign' title='IPv6 address of the DNS server to which hijacked requests will be sent. Leave blank to have this device handle IPv6 DNS.'></span>","dest_ipv6",dest_ipv6,monitor6_attr,helpmsg["dest_ipv6"]),
        ui_helper.createSwitch(T"Verbose Logging","verbose",content["verbose"],monitor6_attr,vhelpmsg["verbose"]),'\
        <div class="control-group" style="display; block">\
        <label class="control-label">',T"Excluded LAN IPs",'</label><div class="controls">',
        ui_helper.createTable(ipv6cols,ipv6excdata,ipv6opts,nil,ipv6exchelpmsg),'\
        </div></div>')
    end
    ngx.print('\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>');
if dns_int then
  if enabledv4 == "1" and dest_ipv4 ~= "" then
    ngx.print('<script>$("#excludedIPv4 td:contains('); ngx.print(T(dest_ipv4)); ngx.print(')").siblings(":last").children(".btn-table-edit,.btn-table-delete").hide();</script>');
  end
  if enabledv6 == "1" and dest_ipv6 ~= "" then
    ngx.print('<script>$("#excludedIPv6 td:contains('); ngx.print(T(dest_ipv6)); ngx.print(')").siblings(":last").children(".btn-table-edit,.btn-table-delete").hide();</script>');
  end
end
ngx.print(ui_helper.createFooter());