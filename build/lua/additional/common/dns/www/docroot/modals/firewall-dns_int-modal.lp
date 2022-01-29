--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
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
  netmask = "uci.network.interface.@lan.netmask"
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

local function proxySet(p,v)
  local success,errors = proxy.set(p,v)
  if not success then
    for _,err in ipairs(errors) do
      message_helper.pushMessage(T(string.format("Failed to set %s to '%s': %s (%s)",err.path,v,err.errmsg,err.errcode)),"error")
    end
  end
  return success
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
  if cronjob and path["Masquerade-DNS"] and path["Allow-DoT"] and path["Allow-DoTv6"] and path["Deny-DoT"] and path["Allow-DoH"] and path["Allow-DoHv6"] and path["Deny-DoH"] and path["Deny-DoHv6"] and path["dns_xcptn"] and path["doh"] and path["doh6"] and path["/usr/sbin/ipsets-restore"] and path["dns6_xcptn"] and path["/usr/sbin/tproxy-firewall"] and path["Intercept-DNSv6"] then
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
            proxySet(dns_int.."dest_ip","")
            proxySet(path["Masquerade-DNS"].."enabled",new_enabledv4)
            proxySet(path["Masquerade-DNS"].."dest_ip","")
            dest_ipv4 = ""
          elseif new_enabledv4 == "1" then
            local dhcp_option6_path
            local dhcp_options_path = "uci.dhcp.dhcp.@lan.dhcp_option."
            local dhcp_options = proxy.get(dhcp_options_path)
            if dhcp_options then
              for _,o in ipairs(dhcp_options) do
                if find(untaint(o.value),"^6,") == 1 then
                  dhcp_option6_path = o.path.."value"
                  break
                end
              end
            end
            if not dhcp_option6_path then
              local index = proxy.add(dhcp_options_path)
              dhcp_option6_path = dhcp_options_path.."@"..index..".value"
            end
            if new_dest_ipv4 and new_dest_ipv4 ~= "" then
              proxySet(dhcp_option6_path,"6,"..new_dest_ipv4)
              proxySet(dns_int.."dest_ip",new_dest_ipv4)
              proxySet(dns_int.."name","Redirect-DNS")
              proxySet(path["Masquerade-DNS"].."dest_ip",new_dest_ipv4)
              proxySet(path["Masquerade-DNS"].."enabled",new_enabledv4)
              if vSIDIP(new_dest_ipv4) then
                proxySet("rpc.gui.firewall.dns_intercept.add_exception",new_dest_ipv4)
              end
              dest_ipv4 = new_dest_ipv4
            else
              proxySet(dhcp_option6_path,"6,"..ipdetails.gw)
              proxySet(dns_int.."dest_ip","")
              proxySet(dns_int.."name","Intercept-DNS")
              proxySet(path["Masquerade-DNS"].."dest_ip","")
              proxySet(path["Masquerade-DNS"].."enabled","0")
              dest_ipv4 = ""
            end
          end
          proxySet(dns_int.."enabled",new_enabledv4)
          local paths = { "Allow-DoT","Allow-DoH","Deny-DoT","Deny-DoH","dns_xcptn","doh","/usr/sbin/ipsets-restore" }
          for _,p in ipairs(paths) do
            proxySet(path[p].."enabled",new_enabledv4)
          end
          enabledv4 = new_enabledv4
        end

        local new_enabledv6 = untaint(args["enabledv6"])
        local new_dest_ipv6 = gsub(untaint(args["dest_ipv6"]),"^%s*(.-)%s*$","%1")

        local validv6,errmsg = vB(new_enabledv6)
        if not validv6 then
          helpmsg["enabledv6"] = errmsg
        elseif new_dest_ipv6 ~= "" then
          validv6,errmsg = vSIIP6(new_dest_ipv6)
          if not validv6 then
            helpmsg["dest_ipv6"] = errmsg
          end
        end

        if validv6 then
          if new_enabledv6 == "0" then
            proxySet(path["Intercept-DNSv6"].."destIP","")
            dest_ipv6 = ""
          elseif new_enabledv6 == "1" then
            local lan_dns_path = "uci.dhcp.dhcp.@lan.dns."
            local first_lan_dns_svr = lan_dns_path.."@1."
            while (proxy.get(first_lan_dns_svr)) do
                proxy.del(first_lan_dns_svr)
            end
            if new_dest_ipv6 and new_dest_ipv6 ~= "" then
              local idx = proxy.add(lan_dns_path)
              proxySet(lan_dns_path.."@"..idx..".value",new_dest_ipv6)
              proxySet(path["Intercept-DNSv6"].."destIP",new_dest_ipv6)
              proxySet("rpc.gui.firewall.dns6_intercept.add_exception",new_dest_ipv6)
              dest_ipv6 = new_dest_ipv6
            else
              proxySet(path["Intercept-DNSv6"].."destIP","")
              local ula = proxy.get("rpc.network.interface.@lan.ipv6uniquelocaladdr")
              if ula and ula[1].value ~= "" then
                local idx = proxy.add(lan_dns_path)
                proxySet(lan_dns_path.."@"..idx..".value",untaint(ula[1].value))
              end
              dest_ipv6 = ""
            end
          end
          local paths = { "Intercept-DNSv6","dns6_xcptn","Allow-DoTv6","Allow-DoHv6","Deny-DoHv6","doh6" }
          for _,p in ipairs(paths) do
            proxySet(path[p].."enabled",new_enabledv6)
          end
          enabledv6 = new_enabledv6
        end

        local cron = "0"
        if new_enabledv4 == "1" or new_enabledv6 == "1" then
          proxySet("rpc.gui.firewall.dns_intercept.populate_ipsets","1")
          cron="1"
        end
        if proxy.get(cronjob.."enabled")[1].value ~= cron and proxySet(cronjob.."enabled",cron) then
          proxySet("rpc.gui.cron.writecrontab","1")
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

local ipv4cols,ipv4excdata,ipv4opts,ipv4exchelpmsg
if dns_int and enabledv4 == "1" then
  local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"
  local ipv4validation = {
    ip = vSIDIP,
  }
  ipv4cols = {
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
  ipv4opts = {
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
  ipv4excdata,ipv4exchelpmsg = post_helper.handleTableQuery(ipv4cols,ipv4opts,ip2hostnameFilter,nil,ipv4validation)
end

local ipv6cols,ipv6excdata,ipv6opts,ipv6exchelpmsg
if dns_int and enabledv6 == "1" then
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
  ipv6cols = {
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
  ipv6opts = {
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
          proxySet("rpc.gui.firewall.dns6_intercept.exceptions."..index..".ip",ip)
          proxy.apply()
          first = false
        else
          local next = proxy.add("rpc.gui.firewall.dns6_intercept.exceptions.")
          proxySet("rpc.gui.firewall.dns6_intercept.exceptions."..next..".ip",ip)
          proxy.apply()
        end
      end
    end
  }
  ipv6excdata,ipv6exchelpmsg = post_helper.handleTableQuery(ipv6cols,ipv6opts,ip2hostnameFilter,nil,ipv6validation)
end

ngx.print(ui_helper.createHeader(T"DNS",true,false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-dns_int-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>'); ngx.print( T"DNS Hijacking" ); ngx.print('</legend>');
    ngx.print('<legend><small>'); ngx.print( T"IPv4" ); ngx.print('</small></legend>');
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
    ngx.print(ui_helper.createSwitch(T"Enabled","enabledv4",enabledv4,enabled_attr))
    ngx.print(ui_helper.createInputText(T"DNS Server Address<span class='icon-question-sign' title='IPv4 address of the DNS server to which hijacked requests will be sent. Leave blank to have this device handle IPv4 DNS.'></span>","dest_ipv4",dest_ipv4,monitor_attr,helpmsg["dest_ipv4"]))
    if dns_int and enabledv4 == "1" then
      ngx.print(
        '<div class="control-group monitor-enabledv4 monitor-1 monitor-show-enabledv4" style="display; block">',
        '<label class="control-label">Excluded IPs</label><div class="controls">',
        ui_helper.createTable(ipv4cols,ipv4excdata,ipv4opts,monitor_attr,ipv4exchelpmsg),
        '</div></div>'
      )
    end
    ngx.print('<legend><small>'); ngx.print( T"IPv6" ); ngx.print('</small></legend>');
    ngx.print(ui_helper.createSwitch(T"Enabled","enabledv6",enabledv6,enabled_attr))
    ngx.print(ui_helper.createInputText(T"DNS Server Address<span class='icon-question-sign' title='IPv6 address of the DNS server to which hijacked requests will be sent. Leave blank to have this device handle IPv6 DNS.'></span>","dest_ipv6",dest_ipv6,monitor6_attr,helpmsg["dest_ipv6"]))
    if dns_int and enabledv6 == "1" then
      ngx.print(ui_helper.createSwitch(T"Verbose Logging","verbose",content["verbose"],monitor6_attr,vhelpmsg["verbose"]))
      ngx.print(
        '<div class="control-group monitor-enabledv6 monitor-1 monitor-show-enabledv6" style="display; block">',
        '<label class="control-label">Excluded IPs</label><div class="controls">',
        ui_helper.createTable(ipv6cols,ipv6excdata,ipv6opts,monitor6_attr,ipv6exchelpmsg),
        '</div></div>'
      )
    end
    ngx.print('\
  </form>\
</div>\
');
if dns_int then
  if enabledv4 == "1" and dest_ipv4 ~= "" then
    ngx.print('<script>$("#excludedIPv4 td:contains('); ngx.print(T(dest_ipv4)); ngx.print(')").siblings(":last").children(".btn-table-edit,.btn-table-delete").hide();</script>');
  end
  if enabledv6 == "1" and dest_ipv6 ~= "" then
    ngx.print('<script>$("#excludedIPv6 td:contains('); ngx.print(T(dest_ipv6)); ngx.print(')").siblings(":last").children(".btn-table-edit,.btn-table-delete").hide();</script>');
  end
end
ngx.print(ui_helper.createFooter());