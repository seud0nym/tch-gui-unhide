--pretranslated: do not change this file

-- NOTE: All occurences of the string "Device" in this file will be replaced with the variant (e.g. DJA0231)

-- Localization
gettext.textdomain('webui-core')

local ethernet_helper = require("ethernet_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")

local ngx = ngx
local cur_intf,cur_dhcp_intf,all_intfs,lan_intfs,ppp_dev,ppp_intf,dnsmasq_path,wireless_radio = ethernet_helper.get_interfaces()

--We need to update IPv6 state only when user want to update the IPv6 state alone.
if ngx.var.request_method == "POST" and ngx.req.get_post_args(0).iPv6StateOnlyChanged == "yes" then
  local ipv6MapParam = {
    localIPv6 = "uci.network.interface.@"..cur_intf..".ipv6"
  }
  local ipv6MapValid = {
    localIPv6 = ethernet_helper.validateIPv6(cur_intf,wireless_radio,ppp_dev,ppp_intf)
  }
  post_helper.handleQuery(ipv6MapParam,ipv6MapValid)
  ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.exit(ngx.HTTP_OK)
end

local bridged = require("bridgedmode_helper")
local content_helper = require("web.content_helper")
local lfs = require("lfs")
local proxy = require("datamodel")

local dhcpDefaultExist
if lfs.attributes("/www/lua/dhcpreset_helper.lua","mode") == "file" then
  dhcpDefaultExist = require("dhcpreset_helper")
end

local dns_list = require('dns_servers_helper').all()
local hosts_ac = require("web.uinetwork_helper").getAutocompleteHostsList()
local split = require("split").split

local ipairs,pairs,string = ipairs,pairs,string
local tonumber,tostring = tonumber,tostring
local find,format,gsub,match = string.find,string.format,string.gsub,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local session = ngx.ctx.session

local isBridgedMode = bridged.isBridgedMode()
local hosts_mac = ethernet_helper.get_hosts_mac()
local tags = ethernet_helper.get_dhcp_tags()
local lan_ula = ethernet_helper.get_lan_ula()

local ipv42num = post_helper.ipv42num
local vB = post_helper.validateBoolean
local vIP4N = post_helper.validateIPv4Netmask
local vQTN = post_helper.validateQTN
local gAV = post_helper.getAndValidation
local gOV = post_helper.getOptionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local gVNIR = post_helper.getValidateNumberInRange
local vNES = post_helper.validateNonEmptyString
local vSIDN = post_helper.validateStringIsDomainName
local vSIIP = post_helper.validateStringIsIP
local vSIIP4 = post_helper.validateStringIsIPv4
local vSIIP6 = post_helper.validateStringIsIPv6

local content,helpmsg

local function validateDUID(value,object,_)
  if value ~= "" and (#value < 20 or #value > 28 or not value:match("^[%x]+$")) then
    return nil, T"A Client DUID can only contain 28 hexadecimal digits."
  end
  if value == "" and object["hostid"] ~= "" then
    return nil, T"A Client DUID is required when setting the IPv6 Host ID."
  end
  return true
end

local function validateHostID(value,_,_)
  if value ~= "" and (#value > 8 or not value:match("^[%x]+$")) then
    return nil, T"A Host ID can only contain 1 to 8 hexadecimal digits."
  end
  return true
end

local function validateHint(value,_,_)
  if value ~= "" and (#value > 4 or not value:match("^[%x]+$")) then
    return nil, T"A hint can only contain 1 to 4 hexadecimal digits."
  end
  return true
end

local sleases_mac_attr = { class="span2",maxlength="17",id="lease_mac",style="width:115px" }

-- Static Leases
local sleases_columns = {
  {
    header = T"MAC Address",
    name = "sleases_mac",
    param = "mac",
    type = "select",
    values = hosts_mac,
    attr = { select = sleases_mac_attr,autocomplete = hosts_mac },
    unique = true,
  },
  {
    header = T"Host Name" ,
    name = "sleases_name",
    param = "name",
    type = "text",
    attr = { input = { class="span2",style="width:90px" } },
  },
  {
    header = T"IPv4 Address",
    name = "sleases_ip",
    param = "ip",
    type = "text",
    attr = { input = { class="span2",maxlength="15",style="width:110px" },autocomplete = hosts_ac },
  },
  {
    header = T"Client DUID&nbsp;<span class='icon-question-sign' title='DHCP Unique Identifier as 28 hexadecimal digits is required to assign the IPv6 Host ID.'></span>" ,
    name = "sleases_duid",
    param = "duid",
    type = "text",
    attr = { input = { class="span2",maxlength="28",minlength="20",style="width:210px" } },
  },
  {
    header = T"IPv6 Host ID" ,
    name = "sleases_hostid",
    param = "hostid",
    type = "text",
    attr = { input = { class="span2",maxlength="8",style="width:60px" } },
  },
  {
    header = T"Custom DHCP Options Tag",
    name = "sleases_tag",
    param = "tag",
    type = "select",
    attr = { select = { class="span2",style="width:120px" } },
    values = tags,
  },
  {
    header = T"Add to DNS&nbsp;<span class='icon-question-sign' title='Add static forward and reverse DNS entries for this host, so that the host name resolves even when lease is not active.'></span>",
    name = "sleases_dns",
    param = "dns",
    type = "switch",
    default = "1",
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

local sleases_valid = {
  sleases_name = gAV(ethernet_helper.validateStaticLeaseName,vSIDN),
  sleases_mac = gAV(ethernet_helper.validateStaticLeaseMAC,vQTN),
  sleases_ip = gOV(gAV(ethernet_helper.validateStaticLeaseIP(cur_intf),vQTN)),
  sleases_hostid = validateHostID,
  slease_duid = validateDUID,
}

local sleases_options = {
  tableid = "sleases",
  basepath = "uci.dhcp.host.@.",
  createMsg = T"Add Static Lease",
  objectName = post_helper.getRandomKey(),
  addNamedObject = true,
  sorted = ethernet_helper.sort_static_leases,
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
  tags_name = ethernet_helper.validateTagName,
  tags_dns1 = vSIIP4,
  tags_dns2 = gOV(vSIIP4),
}

local tags_options = {
  tableid = "tags",
  basepath = "uci.dhcp.tag.@.",
  createMsg = T"Add Tag",
  addNamedObject = true,
  canEdit = false,
  onAdd = ethernet_helper.onAddTag,
  sorted = ethernet_helper.sort_tags,
}

local tags_data,tags_helpmsg = post_helper.handleTableQuery(tags_columns,tags_options,tags_filter,nil,tags_valid)
for i,row in pairs(tags_data) do
  tags_data[i][2] = match(row[4],"^6,([^,]+)") or ""
  tags_data[i][3] = match(row[4],"^6,[^,]+,(.+)") or ""
end

local inUseFlag = false
local local_dev_ip
if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args(0)
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
        sleases_mac_attr.value = text_mac
        sleases_columns[1] = {
          header = T"MAC Address",
          name = "sleases_mac",
          param = "mac",
          type = "text",
          attr = { input = sleases_mac_attr },
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
  local_dev_ip = post_data["localdevIP"]
else
  local_dev_ip = proxy.get("rpc.network.interface.@"..cur_intf..".ipaddr")[1].value
end

local dnsIndex = 0
local ntpIndex = 0
local dns_servers = {""}
local baseDHCPOptions = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcp_option."
local baseDHCPDNS = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dns."

local mapParams = {
  dhcpIgnore = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".ignore",
  dhcpLimit = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".limit",
  dhcplog = dnsmasq_path.."logdhcp",
  dhcpquiet = dnsmasq_path.."quietdhcp",
  dhcplogv6 = "uci.dhcp.odhcpd.@odhcpd.loglevel",
  dhcpStart = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".start",
  dhcpv4State = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcpv4",
  dhcpv6State = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcpv6",
  leaseTime = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".leasetime",
  localdomain = dnsmasq_path.."domain",
  localgw = "uci.network.interface.@"..cur_intf..".gateway",
  localdevIP = "uci.network.interface.@"..cur_intf..".ipaddr",
  localIPv6 = "uci.network.interface.@"..cur_intf..".ipv6",
  localdevmask = "uci.network.interface.@"..cur_intf..".netmask",
  slaacState = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".ra",
  ip6assign = "uci.network.interface.@" .. cur_intf .. ".ip6assign",
  ip6hint = "uci.network.interface.@" .. cur_intf .. ".ip6hint",
  ula_prefix = "uci.network.globals.ula_prefix",
  proto = "uci.network.interface.@lan.proto",
}

if isBridgedMode and cur_intf == "lan" then
  for k,_ in pairs(mapParams) do
    if k ~= "localdevIP" and k ~= "localIPv6" and k ~= "localdevmask" and k ~= "localgw" and k ~= "proto" then
      mapParams[k] = nil
    end
  end
else
  mapParams["proto"] = nil

  local dhcp = {}
  local dhcp_dns = {}
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

local function validateDNSv4(value,object,key)
  local dns_str = ""
  local post_data = ngx.req.get_post_args(0)

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
    if value == local_dev_ip then
      dns_str = object["localdevIP"]
    elseif vSIIP4(value) then
      dns_str = value
    else
      return nil,"Invalid DNS server"
    end
  end

  if post_data.ipv4_dns_sec ~= "" then
    if post_data.ipv4_dns_sec == local_dev_ip then
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

local mapValid = {
  dhcpIgnore = ethernet_helper.validateDHCPIgnore,
  dhcpLimit = ethernet_helper.validateDHCPLimit(cur_intf,cur_dhcp_intf,local_dev_ip),
  dhcplog = gOV(vB),
  dhcpquiet = gOV(vB),
  dhcplogv6 = gOV(gVNIR(4,7)),
  dhcpStart = ethernet_helper.validateDHCPStart(cur_intf,cur_dhcp_intf,local_dev_ip),
  dhcpv4State = ethernet_helper.validateDHCPState(),
  dhcpv6State = ethernet_helper.validateDHCPState(),
  dns_v4_pri = validateDNSv4,
  dns_v6_pri = validateDNSv6,
  dns_v6_sec = validateDNSv6,
  ip6assign = gOV(gVNIR(48,64)),
  ip6hint = gOV(validateHint),
  leaseTime = ethernet_helper.validateLeaseTime,
  localdomain = vNES,
  localgw = gOV(vSIIP4),
  localdevIP = ethernet_helper.validateGatewayIP(cur_intf,all_intfs,lan_intfs),
  localIPv6 = ethernet_helper.validateIPv6(cur_intf,wireless_radio,ppp_dev,ppp_intf),
  localdevmask = gAV(vIP4N,vSIIP),
  ntpServer = validateNTP,
  proto = gVIES(ethernet_helper.proto),
  slaacState = ethernet_helper.validateDHCPState(),
  ula_prefix = ethernet_helper.validateULAPrefix,
}

content,helpmsg = post_helper.handleQuery(mapParams,mapValid)

local function proxySet(p,v)
  local success,errors = proxy.set(p,v)
  if not success then
    for _,err in ipairs(errors) do
      message_helper.pushMessage(T(string.format("Failed to set %s to '%s': %s (%s)",err.path,v,err.errmsg,err.errcode)),"error")
    end
  end
  return success
end

if ngx.var.request_method == "POST" then
  if cur_intf == "lan" then
    bridged.configure(ngx.req.get_post_args(0)["action"])
  end

  if isBridgedMode then
    local proto = session:retrieve("old_proto")
    session:store("old_proto",(content["proto"]))
    if content["proto"] ~= proto and not next(helpmsg) then
      local cur_ip
      repeat
        ngx.sleep(5)
        cur_ip = proxy.get("rpc.network.interface.@lan.ipaddr","rpc.network.interface.@lan.ipmask")
        content["localdevIP"] = untaint(cur_ip[1].value)
        content["localdevmask"] = bridged.maskCIDRToDottedDecimal(cur_ip[2].value)
      until content["localdevIP"] ~= ""
      if content["proto"] == "dhcp" then
        proxySet("uci.network.interface.@lan.reqopts","1 3 6 43 51 58 59")
      elseif content["proto"] == "static" then
        local uci_ipaddr = proxy.get("uci.network.interface.@lan.ipaddr")[1].value
        if uci_ipaddr == "" then
          proxySet("uci.network.interface.@lan.ipaddr",content["localdevIP"])
        else
          content["localdevIP"] = untaint(uci_ipaddr)
        end
        local uci_mask = proxy.get("uci.network.interface.@lan.netmask")[1].value
        if uci_mask == "" then
          content["localdevmask"] = uci_mask
          proxySet("uci.network.interface.@lan.netmask",content["localdevmask"])
        else
          content["localdevmask"] = untaint(uci_mask)
        end
      end
      proxy.apply()
    end
  end

  local gwIP = session:retrieve("old_ip")
  session:store("old_ip",(content["localdevIP"]))
  if cur_intf == "lan" and content["localdevIP"] ~= gwIP and not next(helpmsg) then
    ngx.print('\
    <script type="text/javascript">\
      window.location="http://',content["localdevIP"],'";\
    </script>\
    ')
    session:logout()
  end
else
  session:store("old_ip",(content["localdevIP"]))
  if content["localdomain"] then
    session:store("old_domain",(content["localdomain"]))
  end
  if isBridgedMode then
    session:store("old_proto",(content["proto"]))
  end
end

if ngx.var.request_method == "POST" and not next(helpmsg) then
  local dhcp_options_path = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcp_option."
  local dhcp_options = content_helper.convertResultToObject(dhcp_options_path,proxy.get(dhcp_options_path))
  for _,v in ipairs(dhcp_options) do
    if v.value:match("^3,") then
      proxy.del(dhcp_options_path..v.paramindex..".")
    elseif v.value:find("^6,") == 1 then
      local dns4pri = match(v.value,"^6,([^,]+)")
      local dns4sec = match(v.value,"^6,[^,]+,(.+)")
      if dns4sec and not ethernet_helper.find_dns(dns4sec,dns_list[1]) then
        table.insert(dns_list[1],3,{dns4sec, T("Custom ("..dns4sec..")")})
      end
      if dns4pri and not ethernet_helper.find_dns(dns4pri,dns_list[1]) then
        table.insert(dns_list[1],3,{dns4pri, T("Custom ("..dns4pri..")")})
      end
    end
  end
end

if #dns_servers == 1 and content["dns_v4_pri"] ~= nil and content["dns_v4_pri"] ~= "" then
  dns_servers = split(format("%s",content["dns_v4_pri"]),",")
end
if dns_servers[2] ~= nil and dns_servers[2] ~= "" and not ethernet_helper.find_dns(dns_servers[2],dns_list[1]) then
  dns_list[1][#dns_list[1]+1]={dns_servers[2],T("Custom ("..dns_servers[2]..")")}
end
if dns_servers[3] ~= nil and dns_servers[3] ~= "" and not ethernet_helper.find_dns(dns_servers[3],dns_list[1]) then
  dns_list[1][#dns_list[1]+1]={dns_servers[3],T("Custom ("..dns_servers[3]..")")}
end

content["dns_v4_pri"] = content["localdevIP"]
content["dns_v4_sec"] = ""
if dns_servers[2] ~= nil and ethernet_helper.find_dns(dns_servers[2],dns_list[1]) then
  content["dns_v4_pri"] = dns_servers[2]
end
if dns_servers[3] ~= nil and ethernet_helper.find_dns(dns_servers[3],dns_list[1]) then
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

if content["dns_v6_pri"] ~= nil and content["dns_v6_pri"] ~= "" and content["dns_v6_pri"] ~= "--1" and not ethernet_helper.find_dns(content["dns_v6_pri"],dns_list[2]) then
  local customipv6dns = content["dns_v6_pri"]:gsub("-",":")
  dns_list[2][#dns_list[2]+1]={ content["dns_v6_pri"],T("Custom ("..customipv6dns..")")}
end
if content["dns_v6_sec"] ~= nil and content["dns_v6_sec"] ~= "" and content["dns_v6_sec"] ~= "--1" and not ethernet_helper.find_dns(content["dns_v6_sec"],dns_list[2]) then
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

if content["dhcpv4State"] and content["dhcpv4State"] == "" then
  content["dhcpv4State"] = "server"
end
if content["localIPv6"] and content["localIPv6"] == "" then
  content["localIPv6"] = "1"
end
if content["dhcplog"] and content["dhcplog"] == "" then
  content["dhcplog"] = "0"
end
if content["dhcpquiet"] and content["dhcpquiet"] == "" then
  content["dhcpquiet"] = "0"
end
if content["dhcplogv6"] and content["dhcplogv6"] == "" then
  content["dhcplogv6"] = "6"
elseif tonumber(content["dhcplogv6"]) < 4 then
  content["dhcplogv6"] = "4"
end

local ipstart,ipend,network
if content["leaseTime"] then
  if not match(content["leaseTime"],"^0*([1-9]%d*[smhdw])$") then
    content["leaseTime"] = content["leaseTime"].."s"
  end
  local netmask = ipv42num(content["localdevmask"])
  local baseip = ipv42num(content["localdevIP"])
  local start = tonumber(content["dhcpStart"])
  local numips = tonumber(content["dhcpLimit"])
  if start and numips and not helpmsg["localdevIP"] and not helpmsg["localdevmask"] and not helpmsg["dhcpStart"] and not helpmsg["dhcpLimit"] then
    ipstart,ipend,network = ethernet_helper.calculateDHCPStartAddress(baseip,netmask,start,numips)
    if ipstart then
      session:store("dhcpStartAddr",ipstart)
      session:store("dhcpEndAddr",ipend)
      session:store("networkAddr",network)
    end
  else
    ipstart = ngx.req.get_post_args(0).dhcpStartAddress
    ipend = ngx.req.get_post_args(0).dhcpEndAddress
    network = session:retrieve("networkAddr")
  end
  if helpmsg["leaseTime"] then
    content["leaseTime"] = ngx.req.get_post_args(0).leaseTime
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
if cur_intf == "lan" then
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
  if content["localIPv6"] == "1" then
    local values = proxy.get("uci.tproxy.config.enabled","uci.tproxy.rule.@dnsv6.enabled","uci.tproxy.rule.@dnsv6.destIP")
    if values then
      dns_int_ipv6 = values[1].value ~= "0" and values[2].value ~= "0"
      dns_int_ipv6_dest_ip = untaint(values[3].value)
      if not dns_int_ipv6_dest_ip or dns_int_ipv6_dest_ip == "" then
        dns_int_ipv6_dest_ip = lan_ula
      end
    end
  end
end
local dns_int_ipv4_ok = dns_int_ipv4 and content["dns_v4_pri"] == dns_int_ipv4_dest_ip
local dns_int_ipv6_ok = dns_int_ipv6 and (dns_int_ipv6_dest_ip == content["dns_v6_pri"] or find(dns_int_ipv6_dest_ip:gsub(":","-"),untaint(content["dns_v6_pri"]),1,true))

ngx.print(ui_helper.createHeader(T"Local Network",true,true),'\
   <div class="modal-body update">\
     <form class="form-horizontal" method="post" action="',ngx.var.request_uri,'">\
     ');
     if #lan_intfs > 1 and not isBridgedMode then
       ngx.print('\
     <div class="span2">\
        <ul class="nav nav-list">\
        ');
        local html = { format('<li class="nav-header">%s</li>',T"LAN Interfaces") }
        for _,v in ipairs(lan_intfs) do
            local active = ""
            if v.index == cur_intf then
              active = "active"
            end
            if v.index ~= "loopback" then
              local url = "modals/ethernet-modal.lp?intf="..v.index
              html[#html+1] = format('<li class="%s"><a id = "%s" href="#" data-remote="%s">%s</a></li>',active,v.index,url,v.name)
            end
        end
        ngx.print(html,'\
        </ul>\
     </div>\
     <div class="span9">\
        ');
      end
      ngx.print(ui_helper.createMessages(message_helper.popMessages()),'\
        <fieldset>\
          '); if not isBridgedMode then ngx.print('\
          <div class="span4">\
          '); end ngx.print('\
            <legend>',T"Global Information",'</legend>\
            ');
            local advanced = {
              group = {
                class = "advanced"
              },
              input = {
                class = "span2"
              }
            }
            local hijacked_label = {
              group = {
                class = "advanced"
              },
              span = {
                style = "white-space:nowrap;"
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
            local ipv6_attr = {
              group = {
                class = "monitor-localIPv6 monitor-1",
              }
            }
            local dhcp_attr = {
              group = {
                class = "monitor-proto monitor-dhcp",
              }
            }
            local static_attr = {
              group = {
                class = "advanced monitor-proto monitor-static",
              },
              input = {
                class = "span2"
              }
            }

            if isBridgedMode and cur_intf == "lan" then
              local cur_ip = proxy.get("rpc.network.interface.@lan.ipaddr","rpc.network.interface.@lan.ipmask","rpc.network.interface.@lan.nexthop")
              local dhcp_warn_attr = {
                alert = {
                  class = "monitor-proto monitor-dhcp alert-danger"
                },
              }
              local proto_attr = {
                input = {
                  class = "monitor-changes",
                },
              }
              ngx.print(
                ui_helper.createInputRadio(T"IPv4 Address Source","proto",ethernet_helper.proto,content["proto"],proto_attr,helpmsg["proto"]),
                ui_helper.createAlertBlock(T"DHCP Assigned IP address must <I>only</I> be used on LAN bridges. Do <B>NOT</B> use on a WAN bridge!",dhcp_warn_attr)
              )
              if cur_ip and cur_ip[2].value ~= "" then
                ngx.print(
                  ui_helper.createLabel(T"Device IPv4 Address",cur_ip[1].value,dhcp_attr),
                  ui_helper.createLabel(T"Local Network Subnet",bridged.maskCIDRToDottedDecimal(cur_ip[2].value),dhcp_attr),
                  ui_helper.createLabel(T"Local Gateway",cur_ip[3].value,dhcp_attr)
                )
              end
            end
            ngx.print(
              ui_helper.createInputText(T"Device IPv4 Address","localdevIP",content["localdevIP"],static_attr,helpmsg["localdevIP"]),
              ui_helper.createInputText(T"Local Network Subnet","localdevmask",content["localdevmask"],static_attr,helpmsg["localdevmask"])
            )
            if isBridgedMode then
              ngx.print(ui_helper.createInputText(T"Local Gateway","localgw",content["localgw"],static_attr,helpmsg["localgw"]))
            else
              ngx.print(ui_helper.createInputText(T"Local Domain Name","localdomain",content["localdomain"],advanced,helpmsg["localdomain"]))
            end

            ngx.print(ui_helper.createSwitch(T"IPv6 State","localIPv6",content["localIPv6"],switch_class_enable))
            if content["localIPv6"] == "1" then
              local ipv6addr
              if cur_intf == "lan" then
                ipv6addr = proxy.get("rpc.network.interface.@lan6.ip6addr")
              end
              if not ipv6addr then
                ipv6addr = proxy.get("rpc.network.interface.@"..cur_intf..".ip6addr")
              end
              if ipv6addr then
                ngx.print(ui_helper.createLabel(T"Device IPv6 Address",ipv6addr[1].value,ipv6_attr))
              end
            end

            if not isBridgedMode then
              if cur_intf == "lan" then
                local ula_attr = {
                  controls = { style = "width:220px", },
                  group = { class = "monitor-localIPv6 monitor-1 monitor-hidden-localIPv6", },
                  input = { style = "width:180px", }
                }
                ngx.print(ui_helper.createInputText(T"IPv6 ULA Prefix<span class='icon-question-sign' title='IPv6 equivalent of IPv4 private addresses. Must start with fd followed by 40 random bits and a /48 range (e.g. fd12:3456:789a::/48)'></span>", "ula_prefix", content["ula_prefix"], ula_attr, helpmsg["ula_prefix"]))
              end
              local min_prefix_len = 48
              local ip6prefix = proxy.get("rpc.network.interface.@wan6.ip6prefix")
              if ip6prefix and ip6prefix[1].value ~= "" then
                min_prefix_len = tonumber(match(untaint(ip6prefix[1].value),"/(%d+)$")) or 48
              end
              if min_prefix_len < 64 then min_prefix_len = min_prefix_len + 1 end
              local req_prefix_map = {}
              for prefix_len = 64,min_prefix_len,-1 do
                req_prefix_map[#req_prefix_map+1] = { tostring(prefix_len),T("/"..prefix_len) }
              end
              local number_attr = {
                group = { class = "monitor-localIPv6 monitor-1 monitor-hidden-localIPv6", },
                select = { style = "width:100px", }
              }
              local hex_attr = {
                group = { class = "monitor-localIPv6 monitor-1 monitor-hidden-localIPv6", },
                input = { style = "width:80px", }
              }
              ngx.print(
                ui_helper.createInputSelect(T"IPv6 Prefix Size<span class='icon-question-sign' title='Delegate a prefix of the given length to this interface'></span>","ip6assign",req_prefix_map,content["ip6assign"],number_attr,helpmsg["ip6assign"]),
                ui_helper.createInputText(T"IPv6 Subprefix Hint<span class='icon-question-sign' title='Subprefix ID to be used if available'></span>","ip6hint",content["ip6hint"],hex_attr,helpmsg["ip6hint"])
              )

              if cur_dhcp_intf == "lan" then
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
                if dns_int_ipv4 or dns_int_ipv6 then
                  if dns_int_ipv4 and not dns_int_ipv4_ok then
                    ngx.print(ui_helper.createAlertBlock(T("DNS Hijacking is enabled, so IPv4 Primary DNS should be set to '"..dns_int_ipv4_dest_ip.."'."),warn_attr))
                  end
                  if dns_int_ipv6 and not dns_int_ipv6_ok then
                    ngx.print(ui_helper.createAlertBlock(T("DNS Hijacking is enabled, so IPv6 Primary DNS should be set to '"..dns_int_ipv6_dest_ip.."'."),warn_attr))
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
                <legend>',T"DHCP Settings",'</legend>\
              ');
              if adguard and content["dhcpv4State"] == "disabled" then
                ngx.print(format(T'Configure DHCP through <a style="color:inherit;text-decoration:none;" target="_blank" href="http://%s:8008"><b>AdGuard Home</b></a>',content["localdevIP"]))
              else
                local switchDHCP = {
                  values = {
                    on = "server",
                    off = "disabled"
                  }
                }
                local switchDHCPv4Log = {
                  values = {
                    on = "0",
                    off = "1"
                  }
                }
                ngx.print(
                  ui_helper.createSwitch(T"DHCPv4 Server","dhcpv4State",content["dhcpv4State"],switchDHCP),
                  ui_helper.createLabel(T"Network Address",network,advanced),
                  ui_helper.createInputText(T"DHCP Start Address","dhcpStartAddress",ipstart,advanced,helpmsg["dhcpStart"]),
                  ui_helper.createInputText(T"DHCP End Address","dhcpEndAddress",ipend,advanced,helpmsg["dhcpLimit"]),
                  ui_helper.createInputText(T"Lease Time","leaseTime",content["leaseTime"],advanced,helpmsg["leaseTime"]),
                  ui_helper.createSwitch(T"Standard DHCP Logging","dhcpquiet",content["dhcpquiet"],switchDHCPv4Log),
                  ui_helper.createSwitch(T"Detailed DHCP Logging","dhcplog",content["dhcplog"])
                )
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
                  ngx.print(ui_helper.createLabel(T"IPv4 DNS Server<span class='icon-question-sign' title='IPv4 DNS Hijacking enabled'></span>",content["dns_v4_pri"],hijacked_label))
                  ngx.print(ui_helper.createSimpleInputHidden("dns_v4_pri",content["dns_v4_pri"]))
                  ngx.print(ui_helper.createSimpleInputHidden("ipv4_dns_sec",""))
                else
                  ngx.print(ui_helper.createInputSelect(T"IPv4 Primary DNS","dns_v4_pri",dns_list[1],content["dns_v4_pri"],stdattributes,helpmsg["dns_v4_pri"]))
                  ngx.print(ui_helper.createInputSelect(T"IPv4 Secondary DNS","ipv4_dns_sec",dns_list[1],content["dns_v4_sec"],stdattributes,helpmsg["dns_v4_sec"]))
                end
                ngx.print(
                  [[<div class="monitor-localIPv6 monitor-1 monitor-hidden-localIPv6">]],
                  ui_helper.createSwitch(T"DHCPv6 Server","dhcpv6State",content["dhcpv6State"],switchDHCP),
                  ui_helper.createSwitch(T"SLAAC + RA<span class='icon-question-sign' title='IPv6 Stateless Address Auto-Configuration + Router Advertisement'></span>","slaacState",content["slaacState"],switchDHCP)
                )
                if cur_dhcp_intf == "lan" then
                  local select_attr = {
                    select = { class = "span2", }
                  }
                  local dhcpv6LogLevel = {
                    {"4",T"Warnings and Errors only"},
                    {"5",T"Minimal"},
                    {"6",T"Standard"},
                    {"7",T"Detailed"},
                  }
                  ngx.print(ui_helper.createInputSelect(T"DHCPv6 Logging","dhcplogv6",dhcpv6LogLevel,content["dhcplogv6"],select_attr,helpmsg["dhcplogv6"]))
                end
                if dns_int_ipv6_ok and not helpmsg["dns_v6_pri"] and not helpmsg["dns_v6_sec"]  then
                  local dns_v6_pri = gsub(untaint(content["dns_v6_pri"]),"-",":")
                  if dns_v6_pri == "::1" then
                    local dhcp_dns_v6 = proxy.get("uci.dhcp.dhcp.@lan.dns.@1.value")
                    if dhcp_dns_v6 then
                      dns_v6_pri = dhcp_dns_v6[1].value
                    end
                  end
                  ngx.print(ui_helper.createLabel(T"IPv6 DNS Server<span class='icon-question-sign' title='IPv6 DNS Hijacking enabled'></span>",dns_v6_pri,hijacked_label))
                  ngx.print(ui_helper.createSimpleInputHidden("dns_v6_pri",content["dns_v6_pri"]))
                  ngx.print(ui_helper.createSimpleInputHidden("dns_v6_sec",""))
                else
                  ngx.print(ui_helper.createInputSelect(T"IPv6 Primary DNS","dns_v6_pri",dns_list[2],content["dns_v6_pri"],stdattributes,helpmsg["dns_v4_pri"]))
                  ngx.print(ui_helper.createInputSelect(T"IPv6 Secondary DNS","dns_v6_sec",dns_list[2],content["dns_v6_sec"],stdattributes,helpmsg["dns_v6_sec"]))
                end
                ngx.print([[</div>]])
              end
              ngx.print('\
            </div>\
              ');
              if not (adguard and content["dhcpv4State"] == "disabled") then
                ngx.print('\
              </fieldset>\
              <fieldset>\
                <legend>',T"Static Leases",'</legend>\
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
                <legend>',T"Custom DHCP Options",'</legend>\
                ',ui_helper.createTable(tags_columns,tags_data,tags_options,nil,tags_helpmsg),'\
              </fieldset>\
                ');
              end
              if cur_intf == "lan" then
                ngx.print('\
              <fieldset>\
                <legend>',T"Network Mode",'</legend>\
                ');
                local html = {}
                bridged.addBridgedModeButtons(html)
                ngx.print(html)
              end
            else
              local html = {}
              html[#html + 1] = "<fieldset>"
              html[#html + 1] = "<legend>Network Mode</legend>"
              bridged.addRoutedModeButtons(html)
              ngx.print(html)
            end
            ngx.print('\
          </fieldset>\
          ');
          if #lan_intfs> 1 and (not isBridgedMode) then
            ngx.print('\
          </div>\
          ');
          end
          ngx.print('\
          <input type="hidden" name="isAdvanced" id="isAdvanced" value="0">\
        </form>\
   </div>\
<script type="text/javascript">\
function rand(max){return Math.floor(Math.random()*max);}\
function rand16() {return rand(2**16).toString(16);}\
 $(function() {\
    var gen_ula_span = document.createElement("SPAN");\
    gen_ula_span.setAttribute("id","random_ula_prefix");\
    gen_ula_span.setAttribute("class","btn icon-random");\
    gen_ula_span.setAttribute("style","padding:5px 3px 8px 3px;");\
    gen_ula_span.setAttribute("title","Click to generate a random ULA prefix");\
    $("#ula_prefix").after(gen_ula_span);\
    $("#random_ula_prefix").click(function(){var i=$("#ula_prefix");i.val((parseInt("fd00",16)+rand(2**8)).toString(16)+":"+rand16()+":"+rand16()+"::/48");var e=jQuery.Event("keydown");e.which=e.keyCode=13;i.trigger(e);});\
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
$("#sleases>tbody>tr:not(.line-edit)>td:nth-child(1)").each(function(){\
  $(this).text(function(i,content){\
    return content.split(" ")[0];\
  });\
});\
'); if dhcpDefaultExist then ngx.print('\
$("#btn-dhcp-reset").click(function(){\
  '); for k,v in pairs(dhcpDefaultExist) do ngx.print('\
    $("#'); ngx.print(k); ngx.print('").val("'); ngx.print(v); ngx.print('");\
  '); end ngx.print('\
  $("#localIPv6").prev().removeClass("switcherOn").closest(".switch").removeClass("switchOn");\
  $("#dhcpState").prev().addClass("switcherOn").closest(".switch").addClass("switchOn");\
  $("#modal-changes").show();\
  $("#modal-no-change").hide();\
});\
'); end ngx.print('\
  '); if inUseFlag == true then ngx.print('\
  $("#confilictipchange-msg").show();\
  '); end ngx.print('\
</script>\
',ui_helper.createFooter(),'\
<script src="/js/srp-min.js"></script>\
<script src="/js/bridgedmode.js"></script>\
<script>\
(function() {\
  $(\'input[name="localdevIP"]\').keydown(function(){\
    var msg = $("#lanipchange-msg");\
    var msg_dst = $(this);\
    msg_dst.after(msg);\
    msg.removeClass("hide");\
  });\
}());\
</script>\
');