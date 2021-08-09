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
local hosts_ac = uinetwork.getAutocompleteHostsList()
local find, match, gsub, untaint = string.find, string.match, string.gsub, string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local ipdetails = {
  gw = "uci.network.interface.@lan.ipaddr",
  netmask = "uci.network.interface.@lan.netmask"
}
content_helper.getExactContent(ipdetails)

local vSIIP4 = post_helper.validateStringIsIPv4
local gVSIDIP = post_helper.getValidateStringIsDeviceIPv4
local vSIDIP = gVSIDIP(ipdetails.gw, ipdetails.netmask)
local vB = post_helper.validateBoolean

local path = {}

local function findPath(basepath, option)
  local v
  for _,v in ipairs(proxy.getPN(basepath, true)) do
    local values = proxy.get(v.path..(option or "name"))
    if values then
      local name = untaint(values[1].value)
      if name and name ~= "" then
        path[name] = v.path
      end
    end
  end
end

local function proxySet(path, value)
  local success, errmsg = proxy.set(path, value)
  if not success then
    message_helper.pushMessage(T("Failed to set "..path.." to "..value..": "..errmsg), "error")
  end
  return success
end

findPath("uci.firewall.redirect.")
findPath("uci.firewall.nat.")
findPath("uci.firewall.rule.")
findPath("uci.firewall.include.", "path")
findPath("uci.firewall.ipset.")

local cronjob
local data, v = proxy.getPN("rpc.gui.cron.entries.", true)
for _,v in ipairs(data) do
  local cmd = proxy.get(v.path.."command")
  if cmd and find(untaint(cmd[1].value),"doh%-ipsets%-maintain") then
    cronjob = v.path
    break
  end
end

local helpmsg = {}

local enabled = "0"
local dest_ip
local ipv6 = "1"
local dns_int = path["Intercept-DNS"] or path["Redirect-DNS"]
if dns_int then
  if cronjob and path["Masquerade-DNS"] and path["Allow-DoT"] and path["Deny-DoT"] and path["Allow-DoH"] and path["Deny-DoH"] and path["Deny-v6-DoH"] and path["dns_xcptn"] and path["doh"] and path["doh6"] and path["/usr/sbin/ipsets-restore"] then
    local state = proxy.get(dns_int.."enabled", path["Masquerade-DNS"].."dest_ip", path["Deny-DoT"].."family")
    if state then
      if state[1].value and state[1].value ~= "0" then
        enabled = "1"
      end
      dest_ip = untaint(state[2].value)
      if state[3].value and state[3].value == "ipv4" then
        ipv6 = "0"
      end
    end

    if ngx.var.request_method == "POST" then
      local args = ngx.req.get_post_args()
      if args["action"] == "SAVE" then
        local new_enabled = untaint(args["enabled"])
        local new_dest_ip = gsub(untaint(args["dest_ip"]),"^%s*(.-)%s*$","%1")
        local new_ipv6 = untaint(args["ipv6"])

        local valid, errmsg = vB(new_enabled)
        if not valid then
          helpmsg["enabled"] = errmsg
        else
          valid, errmsg = vB(new_ipv6)
          if not valid then
            helpmsg["ipv6"] = errmsg
          end
        end
        if valid and new_enabled == "1" and enabled == "0" then
          if new_dest_ip ~= "" then
            valid, errmsg = vSIIP4(new_dest_ip)
            if not valid then 
              helpmsg["dest_ip"] = errmsg
            end
          end
        end

        if valid then
          if new_enabled == "0" then
            proxySet(dns_int.."dest_ip", "")
            proxySet(path["Masquerade-DNS"].."dest_ip", "")
            proxySet("rpc.gui.firewall.dns_intercept.delete_all_exceptions", "1")
            local paths, p = { "Masquerade-DNS", "Deny-v6-DoH", "doh6" }
            for _,p in pairs(paths) do
              proxySet(path[p].."enabled", new_enabled)
            end
            dest_ip = ""
          elseif new_enabled == "1" then
            if new_dest_ip and new_dest_ip ~= "" then
              proxySet(dns_int.."dest_ip", new_dest_ip)
              proxySet(dns_int.."name", "Redirect-DNS")
              proxySet(path["Masquerade-DNS"].."dest_ip", new_dest_ip)
              proxySet(path["Masquerade-DNS"].."enabled", new_enabled)
              if vSIDIP(new_dest_ip) then
                proxySet("rpc.gui.firewall.dns_intercept.add_exception", new_dest_ip)
              end
              dest_ip = new_dest_ip
            else
              proxySet(dns_int.."dest_ip", "")
              proxySet(dns_int.."name", "Intercept-DNS")
              proxySet(path["Masquerade-DNS"].."dest_ip", "")
              proxySet(path["Masquerade-DNS"].."enabled", "0")
              if vSIDIP(dest_ip) then
                proxySet("rpc.gui.firewall.dns_intercept.delete_exception", dest_ip)
              end
              dest_ip = ""
            end
            if new_ipv6 == "0" then
              proxySet(path["Deny-DoT"].."family", "ipv4")
              proxySet(path["Deny-v6-DoH"].."enabled", "0")
              proxySet(path["doh6"].."enabled", "0")
            else
              proxySet(path["Deny-DoT"].."family", "")
              proxySet(path["Deny-v6-DoH"].."enabled", new_enabled)
              proxySet(path["doh6"].."enabled", new_enabled)
            end
          end
          proxySet(dns_int.."enabled", new_enabled)
          local paths, p = { "Allow-DoT", "Allow-DoH", "Deny-DoT", "Deny-DoH", "dns_xcptn", "doh", "/usr/sbin/ipsets-restore" }
          for _,p in pairs(paths) do
            proxySet(path[p].."enabled", new_enabled)
          end
          proxySet("rpc.gui.firewall.dns_intercept.populate_ipsets", new_enabled)
          if proxySet(cronjob.."enabled", new_enabled) then
            proxySet("rpc.gui.cron.writecrontab", "1")
          end
          enabled = new_enabled
          ipv6 = new_ipv6
        end
        proxy.apply()
      end
    end
  else
    dns_int = nil
    message_helper.pushMessage(T"Firewall configuration for DNS interception is incomplete! Please re-run tch-gui-unhide to fix.", "error")
  end
end

ngx.print(ui_helper.createHeader(T"Firewall", true, false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-dns_int-modal.lp">\
    '); 
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>'); ngx.print( T"DNS Interception/Hijacking" ); ngx.print('</legend>');
    local info_attr = {
      alert = {
        class = "alert-info"
      },
    }
    ngx.print(ui_helper.createAlertBlock(T"DNS hijacking only applies to IPv4 DNS requests. IPv6 DNS hijacking requires a NAT6 kernel module that is not available for this device.<br>However, IPv6 DNS-over-TLS and DNS-over-HTTPS ARE intercepted and blocked if enabled.", info_attr))
    local enabled_attr = {
      input = {
        class = "monitor-changes"
      }
    }
    local monitor_attr = { 
      group = { 
        class = "monitor-enabled monitor-1" 
      }, 
    }
    ngx.print(ui_helper.createSwitch(T"Enabled", "enabled", enabled, enabled_attr)) 
    ngx.print(ui_helper.createInputText(T"DNS Server Address<span class='icon-question-sign' title='IPv4 address of the DNS server to which intercepted requests will be sent. Leave blank to have this device handle DNS.'></span>", "dest_ip", dest_ip, monitor_attr, helpmsg["dest_ip"]))
    ngx.print(ui_helper.createSwitch(T"Block IPv6 DoH/DoT<span class='icon-question-sign' title='Enable to block DNS-over-HTTPS and DNS-over-TLS for IPv6'></span>", "ipv6", ipv6, monitor_attr, helpmsg["ipv6"])) 
    if dns_int and enabled == "1" then
      local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"
      local cols = {
        {
          header = T"IP Addresses Excluded from Interception",
          name = "ip",
          param = "ip",
          type = "text",
          attr = { input = { class="span2", maxlength="15", pattern = ipv4pattern }, autocomplete=hosts_ac },
          unique = true
        },
      }
      local validation = {
        ip = vSIDIP,
      }
      local opts = {
        canEdit = true,
        canAdd = true,
        canDelete = true,
        tableid = "excludedIPs",
        basepath = "rpc.gui.firewall.dns_intercept.exceptions.@.",
        createMsg = T"Add New Excluded IPv4 Address",
        onAdd = function(index, content)
          proxy.set("rpc.gui.firewall.dns_intercept.exceptions."..index..".ip", content.ip)
        end
      }

      local data, exchelpmsg = post_helper.handleTableQuery(cols, opts, nil, nil, validation)
      ngx.print(ui_helper.createTable(cols, data, opts, monitor_attr, exchelpmsg))
    end
    ngx.print('\
  </form>\
</div>\
'); 
if dns_int and enabled == "1" then
  ngx.print('<script>$("#excludedIPs td:contains('); ngx.print(T(dest_ip)); ngx.print(')").siblings(":last").children(".btn-table-edit,.btn-table-delete").hide();</script>');
end
ngx.print(ui_helper.createFooter());