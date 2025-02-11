--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local proxy = require("datamodel")
local proxy_helper = require("proxy_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local uinetwork = require("web.uinetwork_helper")
local splitter = require("split")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local find,gsub,lower,match = string.find,string.gsub,string.lower,string.match

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local vSIIP4 = post_helper.validateStringIsIPv4
local vSIIP6 = post_helper.validateStringIsIPv6
local vSIP = post_helper.validateStringIsPort
local gVSIDIP = post_helper.getValidateStringIsDeviceIPv4
local vB = post_helper.validateBoolean

--region host names
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
--endregion

--region cron
local cronjob
local data = proxy.getPN("rpc.gui.cron.entries.",true)
for _,v in ipairs(data) do
  local cmd = proxy.get(v.path.."command")
  if cmd and find(untaint(cmd[1].value),"doh%-ipsets%-maintain") then
    cronjob = v.path
    break
  end
end
--endregion

--region paths
local path = {}
local function findPath(basepath)
  local paths = proxy.getPN(basepath,true)
  for i=1,#paths,1 do
    local p = paths[i]
    local v = match(p.path,"^uci%.[^%.]+%.[^%.]+%.@([^%.]+)%.$")
    if find(v,"dns",1,true) or find(v,"doh",1,true) or find(v,"dot",1,true) then
      path[v] = p.path
    end
  end
end
findPath("uci.firewall.redirect.")
findPath("uci.firewall.nat.")
findPath("uci.firewall.rule.")
findPath("uci.firewall.include.")
findPath("uci.firewall.ipset.")
findPath("uci.tproxy.rule.")
--endregion

--region zones
local lan_zone
local zones = {}
local zone_paths = proxy.getPN("uci.firewall.zone.",true)
for z=1,#zone_paths,1 do
  local p = zone_paths[z]
  local v = proxy.get(p.path.."enabled",p.path.."wan",p.path.."name")
  if v and v[1].value ~= "0" and v[2].value ~= "1" and v[3].value ~= "loopback" then
    local name = untaint(v[3].value)
    local suffix = name == "lan" and "" or "_"..name
    local dns_int = path["dns_int"..suffix]
    if dns_int then
      local masquerade = path["dns_masq"..suffix]
      local state = proxy.get(dns_int.."enabled",masquerade.."dest_ip",dns_int.."dest_port")
      local networks = proxy.get(p.path.."network.")
      local validate = {}
      for n=1,#networks,1 do
        local network = untaint(networks[n].value)
        local ipdetails = proxy.get("uci.network.interface.@"..network..".ipaddr","uci.network.interface.@"..network..".netmask")
        validate[#validate+1] = gVSIDIP(untaint(ipdetails[1].value),untaint(ipdetails[2].value))
      end
      zones[#zones+1] = {
        name=name,
        paths={
          dns_int=dns_int,                                dns_masq=masquerade,
          doh_fwd_xcptn=path["doh_fwd_xcptn"..suffix],    doh_fwd=path["doh_fwd"..suffix],
          doh6_fwd_xcptn=path["doh6_fwd_xcptn"..suffix],  doh6_fwd=path["doh6_fwd"..suffix],
          dot_fwd_xcptn=path["dot_fwd_xcptn"..suffix],    dot_fwd=path["dot_fwd"..suffix],
          dot6_fwd_xcptn=path["dot6_fwd_xcptn"..suffix],  dot6_fwd=path["dot6_fwd"..suffix],
        },
        enabled=state[1].value == "0" and "0" or "1",
        dest_ipv4=state[2].value,
        dest_port=state[3].value,
        networks=networks,
        validate=validate
      }
      if name == "lan" then
        lan_zone = #zones
      end
    end
  end
end
table.sort(zones,function(a,b)
  if a.name == "lan" then
    return true
  elseif b.name == "lan" then
    return false
  end
  return lower(a.name) < lower(b.name)
end)
--endregion

--region functions
local function add_exception_if_in_zone(new_dest_ipv4)
  for i=1,#zones,1 do
    local vSIDIP = zones[i]["validate"]
    for v=1,#vSIDIP,1 do
      if vSIDIP[v](new_dest_ipv4) then
        proxy_helper.set("rpc.gui.firewall.dns_intercept.add_exception",new_dest_ipv4)
      end
    end
  end
end

local function zones_proxy_set(basepath,option,value)
  for i=1,#zones,1 do
    proxy_helper.set(zones[i]["paths"][basepath]..option,value)
  end
end
--endregion

local helpmsg = {}

local enabledv4,enabledv6 = "0","0"
local dest_ipv4,dest_ipv6,ip6addr = "","",""
local dest_port,targetPort = "",""
if #zones > 0 and lan_zone and cronjob and path["doh_fwd_xcptn"] and path["doh6_fwd_xcptn"] and path["doh_fwd"] and path["doh6_fwd"] and path["dot_fwd_xcptn"] and path["dot_fwd"] and path["dot6_fwd_xcptn"] and path["dot6_fwd"] and path["dns_xcptn"] and path["doh"] and path["doh6"] and path["dns6_xcptn"] and path["dnsv6"] and path["dns_dhcp_xcptn"] then
  --region IPv4 current state
  enabledv4 = zones[lan_zone]["enabled"]
  dest_ipv4 = zones[lan_zone]["dest_ipv4"]
  dest_port = zones[lan_zone]["dest_port"]
  --endregion
  --region IPv6 current state
  local statev6 = proxy.get(path["dnsv6"].."enabled",path["dnsv6"].."destIP","rpc.network.interface.@lan.ip6addr",path["dnsv6"].."targetPort")
  ip6addr = statev6[3].value
  if statev6 then
    if statev6[1].value and statev6[1].value ~= "0" then
      enabledv6 = "1"
    end
    if enabledv6 == "1" and ip6addr == "" then
      enabledv6 = "0"
      dest_ipv6 = ""
      proxy_helper.set(path["dnsv6"].."enabled",enabledv6)
      proxy_helper.set(path["dnsv6"].."destIP",dest_ipv6)
    else
      dest_ipv6 = untaint(statev6[2].value)
      targetPort = untaint(statev6[4].value)
    end

  end
  --endregion

  if ngx.var.request_method == "POST" then
    local args = ngx.req.get_post_args()
    if args["action"] == "SAVE" then
      --region IPv4 update
      local new_enabledv4 = untaint(args["enabledv4"])
      local new_dest_ipv4 = gsub(untaint(args["dest_ipv4"]),"^%s*(.-)%s*$","%1")
      local new_dest_port = untaint(args["dest_port"])
      local validv4,errmsg = vB(new_enabledv4)
      if not validv4 then
        helpmsg["enabledv4"] = errmsg
      end
      if new_dest_ipv4 ~= "" then
        local valid,msg = vSIIP4(new_dest_ipv4)
        if not valid then
          helpmsg["dest_ipv4"] = msg
          validv4 = valid
        end
      end
      if new_dest_port ~= "" then
        local valid,msg = vSIP(new_dest_port)
        if not valid then
          helpmsg["dest_port"] = msg
          validv4 = valid
        end
      end

      if validv4 then
        if new_enabledv4 == "0" then
          dest_ipv4 = ""
        elseif new_enabledv4 == "1" then
          if new_dest_ipv4 and new_dest_ipv4 ~= "" then
            zones_proxy_set("dns_masq","enabled",new_enabledv4)
            zones_proxy_set("dns_masq","dest_ip",new_dest_ipv4)
            zones_proxy_set("dns_int","dest_ip",new_dest_ipv4)
            add_exception_if_in_zone(new_dest_ipv4)
            dest_ipv4 = new_dest_ipv4
          else
            zones_proxy_set("dns_masq","enabled","0")
            dest_ipv4 = ""
          end
          if new_dest_port then
            zones_proxy_set("dns_int","dest_port",new_dest_port)
            dest_port = new_dest_port
          end
        end
        local paths = { "dns_int","doh_fwd_xcptn","doh_fwd","dot_fwd_xcptn","dot_fwd" }
        for p=1,#paths,1 do
          zones_proxy_set(paths[p],"enabled",new_enabledv4)
        end
        paths = { "dns_xcptn","dns_dhcp_xcptn","doh" }
        for p=1,#paths,1 do
          proxy_helper.set(path[paths[p]].."enabled",new_enabledv4)
        end
        enabledv4 = new_enabledv4
      end
      --endregion
      --region IPv6 update
      local new_enabledv6 = untaint(args["enabledv6"])
      local new_dest_ipv6 = gsub(untaint(args["dest_ipv6"]),"^%s*(.-)%s*$","%1")
      local new_targetPort = untaint(args["targetPort"])
      local validv6,errmsg6 = vB(new_enabledv6)
      if not validv6 then
        helpmsg["enabledv6"] = errmsg6
      end
      if new_dest_ipv6 ~= "" then
        local valid,msg = vSIIP6(new_dest_ipv6)
        if not valid then
          helpmsg["dest_ipv6"] = msg
          validv6 = valid
        end
      end
      if new_targetPort ~= "" then
        local valid,msg = vSIP(new_targetPort)
        if not valid then
          helpmsg["targetPort"] = msg
          validv4 = valid
        end
      end

      if validv6 then
        if new_enabledv6 == "0" or (new_enabledv6 == "1" and ip6addr == "") then
          new_enabledv6 = "0"
        elseif new_enabledv6 == "1" then
          if new_dest_ipv6 then
            proxy_helper.set(path["dnsv6"].."destIP",new_dest_ipv6)
            proxy_helper.set(path["dnsv6"].."targetPort",new_targetPort)
            local hosts = proxy.getPN("rpc.hosts.host.",true)
            for k=1,#hosts,1 do
              local v = hosts[k]
              local addresses = proxy.get(v.path..'IPv6')
              if addresses and addresses[1].value:find(new_dest_ipv6) then
                for address in addresses[1].value:gmatch('%S+') do
                  proxy_helper.set("rpc.gui.firewall.dns6_intercept.add_exception",address)
                end
                break
              end
            end
            dest_ipv6 = new_dest_ipv6
            targetPort = new_targetPort
          else
            dest_ipv6 = ""
          end
        end
        local paths = { "doh6_fwd_xcptn","doh6_fwd","dot6_fwd_xcptn","dot6_fwd" }
        for p=1,#paths,1 do
          zones_proxy_set(paths[p],"enabled",new_enabledv6)
        end
        paths = { "dnsv6","dns6_xcptn","doh6" }
        for p=1,#paths,1 do
          proxy_helper.set(path[paths[p]].."enabled",new_enabledv6)
        end
        enabledv6 = new_enabledv6
      end
      --endregion
      --region cron update
      local cron = "0"
      if new_enabledv4 == "1" or new_enabledv6 == "1" then
        proxy_helper.set("rpc.gui.firewall.dns_intercept.populate_ipsets","1")
        cron="1"
      end
      if proxy.get(cronjob.."enabled")[1].value ~= cron and proxy_helper.set(cronjob.."enabled",cron) then
        proxy_helper.set("rpc.gui.cron.writecrontab","1")
      end
      --endregion
      proxy.apply()
    end
  end
else
  zones = {}
  message_helper.pushMessage(T"Firewall configuration for DNS hijacking is incomplete! Please re-run tch-gui-unhide to fix.","error")
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
  ip = vSIIP4,
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
  input = { class = "monitor-changes" }
}
local monitor_attr = {
  group = { class = "monitor-enabledv4 monitor-1" },
  input = { class = "span2", placeholder = "Leave blank for Device" },
  span = { class = "span7", },
}
local monitor_port_attr = {
  group = { class = "monitor-enabledv4 monitor-1" },
  input = { type = "number", min = "0", max = "65535", style = "width:100px",},
}
local monitor6_attr = {
  group = { class = "monitor-enabledv6 monitor-1" },
  input = { class = "span3", placeholder = "Leave blank for Device" },
  span = { class = "span7", },
}
local monitor6_port_attr = {
  group = { class = "monitor-enabledv6 monitor-1" },
  input = { type = "number", min = "0", max = "65535", style = "width:100px",},
}

ngx.print(ui_helper.createHeader(T"DNS",true,false),'\
<div class="modal-body update">\
  '); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dns-hijacking-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <legend>',T"DNS Hijacking",'</legend>',
      ui_helper.createLabel(T"When Enabled",T"<ul>\
        <li>Plain DNS will be hijacked and forwarded to the specified DNS server</li>\
        <li>Plain DNS will be <b>NOT</b> be hijacked if destined for the specified DNS Server or one defined on the Local Network screens</li>\
        <li>DNS-over-HTTPS, DNS-over-TLS and DNS-over-QUIC will be <b>BLOCKED</b></li>\
        <li>Excluded IPs will have <b>NO</b> DNS requests hijacked or blocked</li>\
        </ul>",{span = { class = "span7", }}),'\
    <legend><small>',T"IPv4",'</small></legend>',
      ui_helper.createSwitch(T"Enabled","enabledv4",enabledv4,enabled_attr),
      ui_helper.createInputText(T"DNS Server Address<span class='icon-question-sign' title='IPv4 address of the DNS server to which hijacked requests will be sent. Leave blank to have this device handle IPv4 DNS.'></span>","dest_ipv4",dest_ipv4,monitor_attr,helpmsg["dest_ipv4"]),
      ui_helper.createInputText(T"DNS Server Port<span class='icon-question-sign' title='Leave blank unless the DNS Server is listening on a non-standard port.'></span>","dest_port",dest_port,monitor_port_attr,helpmsg["dest_port"]),'\
      <div class="control-group" style="display; block">\
      <label class="control-label">',T"Excluded IPs",'</label><div class="controls">',
      ui_helper.createTable(ipv4cols,ipv4excdata,ipv4opts,nil,ipv4exchelpmsg),'\
      </div></div>\
    <legend><small>',T"IPv6",'</small></legend>');
    if ip6addr == "" then
      enabledv6 = "0"
      ngx.print(
        ui_helper.createLabel(T"Disabled","No IPv6 LAN address"),
        ui_helper.createSimpleInputHidden("enabledv6",enabledv6),
        ui_helper.createSimpleInputHidden("dest_ipv6",""))
    else
      ngx.print(
        ui_helper.createSwitch(T"Enabled","enabledv6",enabledv6,enabled_attr),
        ui_helper.createInputText(T"DNS Server Address<span class='icon-question-sign' title='IPv6 address of the DNS server to which hijacked requests will be sent. Leave blank to have this device handle IPv6 DNS.'></span>","dest_ipv6",dest_ipv6,monitor6_attr,helpmsg["dest_ipv6"]),
        ui_helper.createInputText(T"DNS Server Port<span class='icon-question-sign' title='Leave blank unless the DNS Server is listening on a non-standard port.'></span>","targetPort",targetPort,monitor6_port_attr,helpmsg["targetPort"]),
        ui_helper.createSwitch(T"Verbose Logging","verbose",content["verbose"],monitor6_attr,vhelpmsg["verbose"]),'\
        <div class="control-group" style="display; block">\
        <label class="control-label">',T"Excluded IPs",'</label><div class="controls">',
        ui_helper.createTable(ipv6cols,ipv6excdata,ipv6opts,nil,ipv6exchelpmsg),'\
        </div></div>')
    end
    ngx.print('\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>');
if enabledv4 == "1" and dest_ipv4 ~= "" then
  ngx.print('<script>$("#excludedIPv4 td:contains(',T(dest_ipv4),')").siblings(":last").children(".btn-table-edit,.btn-table-delete").hide();</script>');
end
if enabledv6 == "1" and dest_ipv6 ~= "" then
  ngx.print('<script>$("#excludedIPv6 td:contains(',T(dest_ipv6),')").siblings(":last").children(".btn-table-edit,.btn-table-delete").hide();</script>');
end
ngx.print(ui_helper.createFooter());