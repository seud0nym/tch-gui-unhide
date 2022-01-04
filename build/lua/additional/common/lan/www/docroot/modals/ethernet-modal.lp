--pretranslated: do not change this file

-- NOTE: All occurences of the string "Device" in this file will be replaced with the variant (e.g. DJA0231)

-- Localization
gettext.textdomain('webui-core')

-- Process POST query
local bit = require("bit")
local proxy = require("datamodel")
local lfs = require("lfs")
local pairs,string ,ipairs,ngx = pairs,string,ipairs,ngx
local session = ngx.ctx.session
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local content_helper = require("web.content_helper")
local uinetwork = require("web.uinetwork_helper")
local hosts_ac = uinetwork.getAutocompleteHostsList()
local tonumber,tostring = tonumber,tostring
local content,helpmsg
local match,format,toupper = string.match,string.format,string.upper
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local find = string.find
local ipv42num = post_helper.ipv42num
local bridged = require("bridgedmode_helper")

-- Shortcuts to validation helpers to make lines shorter
local aIPV = post_helper.advancedIPValidation
local vB = post_helper.validateBoolean
local vIP4N = post_helper.validateIPv4Netmask
local vQTN = post_helper.validateQTN
local gAV = post_helper.getAndValidation
local gOV = post_helper.getOptionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local gVNIR = post_helper.getValidateNumberInRange
local sLIPV = post_helper.staticLeaseIPValidation
local vNES = post_helper.validateNonEmptyString
local vSIDN = post_helper.validateStringIsDomainName
local vSIIP = post_helper.validateStringIsIP
local vSIIP4 = post_helper.validateStringIsIPv4
local vSIIP6 = post_helper.validateStringIsIPv6
local vSILT = post_helper.validateStringIsLeaseTime
local vSIM = post_helper.validateStringIsMAC

local dhcpDefaultExist

local hosts_mac = {}
for k,_ in pairs(hosts_ac) do
  local mac = match(k,"%[(.*)%]")
  hosts_mac[#hosts_mac+1] = {mac,mac}
end
hosts_mac[#hosts_mac+1] = {"custom",T"custom"}

local dns_list = {
  { -- IPv4 DNS servers
    {"",T""},
    {"127.0.0.1",T"Device"},
    {"custom",T"Enter Custom IP Address"},
    {"8.8.8.8",T"Google (8.8.8.8)"},
    {"8.8.4.4",T"Google (8.8.4.4)"},
    {"1.1.1.1",T"Cloudflare (1.1.1.1)"},
    {"1.0.0.1",T"Cloudflare (1.0.0.1)"},
    {"1.1.1.2",T"Cloudflare (1.1.1.2)"},
    {"1.0.0.2",T"Cloudflare (1.0.0.2)"},
    {"1.1.1.3",T"Cloudflare (1.1.1.3)"},
    {"1.0.0.3",T"Cloudflare (1.0.0.3)"},
    {"208.67.222.222",T"OpenDNS (208.67.222.222)"},
    {"208.67.220.220",T"OpenDNS (208.67.220.220)"},
    {"208.67.222.123",T"OpenDNS FamilyShield (208.67.222.123)"},
    {"208.67.220.123",T"OpenDNS FamilyShield (208.67.220.123)"},
    {"9.9.9.9",T"Quad9 (9.9.9.9)"},
    {"149.112.112.112",T"Quad9 (149.112.112.112)"},
    {"64.6.64.6",T"Verisign (64.6.64.6)"},
    {"64.6.65.6",T"Verisign (64.6.65.6)"},
    {"8.26.56.26",T"Comodo (8.26.56.26)"},
    {"8.20.247.20",T"Comodo (8.20.247.20)"},
    {"81.218.119.11",T"GreenTeam (81.218.119.11)"},
    {"209.88.198.133",T"GreenTeam (209.88.198.133)"},
    {"195.46.39.39",T"SafeDNS (195.46.39.39)"},
    {"195.46.39.40",T"SafeDNS (195.46.39.40)"},
    {"216.146.35.35",T"Dyn (216.146.35.35)"},
    {"216.146.36.36",T"Dyn (216.146.36.36)"},
    {"198.101.242.72",T"Alternate DNS (198.101.242.72)"},
    {"23.253.163.53",T"Alternate DNS (23.253.163.53)"},
    {"77.88.8.8",T"Yandex.DNS (77.88.8.8)"},
    {"77.88.8.1",T"Yandex.DNS (77.88.8.1)"},
    {"91.239.100.100",T"UncensoredDNS (91.239.100.100)"},
    {"89.233.43.71",T"UncensoredDNS (89.233.43.71)"},
    {"156.154.70.1",T"Neustar (156.154.70.1)"},
    {"156.154.71.1",T"Neustar (156.154.71.1)"},
    {"45.77.165.194",T"Fourth Estate (45.77.165.194)"},
    {"45.32.36.36",T"Fourth Estate (45.32.36.36)"},
    {"185.228.168.9",T"CleanBrowsing (185.228.168.9)"},
    {"185.228.169.9",T"CleanBrowsing (185.228.169.9)"},
    {"54.252.183.4",T"GetFlix (54.252.183.4)"},
    {"54.252.183.5",T"GetFlix (54.252.183.5)"},
  },
  { -- IPv6 DNS servers
    {"",T""},
    {"custom",T"Enter Custom IP Address"},
    {"2001-4860-4860--8888",T"Google (2001:4860:4860::8888)"},
    {"2001-4860-4860--8844",T"Google (2001:4860:4860::8844)"},
    {"2606-4700-4700--1111",T"Cloudflare (2606:4700:4700::1111)"},
    {"2606-4700-4700--1001",T"Cloudflare (2606:4700:4700::1001)"},
    {"2606-4700-4700--1112",T"Cloudflare (2606:4700:4700::1112)"},
    {"2606-4700-4700--1002",T"Cloudflare (2606:4700:4700::1002)"},
    {"2606-4700-4700--1113",T"Cloudflare (2606:4700:4700::1113)"},
    {"2606-4700-4700--1003",T"Cloudflare (2606:4700:4700::1003)"},
    {"2620-119-35--35",T"OpenDNS (2620:119:35::35)"},
    {"2620-119-53--53",T"OpenDNS (2620:119:53::53)"},
    {"2620-74-1b--1-1",T"Verisign (2620:74:1b::1:1)"},
    {"2620-74-1c--2-2",T"Verisign (2620:74:1c::2:2)"},
    {"2a02-6b8--feed-0ff",T"Yandex.DNS (2a02:6b8::feed:0ff)"},
    {"2a02-6b8-0-1--feed-0ff",T"Yandex.DNS (2a02:6b8:0:1::feed:0ff)"},
    {"2001-67c-28a4--",T"UncensoredDNS (2001:67c:28a4::)"},
    {"2a01-3a0-53-53--",T"UncensoredDNS (2a01:3a0:53:53::)"},
    {"2610-a1-1018--1",T"Neustar (2610:a1:1018::1)"},
    {"2610-a1-1019--1",T"Neustar (2610:a1:1019::1)"},
    {"2a0d-2a00-1--2",T"CleanBrowsing (2a0d:2a00:1::2)"},
    {"2a0d-2a00-2--2",T"CleanBrowsing (2a0d:2a00:2::2)"},
  }
}

if lfs.attributes("/www/lua/dhcpreset_helper.lua","mode") == "file" then
  dhcpDefaultExist = require("dhcpreset_helper")
end

local function num2ipv4(ip)
  local ret = bit.band(ip,255)
  ip = bit.rshift(ip,8)
  for _=1,3 do
    ret = bit.band(ip,255).."."..ret
    ip = bit.rshift(ip,8)
  end
  return ret
end

local getargs = ngx.req.get_uri_args()
local getintf = getargs.intf

local curintf = "lan"
-- Get all the LAN interfaces
local net_intfs_path = "rpc.network.interface."
local all_intfs = content_helper.convertResultToObject(net_intfs_path.."@.",proxy.get(net_intfs_path))
local wireless_radio = {}
for _,v in ipairs(proxy.getPN("rpc.wireless.ssid.",true)) do
  local radios = match(v.path,"rpc%.wireless%.ssid%.@([^%.]+)%.")
  if radios then
    wireless_radio[radios] = true
  end
end
local lan_intfs = {}
local pppIntf,pppDev
for _,v in ipairs(all_intfs) do
  if v.type == "lan" and v.name ~= "wg0" and v.proto ~= "wireguard" and (not find(v["ppp.ll_intf"],"wl") or wireless_radio[untaint(v["ppp.ll_intf"])]) then
    if v.name and v.name ~= "" then
      lan_intfs[#lan_intfs + 1] = {name = v.name,index = v.paramindex}
    else
      lan_intfs[#lan_intfs + 1] = {name = v.paramindex,index = v.paramindex}
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
     local radio = proxy.get(format("rpc.wireless.ssid.@%s.radio",intf))
     local isRemote = proxy.get(format("rpc.wireless.radio.@%s.remotely_managed",radio[1].value))
     if isRemote and isRemote[1].value == "1" then
        for _,v in ipairs(proxy.getPN("rpc.wireless.ap.",true)) do
          local isGuest = proxy.get(v.path.."ap_isolation")
          if isGuest and isGuest[1].value == "1" then
            return true
          end
        end
     end
  end
end

local IP6assign = "uci.network.interface.@"..curintf..".ip6assign"

local mapParam = {
  localIPv6 = "uci.network.interface.@"..curintf..".ipv6"
}

local function validateIPv6(value,_,_)
  local valid,msg = vB(value)
  if valid then
    if value == "0" then
      -- In case we disable IPv6, we must first invalidate the existing prefix so that local devices know not to use IPv6 anymore
      -- Do this here by set the ip6assign pref and only on ipv6 state change
      local ipv6 = proxy.get(mapParam["localIPv6"]) -- get current value in datamodel to know if we're switching state
      if ipv6 and untaint(ipv6[1].value) ~= "0" then -- default is enabled so anything non 0 is enabled
        proxy.set(IP6assign,"0") -- the value will be set back to its current value by process_query
        proxy.apply()
        ngx.sleep(3) -- ugly but need to give it the time to complete
    end
    else
      proxy.set(IP6assign,"64")
      proxy.apply()
    end
    if isQtnGuestWiFi(pppDev) then
      local ucipath = content_helper.getMatchedContent("uci.network.device.",{ifname = pppIntf})
      if ucipath and #ucipath > 0 then
        proxy.set(ucipath[1].path.."ipv6",value)
      end
    end
  end
  return valid,msg
end
--We need to update IPv6 state only when user want to update the IPv6 state alone.
if ngx.var.request_method == "POST" and ngx.req.get_post_args().iPv6StateOnlyChanged == "yes" then
  local validation = {
    localIPv6 = validateIPv6,
  }
  post_helper.handleQuery(mapParam,validation)
  ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.exit(ngx.HTTP_OK)
end

local tags = { {"",T""},}
for _,tag in pairs(proxy.getPN("uci.dhcp.tag.",true)) do
  local name = match(tag.path,"uci%.dhcp%.tag%.@([^%.]+)%.")
  tags[#tags+1] = { name,T(name) }
end

-- Static Leases
local sleases_columns = {
  {
    header = T"MAC Address",
    name = "sleases_mac",
    param = "mac",
    type = "select",
    values = hosts_mac,
    attr = { select = { class="span2",maxlength="17",id="lease_mac",style="width:150px" },autocomplete=hosts_mac },
    unique = true,
  },
  {
    header = T"IP Address",
    name = "sleases_ip",
    param = "ip",
    type = "text",
    attr = { input = { class="span2",maxlength="15",style="width:125px" },autocomplete=hosts_ac },
  },
  {
    header = T"Host Name" ,
    name = "sleases_name",
    param = "name",
    type = "text",
    attr = { input = { class="span2" } },
  },
  {
    header = T"Custom DHCP Options Tag",
    name = "sleases_tag",
    param = "tag",
    type = "select",
    attr = { select = { class="span2",style="width:180px" },autocomplete=hosts_ac },
    values = tags,
  },
  {
    header = T"Add to DNS&nbsp;<span class='icon-question-sign' title='Add static forward and reverse DNS entries for this host, so that the host name resolves even when lease is not active.'></span>",
    name = "sleases_dns",
    param = "dns",
    type = "switch",
    default = "0",
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
    return nil,T"Cannot use reserved names as static lease name"
  end
  return true
end

local function sleases_mac_validation(value,object,key)
  local r1,r2 = vSIM(value)
  if r1 then
    if string.lower(value) == "ff:ff:ff:ff:ff:ff" then
      return nil,T"The requested MAC Address can't be the broadcast MAC"
    else
      value = value:match("^%x%x%-%x%x%-%x%x%-%x%x%-%x%x%-%x%x$") and value:gsub("-",":") or value
      object[key] = string.lower(value)
    end
  end
  return r1,r2
end

local function sleases_ip_validation(value,object,_)
  if value == "" then
    return true
  end
  for _,host in pairs(proxy.getPN("uci.dhcp.host.",true)) do
    local existing = proxy.get(host.path.."ip", host.path.."mac")
    if value == existing[1].value and object.mac ~= existing[2].value then
      return nil,"IP already in use for MAC "..untaint(existing[2].value)
    end
  end
  local contentdata = {
    localdevIP = "uci.network.interface.@"..curintf..".ipaddr",
    localdevmask = "uci.network.interface.@"..curintf..".netmask",
  }
  content_helper.getExactContent(contentdata)
  return sLIPV(value,contentdata)
end

local sleases_valid = {
  sleases_name = gAV(sleases_name_validation,vSIDN),
  sleases_mac = gAV(sleases_mac_validation,vQTN),
  sleases_ip = gOV(gAV(sleases_ip_validation,vQTN)),
}

local sleases_options = {
  tableid = "sleases",
  basepath = "uci.dhcp.host.@.",
  createMsg = T"Add Static Lease",
  objectName = post_helper.getRandomKey(),
  addNamedObject = true,
  sorted = function(a,b)
    return toupper(a.name or "") < toupper(b.name or "")
  end,
}

local used_tags = {}
local syshosts_data = proxy.get("sys.hosts.host.")
local sleases_filter = function(data)
  used_tags[untaint(data['tag'])] = true
  if data['owner'] == "portforward" then
    local path
    if syshosts_data then
      for _,v in pairs(syshosts_data) do
        if v.value == data['mac'] then
          path = v.path
          break
        end
      end
      for _,v in pairs(syshosts_data) do
        if v.path == path and v.param == "HostName" then
          data['name'] = v.value
          break
        end
      end
    end
    return { canEdit = false,canDelete = false }
  end
  if (data['name']:find("^ReservedStatic") == 1) then
    return false
  end
  return true
end

local sleases_data,sleases_helpmsg = post_helper.handleTableQuery(sleases_columns,sleases_options,sleases_filter,nil,sleases_valid)

-- Tags
local tags_columns = {
  {
    header = T"Tag",
    name = "tags_name",
    param = "paramindex",
    type = "text",
    attr = { input = { class="span1" } },
    unique = true,
  },
  {
    header = T"IPv4 Primary DNS",
    name = "tags_dns1",
    type = "select",
    values = dns_list[1],
    attr = { input = { class="span2" } },
  },
  {
    header = T"IPv4 Secondary DNS",
    name = "tags_dns2",
    type = "select",
    values = dns_list[1],
    attr = { input = { class="span2" } },
  },
  {
    header = T"",
    name = "dhcp_option",
    param = "dhcp_option",
    type = "hidden",
  },
}

local tags_filter = function(data)
  for k,v in pairs(data) do
    if match(k,"^dhcp_option.@") and match(v,"^6,") then
      data.dhcp_option = v
    end
  end
  if used_tags[untaint(data.paramindex)] then
    return { canDelete = false }
  end
  return true
end

local tags_valid = {
  tags_name = function(value,_,_)
    if not value:match("^[%w]+$") then
      return nil,"must not be empty and must only contain alphanumeric characters"
    end
    return true
  end,
  tags_dns1 = vSIIP4,
  tags_dns2 = gOV(vSIIP4),
}

local tags_options = {
  tableid = "tags",
  basepath = "uci.dhcp.tag.@.",
  createMsg = T"Add Tag",
  addNamedObject = true,
  canEdit = false,
  onAdd = function(index,added)
    local value = ""
    if added.tags_dns1 ~= "" then
      value = "6,"..added.tags_dns1
      if added.tags_dns2 ~= "" then
        value = value..","..added.tags_dns2
      end
    end
    if value ~= "" then
      local key,errmsg = proxy.add("uci.dhcp.tag.@"..index..".dhcp_option.")
      if errmsg then
        message_helper.pushMessage(T(string.format("Failed to add DHCP option to Tag %s: %s",index,errmsg)),"error")
      else
        local success,errors = proxy.set("uci.dhcp.tag.@"..index..".dhcp_option.@"..key..".value",value)
        if success then
          proxy.apply()
        else
          for _,err in ipairs(errors) do
            message_helper.pushMessage(T(string.format("Failed to set %s to '%s': %s (%s)",err.path,value,err.errmsg,err.errcode)),"error")
          end
        end
      end
    end
  end,
  sorted = function(a,b)
    return toupper(a.paramindex or "") < toupper(b.paramindex or "")
  end,
}

local tags_data,tags_helpmsg = post_helper.handleTableQuery(tags_columns,tags_options,tags_filter,nil,tags_valid)
for i,row in pairs(tags_data) do
  tags_data[i][2] = match(row[4],"^6,([^,]+)") or ""
  tags_data[i][3] = match(row[4],"^6,[^,]+,(.+)") or ""
end

local inUseFlag = false
if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args()
  if post_data.tableid == "sleases" then
    if (post_data.action == "TABLE-MODIFY" or post_data.action == "TABLE-EDIT" or post_data.action == "TABLE-ADD") then
      local text_mac_index = tonumber(post_data.index)
      local text_mac
      if post_data.action == "TABLE-ADD" then
        text_mac = post_data["sleases_mac"]
      else
        text_mac = sleases_data[text_mac_index][1]
      end
      local flag = true
      for _,j in ipairs(hosts_mac) do
        if text_mac == j[1] then
          flag = false
          break
        end
      end
      if flag then
        sleases_columns[1] = {
          header = T"MAC Address",
          name = "sleases_mac",
          param = "mac",
          type = "text",
          attr = { input = { class="span2",maxlength="17",style="width:150px",id="lease_mac",value=text_mac }},
          unique = true,
        }
      end
      if post_data.action == "TABLE-MODIFY" or post_data.action == "TABLE-ADD" then
        local slease_ip = post_data.sleases_ip
        if slease_ip ~= "" then
          local arranged_data = content_helper.convertResultToObject("sys.hosts.host.",syshosts_data)
          if arranged_data then
            for _,v in pairs(arranged_data) do
              if v.DhcpLeaseTime ~= "" and v.IPAddress == slease_ip then
                inUseFlag = true
                break
              end
            end
          end
        end
      end
    end
  end
end

local cur_dhcp_intf = "lan"
local dhcp_intfs_path = "uci.dhcp.dhcp."
local all_dhcp_intfs = content_helper.convertResultToObject(dhcp_intfs_path.."@.",proxy.get(dhcp_intfs_path))

for _,v in ipairs(all_dhcp_intfs) do
  if v.interface == curintf then
    cur_dhcp_intf = v.paramindex
    break
  end
end

local function getDomainNamePath()
  for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.",true)) do
    for _,dnsmif in pairs(proxy.get(dnsmidx.path.."interface.")) do
      if dnsmif.value == "lan" then
        return dnsmidx.path
      end
    end
  end
end

-- Standard parameters
local mapParams = {
  dhcpStart = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".start",
  dhcpLimit = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".limit",
  dhcpv4State = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcpv4",
  dhcpv6State = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcpv6",
  slaacState = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".ra",
  localdomain = getDomainNamePath().."domain",
  localgw = "uci.network.interface.@"..curintf..".gateway",
  dhcpIgnore = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".ignore",
  leaseTime = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".leasetime",
  localdevIP = "uci.network.interface.@"..curintf..".ipaddr",
  localdevmask = "uci.network.interface.@"..curintf..".netmask",
  localIPv6 = "uci.network.interface.@"..curintf..".ipv6",
  dhcplog = getDomainNamePath().."logdhcp",
}

local dnsIndex = 0
local ntpIndex = 0
local dhcp = {}
local dhcp_dns = {}
local baseDHCPOptions = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcp_option."
local baseDHCPDNS = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dns."
local dns_servers = {""}

local function isNonPublicIPRange(ip)
  if ip >= ipv42num("10.0.0.0") and ip <= ipv42num("10.255.255.255") or
     ip >= ipv42num("172.16.0.0") and ip <= ipv42num("172.31.255.255") or
     ip >= ipv42num("192.168.0.0") and ip <= ipv42num("192.168.255.255") then
    return true
  end
  return nil
end

local splitter = require("split")
local split = splitter.split


local function get_lan_ula()
  local ula = proxy.get("rpc.network.interface.@lan.ipv6uniquelocaladdr")
  if ula == nil or ula[1].value == nil then
    return nil
  end

  return split(format("%s",ula[1].value),"&")[1]
end

local lan_ula = get_lan_ula()

local function find_dns(ip,list)
  local dns = untaint(ip)
  for i,v in ipairs(list) do
    if i > 1 and dns == v[1] then
      return true
    end
  end
  return false
end

local function validateDNSv6(value,object,key)
  if value ~= "" and vSIIP6(untaint(value):gsub("-",":")) ~= true then
    if mapParams[key] ~= nil then
      proxy.del(mapParams[key]:sub(1,-6))
      mapParams[key] = nil
    end
    return nil,"Invalid DNS server"
  end

  if object["dns_v6_pri"] == "" and object["dns_v6_sec"] ~= "" then
    return nil,"Primary should not be empty"
  end

  if object["dns_v6_pri"] == object["dns_v6_sec"] and object["dns_v6_pri"] ~= "" then
    return nil,"Primary and secondary DNS servers should not be same"
  end

  if value == "" then
    if mapParams[key] ~= nil then
      proxy.del(mapParams[key]:sub(1,-6))
      mapParams[key] = nil
    end
    return true
  end

  local dns_ip = value:gsub("-",":")

  if dns_ip == "::1" then -- Localhost as DNS server
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
      mapParams["dns_v6_pri"] = baseDHCPDNS.."@1.value"
    end
  end

  if object["dns_v6_sec"] ~= nil and object["dns_v6_sec"] ~= "" and mapParams["dns_v6_sec"] == nil then
    local index = proxy.add(baseDHCPDNS)
    if index == "2" then
      mapParams["dns_v6_sec"] = baseDHCPDNS.."@2.value"
    end
  end

  object[key] = dns_ip

  return true
end

local function validateDNS(value,object,key)
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
    return nil,"Primary should not be empty"
  end

  if value == post_data.ipv4_dns_sec and value ~= "" then
    return nil,"Primary and secondary DNS servers should not be same"
  end

  if value ~= "" then
    if value == old_lan_ip then
      dns_str = object["localdevIP"]
    elseif vSIIP4(value) then
      dns_str = value
    else
      return nil,"Invalid DNS server"
    end
  end

  if post_data.ipv4_dns_sec ~= "" then
    if post_data.ipv4_dns_sec == old_lan_ip then
      dns_str = dns_str..","..object["localdevIP"]
    elseif vSIIP4(post_data.ipv4_dns_sec) then
      dns_str = dns_str..","..post_data.ipv4_dns_sec
    else
      return nil,"Invalid DNS server"
    end
  end

  if dns_str == object["localdevIP"] then
    dns_str = "" -- only the localhost as the dns server
  end

  if dnsIndex == 0 then
    -- no dhcp_option for DNS
    if dns_str ~= "" then
      local index = proxy.add(baseDHCPOptions)
      mapParams[key] = baseDHCPOptions.."@"..index..".value"
      object[key] = "6,"..dns_str -- DHCP option for DNS server is option 6
      return true
    else
      -- do nothing
      return true
    end
  else
    if dns_str ~= "" then
      mapParams[key] = baseDHCPOptions.."@"..dnsIndex..".value"
      object[key] = "6,"..dns_str -- DHCP option for DNS server is option 6
      return true
    else
      proxy.del(baseDHCPOptions.."@"..dnsIndex..".")
      mapParams[key] = nil
      return true
    end
  end

  dns_servers = {""}
  return true
end

local function validateNTP(value,object,key)
  if cur_dhcp_intf == "lan" then
    if value == "" or value == "0" then
      if ntpIndex ~= 0 then
        proxy.del(baseDHCPOptions.."@"..ntpIndex..".")
        mapParams[key] = nil
      end
    else
      if ntpIndex == 0 then
        ntpIndex = proxy.add(baseDHCPOptions)
      end
      mapParams[key] = baseDHCPOptions.."@"..ntpIndex..".value"
      object[key] = "42,"..object["localdevIP"] -- DHCP option for NTP server is option 42
    end
  end
  return true
end

local function validateLeaseTime(value,postdata,key)
  if value == '-1' then -- included '-1' as a feasible set value as specified in TR 181
    postdata[key] = "infinite" -- included to ensure uci parameter is set as infinite
    return true
  else
    local isLeaseTime,msg = vSILT(value)
    if isLeaseTime then
      postdata["leaseTime"] = match(untaint(value),"^0*([1-9]%d*[smhdw]?)$")
      return true
    else
      return nil,msg
    end
  end
end

local function calculateDHCPStartAddress (baseip,netmask,start,numips)
  local network = bit.band(baseip,netmask)
  local ipmax = bit.bor(network,bit.bnot(netmask)) - 1
  local ipstart = bit.bor(network,bit.band(start,bit.bnot(netmask)))
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

  return baseip,netmask,dhcpstart,dhcpend
end

-- Validation is done for the dhcpLimit for the particular subnet
-- If different subnet mask is given other than 255.255.255.0,then the
-- DHCP Range limit has to be calculated from the new subnet and the validation
-- has to be done for the new limit.
local function DHCPValidationNotRequired()
  local post_data = ngx.req.get_post_args()
  local localdevIP = proxy.get("uci.network.interface.@"..curintf..".ipaddr")
  local dhcpIgnore = proxy.get(mapParams["dhcpIgnore"])
  if (localdevIP and localdevIP[1].value ~= post_data["localdevIP"]) or (dhcpIgnore and dhcpIgnore[1].value == "1") then
    return true
  end
  return false
end
local function validateLimit(value,object)
    if DHCPValidationNotRequired() then
      return true
    end
  if match(value,"^[0-9]*$") then
    local baseip,netmask,dhcpstart,dhcpend = getDHCPData(object)

    if not dhcpend then
       return nil,T"DHCP End Address is Invalid"
    end

    if dhcpstart and dhcpstart > dhcpend then
       return nil,T"DHCP Start Address should not be greater than End Address"
    end

    if baseip and netmask and dhcpstart then
      local network = bit.band(baseip,netmask)
      local ipmax = bit.bor(network,bit.bnot(netmask))
      local numips = dhcpend - dhcpstart + 1
      local limit = ipmax - network - 1

      if dhcpend == ipmax then
          return nil,T"Broadcast Address should not be used"
      end

      local validatorNumberInRange = gVNIR(1,limit)
      local limitvalue = validatorNumberInRange(numips)
      if not limitvalue or dhcpend <= network or dhcpend >= ipmax then
          return nil,T"DHCP End Address is not valid in Subnet Range"
      end
      return true
    else
      return nil
    end
  else
    return nil,T"DHCP End Address is Invalid"
  end
end

-- Validation is done for the DHCP start Address for the particular subnet
-- For different subnets,validation for dhcpStart Address has to be done
-- from the new DHCP Range with respect to the subnet mask & Network Address
local function validateDHCPStart(value,object)
    if DHCPValidationNotRequired() then
      return true
    end
    if match(value,"^[0-9]*$") then
      local baseip,netmask,dhcpstart,dhcpend = getDHCPData(object)

      if not dhcpstart then
         return nil,T"DHCP Start Address is Invalid"
      end

      if baseip and netmask and dhcpend then
        local network = bit.band(baseip,netmask)
        local ipmax = bit.bor(network,bit.bnot(netmask))
        local start = dhcpstart - network
        local numips = dhcpend - dhcpstart + 1

        local limit = ipmax - network - 1

        local validatorNumberInRange = gVNIR(1,limit)

        if dhcpstart == baseip then
           return nil,T"DHCP Start Address should not be the Device IP Address"
        elseif dhcpstart == network then
           return nil,T"DHCP Start Address should not be a Network Address"
        end

        local val = validatorNumberInRange(start)
        if not val or dhcpstart <= network or dhcpstart >= ipmax then
            return nil,T"DHCP Start Address is not valid in Subnet Range"
        end

        -- Setting the dhcpStart and dhcpLimit from the calculated DHCP Range
        object["dhcpStart"] = tostring(start)
        object["dhcpLimit"] = tostring(numips)

        return true
      else
        return nil
      end
    else
      return nil,T"DHCP Start Address is Invalid"
    end
end

-- This function will validate the Modem IP Address and check for
-- Valid IP Format,Limited Broadcast Address,Public IP Range,Multicast Address Range
local function validateGWIP(value,object,key)
  local val,errmsg = aIPV(value,object,key)
  if not val then
    return nil,errmsg
  end
  if post_helper.isWANIP then
    local isWan,intf = post_helper.isWANIP(value,all_intfs)
    if isWan then
      return nil,T("Device IP should not be in "..intf.." IP Range")
    end
  end
  if post_helper.isLANIP then
    local isLan,intf = post_helper.isLANIP(value,all_intfs,curintf)
    if isLan then
      return nil,T("Device IP should not be in "..intf.." IP Range")
    end
  end
  local ip = ipv42num(value)

  for _,intf in pairs(lan_intfs) do
    if intf.index ~= curintf then
      local ipaddr = proxy.get("uci.network.interface.@"..intf.index..".ipaddr")[1].value
      local mask = proxy.get("uci.network.interface.@"..intf.index..".netmask")[1].value
      local baseip = vSIIP(ipaddr) and ipv42num(ipaddr)
      local netmask = vSIIP(mask) and ipv42num(mask)

      local network,ipmax
      if baseip and netmask then
        network = bit.band(baseip,netmask)
        ipmax = bit.bor(network,bit.bnot(netmask))
      end

      if network and ipmax then
        if ip >= network and ip <= ipmax then
              return nil,T"Device IP should not be in "..intf.name..T" IP Range"
        end
      end
    end
  end

  if isNonPublicIPRange(ip) then
    return true
  else
    return nil,T"Public IP Range should not be used"
  end
end

local function validatedhcpIgnore(value,object,_)
  local valid,msg = gOV(value)
  if not valid then
    return nil,msg
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
  dns_v4_pri = validateDNS,
  ntpServer = validateNTP,
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
  dhcplog = vB,
}

local function validateByPass(_,_,_)
    return true
end
if bridged.isBridgedMode() then
    mapValid.dhcpStart = validateByPass
    mapValid.dhcpLimit = validateByPass
    mapValid.dhcpv4State = validateByPass
end

-- Check if there is a custom DNS server, if so, then retrieve its index
-- we'll update the parameters list to include it for retrieval
content_helper.addListContent(dhcp,{ options = baseDHCPOptions })
for i,v in ipairs(dhcp.options) do
  if v:find("^6,") == 1 then
    dnsIndex = i
    mapParams["dns_v4_pri"] = baseDHCPOptions.."@"..i..".value"
  end
  if v:find("^42,") == 1 then
    ntpIndex = i
    mapParams["ntpServer"] = baseDHCPOptions.."@"..i..".value"
  end
end

content_helper.addListContent(dhcp_dns,{ dnses = baseDHCPDNS })
if dhcp_dns.dnses[1] ~= nil then
  mapParams["dns_v6_pri"] = baseDHCPDNS.."@".."1.value"
end

if dhcp_dns.dnses[2] ~= nil then
  mapParams["dns_v6_sec"] = baseDHCPDNS.."@".."2.value"
end

content,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if (ngx.var.request_method == "POST") and not next(helpmsg) then
  local dhcp_options_path = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcp_option."
  local dhcp_options = content_helper.convertResultToObject(dhcp_options_path,proxy.get(dhcp_options_path))
  for _,v in ipairs(dhcp_options) do
    if v.value:match("^3,") then
      proxy.del(dhcp_options_path..v.paramindex..".")
    elseif v.value:find("^6,") == 1 then
      local dns4pri = match(v.value,"^6,([^,]+)")
      local dns4sec = match(v.value,"^6,[^,]+,(.+)")
      if dns4sec and not find_dns(dns4sec,dns_list[1]) then
        table.insert(dns_list[1],3,{dns4sec, T("Custom ("..dns4sec..")")})
      end
      if dns4pri and not find_dns(dns4pri,dns_list[1]) then
        table.insert(dns_list[1],3,{dns4pri, T("Custom ("..dns4pri..")")})
      end
    end
  end
end

if #dns_servers == 1 and content["dns_v4_pri"] ~= nil and content["dns_v4_pri"] ~= "" then
  dns_servers = split(format("%s",content["dns_v4_pri"]),",")
end
if dns_servers[2] ~= nil and dns_servers[2] ~= "" and not find_dns(dns_servers[2],dns_list[1]) then
  dns_list[1][#dns_list[1]+1]={dns_servers[2],T("Custom ("..dns_servers[2]..")")}
end
if dns_servers[3] ~= nil and dns_servers[3] ~= "" and not find_dns(dns_servers[3],dns_list[1]) then
  dns_list[1][#dns_list[1]+1]={dns_servers[3],T("Custom ("..dns_servers[3]..")")}
end

-- set the local host IP
dns_list[1][2][1] = content["localdevIP"]

content["dns_v4_pri"] = content["localdevIP"]
content["dns_v4_sec"] = ""
if dns_servers[2] ~= nil and find_dns(dns_servers[2],dns_list[1]) then
  content["dns_v4_pri"] = dns_servers[2]
end
if dns_servers[3] ~= nil and find_dns(dns_servers[3],dns_list[1]) then
  content["dns_v4_sec"] = dns_servers[3]
end

if content["dns_v6_pri"] ~= nil and content["dns_v6_pri"] ~= "" then
  if content["dns_v6_pri"] == lan_ula then
    content["dns_v6_pri"] = "::1"
  end
  content["dns_v6_pri"] = content["dns_v6_pri"]:gsub(":","-")
else
  content["dns_v6_pri"] = "--1"
end
if content["dns_v6_sec"] ~= nil then
  if content["dns_v6_sec"] == lan_ula then
    content["dns_v6_sec"] = "::1"
  end
  content["dns_v6_sec"] = content["dns_v6_sec"]:gsub(":","-")
else
  content["dns_v6_sec"] = "--1"
end

if content["dns_v6_pri"] ~= nil and content["dns_v6_pri"] ~= "" and content["dns_v6_pri"] ~= "--1" and not find_dns(content["dns_v6_pri"],dns_list[2]) then
  local customipv6dns = content["dns_v6_pri"]:gsub("-",":")
  dns_list[2][#dns_list[2]+1]={ content["dns_v6_pri"],T("Custom ("..customipv6dns..")")}
end
if content["dns_v6_sec"] ~= nil and content["dns_v6_sec"] ~= "" and content["dns_v6_sec"] ~= "--1" and not find_dns(content["dns_v6_sec"],dns_list[2]) then
  local customipv6dns = content["dns_v6_sec"]:gsub("-",":")
  dns_list[2][#dns_list[2]+1]={ content["dns_v6_sec"],T("Custom ("..customipv6dns..")")}
end

-- because we do the localdevmask validation in localdevIP validation, we check if local ip starts with error msg "[netmask]".
-- If so move it to localdevmask
if helpmsg["localdevIP"] then
  local msg = match(helpmsg["localdevIP"],'^%[netmask%]%s(.*)')
  if msg then
    helpmsg["localdevIP"] = nil
    helpmsg["localdevmask"] = msg
  end
end
if helpmsg["dhcpLimit"] then
    local msg = match(helpmsg["dhcpLimit"],'^%[dhcpStart%]%s(.*)')
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
if content["dhcplog"] == "" then
  content["dhcplog"] = "0"
end

if not match(content["leaseTime"],"^0*([1-9]%d*[smhdw])$") then
  content["leaseTime"] = content["leaseTime"].."s"
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

local function resetreboot(path,value)
  proxy.set(path,value)
  ngx.header.content_type = "application/json"
  ngx.print('{ "success":"true" }')
  ngx.timer.at(0,function()
    proxy.apply()
  end)
  ngx.exit(ngx.HTTP_OK)
end

if ngx.var.request_method == "POST" then
  local gwIP = session:retrieve("old_ip")
  session:store("old_ip",(content["localdevIP"]))

  if curintf == "lan" and content["localdevIP"]~= gwIP and not next(helpmsg) then ngx.print('\
    <script type="text/javascript">\
      window.location="http://'); ngx.print(content["localdevIP"]); ngx.print('";\
    </script>\
    '); session:logout()
  end

  local content1 = ngx.req.get_post_args()
  local action = content1["action"]
  if action == "BRIDGED" then
    if not bridged.isBridgedMode() then
      bridged.configBridgedMode()
      return resetreboot("rpc.system.reboot","GUI")
    end
  end

  if curintf == "lan" then
    local local_path = getDomainNamePath().."local"
    local localValue = untaint(proxy.get(local_path)[1].value)
    local localName = format("/%s/",content["localdomain"])
    local domainName = session:retrieve("old_domain")
    session:store("old_domain",(content["localdomain"]))

    if (content["localdomain"] ~= domainName or localValue ~= localName) and not next(helpmsg) then
      proxy.set(local_path,localName)
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

local dns_int_ipv4 = false
local dns_int_ipv4_dest_ip = ""
local dns_int_ipv6 = false
local dns_int_ipv6_dest_ip = ""
if curintf == "lan" then
  for _,v in ipairs(proxy.getPN("uci.firewall.redirect.",true)) do
    local values = proxy.get(v.path.."name")
    if values then
      local name = untaint(values[1].value)
      if name and (name == "Intercept-DNS" or name == "Redirect-DNS") then
        values = proxy.get(v.path.."enabled",v.path.."dest_ip")
        if values then
          dns_int_ipv4 = values[1].value ~= "0"
          dns_int_ipv4_dest_ip = untaint(values[2].value)
          if not dns_int_ipv4_dest_ip or dns_int_ipv4_dest_ip == "" then
            dns_int_ipv4_dest_ip = untaint(content["localdevIP"])
          end
        end
      end
    end
  end
  local values = proxy.get("uci.tproxy.config.enabled","uci.tproxy.rule.@dnsv6.enabled","uci.tproxy.rule.@dnsv6.destIP")
  if values then
    dns_int_ipv6 = values[1].value ~= "0" and values[2].value ~= "0"
    dns_int_ipv6_dest_ip = untaint(values[3].value)
    if not dns_int_ipv6_dest_ip or dns_int_ipv6_dest_ip == "" then
      dns_int_ipv6_dest_ip = lan_ula
    end
  end
end
local dns_int_ipv4_ok = dns_int_ipv4 and content["dns_v4_pri"] == dns_int_ipv4_dest_ip
local dns_int_ipv6_ok = dns_int_ipv6 and find(dns_int_ipv6_dest_ip:gsub(":","-"),untaint(content["dns_v6_pri"]),1,true)

ngx.print(ui_helper.createHeader(T"Local Network",true,false));
ngx.print('\
   <div class="modal-body update">\
     <form class="form-horizontal" method="post" action="'); ngx.print(ngx.var.request_uri); ngx.print('">\
     ');
     if #lan_intfs> 1 and (not bridged.isBridgedMode()) then
       ngx.print('\
     <div class="span2">\
        <ul class="nav nav-list">\
        ');
        local html = { format('<li class="nav-header">%s</li>',T"LAN Interfaces") }
        for _,v in ipairs(lan_intfs) do
            local active = ""
            if v.index == curintf then
              active = "active"
            end
            if v.index ~= "loopback" then
              local url = "modals/ethernet-modal.lp?intf="..v.index
              html[#html+1] = format('<li class="%s"><a id = "%s" href="#" data-remote="%s">%s</a></li>',active,v.index,url,v.name)
            end
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
          '); if not bridged.isBridgedMode() then ngx.print('\
          <div class="span4">\
          '); end ngx.print('\
            <legend>'); ngx.print(T"Global Information"); ngx.print('</legend>\
            ');
            local advanced = {
              group = {
                class = "advanced"
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
            local stdattributes = {
              group = {
                class = "advanced",
              },
              select = {
                class = "monitor-changes span2"
              }
            }
            ngx.print(
              ui_helper.createInputText(T"Device IP Address","localdevIP",content["localdevIP"],{input = {class = "span2"}},helpmsg["localdevIP"]),
              ui_helper.createInputText(T"Local Network Subnet","localdevmask",content["localdevmask"],advanced,helpmsg["localdevmask"])
            )

            if curintf == "lan" then
              if bridged.isBridgedMode() then
                ngx.print(ui_helper.createInputText(T"Local Gateway","localgw",content["localgw"],advanced,helpmsg["localgw"]))
              end
              ngx.print(ui_helper.createInputText(T"Local Domain Name","localdomain",content["localdomain"],advanced,helpmsg["localdomain"]))
            end

            ngx.print(ui_helper.createSwitch(T"IPv6 State","localIPv6",content["localIPv6"],switch_class_enable))

            if not bridged.isBridgedMode() then
              local info_attr = {
                alert = {
                  class = "alert-info"
                },
              }
              local warn_attr = {
                alert = {
                  class = "alert-danger"
                },
              }
              if cur_dhcp_intf == "lan" then
                if dns_int_ipv4 or dns_int_ipv6 then
                  if dns_int_ipv4 and not dns_int_ipv4_ok then
                    ngx.print(ui_helper.createAlertBlock(T("DNS Intercept is enabled, so IPv4 Primary DNS should be set to '"..dns_int_ipv4_dest_ip.."'."),warn_attr))
                  end
                  if dns_int_ipv6 and not dns_int_ipv6_ok then
                    ngx.print(ui_helper.createAlertBlock(T("DNS Intercept is enabled, so IPv6 Primary DNS should be set to '"..dns_int_ipv6_dest_ip.."'."),warn_attr))
                  end
                else
                  if adguard then
                    ngx.print(ui_helper.createAlertBlock(T"DNS Servers defined on this screen only affect what is provided to LAN client devices via DHCP. You should configure custom DNS Servers for the device in AdGuard Home.",info_attr))
                  else
                    ngx.print(ui_helper.createAlertBlock(T"DNS Servers defined on this screen only affect what is provided to LAN client devices via DHCP. To configure custom DNS Servers for the device, use the DNS card.",info_attr))
                  end
                end
              end
            ngx.print('\
            </div>\
            <div class="span4">\
              <legend>'); ngx.print(T"DHCP Settings"); ngx.print('</legend>\
              ');
              if adguard then
                ngx.print(format(T'Configure DHCP through <a style="color:inherit;text-decoration:none;" target="_blank" href="http://%s:8008"><b>AdGuard Home</b></a>',content["localdevIP"]))
              else
                local switchDHCP = {
                  values = {
                    on = "server",
                    off = "disabled"
                  }
                }
                ngx.print(
                  ui_helper.createSwitch(T"DHCPv4 Server","dhcpv4State",content["dhcpv4State"],switchDHCP),
                  ui_helper.createLabel(T"Network Address",network,advanced),
                  ui_helper.createInputText(T"DHCP Start Address","dhcpStartAddress",ipstart,advanced,helpmsg["dhcpStart"]),
                  ui_helper.createInputText(T"DHCP End Address","dhcpEndAddress",ipend,advanced,helpmsg["dhcpLimit"]),
                  ui_helper.createInputText(T"Lease Time","leaseTime",content["leaseTime"],advanced,helpmsg["leaseTime"])
                )
                if cur_dhcp_intf == "lan" and not adguard then
                  ngx.print(ui_helper.createSwitch(T"Log DHCP Requests","dhcplog",content["dhcplog"]))
                  local ntpServerEnabled = proxy.get("uci.system.ntp.enable_server")
                  if ntpServerEnabled and ntpServerEnabled[1].value == "1" then
                    local switchNTP = {
                      values = {
                        on = "42,"..content["localdevIP"],
                        off = ""
                      }
                    }
                    ngx.print(ui_helper.createSwitch(T"Device as NTP Server","ntpServer",content["ntpServer"],switchNTP))
                  else
                    ngx.print(ui_helper.createSimpleInputHidden("ntpServer",""))
                  end
                  if dns_int_ipv4_ok and not helpmsg["dns_v4_pri"] and not helpmsg["dns_v4_sec"] then
                    ngx.print(ui_helper.createLabel(T"IPv4 DNS Server","Interception enabled",advanced))
                    ngx.print(ui_helper.createSimpleInputHidden("dns_v4_pri",content["dns_v4_pri"]))
                    ngx.print(ui_helper.createSimpleInputHidden("ipv4_dns_sec",content["dns_v4_sec"]))
                  else
                    ngx.print(ui_helper.createInputSelect(T"IPv4 Primary DNS","dns_v4_pri",dns_list[1],content["dns_v4_pri"],stdattributes,helpmsg["dns_v4_pri"]))
                    ngx.print(ui_helper.createInputSelect(T"IPv4 Secondary DNS","ipv4_dns_sec",dns_list[1],content["dns_v4_sec"],stdattributes,helpmsg["dns_v4_sec"]))
                  end
                end
                ngx.print(
                  [[<div class="monitor-localIPv6 monitor-1 monitor-hidden-localIPv6">]],
                  ui_helper.createSwitch(T"DHCPv6 Server","dhcpv6State",content["dhcpv6State"],switchDHCP),
                  ui_helper.createSwitch(T"SLAAC + RA<span class='icon-question-sign' title='IPv6 Stateless Address Auto-Configuration + Router Advertisement'></span>","slaacState",content["slaacState"],switchDHCP)
                )
                if cur_dhcp_intf == "lan" and not adguard then
                  if dns_int_ipv6_ok and not helpmsg["dns_v6_pri"] and not helpmsg["dns_v6_sec"]  then
                    ngx.print(ui_helper.createLabel(T"IPv6 DNS Server","Interception enabled",advanced))
                    ngx.print(ui_helper.createSimpleInputHidden("dns_v6_pri",content["dns_v6_pri"]))
                    ngx.print(ui_helper.createSimpleInputHidden("dns_v6_sec",content["dns_v6_sec"]))
                  else
                    ngx.print(ui_helper.createInputSelect(T"IPv6 Primary DNS","dns_v6_pri",dns_list[2],content["dns_v6_pri"],stdattributes,helpmsg["dns_v4_pri"]))
                    ngx.print(ui_helper.createInputSelect(T"IPv6 Secondary DNS","dns_v6_sec",dns_list[2],content["dns_v6_sec"],stdattributes,helpmsg["dns_v6_sec"]))
                  end
                end
                ngx.print(
                  [[</div>]]
                )
              end
                 ngx.print('\
            </div>\
              ');
              if curintf == "lan" then
                if not adguard then
                ngx.print('\
              </fieldset>\
              <fieldset>\
                  <legend>'); ngx.print(T"Static Leases"); ngx.print('</legend>\
                  ');
                    local confilictipchange_alert = {
                      alert = {
                          class = "alert hide",
                          id = "confilictipchange-msg"
                      }
                    }
                    ngx.print(
                      ui_helper.createAlertBlock(T"The latest added/modified static lease is set to an IP that is already in use, please perform a reboot to apply the changes",confilictipchange_alert),
                      ui_helper.createTable(sleases_columns,sleases_data,sleases_options,nil,sleases_helpmsg))
                    ngx.print('\
              </fieldset>\
              <fieldset>\
                  <legend>'); ngx.print(T"Custom DHCP Options"); ngx.print('</legend>\
                  ');
                    ngx.print(
                      ui_helper.createTable(tags_columns,tags_data,tags_options,nil,tags_helpmsg))
                    ngx.print('\
              </fieldset>\
                ');
              end
              ngx.print('\
              <fieldset>\
                <legend>'); ngx.print(T"Network Mode"); ngx.print('</legend>\
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
                html[#html+1] = ui_helper.createButton("Bridged Mode","Bridged Mode","icon-cog",bridged_button)
                html[#html+1] = string.format('<div class="control-group controls">')
                html[#html+1] = ui_helper.createAlertBlock(T"Switching to <strong>Bridged Mode</strong> and restarting. Please wait...",rebooting)
                html[#html+1] = ui_helper.createAlertBlock(T"Are you sure you want to switch to <strong>Bridged Mode</strong>?",confirming)
                html[#html+1] = string.format([[
                  <div id="bridged-changes" class="hide">
                    <div id="bridged-confirm" class="btn btn-primary" data-dismiss="modal">%s</div>
                    <div id="bridged-cancel" class="btn">%s</div>
                  </div>
                </div>
                ]],T"Confirm",T"Cancel")
                ngx.print(html)
              end
            else
              local html = {}
              local infomessage = {
                alert = {
                  class = "alert",
                }
              }
              local ipv4DNScolumns = {
                {
                  header = T"IPv4 DNS Server Address",
                  name = "wanDnsParam",
                  param = "value",
                  type = "text",
                  attr = { input = { class="span2",maxlength="15" } },
                },
              }
              local ipv4DNSoptions = {
                canEdit = true,
                canAdd = true,
                canDelete = true,
                tableid = "dns4server",
                basepath = "uci.network.interface.@lan.dns.@.",
                createMsg = T"Add IPv4 DNS Server",
                minEntries = 0,
                maxEntries = 4,
                sorted = function(a,b)
                  return tonumber(a.paramindex) < tonumber(b.paramindex)
                end
              }
              local ipv4DNSvalid = {
                wanDnsParam = aIPV,
              }
              local ipv4DNSdata,ipv4DNShelpmsg = post_helper.handleTableQuery(ipv4DNScolumns,ipv4DNSoptions,nil,nil,ipv4DNSvalid)
              html[#html+1] = "<legend>LAN Domain Name Server Configuration</legend>"
              html[#html+1] = ui_helper.createTable(ipv4DNScolumns,ipv4DNSdata,ipv4DNSoptions,nil,ipv4DNShelpmsg)
              html[#html+1] = "<legend>Network Mode</legend>"
              html[#html+1] = ui_helper.createAlertBlock(T[[Note: your Device is in Bridged Mode. If you want the Device to run in Routed Mode again,you can revert without factory reset <a href="javascript:tch.loadModal('/modals/broadband-modal.lp')">here</a>]],infomessage)
              ngx.print(html)
            end
            ngx.print('\
          </fieldset>\
          ');
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
    $("input,select").on("change",function(){\
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
          },tch.elementCSRFtoken());\
          var target = $(".modal form").attr("action");\
          tch.showProgress("Wait...");\
          $.post(target,params,function(response){\
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
$("[name =\'sleases_mac\'],[name =\'dns_v4_pri\'],[name =\'ipv4_dns_sec\'],[name =\'dns_v6_pri\'],[name =\'dns_v6_sec\'],[name =\'tags_dns1\'],[name =\'tags_dns2\']").change(function () {\
  if ((this.value) == "custom") {\
    $(this).replaceWith($(\'<input/>\',{\'type\':\'text\',\'name\':this.name,\'class\':this.className}));\
  }\
});\
');  if dhcpDefaultExist then ngx.print('\
$("#btn-dhcp-reset").click(function(){\
  '); for k,v in pairs(dhcpDefaultExist) do ngx.print('\
    $("#'); ngx.print(k); ngx.print('").val("'); ngx.print(v); ngx.print('");\
  '); end ngx.print('\
  $("#localIPv6").prev().removeClass("switcherOn").closest(".switch").removeClass("switchOn");\
  $("#dhcpState").prev().addClass("switcherOn").closest(".switch").addClass("switchOn");\
  $("#modal-changes").show();\
  $("#modal-no-change").hide();\
});\
');  end  ngx.print('\
  '); if inUseFlag == true then ngx.print('\
  $("#confilictipchange-msg").show();\
  '); end ngx.print('\
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
    $.ajax({ url: "/",timeout: refreshTimeOut })\
    .done(function(data) {\
      document.open("text/html");\
      document.write(data);\
      document.close();\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running,refreshDelay);\
    });\
  }\
  function wait_for_webserver_down() {\
    $.ajax({ url: target,timeout: refreshTimeOut })\
    .done(function() {\
      window.setTimeout(wait_for_webserver_down,refreshDelay);\
    })\
    .fail(function() {\
      window.setTimeout(wait_for_webserver_running,refreshDelay);\
    });\
  }\
  function resetreboot(msg,msg_dst,action) {\
    msg_dst.after(msg);\
    msg.removeClass("hide");\
    msg[0].scrollIntoView();\
    $.post(\
      target,\
      { action: action,CSRFtoken: $("meta[name=CSRFtoken]").attr("content") },\
      wait_for_webserver_down,\
      "json"\
    );\
    return false;\
  }\
  $("#btn-bridged").click(function() {\
    $("#confirming-msg").removeClass("hide");\
    $("#bridged-changes").removeClass("hide");\
    $(".modal-body").animate({"scrollTop":"+=100px"},"fast")\
  });\
  $("#bridged-confirm").click(function() {\
    $("#confirming-msg").addClass("hide");\
    $("#bridged-changes").addClass("hide");\
    $("#btn-bridged").addClass("hide");\
    return resetreboot($("#rebooting-msg"),$("#btn-bridged"),"BRIDGED");\
  });\
  $("#bridged-cancel").click(function() {\
    $("#confirming-msg").addClass("hide");\
    $("#bridged-changes").addClass("hide");\
    $("#rebooting-msg").addClass("hide");\
  });\
}());\
</script>\
');