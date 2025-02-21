--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local fwrules_helper = require("firewall-rules_helper")
local known_apps = require("pfwd_helper")
local message_helper = require("web.uimessage_helper")
local ports_list = require("portslist_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local splitter = require("split")
local ui_helper = require("web.ui_helper")
local uinetwork = require("web.uinetwork_helper")
local wanservices = require("webui_core.wanservices")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local ipv4_hosts_ac,ipv6_hosts_ac = uinetwork.getAutocompleteHostsList(true)

local ngx = ngx
local pairs,string,table,tonumber = pairs,string,table,tonumber
local concat,sort = table.concat,table.sort
local find,format,match = string.find,string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local lan_details = {
  gw = "uci.network.interface.@lan.ipaddr",
  netmask = "uci.network.interface.@lan.netmask"
}
content_helper.getExactContent(lan_details)

local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean
local vSIPR = post_helper.validateStringIsPortRange
local gVSIDIP = post_helper.getValidateStringIsDeviceIPv4
local vSIIP6 = post_helper.validateStringIsIPv6
local vSIDIP = gVSIDIP(lan_details.gw,lan_details.netmask)
local gAV = post_helper.getAndValidation
local gOrV = post_helper.getOrValidation
local vQTN = post_helper.validateQTN

local isPortAndProtoReserved,reserved_ports = wanservices.isPortAndProtoReserved()

local ipv4_protocol_list = {
  { "tcp",T"TCP" },
  { "udp",T"UDP" },
  { "tcpudp",T"TCP+UDP" }
}

local ipv6_protocol_list = {
  { "tcp",T"TCP" },
  { "udp",T"UDP" },
  { "tcpudp",T"TCP+UDP" },
  { "icmpv6",T"ICMPv6" },
  { "all",T"All" },
}

local getargs = ngx.req.get_uri_args()
if getargs.clear_leases and getargs.clear_leases == "1" then
  proxy.set("rpc.gui.upnpd.clear_leases","1")
end

-- Used to update firewall helpers when rule deleted
local del_ipv4_index = nil
local post_action
if ngx.var.request_method == "POST" then
  local content = ngx.req.get_post_args()
  post_action = content.action
  if post_action == "TABLE-DELETE" and content.tableid == "ipv4_port_forwarding" then
    del_ipv4_index = tonumber(content.index)
  end
  if post_action == "SAVE" or post_action == "TABLE-ADD" or post_action == "TABLE-MODIFY" or post_action == "TABLE-DELETE"  then
    proxy.set("rpc.gui.firewall.clearCache","1")
  end
end

local function remove_key(tbl,key)
  local element = tbl[key]
  tbl[key] = nil
  return element
end

local allowed_portlist = ports_list
for wanPort,_ in pairs(reserved_ports) do
  for i,j in pairs(allowed_portlist) do
    if wanPort and (tonumber(wanPort) == tonumber(j)) then
      remove_key(allowed_portlist,i)
    end
  end
end

local portrange_pattern = "^(%d+)%:(%d+)$"

local function compare_startport(a,b)
  return a.start < b.start
end

local function validate_ports(ports)
  table.sort(ports,compare_startport)
  local curend = -1
  for i=1,#ports do
    local v = ports[i]
    if v.start <= curend then
      ngx.log(ngx.ERR,format("An existing mapping overlaps with the ports range (%s) v.start=%d curend=%d",v.source,v.start,curend))
      return nil,{ wanport = T(format("An existing mapping overlaps with the ports range: %s",v.source)) }
    else
      curend = v["end"]
    end
  end
  return true
end

local function validate_name(value)
  if #value == 0 or #value > 63 then
    return nil,T"A name must be between 1 and 63 characters"
  end
  -- "DMZ rule" is the name reserved for DMZ portmap rule
  if value == "DMZ rule" then
    return nil,T"Reserved name cannot be given as rule name"
  end
  if match(value,"[^%w%-%s]") then
    return nil,T"A name must contain only alphanumeric characters and dash"
  end
  return true
end

--region IPv4 UPnP cross-validation
local upnp_columns = {
  {
    header = T"Protocol",
    name = "protocol",
    param = "proto",
    default = "tcp",
    type = "select",
    values = ipv4_protocol_list,
  },
  {
    header = T"Interface",
    name = "interface",
    param = "interface",
    type = "text",
  },
  {
    header = T"Remote Host",
    name = "remote_host",
    param = "remote_host",
    type = "text",
  },
  {
    header = T"WAN port",
    name = "wanport",
    param = "src_dport",
    type = "text",
  },
  {
    header = T"LAN port",
    name = "lanport",
    param = "dest_port",
    type = "text",
  },
  {
    header = T"Destination",
    name = "destinationip",
    param = "dest_ip",
    type = "text",
  },
  {
    header = T"Description",
    name = "description",
    param = "description",
    type = "text",
  },
}

local upnp_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "upnpportforwarding",
  basepath = "sys.upnp.redirect.",
}

local upnp_data = post_helper.handleTableQuery(upnp_columns,upnp_options,nil,nil,nil)
local tcp = {}
local udp = {}
if upnp_data ~= nil then
  for i=1,#upnp_data do
    local v = upnp_data[i]
    if v[1] == "TCP" or v[1] == "TCP+UDP" then
      tcp[#tcp+1] = { start = tonumber(v[4]),["end"] = tonumber(v[4]),index = i,source = "[UPnP] "..v[7] }
    end
    if v[1] == "UDP" or v[1] == "TCP+UDP" then
      udp[#udp+1] = { start = tonumber(v[4]),["end"] = tonumber(v[4]),index = i,source = "[UPnP] "..v[7] }
    end
  end
end
if not validate_ports(tcp) or not validate_ports(udp) then
  message_helper.pushMessage(T"UPnP leases contain port duplicates! This will <b>prevent</b> you from saving IPv4 port forwards.","error")
end
--endregion

local ipv4_columns = {
 {
   header = "",
   name = "enabled",
   param = "enabled",
   type = "switch",
   default = "1",
   attr = { switch = { ["data-placement"] = "right",class = "no-save" }}
 },
 {
   header = T"Name",
   name = "name",
   param = "name",
   type = "text",
   unique = true,
   attr = { input = { class="span2 no-save" } },
 },
 {
   header = T"Protocol",
   name = "protocol",
   param = "proto.@1.value",
   default = "tcp",
   type = "select",
   values = ipv4_protocol_list,
   attr = { select = { class="span2 no-save" } },
 },
 {
   header = T"WAN Port",
   name = "wanport",
   param = "src_dport",
   type = "text",
   attr = { input = { class="span1 no-save",maxlength="11" },autocomplete = allowed_portlist },
 },
 {
   header = T"LAN Port",
   name = "lanport",
   param = "dest_port",
   type = "text",
   attr = { input = { class="span1 no-save",maxlength="11" },autocomplete = ports_list },
 },
 {
   header = T"Destination IP",
   name = "destinationip",
   param = "dest_ip",
   type = "text",
   attr = { input = { class="span2 no-save",maxlength="15" },autocomplete = ipv4_hosts_ac },
 },
 {
   header = T"Destination MAC",
   name = "destinationmac",
   param = "dest_mac",
   type = "text",
   readonly = true,
 },
}

--region IPv4 functions
local del_ipv4_data

local function validate_ipv4_ports(data)
  if del_ipv4_index ~= nil then
    del_ipv4_data = content_helper.loadTableData("rpc.network.firewall.portforward.",ipv4_columns,nil,"name")
  end
  -- tcp and udp tables already contain UPnP leases; add all port forwards to check duplicates across both
  for i=1,#data do
    local v = data[i]
    if v[3] and v[4] then
      local p1,p2
      local proto = v[3]
      local chunks = { match(v[4],portrange_pattern) }
      if #chunks == 2 then
        p1 = tonumber(chunks[1])
        p2 = tonumber(chunks[2])
      else
        p1 = tonumber(v[4])
        p2 = p1
      end
      if proto == "tcp" or proto == "tcpudp" then
        tcp[#tcp+1] = { start = p1,["end"] = p2,index = i,source = v[2] }
      end
      if proto == "udp" or proto == "tcpudp" then
        udp[#udp+1] = { start = p1,["end"] = p2,index = i,source = v[2] }
      end
    end
  end
  local err,msg = validate_ports(tcp)
  if not err then
    return err,msg
  end
  err,msg = validate_ports(udp)
  return err,msg
end

local function set_helper_port(helper,port)
  local path = "uci.firewall.helper."
  local data = proxy.get(path)
  if data == nil then
    return
  end
  for k=1,#data do
    local v = data[k]
    if v.param == "helper" and v.value == helper then
      proxy.set(v.path.."dest_port",port)
      return
    end
  end
end

local pfw_helper_map = {}
pfw_helper_map["FTP server"] = {pfw = "FTP server",helper = "ftp",wanport = "21"}
pfw_helper_map["TFTP server"] = {pfw = "TFTP server",helper = "tftp",wanport = "69"}
pfw_helper_map["PPTP"] = {pfw = "PPTP",helper = "pptp",wanport = "1723"}

local function get_firewall_helper(userredir_name)
  for k=1,#pfw_helper_map do
    local tmp = pfw_helper_map[k]
    if userredir_name == tmp.pfw then
      return tmp
    end
  end
  return nil
end

local function get_firewall_helper_name(userredir_name)
  local tmp = get_firewall_helper(userredir_name)
  if tmp ~= nil then
    return tmp["helper"]
  else
    return nil
  end
end

local function update_firewall_helper(index,content)
  if index == nil then
    return
  end
  local helper = get_firewall_helper_name(content.name)
  -- the corresponding helper in firewall should be updated also
  if helper ~= nil then
    if content.enabled == "1" then
      set_helper_port(helper,untaint(content.wanport))
    else
      set_helper_port(helper,pfw_helper_map[untaint(content.name)].wanport)
    end
  end
end

local function remove_firewall_helper(_)
  if del_ipv4_index == nil then
    return
  end
  local helper = get_firewall_helper(del_ipv4_data[del_ipv4_index][2])
  del_ipv4_index = nil
  if helper ~= nil and helper["wanport"] ~= nil then
    set_helper_port(helper.helper,helper.wanport)
  end
end
--endregion

local ipv4_valid = {
  enabled = vB,
  name = validate_name,
  lanport = vSIPR,
  wanport = gAV(vSIPR,isPortAndProtoReserved),
  destinationip = gAV(vSIDIP,vQTN),
  protocol = gAV(gVIES(ipv4_protocol_list),isPortAndProtoReserved)
}

local ipv4_options = {
  tableid = "ipv4_port_forwarding",
  basepath = "rpc.network.firewall.portforward.",
  createMsg = T"Add IPv4 Port Forward",
  newList = known_apps,
  valid = validate_ipv4_ports,
  sorted = "name",
  onModify = update_firewall_helper,
  onAdd = update_firewall_helper,
  onDelete = remove_firewall_helper,
}

local function ipv4_filter(data)
  if data.target == "DNAT" and data.src == "wan" and data.dest == "lan" and data.family == "ipv4" and data.name ~= "DMZ rule" then
    return true
  end
  return false
end

local ipv4_default_object = {
  src = "wan",
  dest = "lan",
  family = "ipv4",
  target = "DNAT",
}

local ipv4_data,ipv4_helpmsg = post_helper.handleTableQuery(ipv4_columns,ipv4_options,ipv4_filter,ipv4_default_object,ipv4_valid)

local ipv6 = {
  lanIpv6Enabled = "uci.network.interface.@lan.ipv6",
  pinholeEnabled = "uci.firewall.rulesgroup.@pinholerules.enabled",
}
content_helper.getExactContent(ipv6)

local ipv6_columns = {
  {
    header = "",
    name = "enabled_v6",
    param = "enabled",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "right",class = "no-save" }}
  },
  {
    header = T"Name",
    name = "name",
    param = "name",
    type = "text",
    unique = true,
    attr = { input = { class="span2 no-save" } },
  },
  {
    header = T"Protocol",
    name = "protocol",
    param = "proto.@1.value",
    default = "tcp",
    type = "select",
    values = ipv6_protocol_list,
    attr = { select = { class="span2 no-save" } },
  },
  {
    header = T"Destination Port",
    name = "wanport",
    param = "dest_port",
    type = "text",
    attr = { input = { class="span1 no-save",maxlength="11" },autocomplete = ports_list },
  },
  {
    header = T"Destination IP",
    name = "dest_ip_v6",
    param = "dest_ip",
    type = "text",
    attr = { input = { class="span2 no-save",maxlength="39" },autocomplete = ipv6_hosts_ac },
  },
  {
    header = T"Destination MAC",
    name = "destinationmac_v6",
    param = "dest_mac",
    type = "text",
    readonly = true,
  },
}

--region IPv6 functions
local function validate_ipv6_address(value,object,key)
  local v = untaint(value)
  local delimiter = find(v,"<br>",1,true) and "<br>" or " "
  local addresses = splitter.split(v,delimiter)
  for k=1,#addresses do
    local addr = addresses[k]
    local valid,errmsg = vSIIP6(addr)
    if not valid then
      return valid,addr..": "..errmsg
    end
  end
  object[key] = concat(addresses," ")
  return true
end

local function validate_ipv6_port(_,data)
  if data.protocol == "icmpv6" or data.protocol == "all" then
    return true
  end
  return false
end

local function validate_ipv6_duplicate_rules(object)
  local function protocolDuplicated(protocol_1,protocol_2)
    local protoMap  = {
      tcp    = {"tcp"},
      udp    = {"udp"},
      tcpudp = {"tcp","udp"},
      icmpv6 = {"icmpv6"},
      all    = {"all"},
    }
    local port_1_val = protoMap[untaint(protocol_1)]
    local port_2_val = protoMap[untaint(protocol_2)]
    for i=1,#port_1_val do
      local proto_1 = port_1_val[i]
      for k=1,#port_2_val do
        local proto_2 = port_2_val[k]
        if proto_1 == proto_2 then
          -- return true since protocols duplicate
          return true
        end
      end
    end
    return false
  end

  local function portsOverlap(port_1,port_2)
    local port_1_splitted = { port_1:match(portrange_pattern) }
    local port_2_splitted = { port_2:match(portrange_pattern) }
    -- split range
    local port_1_start = #port_1_splitted == 2 and tonumber(port_1_splitted[1]) or tonumber(port_1)
    local port_1_end   = #port_1_splitted == 2 and tonumber(port_1_splitted[2]) or port_1_start
    local port_2_start = #port_2_splitted == 2 and tonumber(port_2_splitted[1]) or tonumber(port_2)
    local port_2_end   = #port_2_splitted == 2 and tonumber(port_2_splitted[2]) or port_2_start
    -- return true if overlap
    return math.max(port_1_start,port_2_start) <= math.min(port_1_end,port_2_end)
  end

  local function destinationsSame(destination_1,destination_2)
    for dest_1 in destination_1:gmatch("([^%s]+)") do
      for dest_2 in destination_2:gmatch("([^%s]+)") do
        if dest_1 == dest_2 then
          return true
        end
      end
    end
    return false
  end

  for row=1,#object do
    for n=row+1,#object do
      if object[n][3] ~= "icmpv6" and object[n][3] ~= "all" and object[row][3] ~= "icmpv6" and object[row][3] ~= "all" then
        if destinationsSame(object[row][5],object[n][5]) and portsOverlap(object[row][4],object[n][4]) and protocolDuplicated(object[row][3],object[n][3]) then
          return nil,{ wanport = T"duplicate value" }
        end
      else
        if destinationsSame(object[row][5],object[n][5]) and object[row][3] == object[n][3]  then
          return nil,{ protocol = T"duplicate value" }
        end
      end
    end
  end
  return true
end
--endregion

local ipv6_valid = {
  enabled_v6 = vB,
  name = validate_name,
  protocol = gVIES(ipv6_protocol_list),
  wanport = gOrV(validate_ipv6_port,gAV(vSIPR,isPortAndProtoReserved)),
  dest_ip_v6 = validate_ipv6_address,
}

local ipv6_options = {
  tableid = "ipv6_port_forwarding",
  basepath = "rpc.network.firewall.pinholerule.",
  createMsg = T"Add IPv6 Pin Hole Rule",
  sorted = "name",
  newList = known_apps,
  valid = validate_ipv6_duplicate_rules
}

local function ipv6_filter(data)
  if data.target == "ACCEPT" and data.src == "wan" and data.dest == "lan" and data.family == "ipv6" then
    local addresses = splitter.split(untaint(data.dest_ip)," ")
    sort(addresses)
    data.dest_ip = concat(addresses,"<br>")
    return true
  end
  return false
end

local ipv6_default_object = {
  target = "ACCEPT",
  src = "wan",
  dest = "lan",
  family = "ipv6",
}

local ipv6_data,ipv6_helpmsg = post_helper.handleTableQuery(ipv6_columns,ipv6_options,ipv6_filter,ipv6_default_object,ipv6_valid)

local fw_redirect_options = {
  tableid = "fw_redirect_rules",
  basepath = "uci.firewall.redirect.@.",
  canAdd = false,
  canEdit = true,
  canDelete = false,
}

local fw_redirect_columns,fw_redirect_data,fw_redirect_helpmsg = fwrules_helper.handleTableQuery(fw_redirect_options)

local fw_nat_options = {
  tableid = "fw_nat_rules",
  basepath = "uci.firewall.nat.@.",
  canAdd = false,
  canEdit = true,
  canDelete = false,
}

local fw_nat_columns,fw_nat_data,fw_nat_helpmsg = fwrules_helper.handleTableQuery(fw_nat_options)

local info = { alert = { class = "alert-info" }, }
local warning = { alert = { class = "alert-range" }, }

ngx.print(ui_helper.createHeader(T"Firewall",true,false),'\
<div class="modal-body update">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-port-forwarding-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),
    ui_helper.createAlertBlock(T"To enter WAN port ranges use <b>':'</b> (for example 500:505)",info),'\
    <fieldset>\
      <legend>',T"IPv4 Port Forwarding Rules",'</legend>')
        if #upnp_data > 0 then
          ui_helper.createAlertBlock(T"WARNING: WAN ports can <b>NOT</b> overlap existing UPnP leases, even if UPnP is <i>disabled</i>!",warning)
        end
        ngx.print(ui_helper.createTable(ipv4_columns,ipv4_data,ipv4_options,nil,ipv4_helpmsg),'\
    </fieldset>');
    if ipv6.lanIpv6Enabled ~= "0" and ipv6.pinholeEnabled == "1" then
      ngx.print('\
      <fieldset>\
        <legend>',T"IPv6 Pin Hole Rules",'</legend>',
          ui_helper.createTable(ipv6_columns,ipv6_data,ipv6_options,nil,ipv6_helpmsg),'\
      </fieldset>\
      ');
    end
    if #upnp_data > 0 then
      ngx.print('\
      <fieldset>\
        <legend>',T"UPnP Leases",'</legend>',
          ui_helper.createTable(upnp_columns,upnp_data,upnp_options,nil,nil),
          ui_helper.createButton("Clear Leases","Restart UPnP Service","icon-refresh",{ button = { id = "btn-clear-leases" }, }),'\
          <script>$("#btn-clear-leases").click(function(){tch.loadModal($(".modal form").attr("action")+"?clear_leases=1");})</script>\
      </fieldset>\
      ');
    end
    ngx.print('\
    <fieldset>\
      <legend>',T"System Redirect Rules",'</legend>',
        ui_helper.createTable(fw_redirect_columns,fw_redirect_data,fw_redirect_options,nil,fw_redirect_helpmsg),'\
    </fieldset>\
    <fieldset>\
      <legend>',T"System NAT Rules",'</legend>',
        ui_helper.createTable(fw_nat_columns,fw_nat_data,fw_nat_options,nil,fw_nat_helpmsg),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter());