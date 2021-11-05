--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
local portslist = require("portslist_helper")
local knownapps = require("pfwd_helper")
local uinetwork = require("web.uinetwork_helper")
local fwrules_helper = require("firewall-rules_helper")

local hosts_ac, hosts_ac_v6 = uinetwork.getAutocompleteHostsList()
local pairs, string = pairs, string
local table, ipairs, tonumber, format, match, untaint = table, ipairs, tonumber, string.format, string.match, string.untaint

local del_pfw_index = nil

local lp = require("web.lp")
lp.setpath("/www/snippets/")

if ngx.var.request_method == "POST" then
  local content = ngx.req.get_post_args()
  local action = content.action
  if action == "SAVE" or action == "TABLE-ADD" or action == "TABLE-MODIFY" or action == "TABLE-DELETE"  then
    proxy.set("rpc.gui.firewall.clearCache", "1")
  end

  if action == "TABLE-DELETE" and content.tableid == "portforwarding" then
    del_pfw_index = tonumber(content.index)
  end
end

-- Templates for pre-defined rules
local function table_removekey(table, key)
  local element = table[key]
  table[key] = nil
  return element
end

local wan_app = {}
-- Retrieve all wan-services sections in system config
local servicesTable = proxy.getPN("uci.system.wan-service.", true)
if servicesTable then
  for _,service in ipairs(servicesTable) do
    local port = match(service.path, "uci%.system%.wan%-service%.@([^%.]+)%.")
    if port then
      wan_app[#wan_app + 1 ] = port
    end
  end
end

local wan_ports ={}
-- Retrieve the list of ports in all wan-services section
for _,app in ipairs(wan_app) do
  local wan_port_path = "uci.system.wan-service.@".. app .. ".ports"
  wan_ports[#wan_ports + 1 ] = proxy.get(wan_port_path)
end

local reserved_ports = {}
for _,wanPort in ipairs(wan_ports) do
  for port in wanPort[1].value:gmatch("%w+") do
    reserved_ports [#reserved_ports +1] = port
  end
end

--Remove the ports configured in system.wan-service in allowed_portlist
local allowed_portlist = portslist
for _,wanPort in ipairs(reserved_ports) do
  for i,j in pairs(allowed_portlist) do
    if wanPort and (tonumber(wanPort) == tonumber(j)) then
      table_removekey(allowed_portlist,i)
    end
  end
end

-- Function to check the port are reserved or not while adding the port mapping rule.
local function allowed_ports()
 return function(ports, postdata, key)
  for _,wanPort in ipairs(reserved_ports) do
    if wanPort and (wanPort == ports) then
      return nil, T"Ports already Reserved"
    end
    end
    return true
 end
end

local knownapps = require("pfwd_helper")

local pfw_helper_map = {}
pfw_helper_map["FTP server"] = {pfw = "FTP server", helper = "ftp", wanport = "21"}
pfw_helper_map["TFTP server"] = {pfw = "TFTP server", helper = "tftp", wanport = "69"}
pfw_helper_map["PPTP"] = {pfw = "PPTP", helper = "pptp", wanport = "1723"}

local portrange_pattern = "^(%d+)%:(%d+)$"
local function compare_startport(a,b)
  return a.start < b.start
end

local function validPorts(ports)
  local curend = -1
  for _,v in ipairs(ports) do
    if v.start <= curend then
      return nil, { wanport = T"An existing mapping overlaps with the ports range" }
    else
      curend = v["end"]
    end
  end
  return true
end

-- Firewall forwarding rules
local pfw_columns = {
 {
   header = "",
   name = "enabled",
   param = "enabled",
   type = "switch",
   default = "1",
   attr = { switch = { ["data-placement"] = "right" }}
 },
 {
   header = T"Name",
   name = "name",
   param = "name",
   type = "text",
   unique = true,
   attr = { input = { class="span2" } },
 },
 {
   header = T"Protocol",
   name = "protocol",
   param = "proto.@1.value",
   default = "tcp",
   type = "select",
   values = {
     { "tcp", "TCP"},
     { "udp", "UDP"},
     { "tcpudp", "TCP/UDP"}
   },
   attr = { select = { class="span2" } },
 },
 {
   header = T"WAN Port",
   name = "wanport",
   param = "src_dport",
   type = "text",
   attr = { input = { class="span1", maxlength="11" }, autocomplete=allowed_portlist },
 },
 {
   header = T"LAN Port",
   name = "lanport",
   param = "dest_port",
   type = "text",
   attr = { input = { class="span1", maxlength="11" }, autocomplete=portslist },
 },
 {
   header = T"Destination IP",
   name = "destinationip",
   param = "dest_ip",
   type = "text",
   attr = { input = { class="span2", maxlength="15" }, autocomplete=hosts_ac },
 },
 {
   header = T"Destination MAC",
   name = "destinationmac",
   param = "dest_mac",
   type = "text",
   readonly = true,
   attr = {  },
 },
}

local function globalValid(data)
  local tcp = {}
  local udp = {}
  local p1,p2
  local err, msg

  local allowedIndexes
  if del_pfw_index ~= nil then
    del_pfw_data, allowedIndexes = content_helper.loadTableData("rpc.network.firewall.portforward.", pfw_columns, nil, "name")
  end

  for i,v in ipairs(data) do
    if v[3] and v[4] then
      local chunks = { v[4]:match(portrange_pattern) }
      if #chunks == 2 then
        p1 = tonumber(chunks[1])
        p2 = tonumber(chunks[2])
      else
        p1 = tonumber(v[4])
        p2 = p1
      end

      local proto = v[3]
      if proto == "tcp" or proto == "tcpudp" then
        tcp[#tcp+1] = { start = p1, ["end"] = p2, index = i }
      end
      if proto == "udp" or proto == "tcpudp" then
        udp[#udp+1] = { start = p1, ["end"] = p2, index = i }
      end
    end
  end

  table.sort(tcp, compare_startport)
  table.sort(udp, compare_startport)

  err, msg = validPorts(tcp)
  if not err then
    return err, msg
  end
  err, msg = validPorts(udp)

  return err, msg
end

local function getValidateName(value)
 if #value == 0 or #value > 63 then
   return nil, T"A name must be between 1 and 63 characters"
 end
 -- "DMZ rule" is the name reserved for DMZ portmap rule
 if value == "DMZ rule" then
   return nil, T"Reserved name cannot be given as rule name"
 end
 if match(value, "[^%w%-%s]") then
   return nil, T"A name must contain only alphanumeric characters and dash"
 end
 return true
end

local protocolList = {
   { "tcp", "TCP"},
   { "udp", "UDP"},
   { "tcpudp", "TCP+UDP"}
}

-- Retrieve GW IP + netmask for use by validation function
local ipdetails = {
  gw = "uci.network.interface.@lan.ipaddr",
  netmask = "uci.network.interface.@lan.netmask"
}
content_helper.getExactContent(ipdetails)
-- Shortcuts to validation helpers to make lines shorter
local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean
local vSIPR = post_helper.validateStringIsPortRange
local gVSIDIP = post_helper.getValidateStringIsDeviceIPv4
local vSIIP6 = post_helper.validateStringIsIPv6
local vSIDIP = gVSIDIP(ipdetails.gw, ipdetails.netmask)
local gAV = post_helper.getAndValidation
local vQTN = post_helper.validateQTN
local validateDMZ = post_helper.validateDMZ

local pfw_valid = {
   enabled = vB,
   name = getValidateName,
   lanport = vSIPR,
   wanport = gAV(vSIPR,allowed_ports()),
   destinationip = gAV(vSIDIP,vQTN),
   protocol = gVIES(protocolList),
}
-- ip handleTableQuery parameter filter callback to only show ipv4 port forwardings ...
-- and only user created rules
-- return true if entry should be displayed
local function pfw_filter(data)
  if (data.target == "DNAT" and
    data.src == "wan" and
    data.dest == "lan" and
    data.family == "ipv4" and data.name ~= "DMZ rule") then
    return true
  end
  return false
end

-- Warning, this uses transformer paths. So use correct naming
local pfw_defaultObject = {
  src = "wan",
  dest = "lan",
  family = "ipv4",
  target = "DNAT",
}

local function set_helper_port(helper, port)
  local path = "uci.firewall.helper."
  local data = proxy.get(path)
  if data == nil then
    return
  end
  for _,v in ipairs(data) do
    if v.param == "helper" and v.value == helper then
      proxy.set(v.path .. "dest_port", port)
      return
    end
  end
end

local function get_firewall_helper(userredir_name)
  local tmp
  for _,tmp in pairs(pfw_helper_map) do
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

local function update_firewall_helper(index, content)
  if index == nil then
    return
  end

  local helper = get_firewall_helper_name(content.name)

  -- the corresponding helper in firewall should be updated also
  if helper ~= nil then
    if content.enabled == "1" then
      set_helper_port(helper, string.untaint(content.wanport))
    else
      set_helper_port(helper, pfw_helper_map[string.untaint(content.name)].wanport)
    end
  end
end


local function onDelete(index)
  if del_pfw_index == nil then
    return
  end

  local helper = get_firewall_helper(del_pfw_data[del_pfw_index][2])
  del_pfw_index = nil

  if helper ~= nil and helper["wanport"] ~= nil then
    set_helper_port(helper.helper, helper.wanport)
  end
end

local pfw_options = {
  tableid = "portforwarding",
  basepath = "rpc.network.firewall.portforward.",
  createMsg = T"Add IPv4 port mapping",
  newList = knownapps,
  valid = globalValid,
  sorted = "name",
  onModify = update_firewall_helper,
  onAdd = update_firewall_helper,
  onDelete = onDelete,
}

local pfw_data, pfw_helpmsg = post_helper.handleTableQuery(pfw_columns, pfw_options, pfw_filter, pfw_defaultObject, pfw_valid)

-- Ipv6 data retrieval - Start
-- Useful pieces of transformer data for ipv6
local ipv6Data = {
  -- is IPv6 enabled on the LAN
  lanIpv6Enabled = "uci.network.interface.@lan.ipv6",
  pinholeEnabled = "uci.firewall.rulesgroup.@pinholerules.enabled",
}
content_helper.getExactContent(ipv6Data)

-- for drop down selector and validation
local protocolList_v6 = {
  { "tcp", T"TCP"},
  { "udp", T"UDP"},
  { "tcpudp", T"TCP/UDP"},
--{ "udplite", T"UDPLite"}, -- doesn't exist
  { "icmpv6", T"ICMPv6"},
--{ "esp", T"ESP"},  -- fails
--{ "ah", T"AH"},  -- fails
--{ "sctp", T"SCTP"}, -- fails
  { "all", T"All"},
}
-- ipv6 handleTableQuery parameter to match columns to rpc table data
-- ipv6 Firewall forwarding rules
local pfw_v6_columns = {
  {
    header = "",
    name = "enabled_v6",
    param = "enabled",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }}
  },
  {
    header = T"Name",
    name = "name",
    param = "name",
    type = "text",
    unique = true,
    attr = { input = { class="span2" } },
  },
  {
    header = T"Protocol",
    name = "protocol",
    param = "proto.@1.value",
    default = "tcp",
    type = "select",
    values = protocolList_v6,
    attr = { select = { class="span2" } },
  },
  {
    header = T"Destination Port",
    name = "wanport",
    param = "dest_port",
    type = "text",
    attr = { input = { class="span1", maxlength="11" }, autocomplete=portslist },
  },
  {
    header = T"Destination IP",
    name = "dest_ip_v6",
    param = "dest_ip",
    type = "text",
    attr = { input = { class="span2", maxlength="39" }, autocomplete=hosts_ac_v6 },
  },
  {
    header = T"Destination MAC",
    name = "destinationmac_v6",
    param = "dest_mac",
    type = "text",
    readonly = true,
    attr = {  },
  },
}
-- ipv6 handleTableQuery parameter to specify transformer table to use
local pfw_v6_options = {
  tableid = "fwrules_v6",
  basepath = "rpc.network.firewall.pinholerule.",
  createMsg = T"Add IPv6 forwarding rule",
  sorted = "name",
  newList = knownapps,
}
-- ipv6 handleTableQuery parameter filter callback to only show ipv6 rules ...
-- and only user created rules
-- return true if entry should be displayed
local function pfw_v6_filter(data)
  if (data.target == "ACCEPT" and data.src == "wan" and data.dest == "lan" and data.family == "ipv6") then
    return true
  end
  return false
end
-- ipv6 handleTableQuery parameter for default values when adding entry
local pfw_v6_defaultObject = {
  target = "ACCEPT",
  src = "wan",
  dest = "lan",
  family = "ipv6",
}
-- ipv6 handleTableQuery parameter to check validity of new entry values
-- every editable column needs a validity check function
local pfw_v6_valid = {
  enabled_v6 = vB,
  name = getValidateName,
  protocol = gVIES(protocolList_v6),
  wanport = vSIPR,
  dest_ip_v6 = vSIIP6,
}
-- ipv6 pull in all the data for the display
local pfw_v6_data, pfw_v6_helpmsg = post_helper.handleTableQuery(pfw_v6_columns, pfw_v6_options, pfw_v6_filter, pfw_v6_defaultObject, pfw_v6_valid)
-- Ipv6 data retrieval - End

local fw_redirect_options = {
  tableid = "fw_redirect_rules",
  basepath = "uci.firewall.redirect.@.",
  canAdd = false,
  canEdit = true,
  canDelete = false,
}
local fw_redirect_columns, fw_redirect_data, fw_redirect_helpmsg = fwrules_helper.handleTableQuery(fw_redirect_options)

local fw_nat_options = {
  tableid = "fw_nat_rules",
  basepath = "uci.firewall.nat.@.",
  canAdd = false,
  canEdit = true,
  canDelete = false,
}
local fw_nat_columns, fw_nat_data, fw_nat_helpmsg = fwrules_helper.handleTableQuery(fw_nat_options)

local range_warning = {
  alert = {
    class = "alert-range"
  },
}

ngx.print(ui_helper.createHeader(T"Firewall", true, false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-port-forwarding-modal.lp">\
    ');  
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print(ui_helper.createAlertBlock(format("%s <strong>\":\"</strong> %s",T"To use port ranges use", T"for example 500:505."), range_warning))
    ngx.print('\
    <fieldset>\
      <legend>');  ngx.print( T"IPv4 Port Forwarding" ); ngx.print('</legend>\
      ');  
      ngx.print(ui_helper.createTable(pfw_columns, pfw_data, pfw_options, nil, pfw_helpmsg))
      ngx.print('\
    </fieldset>\
    ');  
    if ipv6Data.lanIpv6Enabled ~= "0" and ipv6Data.pinholeEnabled == "1" then
      ngx.print('\
      <fieldset>\
        <legend>');  ngx.print( T"IPv6 Port Forwarding" ); ngx.print('</legend>\
        ');  
          -- magic
          -- display/edit all the retrieved ipv6 firewall rule data
          ngx.print(ui_helper.createTable(pfw_v6_columns, pfw_v6_data, pfw_v6_options, nil, pfw_v6_helpmsg))
          ngx.print('\
      </fieldset>\
      ');  
    end
    ngx.print('\
    <fieldset>\
      <legend>');  ngx.print( T"System Redirect Rules" ); ngx.print('</legend>\
      ');  
      ngx.print(ui_helper.createTable(fw_redirect_columns, fw_redirect_data, fw_redirect_options, nil, fw_redirect_helpmsg))
      ngx.print('\
    </fieldset>\
    <fieldset>\
      <legend>');  ngx.print( T"System NAT Rules" ); ngx.print('</legend>\
      ');  
      ngx.print(ui_helper.createTable(fw_nat_columns, fw_nat_data, fw_nat_options, nil, fw_nat_helpmsg))
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter());