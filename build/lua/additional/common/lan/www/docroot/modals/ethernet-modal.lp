--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

-- Process POST query
local bit = require("bit")
local proxy = require("datamodel")
local lfs = require("lfs")
local pairs, string ,ipairs, ngx = pairs, string, ipairs, ngx
local session = ngx.ctx.session
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local content_helper = require("web.content_helper")
local uinetwork = require("web.uinetwork_helper")
local hosts_ac = uinetwork.getAutocompleteHostsList()
local tonumber, tostring = tonumber, tostring
local content, helpmsg
local match, format = string.match, string.format
local find, untaint, gsub = string.find, string.untaint, string.gsub
local ipv42num = post_helper.ipv42num
local broadcast_ip =post_helper.ipv42num("255.255.255.255")
local bridged = require("bridgedmode_helper")

-- Shortcuts to validation helpers to make lines shorter
local aIPV = post_helper.advancedIPValidation
local vB = post_helper.validateBoolean
local vIP4N = post_helper.validateIPv4Netmask
local vQTN = post_helper.validateQTN
local gAV = post_helper.getAndValidation
local gOV = post_helper.getOptionalValidation
local gVIC = post_helper.getValidateInCheckboxgroup
local gVIES = post_helper.getValidateInEnumSelect
local gVNIR = post_helper.getValidateNumberInRange
local sLIPV = post_helper.staticLeaseIPValidation
local vNES = post_helper.validateNonEmptyString
local vSIDN = post_helper.validateStringIsDomainName
local vSIIP = post_helper.validateStringIsIP
local vSIIP4 = post_helper.validateStringIsIPv4
local vSILT = post_helper.validateStringIsLeaseTime
local vSIM = post_helper.validateStringIsMAC

local dhcpDefaultExist

local hosts_mac = {}
for k, v in pairs(hosts_ac) do
  local mac = match(k, "%[(.*)%]")
  hosts_mac[#hosts_mac+1] = {mac, mac}
end

local wanpeerdns = proxy.get("uci.network.interface.@wan.peerdns")
if wanpeerdns and wanpeerdns[1].value ~= "0" then
  wanpeerdns = T("RSP/ISP DNS Servers")
else
  wanpeerdns = T("Custom WAN DNS Servers")
end
local dns_list = {
  { -- IPv4 DNS servers
    {"", T""},
    {"127.0.0.1", wanpeerdns},
    {"8.8.8.8", T"Google (8.8.8.8)"},
    {"8.8.4.4", T"Google (8.8.4.4)"},
    {"1.1.1.1", T"Cloudflare (1.1.1.1)"},
    {"1.0.0.1", T"Cloudflare (1.0.0.1)"},
    {"1.1.1.2", T"Cloudflare (1.1.1.2)"},
    {"1.0.0.2", T"Cloudflare (1.0.0.2)"},
    {"1.1.1.3", T"Cloudflare (1.1.1.3)"},
    {"1.0.0.3", T"Cloudflare (1.0.0.3)"},
    {"208.67.222.222", T"OpenDNS (208.67.222.222)"},
    {"208.67.220.220", T"OpenDNS (208.67.220.220)"},
    {"9.9.9.9", T"Quad9 (9.9.9.9)"},
    {"149.112.112.112", T"Quad9 (149.112.112.112)"},
    {"64.6.64.6", T"Verisign (64.6.64.6)"},
    {"64.6.65.6", T"Verisign (64.6.65.6)"},
    {"8.26.56.26", T"Comodo (8.26.56.26)"},
    {"8.20.247.20", T"Comodo (8.20.247.20)"},
    {"81.218.119.11", T"GreenTeam (81.218.119.11)"},
    {"209.88.198.133", T"GreenTeam (209.88.198.133)"},
    {"195.46.39.39", T"SafeDNS (195.46.39.39)"},
    {"195.46.39.40", T"SafeDNS (195.46.39.40)"},
    {"216.146.35.35", T"Dyn (216.146.35.35)"},
    {"216.146.36.36", T"Dyn (216.146.36.36)"},
    {"198.101.242.72", T"Alternate DNS (198.101.242.72)"},
    {"23.253.163.53", T"Alternate DNS (23.253.163.53)"},
    {"77.88.8.8", T"Yandex.DNS (77.88.8.8)"},
    {"77.88.8.1", T"Yandex.DNS (77.88.8.1)"},
    {"91.239.100.100", T"UncensoredDNS (91.239.100.100)"},
    {"89.233.43.71", T"UncensoredDNS (89.233.43.71)"},
    {"156.154.70.1", T"Neustar (156.154.70.1)"},
    {"156.154.71.1", T"Neustar (156.154.71.1)"},
    {"45.77.165.194", T"Fourth Estate (45.77.165.194)"},
    {"45.32.36.36", T"Fourth Estate (45.32.36.36)"},
    {"185.228.168.9", T"CleanBrowsing (185.228.168.9)"},
    {"185.228.169.9", T"CleanBrowsing (185.228.169.9)"},
    {"54.252.183.4", T"GetFlix (54.252.183.4)"},
    {"54.252.183.5", T"GetFlix (54.252.183.5)"},
  },
  { -- IPv6 DNS servers
    {"", T""},
    {"2001-4860-4860--8888", T"Google (2001:4860:4860::8888)"},
    {"2001-4860-4860--8844", T"Google (2001:4860:4860::8844)"},
    {"2606-4700-4700--1111", T"Cloudflare (2606:4700:4700::1111)"},
    {"2606-4700-4700--1001", T"Cloudflare (2606:4700:4700::1001)"},
    {"2606-4700-4700--1112", T"Cloudflare (2606:4700:4700::1112)"},
    {"2606-4700-4700--1002", T"Cloudflare (2606:4700:4700::1002)"},
    {"2606-4700-4700--1113", T"Cloudflare (2606:4700:4700::1113)"},
    {"2606-4700-4700--1003", T"Cloudflare (2606:4700:4700::1003)"},
    {"2620-119-35--35", T"OpenDNS (2620:119:35::35)"},
    {"2620-119-53--53", T"OpenDNS (2620:119:53::53)"},
    {"2620-74-1b--1-1", T"Verisign (2620:74:1b::1:1)"},
    {"2620-74-1c--2-2", T"Verisign (2620:74:1c::2:2)"},
    {"2a02-6b8--feed-0ff", T"Yandex.DNS (2a02:6b8::feed:0ff)"},
    {"2a02-6b8-0-1--feed-0ff", T"Yandex.DNS (2a02:6b8:0:1::feed:0ff)"},
    {"2001-67c-28a4--", T"UncensoredDNS (2001:67c:28a4::)"},
    {"2a01-3a0-53-53--", T"UncensoredDNS (2a01:3a0:53:53::)"},
    {"2610-a1-1018--1", T"Neustar (2610:a1:1018::1)"},
    {"2610-a1-1019--1", T"Neustar (2610:a1:1019::1)"},
    {"2a0d-2a00-1--2", T"CleanBrowsing (2a0d:2a00:1::2)"},
    {"2a0d-2a00-2--2", T"CleanBrowsing (2a0d:2a00:2::2)"},
  }
}
hosts_mac[#hosts_mac+1] = {"custom",T"custom"}

if lfs.attributes("/www/lua/dhcpreset_helper.lua", "mode") == "file" then
  dhcpDefaultExist = require("dhcpreset_helper")
end

local function num2ipv4(ip)
  local ret = bit.band(ip, 255)
  ip = bit.rshift(ip,8)
  for i=1,3 do
    ret = bit.band(ip,255) .. "." .. ret
    ip = bit.rshift(ip,8)
  end
  return ret
end

local getargs = ngx.req.get_uri_args()
local getintf = getargs.intf

local curintf = "lan"
-- Get all the LAN interfaces
local net_intfs_path = "rpc.network.interface."
local all_intfs = content_helper.convertResultToObject(net_intfs_path .. "@.", proxy.get(net_intfs_path))
local wireless_radio = {}
for i,v in ipairs(proxy.getPN("rpc.wireless.ssid.", true)) do
  local radios = match(v.path, "rpc%.wireless%.ssid%.@([^%.]+)%.")
  if radios then
    wireless_radio[radios] = true
  end
end
local lan_intfs = {}
local pppIntf, pppDev
for k,v in ipairs(all_intfs) do
  if v.type == "lan" and (not find(v["ppp.ll_intf"], "wl") or wireless_radio[untaint(v["ppp.ll_intf"])])then
    if v.name and v.name ~= "" then
      lan_intfs[#lan_intfs + 1] = {name = v.name, index = v.paramindex}
    else
      lan_intfs[#lan_intfs + 1] = {name = v.paramindex, index = v.paramindex}
    end
  end
  if v.paramindex == getintf then
    curintf = v.paramindex
    pppDev = v["ppp.ll_dev"]
    pppIntf = v["ppp.ll_intf"]
  end
end

local function isQtnGuestWiFi(intf)
  if wireless_radio[untaint(intf)] then
     local radio = proxy.get(format("rpc.wireless.ssid.@%s.radio", intf))
     local isRemote = proxy.get(format("rpc.wireless.radio.@%s.remotely_managed", radio[1].value))
     if isRemote and isRemote[1].value == "1" then
        for _, v in ipairs(proxy.getPN("rpc.wireless.ap.", true)) do
          local isGuest = proxy.get(v.path.."ap_isolation")
          if isGuest and isGuest[1].value == "1" then
            return true
          end
        end
     end
  end
end

local IP6assign = "uci.network.interface.@" .. curintf .. ".ip6assign"

local mapParam = {
  localIPv6 = "uci.network.interface.@" .. curintf .. ".ipv6"
}

local function validateIPv6(value, object, key)
  local valid, msg = vB(value, object, key)
  if valid then
    if value == "0" then
      -- In case we disable IPv6, we must first invalidate the existing prefix so that local devices know not to use IPv6 anymore
      -- Do this here by set the ip6assign pref and only on ipv6 state change
      local ipv6 = proxy.get(mapParam["localIPv6"]) -- get current value in datamodel to know if we're switching state
      if ipv6 and untaint(ipv6[1].value) ~= "0" then -- default is enabled so anything non 0 is enabled
        proxy.set(IP6assign, "0") -- the value will be set back to its current value by process_query
        proxy.apply()
        ngx.sleep(3) -- ugly but need to give it the time to complete
    end
    else
      proxy.set(IP6assign, "64")
      proxy.apply()
    end
    if isQtnGuestWiFi(pppDev) then
      local ucipath = content_helper.getMatchedContent("uci.network.device.",{ifname = pppIntf})
      if ucipath and #ucipath > 0 then
        proxy.set(ucipath[1].path .. "ipv6", value)
      end
    end
  end
  return valid, msg
end
--We need to update IPv6 state only when user want to update the IPv6 state alone.
if ngx.var.request_method == "POST" and ngx.req.get_post_args().iPv6StateOnlyChanged == "yes" then
  local validation = {
    localIPv6 = validateIPv6,
  }
  post_helper.handleQuery(mapParam, validation)
  ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.exit(ngx.HTTP_OK)
end
-- Static Leases
local sleases_columns = {
  {
    header = T"MAC Address",
    name = "sleases_mac",
    param = "mac",
    type = "select",
    values = hosts_mac,
    attr = { input = { class="span2", maxlength="17", id= "lease_mac" }, autocomplete=hosts_mac },
    unique = true,
  },
  {
    header = T"IP",
    name = "sleases_ip",
    param = "ip",
    type = "text",
    attr = { input = { class="span2", maxlength="15"}, autocomplete=hosts_ac },
    unique = true,
  },
  {
    header = T"Hostname" ,
    name = "sleases_name",
    param = "name",
    type = "text",
    attr = { input = { class="span2" } },
  },
  {
    header = "",
    name = "sleases_owner",
    param = "owner",
    type = "hidden",
    readonly = true,
    attr = { },
  },
}

-- Function to avoid users to enter ReservedStatic name as custom static lease name
local function sleases_name_validation(value)
  if (value:find("^ReservedStatic") ~= nil) then
    return nil, T"Cannot use reserved names as static lease name"
  end
  return true
end

local function sleases_mac_validation(value, object, key)
  local r1, r2 = vSIM(value)
  if r1 then
    if string.lower(value) == "ff:ff:ff:ff:ff:ff" then
      return nil, T"The requested MAC Address can't be the broadcast MAC"
    else
      value = value:match("^%x%x%-%x%x%-%x%x%-%x%x%-%x%x%-%x%x$") and value:gsub("-",":") or value
      object[key] = string.lower(value)
    end
  end
  return r1, r2
end

-- Function to check the input IP is in Reserved IP list
local function reserved_ip_validation(value)
    local reservedList = {}
    local reservedIPList = proxy.get("uci.dhcp.host.")
    reservedIPList = content_helper.convertResultToObject("uci.dhcp.host.", reservedIPList)
    if reservedIPList then
      for _,v in pairs(reservedIPList) do
        if v["name"]:find("^ReservedStatic") ~= nil then
          reservedList[#reservedList + 1] = v["ip"]
          if value == v["ip"] then
            return nil
          end
        end
      end
    end
    return true
end

local function sleases_ip_validation(value, object, key)
  local contentdata = {
    localdevIP = "uci.network.interface.@" .. curintf .. ".ipaddr",
    localdevmask = "uci.network.interface.@" .. curintf .. ".netmask",
  }
  content_helper.getExactContent(contentdata)
  return sLIPV(value, contentdata)
end

local sleases_valid = {
  sleases_name = gAV(sleases_name_validation,vSIDN),
  sleases_mac = gAV(sleases_mac_validation,vQTN),
  sleases_ip = gAV(sleases_ip_validation,vQTN),
}

local sleases_options = {
  tableid = "sleases",
  basepath = "uci.dhcp.host.@.",
  createMsg = T"Add new static lease",
  objectName  = post_helper.getRandomKey(),
  addNamedObject = true,
}


local syshosts_data = proxy.get("sys.hosts.host.")
local sleases_filter = function(data)
  if data['owner'] == "portforward" then
    local path
    if syshosts_data then
      for _, v in pairs(syshosts_data) do
        if v.value == data['mac'] then
          path = v.path
          break
        end
      end
      for _, v in pairs(syshosts_data) do
        if v.path == path and v.param == "HostName" then
          data['name'] = v.value
          break
        end
      end
    end
    return { canEdit = false, canDelete = false }
  end
  if (data['name']:find("^ReservedStatic") == 1) then
    return false
  end
  return true
end

local sleases_data, sleases_helpmsg = post_helper.handleTableQuery(sleases_columns, sleases_options, sleases_filter, nil, sleases_valid)

local inUseFlag = false
if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args()
  if ( post_data.action == "TABLE-MODIFY" or  post_data.action == "TABLE-EDIT"  or  post_data.action == "TABLE-ADD") then
    local text_mac_index = tonumber(post_data.index)
    local text_mac
    if  post_data.action == "TABLE-ADD" then
      text_mac = post_data["sleases_mac"]
    else
      text_mac = sleases_data[text_mac_index][2]
    end
    local flag = true
    for _,j in ipairs(hosts_mac) do
      if text_mac == j[1] then
        flag = false
        break
      end
    end
    if flag   then
      sleases_columns[2] = {
        header = T"MAC Address",
        name = "sleases_mac",
        param = "mac",
        type = "text",
        attr = { input = { class="span2", maxlength="17", id= "lease_mac", value = text_mac }},
        unique = true,
      }
    end
    if post_data.action == "TABLE-MODIFY" or  post_data.action == "TABLE-ADD" then
      local slease_ip = post_data.sleases_ip
      local arranged_data = content_helper.convertResultToObject("sys.hosts.host.", syshosts_data)
      if arranged_data then
        for _, v in pairs(arranged_data) do
          if v.DhcpLeaseTime ~= "" and v.IPAddress == slease_ip then
            inUseFlag = true
            break
          end
        end
      end
    end
  end
end

local cur_dhcp_intf = "lan"
local dhcp_intfs_path = "uci.dhcp.dhcp."
local all_dhcp_intfs = content_helper.convertResultToObject(dhcp_intfs_path .. "@.", proxy.get(dhcp_intfs_path))

for k,v in ipairs(all_dhcp_intfs) do
    if v.interface == curintf then
        cur_dhcp_intf = v.paramindex
        break
    end
end

local function getDomainNamePath()
  local dnsmidx, dnsmif
  for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.", true)) do
    for _,dnsmif in pairs(proxy.get(dnsmidx.path.."interface.")) do
      if dnsmif.value == "lan" then
        return dnsmidx.path .. "domain", dnsmidx.path .. "local"
      end
    end
  end
end

-- Standard parameters
local mapParams = {
  dhcpStart = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".start",
  dhcpLimit = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".limit",
  dhcpv4State = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".dhcpv4",
  dhcpv6State = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".dhcpv6",
  slaacState = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".ra",
  localdomain = getDomainNamePath(),
  localgw = "uci.network.interface.@" .. curintf .. ".gateway",
  dhcpIgnore = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".ignore",
  leaseTime = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".leasetime",
  localdevIP = "uci.network.interface.@" .. curintf .. ".ipaddr",
  localdevmask = "uci.network.interface.@" .. curintf .. ".netmask",
  localIPv6 = "uci.network.interface.@" .. curintf .. ".ipv6",
}

local dnsIndex = 0
local dhcp = {}
local dhcp_dns = {}
local baseDHCPOptions = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".dhcp_option."
local baseDHCPDNS = "uci.dhcp.dhcp.@" .. cur_dhcp_intf .. ".dns."
local dns_servers = {""}

local function isNonPublicIPRange(ip)
    if ip >= ipv42num("10.0.0.0") and ip <= ipv42num("10.255.255.255") or
       ip >= ipv42num("172.16.0.0") and ip <= ipv42num("172.31.255.255") or
       ip >= ipv42num("192.168.0.0") and ip <= ipv42num("192.168.255.255") then
      return true
    end
      return nil
end

local function find_dns (dns, list)
  for i,v in ipairs(list) do
    if i > 1 and dns == v[1] then
      return true
    end
  end
  return false
end

local function validateDNSv6(value, object, key)
  if value ~= "" and find_dns(value, dns_list[2]) ~= true then
    if mapParams[key] ~= nil then
        proxy.del(mapParams[key]:sub(1,-6))
        mapParams[key] = nil
    end
    return nil, "Invalid DNS server"
  end

  if object["dns_v6_pri"] == "" and object["dns_v6_sec"] ~= "" then
    return nil, "Primary should not be empty"
  end

  if object["dns_v6_pri"] == object["dns_v6_sec"]  and object["dns_v6_pri"] ~= "" then
    return nil, "Primary and secondary DNS servers should not be same"
  end

  if value == "" then
    if mapParams[key] ~= nil then
        proxy.del(mapParams[key]:sub(1,-6))
        mapParams[key] = nil
    end
    return true
  end

  local dns_ip = value:gsub("-", ":")

  if dns_ip == "::1" then   -- Localhost as DNS server
    if object["dns_v6_sec"] == "" then
     -- only localhost
      if mapParams["dns_v6_pri"] ~= nil then
        proxy.del(mapParams["dns_v6_pri"]:sub(1,-6))
        mapParams["dns_v6_pri"] = nil
      end
      if mapParams["dns_v6_sec"] ~= nil then
        proxy.del(mapParams["dns_v6_sec"]:sub(1,-6))
        mapParams["dns_v6_sec"] = nil
      end
      object["dns_v6_pri"] = ""
      return true
    else
      dns_ip = lan_ula
    end
  end

  if object["dns_v6_pri"] ~= nil and object["dns_v6_pri"] ~= "" and mapParams["dns_v6_pri"] == nil then
     local index = proxy.add(baseDHCPDNS)
     if index == "1" then
        mapParams["dns_v6_pri"] = baseDHCPDNS .. "@1.value"
     end
  end

  if object["dns_v6_sec"] ~= nil and object["dns_v6_sec"] ~= "" and mapParams["dns_v6_sec"] == nil then
     local index = proxy.add(baseDHCPDNS)
     if index == "2" then
        mapParams["dns_v6_sec"] = baseDHCPDNS .. "@2.value"
     end
  end

  object[key] = dns_ip

  return true
end

local function validateDNS(value, object, key)
  if cur_dhcp_intf ~= "lan" then
    return true
  end

  local post_data = ngx.req.get_post_args()
  local dns_str = ""
  local old_lan_ip = proxy.get("uci.network.interface.@lan.ipaddr")[1].value

  -- handle IPv4 dns setting
  dns_servers[2] = value
  dns_servers[3] = post_data.ipv4_dns_sec


  if value == "" and post_data.ipv4_dns_sec ~= "" then
    return nil, "Primary should not be empty"
  end

  if value == post_data.ipv4_dns_sec and value ~= "" then
    return nil, "Primary and secondary DNS servers should not be same"
  end

  if value ~= "" then
    if value == old_lan_ip then
      dns_str = object["localdevIP"]
    elseif find_dns(value, dns_list[1]) then
      dns_str = value
    else
      return nil, "Invalid DNS server"
    end
  end

  if post_data.ipv4_dns_sec ~= "" then
    if post_data.ipv4_dns_sec == old_lan_ip then
      dns_str = dns_str .. "," .. object["localdevIP"]
    elseif find_dns(post_data.ipv4_dns_sec, dns_list[1]) then
      dns_str = dns_str .. "," .. post_data.ipv4_dns_sec
    else
      return nil, "Invalid DNS server"
    end
  end

  if dns_str == object["localdevIP"] then
    dns_str = ""    -- only the localhost as the dns server
  end

  if dnsIndex == 0 then
    -- no dhcp_option for DNS
    if dns_str ~= "" then
      local index = proxy.add(baseDHCPOptions)
      mapParams["dnsServer"] = baseDHCPOptions .. "@" .. index .. ".value"
      object[key] = "6," .. dns_str    -- DHCP option for DNS server is option 6
      return true
    else
      -- do nothing
      return true
    end
  else
    if dns_str ~= "" then
      mapParams["dnsServer"] = baseDHCPOptions .. "@" .. dnsIndex .. ".value"
      object[key] = "6," .. dns_str    -- DHCP option for DNS server is option 6
      return true
    else
      proxy.del(baseDHCPOptions.. "@" .. dnsIndex .. ".")
      mapParams["dnsServer"] = nil
      return true
    end
  end

  dns_servers = {""}
  return true
end

local function validateLeaseTime(value, postdata, key)
   if value == '-1' then -- included '-1' as a feasible set value as specified in TR 181
      postdata[key] = "infinite" -- included to ensure uci parameter is set as infinite
      return true
   else
      local success, msg = vSILT(value)
      if success then
         postdata["leaseTime"] = match(untaint(value), "^0*([1-9]%d*[smhdw]?)$")
         return true
      else
         return nil, msg
      end
   end
end

local function calculateDHCPStartAddress (baseip, netmask, start, numips)
    local network = bit.band(baseip, netmask)
    local ipmax = bit.bor(network, bit.bnot(netmask)) - 1
    local ipstart = bit.bor(network, bit.band(start, bit.bnot(netmask)))
    local ipend = ipstart+numips-1
    ipstart = num2ipv4(ipstart)
    if ipend > ipmax then
      ipend = ipmax
    end
    ipend = num2ipv4(ipend)
    network = num2ipv4(network)
  return ipstart,ipend,network
end

local function getDHCPData(object)
  -- Check the entered IP is valid IP and convert it to number
  local baseip = vSIIP(object["localdevIP"]) and ipv42num(object["localdevIP"])
  local netmask = vSIIP(object["localdevmask"]) and ipv42num(object["localdevmask"])
  local dhcpstart = vSIIP(object["dhcpStartAddress"]) and ipv42num(object["dhcpStartAddress"])
  local dhcpend = vSIIP(object["dhcpEndAddress"]) and ipv42num(object["dhcpEndAddress"])

  return baseip, netmask, dhcpstart, dhcpend
end

-- Validation is done for the dhcpLimit for the particular subnet
-- If different subnet mask is given other than 255.255.255.0, then the
-- DHCP Range limit has to be calculated from the new subnet and the validation
-- has to be done for the new limit.
local function DHCPValidationNotRequired()
  local post_data = ngx.req.get_post_args()
  local localdevIP = proxy.get("uci.network.interface.@" .. curintf .. ".ipaddr")
  local dhcpIgnore = proxy.get(mapParams["dhcpIgnore"])
  if (localdevIP and localdevIP[1].value ~= post_data["localdevIP"]) or (dhcpIgnore and dhcpIgnore[1].value == "1") then
    return true
  end
  return false
end
local function validateLimit(value, object)
    if DHCPValidationNotRequired() then
      return true
    end
  if match(value, "^[0-9]*$") then
    local baseip, netmask, dhcpstart, dhcpend = getDHCPData(object)

    if not dhcpend then
       return nil, T"DHCP End Address is Invalid"
    end

    if dhcpstart and dhcpstart > dhcpend then
       return nil, T"DHCP Start Address should not be greater than End Address"
    end

    if baseip and netmask and dhcpstart then
      local network = bit.band(baseip, netmask)
      local ipmax = bit.bor(network, bit.bnot(netmask))
      local numips = dhcpend - dhcpstart + 1
      local limit = ipmax - network - 1

      if dhcpend == ipmax then
          return nil, T"Broadcast Address should not be used"
      end

      local validatorNumberInRange = gVNIR(1,limit)
      local limitvalue =  validatorNumberInRange(numips)
      if not limitvalue or dhcpend <= network or dhcpend >= ipmax then
          return nil, T"DHCP End Address is not valid in Subnet Range"
      end
      return true
    else
      return nil
    end
  else
    return nil, T"DHCP End Address is Invalid"
  end
end

-- Validation is done for the DHCP start Address for the particular subnet
-- For different subnets, validation for dhcpStart Address has to be done
-- from the new DHCP Range with respect to the subnet mask & Network Address
local function validateDHCPStart(value, object)
    if DHCPValidationNotRequired() then
      return true
    end
    if match(value, "^[0-9]*$") then
      local baseip, netmask, dhcpstart, dhcpend = getDHCPData(object)

      if not dhcpstart then
         return nil, T"DHCP Start Address is Invalid"
      end

      if baseip and netmask and dhcpend then
        local network = bit.band(baseip, netmask)
        local ipmax = bit.bor(network, bit.bnot(netmask))
        local start = dhcpstart - network
        local numips = dhcpend - dhcpstart + 1

        local limit = ipmax - network - 1

        local validatorNumberInRange = gVNIR(1,limit)

        if dhcpstart == baseip then
           return nil, T"DHCP Start Address should not be Local Device IP Address"
        elseif dhcpstart == network then
           return nil, T"DHCP Start Address should not be a Network Address"
        end

        local val = validatorNumberInRange(start)
        if not val or dhcpstart <= network or dhcpstart >= ipmax then
            return nil, T"DHCP Start Address is not valid in Subnet Range"
        end

        -- Setting the dhcpStart and dhcpLimit from the calculated DHCP Range
        object["dhcpStart"] = tostring(start)
        object["dhcpLimit"] = tostring(numips)

        return true
      else
        return nil
      end
    else
      return nil, T"DHCP Start Address is Invalid"
    end
end

-- This function will validate the Modem IP Address and check for
-- Valid IP Format, Limited Broadcast Address, Public IP Range, Multicast Address Range
local function validateGWIP(value, object, key)
  local val, errmsg = aIPV(value, object, key)
  if not val then
    return nil, errmsg
  end

--   local isWan, intf = post_helper.isWANIP(value, all_intfs)
--   if isWan then
--     return nil, T(symbolv1 .. " IP should not be in ".. intf .." IP Range")
--   end

--   local isLan, intf = post_helper.isLANIP(value, all_intfs, curintf)
--   if isLan then
--     return nil, T(symbolv1 .. " IP should not be in ".. intf .." IP Range")
--   end
  local ip = ipv42num(value)

  for _,intf in pairs(lan_intfs) do
    if intf.index ~= curintf then
      local ipaddr = proxy.get("uci.network.interface.@" .. intf.index .. ".ipaddr")[1].value
      local mask = proxy.get("uci.network.interface.@" .. intf.index .. ".netmask")[1].value
      local baseip = vSIIP(ipaddr) and ipv42num(ipaddr)
      local netmask = vSIIP(mask) and ipv42num(mask)

      local network, ipmax
      if baseip and netmask then
        network = bit.band(baseip, netmask)
        ipmax = bit.bor(network, bit.bnot(netmask))
      end

      if network and ipmax then
        if ip >= network and ip <= ipmax then
              return nil, T"Gateway IP should not be in " .. intf.name .. T" IP Range"
        end
      end
    end
  end

  if isNonPublicIPRange(ip) then
    return true
  else
    return nil, T"Public IP Range should not be used"
  end
end

local function isChecked(key, checked)
  for _,v in ipairs(checked) do
    if v == key then
      return true
    end
  end
  return false
end

local function validatedhcpIgnore(value, object, key)
  local valid, msg = gOV(value, object, key)
  if not valid then
    return nil,  msg
  end
  if object["dhcpv4State"] == "server" then
    if object["dhcpIgnore"] == "1" then
      object["dhcpIgnore"] = "0"
    end
  end
  return true
end

local dhcpStateselect = {
  {"server"},
  {"disabled"},
}

local mapValid = {
  localdevIP = validateGWIP,
  localIPv6 = validateIPv6,
  dnsServer = validateDNS,
  leaseTime = validateLeaseTime,
  localdevmask = gAV(vIP4N,vSIIP),
  dhcpStart = validateDHCPStart,
  dhcpLimit = validateLimit,
  dhcpv4State = gVIES(dhcpStateselect),
  dhcpv6State = gVIES(dhcpStateselect),
  slaacState = gVIES(dhcpStateselect),
  localdomain = vNES,
  localgw = gOV(vSIIP4),
  dhcpIgnore = validatedhcpIgnore,
  dns_v6_pri = validateDNSv6,
  dns_v6_sec = validateDNSv6,
}

local function find_dns (dns, list)
  for i,v in ipairs(list) do
    if i > 1 and dns == v[1] then
      return true
    end
  end
  return false
end

local function split(x,sep)
    x = tostring(x)
    local sep, fields = sep or ":", {}
    local pattern = string.format("([^%s]+)", sep)
    x:gsub(pattern, function(c) fields[#fields+1] = c end)
    return fields
end

local function validateByPass(value, object, key)
    return true
end
if bridged.isBridgedMode() then
    mapValid.dhcpStart = validateByPass
    mapValid.dhcpLimit = validateByPass
    mapValid.dhcpv4State = validateByPass
end

function get_lan_ula()
  local ula = proxy.get("rpc.network.interface.@lan.ipv6uniquelocaladdr")
  if ula == nil or ula[1].value == nil then
    return nil
  end

  return split(format("%s",ula[1].value), "&")[1]
end

-- Check if there is a custom DNS server, if so, then retrieve its index
-- we'll update the parameters list to include it for retrieval
content_helper.addListContent(dhcp, { options = baseDHCPOptions } )
for i,v in ipairs(dhcp.options) do
  if v:find("^6,") == 1 then
    dnsIndex = i
    mapParams["dnsServer"] = baseDHCPOptions .. "@" .. dnsIndex .. ".value"
  end
end

content_helper.addListContent(dhcp_dns, { dnses = baseDHCPDNS } )
if dhcp_dns.dnses[1] ~= nil then
  mapParams["dns_v6_pri"] = baseDHCPDNS .. "@" .. "1.value"
end

if dhcp_dns.dnses[2] ~= nil then
  mapParams["dns_v6_sec"] = baseDHCPDNS .. "@" .. "2.value"
end

lan_ula = get_lan_ula()

content, helpmsg = post_helper.handleQuery(mapParams, mapValid)
if (ngx.var.request_method == "POST")  and not next(helpmsg) then
  local dhcp_options_path = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcp_option."
  local dhcp_options = content_helper.convertResultToObject(dhcp_options_path,proxy.get(dhcp_options_path))
  for _,v in ipairs(dhcp_options) do
    if v.value:match("^3,") then
      proxy.del(dhcp_options_path..v.paramindex..".")
    end
  end
end

if #dns_servers == 1 and content["dnsServer"] ~= nil and content["dnsServer"] ~= "" then
    dns_servers = split(format("%s", content["dnsServer"]), ",")
end

-- set the local host IP
dns_list[1][2][1] = content["localdevIP"]

content["dnsServer"] = content["localdevIP"]
content["dns_v4_sec"] = ""
if dns_servers[2] ~= nil and find_dns(dns_servers[2], dns_list[1]) then
  content["dnsServer"] = dns_servers[2]
end
if dns_servers[3] ~= nil and find_dns(dns_servers[3], dns_list[1]) then
  content["dns_v4_sec"] = dns_servers[3]
end

if content["dns_v6_pri"] ~= nil and content["dns_v6_pri"] ~= "" then
  if content["dns_v6_pri"] == lan_ula then
    content["dns_v6_pri"] = "::1"
  end
  content["dns_v6_pri"] = content["dns_v6_pri"]:gsub(":", "-")
else
  content["dns_v6_pri"] = "--1"
end
if content["dns_v6_sec"] ~= nil then
  if content["dns_v6_sec"] == lan_ula then
    content["dns_v6_sec"] = "::1"
  end
  content["dns_v6_sec"] = content["dns_v6_sec"]:gsub(":", "-")
end

if content["dns_v6_pri"] ~= nil and content["dns_v6_pri"] ~= "" and content["dns_v6_pri"] ~= "--1" and not find_dns(content["dns_v6_pri"], dns_list[2]) then
  local customipv6dns = content["dns_v6_pri"]:gsub("-", ":")
  dns_list[2][#dns_list[2]+1]={ content["dns_v6_pri"], T("Custom ("..customipv6dns..")")}
end
-- because we do the localdevmask validation in localdevIP validation, we check if local ip starts with error msg "[netmask]".
-- If so move it to localdevmask
if helpmsg["localdevIP"] then
  local msg = match(helpmsg["localdevIP"], '^%[netmask%] (.*)')
  if msg then
    helpmsg["localdevIP"] = nil
    helpmsg["localdevmask"] = msg
  end
end
if helpmsg["dhcpLimit"] then
    local msg = match(helpmsg["dhcpLimit"], '^%[dhcpStart%] (.*)')
    if msg then
        helpmsg["dhcpLimit"] = nil
        helpmsg["dhcpStart"] = msg
    end
end

if content["dhcpv4State"] == "" then
    content["dhcpv4State"] = "server"
end
if content["localIPv6"] == "" then
    content["localIPv6"] = "1"
end
if not match(content["leaseTime"],"^0*([1-9]%d*[smhdw])$") then
  content["leaseTime"] = content["leaseTime"] .. "s"
end
local netmask = ipv42num(content["localdevmask"])
local baseip = ipv42num(content["localdevIP"])
local start = tonumber(content["dhcpStart"])
local numips = tonumber(content["dhcpLimit"])
local ipstart,ipend,network
if start and numips and not helpmsg["localdevIP"] and not helpmsg["localdevmask"] and not helpmsg["dhcpStart"] and not helpmsg["dhcpLimit"] then
  ipstart,ipend,network = calculateDHCPStartAddress(baseip,netmask,start,numips)
  if ipstart then
    session:store("dhcpStartAddr",ipstart)
    session:store("dhcpEndAddr",ipend)
    session:store("networkAddr",network)
  end
else
  ipstart = ngx.req.get_post_args().dhcpStartAddress
  ipend = ngx.req.get_post_args().dhcpEndAddress
  network = session:retrieve("networkAddr")
 end
if helpmsg["leaseTime"] then
  content["leaseTime"] = ngx.req.get_post_args().leaseTime
end

local function resetreboot(path, value)
  proxy.set(path, value)
  ngx.header.content_type = "application/json"
  ngx.print('{ "success":"true" }')
  ngx.timer.at(0, function()
    proxy.apply()
  end)
  ngx.exit(ngx.HTTP_OK)
end

if ngx.var.request_method == "POST" then
  local gwIP = session:retrieve("old_ip")
  session:store("old_ip",(content["localdevIP"]))

  -- Redirect the GUI only if the local GW device IP address is changed for the "lan" interface
  if curintf == "lan" and content["localdevIP"]~= gwIP and not next(helpmsg) then ngx.print('\
    <script type="text/javascript">\
      window.location="http://');  ngx.print(content["localdevIP"]); ngx.print('";\
    </script>\
    ');  session:logout()
  end

  local content1 = ngx.req.get_post_args()
  local action = content1["action"]
  if action == "BRIDGED" then
    if not bridged.isBridgedMode() then
      bridged.configBridgedMode()
      return resetreboot("rpc.system.reboot", "GUI")
    end
  end

  if curintf == "lan" then
    local _,local_path = getDomainNamePath()
    local localValue = untaint(proxy.get(local_path)[1].value)
    local localName = format("/%s/", content["localdomain"])
    local domainName = session:retrieve("old_domain")
    session:store("old_domain",(content["localdomain"]))

    if (content["localdomain"] ~= domainName or localValue ~= localName) and not next(helpmsg) then
      proxy.set(local_path, localName)
      proxy.apply()
    end
  end
else
  session:store("old_ip",(content["localdevIP"]))
  if curintf == "lan" then
    session:store("old_domain",(content["localdomain"]))
  end
end

local adguard = false
if content["dhcpv4State"] == "disabled" and content["dhcpv6State"] == "disabled" and content["slaacState"] == "disabled" then
  adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")
  if adguard and adguard[1].value == "1" then
    adguard = true
  end
end

local uri = ngx.var.uri

ngx.print(ui_helper.createHeader(T"Local Network", true, false, nil, help_link))   ngx.print('\
   <div class="modal-body update">\
     <form class="form-horizontal" method="post" action="');  ngx.print( ngx.var.request_uri ); ngx.print('">\
     ');
     -- Do not display the navlist on the list if only 1 lan interface
     if #lan_intfs> 1 and (not bridged.isBridgedMode()) then
       ngx.print('\
     <div class="span2">\
        <ul class="nav nav-list">\
        ');
        local html = { format('<li class="nav-header">%s</li>', T"LAN Interfaces") }
        for k,v in ipairs(lan_intfs) do
            local active = ""
            if v.index == curintf then
                active = "active"
            end
            local url = "modals/ethernet-modal.lp?intf=" .. v.index
            html[#html+1] = format('<li class="%s"><a id = "%s" href="#" data-remote="%s">%s</a></li>', active, v.index, url, v.name)
        end
        ngx.print(html)
        ngx.print('\
        </ul>\
     </div>\
     <div class="span9">\
        ');
      end
      ngx.print(ui_helper.createMessages(message_helper.popMessages()))
      ngx.print('\
        <fieldset>\
          ');  if not bridged.isBridgedMode() then   ngx.print('\
          <div class="span4">\
          ');  end   ngx.print('\
            <legend>');  ngx.print( T"Global Information" ); ngx.print('</legend>\
            ');
            local advanced = {
              group = {
                class = "advanced hide"
              },
              input = {
                class = "span2"
              }
            }
            local switch_class_enable = {
              input = {
                class = "monitor-changes",
              }
            }
            local number_attr = {
              group = {
                class = "monitor-localIPv6 monitor-1 monitor-hidden-localIPv6",
              },
              input = {
                type = "number",
                min = "0",
                max = "128",
                style = "width:100px",
              }
            }
            local stdattributes = {
              group = {
                class = "advanced hide",
              },
              select = {
                class = "monitor-changes span2"
              }
            }
            -- Labels that shows the Local Device IP address & network subnet
            ngx.print(
              ui_helper.createInputText(T"Local Device IP Address", "localdevIP", content["localdevIP"], {input = {class = "span2"}}, helpmsg["localdevIP"]),
              ui_helper.createInputText(T"Local Network Subnet", "localdevmask", content["localdevmask"], advanced, helpmsg["localdevmask"])
            )

            if curintf == "lan" then
              if bridged.isBridgedMode() then
                ngx.print(ui_helper.createInputText(T"Local Gateway", "localgw", content["localgw"], advanced, helpmsg["localgw"]))
              end
              ngx.print(ui_helper.createInputText(T"Local Domain Name", "localdomain", content["localdomain"], advanced, helpmsg["localdomain"]))
            end

            if cur_dhcp_intf == "lan" and not bridged.isBridgedMode() and not adguard then
              ngx.print(
                ui_helper.createInputSelect(T"IPv4 Primary DNS", "dnsServer", dns_list[1], content["dnsServer"], stdattributes),
                ui_helper.createInputSelect(T"IPv4 Secondary DNS", "ipv4_dns_sec", dns_list[1], content["dns_v4_sec"], stdattributes)
              )
            end

            ngx.print(ui_helper.createSwitch(T"IPv6 State", "localIPv6", content["localIPv6"], switch_class_enable))

            if cur_dhcp_intf == "lan" and not bridged.isBridgedMode() and not adguard then
              ngx.print(
                ui_helper.createInputSelect(T"IPv6 Primary DNS", "dns_v6_pri", dns_list[2], content["dns_v6_pri"], stdattributes),
                ui_helper.createInputSelect(T"IPv6 Secondary DNS", "dns_v6_sec", dns_list[2], content["dns_v6_sec"], stdattributes)
              )
            end

            if not bridged.isBridgedMode() then
                ngx.print('\
            </div>\
            <div class="span4">\
              <legend>');  ngx.print( T"DHCP Settings" ); ngx.print('</legend>\
              ');
              if adguard then
                ngx.print(format(T'Configure DHCP through <a style="color:inherit;text-decoration:none;" target="_blank" href="http://%s:8008"><b>AdGuard Home</b></a>', content["localdevIP"]))
              else
                -- Labels that shows DHCP info
                local switchDHCP = {
                  values = {
                    on = "server",
                    off = "disabled"
                  }
                }
                ngx.print(
                  ui_helper.createSwitch(T"DHCPv4 Server", "dhcpv4State", content["dhcpv4State"],switchDHCP),
                  ui_helper.createLabel(T"Network Address", network, advanced),
                  ui_helper.createInputText(T"DHCP Start Address", "dhcpStartAddress", ipstart, advanced, helpmsg["dhcpStart"]),
                  ui_helper.createInputText(T"DHCP End Address", "dhcpEndAddress", ipend, advanced, helpmsg["dhcpLimit"]),
                  ui_helper.createInputText(T"Lease Time", "leaseTime", content["leaseTime"], advanced, helpmsg["leaseTime"]),
                  [[<div class="monitor-localIPv6 monitor-1 monitor-hidden-localIPv6">]],
                  ui_helper.createSwitch(T"DHCPv6 Server", "dhcpv6State", content["dhcpv6State"], switchDHCP),
                  ui_helper.createSwitch(T"SLAAC + RA<span class='icon-question-sign' title='IPv6 Stateless Address Auto-Configuration + Router Advertisement'></span>", "slaacState", content["slaacState"], switchDHCP),
                  [[</div>]]
                );
              end
                 ngx.print('\
            </div>\
              ');
              -- Do not display the static lease when the interface is not lan
              if curintf == "lan" then
                if not adguard then
                ngx.print('\
              </fieldset>\
              <fieldset>\
                  <legend>');  ngx.print( T"Static leases" ); ngx.print('</legend>\
                  ');
                    local confilictipchange_alert = {
                      alert = {
                          class = "alert hide",
                          id = "confilictipchange-msg"
                      }
                    }
                    ngx.print(
                      ui_helper.createAlertBlock(T"The latest added/modified static lease is set to an IP that is already in use, please perform a reboot to apply the changes", confilictipchange_alert),
                      ui_helper.createTable(sleases_columns, sleases_data, sleases_options, nil, sleases_helpmsg))
                    ngx.print('\
              </fieldset>\
                ');
              end
              ngx.print('\
              <fieldset>\
                <legend>');  ngx.print( T"Network mode" ); ngx.print('</legend>\
                ');
                local html = {}
                local rebooting = {
                  alert = {
                    class = "alert hide",
                    id = "rebooting-msg"
                  }
                }
                local confirming = {
                  alert = {
                    class = "alert hide",
                    id = "confirming-msg"
                  }
                }
                local bridged_button = {
                    button = {
                        id = "btn-bridged"
                    }
                }
                html[#html + 1] = ui_helper.createButton("Bridged Mode", "Bridged Mode", "icon-cog", bridged_button)
                html[#html + 1] = string.format('<div class="control-group controls">')
                html[#html + 1] = ui_helper.createAlertBlock(T"Switching to <strong>Bridged Mode</strong> and restarting, please wait...", rebooting)
                html[#html + 1] = ui_helper.createAlertBlock(T"Are you sure you want to switch to <strong>Bridged Mode</strong>?", confirming)
                html[#html + 1] = string.format([[
                  <div id="bridged-changes" class="hide">
                    <div id="bridged-confirm" class="btn btn-primary" data-dismiss="modal">%s</div>
                    <div id="bridged-cancel" class="btn">%s</div>
                  </div>
                </div>
                ]], T"Confirm", T"Cancel")
                ngx.print(html)
              end
            else
              local html = {}
              local infomessage = {
                alert = {
                  class = "alert",
                }
              }
              local ipv4pattern = "^(((([1]?\d)?\d|2[0-4]\d|25[0-5])\.){3}(([1]?\d)?\d|2[0-4]\d|25[0-5]))$"
              local ipv4DNScolumns = {
                {
                  header = T"IPv4 DNS Server Address",
                  name = "wanDnsParam",
                  param = "value",
                  type = "text",
                  attr = { input = { class="span2", maxlength="15", pattern = ipv4pattern } },
                },
              }
              local ipv4DNSoptions = {
                canEdit = true,
                canAdd = true,
                canDelete = true,
                tableid = "dns4server",
                basepath = "uci.network.interface.@lan.dns.@.",
                createMsg = T"Add New IPv4 DNS Server",
                minEntries = 0,
                maxEntries = 4,
                sorted = function(a, b)
                  return tonumber(a.paramindex) < tonumber(b.paramindex)
                end
              }
              local ipv4DNSvalid = {
                wanDnsParam =  aIPV,
              }
              local ipv4DNSdata, ipv4DNShelpmsg = post_helper.handleTableQuery(ipv4DNScolumns, ipv4DNSoptions, nil, nil, ipv4DNSvalid)
              html[#html + 1] = "<legend>LAN Domain Name Server Configuration</legend>"
              html[#html + 1] = ui_helper.createTable(ipv4DNScolumns, ipv4DNSdata, ipv4DNSoptions, nil, ipv4DNShelpmsg)
              html[#html + 1] = "<legend>Network Mode</legend>"
              html[#html + 1] = ui_helper.createAlertBlock(T[[Note: your gateway is in Bridged Mode. If you want the gateway to run in Routed Mode again, you can revert without factory reset <a href="javascript:tch.loadModal('/modals/broadband-modal.lp')">here</a>]], infomessage)
              ngx.print(html)
            end
            ngx.print('\
          </fieldset>\
          ');
          -- Do not display the navlist on the list if only 1 lan interface
          if #lan_intfs> 1 and (not bridged.isBridgedMode()) then
            ngx.print('\
          </div>\
          ');
          end
            ngx.print('\
          <input type="hidden" name="isAdvanced" id="isAdvanced" value="0">\
        </form>\
   </div>\
<script type="text/javascript">\
$(function() {\
    var iPv6StateOnlyChanged = 0;\
    $("input, select").on("change", function(){\
       if(this.id == "localIPv6" && iPv6StateOnlyChanged == 0)\
         iPv6StateOnlyChanged = 1;\
       else\
         iPv6StateOnlyChanged = 2;\
    });\
    //Override the save button click event to update the IPv6 state alone.\
    $("#save-config").click(function(){\
         if(iPv6StateOnlyChanged == 1){\
          var params = [];\
          params.push({\
            name : "action",\
            value : "SAVE"\
          },\
          {\
            name : "iPv6StateOnlyChanged",\
            value : "yes"\
          },\
          {\
            name : "localIPv6",\
            value : $("#localIPv6").val()\
          }, tch.elementCSRFtoken());\
          var target = $(".modal form").attr("action");\
          tch.showProgress(waitMsg);\
          $.post(target, params, function(response){\
            //The following block of code used to display the success/error message and manage the footer.\
            $(".alert").remove();\
            $("form").prepend(response);\
            $("#modal-changes").attr("style","display:none");\
            $("#modal-no-change").attr("style","display:block");\
            iPv6StateOnlyChanged = 0;\
            tch.removeProgress();\
          });\
          return false;\
         }\
     });\
    $("#save-config").click(function(){\
      if($("#Hide_Advanced_id").is(":visible"))\
        $("#isAdvanced").val("1");\
      else\
        $("#isAdvanced").val("0");\
    });\
});\
$("[name =\'sleases_mac\']").change(function () {\
  if ((this.value) == "custom") {\
    $(this).replaceWith($(\'<input/>\',{\'type\':\'text\', \'name\':\'sleases_mac\'}));\
  }\
});\
');  if dhcpDefaultExist then ngx.print('\
$("#btn-dhcp-reset").click(function(){\
  ');  for k,v in pairs(dhcpDefaultExist) do   ngx.print('\
    $("#');  ngx.print(k); ngx.print('").val("');  ngx.print(v); ngx.print('");\
  ');  end  ngx.print('\
  $("#localIPv6").prev().removeClass("switcherOn").closest(".switch").removeClass("switchOn");\
  $("#dhcpState").prev().addClass("switcherOn").closest(".switch").addClass("switchOn");\
  $("#modal-changes").show();\
  $("#modal-no-change").hide();\
});\
');  end  ngx.print('\
  ');  if inUseFlag == true then  ngx.print('\
  $("#confilictipchange-msg").show();\
  ');  end   ngx.print('\
</script>\
');  ngx.print(ui_helper.createFooter())   ngx.print('\
<script src="/js/srp-min.js"></script>\
<script>\
(function() {\
  $(\'input[name="localdevIP"]\').keydown(function(){\
    var msg = $("#lanipchange-msg");\
    var msg_dst = $(this);\
    msg_dst.after(msg);\
    msg.removeClass("hide");\
  });\
  var refreshTimeOut = 5000;\
  var refreshDelay = 3000;\
  var target = $(".modal form").attr("action");\
  function wait_for_webserver_running() {\
    $.ajax({ url: "/", timeout: refreshTimeOut })\
    .done(function(data) {\
      document.open("text/html");\
      document.write(data);\
      document.close();\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running, refreshDelay);\
    });\
  }\
  function wait_for_webserver_down() {\
    $.ajax({ url: target, timeout: refreshTimeOut })\
    .done(function() {\
      window.setTimeout(wait_for_webserver_down, refreshDelay);\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running, refreshDelay);\
    });\
  }\
  function resetreboot(msg, msg_dst, action) {\
    msg_dst.after(msg);\
    msg.removeClass("hide");\
    msg[0].scrollIntoView();\
    $.post(\
      target,\
      { action: action, CSRFtoken: $("meta[name=CSRFtoken]").attr("content") },\
      wait_for_webserver_down,\
      "json"\
    );\
    return false;\
  }\
  $("#btn-bridged").click(function() {\
    $("#confirming-msg").removeClass("hide");\
    $("#bridged-changes").removeClass("hide");\
    $(".modal-body").animate({"scrollTop":"+=100px"}, "fast")\
  });\
  $("#bridged-confirm").click(function() {\
    $("#confirming-msg").addClass("hide");\
    $("#bridged-changes").addClass("hide");\
    $("#btn-bridged").addClass("hide");\
    return resetreboot($("#rebooting-msg"), $("#btn-bridged"), "BRIDGED");\
  });\
  $("#bridged-cancel").click(function() {\
    $("#confirming-msg").addClass("hide");\
    $("#bridged-changes").addClass("hide");\
    $("#rebooting-msg").addClass("hide");\
  });\
}());\
</script>\
');