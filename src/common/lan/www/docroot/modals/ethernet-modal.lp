--pretranslated: do not change this file

-- NOTE: All occurences of the string "Device" in this file will be replaced with the variant (e.g. DJA0231)

-- Localization
gettext.textdomain('webui-core')

local bridged = require("bridgedmode_helper")
local content_helper = require("web.content_helper")
local ethernet_helper = require("ethernet_helper")
local lp = require("web.lp")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local vlan_helper = require("ethernet-vlan_helper")

local ngx = ngx
local session = ngx.ctx.session
local ipairs,pairs,string = ipairs,pairs,string
local tonumber,tostring = tonumber,tostring
local find,format,gmatch,gsub,match,sub = string.find,string.format,string.gmatch,string.gsub,string.match,string.sub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

if ngx.var.request_method == "POST" then
  local post_args = ngx.req.get_post_args(0)
  if post_args.del_intf then
    if post_args.del_intf == "lan" then
      message_helper.pushMessage(T"Cannot remove the 'lan' LAN Interface!","error")
    else
      local errors,partial = require("ethernet-del-interface_helper").delete_interface(untaint(post_args.del_intf))
      if errors == 0 then
        message_helper.pushMessage(T(format("Successfully removed LAN Interface '%s'",post_args.del_intf)),"success")
      elseif partial then
        message_helper.pushMessage(T(format("Partially removed LAN Interface '%s' - Manually review network, dhcp and firewall configuration files!",post_args.del_intf)),"error")
      end
      ngx.print("<div class='loading-wrapper'><img src='/img/spinner.gif'/></div><script>tch.loadModal('/modals/ethernet-modal.lp?intf=lan');</script>")
      ngx.exit(ngx.HTTP_OK)
      end
  end
end

local cur_intf,cur_dhcp_intf,all_intfs,lan_intfs,ppp_dev,ppp_intf,dnsmasq_path,wireless_radio = ethernet_helper.get_interfaces()
local devices = vlan_helper.get_device_map()

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

local dns_list = require('dns_servers_helper').all()
local split = require("split").split

local isBridgedMode = bridged.isBridgedMode()
local lan_ula = ethernet_helper.get_lan_ula()

local ipv42num = post_helper.ipv42num
local vB = post_helper.validateBoolean
local gOV = post_helper.getOptionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local gVNIR = post_helper.getValidateNumberInRange
local vNES = post_helper.validateNonEmptyString
local vSIIP4 = post_helper.validateStringIsIPv4
local vSIIP6 = post_helper.validateStringIsIPv6

local local_dev_ip
if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args(0)
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
  ip6assign = "uci.network.interface.@"..cur_intf..".ip6assign",
  ip6hint = "uci.network.interface.@"..cur_intf..".ip6hint",
  proto = "uci.network.interface.@"..cur_intf..".proto",
  ifnames = "uci.network.interface.@"..cur_intf..".ifname",
  type = "uci.network.interface.@"..cur_intf..".type",
  auto = "uci.network.interface.@"..cur_intf..".auto",
}

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

local function validateInterfaceNames(value,object,key)
  local lanports
  if type(object.lanport) == "table" then
    lanports = object.lanport
    table.sort(lanports,function (a,b)
      local abase = match(untaint(a),"(eth[0-4])") or ""
      local bbase = match(untaint(b),"(eth[0-4])") or ""
      return abase > bbase
    end)
  elseif object.lanport ~= "" then
    lanports = { object.lanport }
  end
  local updated_ifnames = ""
  for ifname in gmatch(untaint(value),"(%S+)") do
    if not match(ifname,"(eth[0-4])") then -- keep any non-ethernet interfaces
      updated_ifnames = (updated_ifnames == "" and updated_ifnames or updated_ifnames.." ")..ifname
    end
  end
  if lanports then
    for _,v in pairs(lanports) do
      local ifname = untaint(v)
      local base = match(ifname,"(eth[0-4])")
      if base ~= ifname then
        if not devices[base] or not devices[base][ifname] then
          return nil,"Unknown interface: "..ifname
        end
      end
      updated_ifnames = ifname..(updated_ifnames == "" and updated_ifnames or " "..updated_ifnames)
    end
  end
  if value ~= updated_ifnames then
    local ifnames = split(updated_ifnames," ")
    local type = ""
    for i=#ifnames,1,-1 do
      if find(ifnames[i],"^wl") then
        ifnames[i] = nil
      end
    end
    object[key] = updated_ifnames
    if cur_intf == "lan" or (find(cur_intf,"Guest",nil,true) and #ifnames > 0) or #ifnames > 1 then
      type = "bridge"
    end
    if object.type ~= type then
      object.type = type
    end
  end
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

local function validateType(value)
  return value == "" or value == "bridge"
end

local mapValid = {
  dhcpIgnore = vB,
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
  ip6hint = gOV(ethernet_helper.validateHint),
  leaseTime = ethernet_helper.validateLeaseTime,
  localdomain = vNES,
  localgw = gOV(vSIIP4),
  localdevIP = ethernet_helper.validateGatewayIP(cur_intf,all_intfs,lan_intfs),
  localIPv6 = ethernet_helper.validateIPv6(cur_intf,wireless_radio,ppp_dev,ppp_intf),
  localdevmask = ethernet_helper.validateGatewayMask,
  ntpServer = validateNTP,
  proto = gVIES(ethernet_helper.proto),
  slaacState = ethernet_helper.validateDHCPState(),
  ifnames = validateInterfaceNames,
  type = validateType,
  auto = vB,
}

if isBridgedMode and cur_intf == "lan" then
  for k,_ in pairs(mapParams) do
    if k ~= "localdevIP" and k ~= "localIPv6" and k ~= "localdevmask" and k ~= "localgw" and k ~= "proto" and k ~= "ifnames" and k ~= "type" then
      mapParams[k] = nil
    end
  end
else
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

local content,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not helpmsg then helpmsg = {} end

if not content["auto"] or content["auto"] == "" then
  content["auto"] = "1"
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

if ngx.var.request_method == "POST" then
  if isBridgedMode and cur_intf == "lan" then
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

  if not next(helpmsg) then
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
else
  session:store("old_ip",(content["localdevIP"]))
  if content["localdomain"] then
    session:store("old_domain",(content["localdomain"]))
  end
  if isBridgedMode then
    session:store("old_proto",(content["proto"]))
  end
end

--region DNS
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
--endregion DNS

--region DHCP
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

if not content["dhcpIgnore"] or content["dhcpIgnore"] == "" then
  content["dhcpIgnore"] = "0"
end
if not content["dhcpv4State"] or content["dhcpv4State"] == "" then
  content["dhcpv4State"] = "server"
end
if not content["localIPv6"] or content["localIPv6"] == "" then
  content["localIPv6"] = "1"
end
if not content["dhcplog"] or content["dhcplog"] == "" then
  content["dhcplog"] = "0"
end
if not content["dhcpquiet"] or content["dhcpquiet"] == "" then
  content["dhcpquiet"] = "0"
end
if not content["dhcplogv6"] or content["dhcplogv6"] == "" then
  content["dhcplogv6"] = "6"
elseif (tonumber(content["dhcplogv6"]) or 0) < 4 then
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
--endregion DHCP

--region attributes
local warn_attr = {
  alert = { class = "alert-danger" },
}
--endregion attributes

ngx.print(ui_helper.createHeader(T"Local Network",true,true),'\
<div class="modal-body update">\
  '); lp.setpath("/www/snippets/"); lp.include("tabs-lan.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="',ngx.var.request_uri,'">\
    <div class="span2">\
      <ul class="nav nav-list">\
        <li class="nav-header">',T"LAN Interfaces",'</li>');
        local html = {}
        for _,v in ipairs(lan_intfs) do
          if v.index ~= "loopback" and not (isBridgedMode and v.index == "lan6") then
            local url = "modals/ethernet-modal.lp?intf="..v.index
            local active = v.index == cur_intf and "active" or ""
            html[#html+1] = format('<li class="%s"><a id="%s" href="#" data-remote="%s">',active,v.index,url)
            html[#html+1] = ui_helper.createSimpleLight(v.up,v.name,{span={style="display:inline-block;"}})
            if v.index ~= "lan" and not match(v.index,"^Guest") and v.index ~= "fonopen" then
              html[#html+1] = format('<i title="Remove interface %s" id="remove-interface-%s" class="icon-remove-sign" style="float:right;cursor:pointer;"></i>',v.name,v.index)
            end
            html[#html+1] = '</a></li>'
          end
        end
        ngx.print(html,'\
        <li><a id="add_new_intf" href="#" data-remote="modals/ethernet-add-interface-modal.lp"><i><small>',T"Add...",'</small></i></a></li>\
      </ul>\
    </div>\
    <div class="span9">\
      ',ui_helper.createMessages(message_helper.popMessages()),'\
      <div class="span4">\
        <legend>',T"Global Information",'</legend>\
        <fieldset>',
          ui_helper.createLabel("Interface Type",content.type == "bridge" and "Bridge" or "Single Port"),
          ui_helper.createSimpleInputHidden("type",content.type))
          --region Global
          --region attributes
          local advanced = {
            group = { class = "advanced" },
            input = { class = "span2" },
          }
          local hijacked_label = {
            group = { class = "advanced" },
            span = { style = "white-space:nowrap;" },
          }
          local switch_class_enable = {
            input = { class = "monitor-changes", },
          }
          local stdattributes = {
            group = { class = "advanced", },
            select = { class = "monitor-changes span2" },
          }
          local ipv6_attr = {
            group = { class = "monitor-localIPv6 monitor-1", },
          }
          local dhcp_attr = {
            group = { class = "monitor-proto monitor-dhcp", },
          }
          local static_attr = {
            group = { class = "advanced monitor-proto monitor-static", },
            input = { class = "span2" },
          }
          --endregion attributes
          if cur_intf == "lan" then
            ngx.print(ui_helper.createLabel(T"IPv4 Enabled on Boot",(content.auto == "1" and "On" or "Off"),switch_class_enable))
            ngx.print(ui_helper.createSimpleInputHidden("auto",content["auto"]))
          else
            ngx.print(ui_helper.createSwitch(T"IPv4 Enabled on Boot","auto",content["auto"],switch_class_enable))
          end
          ngx.print([[<div class="monitor-auto monitor-1 monitor-hidden-auto">]])
          if isBridgedMode and cur_intf == "lan" then
            local cur_ip = proxy.get("rpc.network.interface.@lan.ipaddr","rpc.network.interface.@lan.ipmask","rpc.network.interface.@lan.nexthop")
            --region attributes
            local proto_attr = {
              input = {
                class = "monitor-changes",
              },
            }
            --endregion attributes
            ngx.print(ui_helper.createInputRadio(T"IPv4 Address Source","proto",ethernet_helper.proto,content["proto"],proto_attr,helpmsg["proto"]))
            if cur_ip and cur_ip[2].value ~= "" then
              ngx.print(
                ui_helper.createLabel(T"Device IPv4 Address",cur_ip[1].value,dhcp_attr),
                ui_helper.createLabel(T"Local Network Subnet",bridged.maskCIDRToDottedDecimal(cur_ip[2].value),dhcp_attr),
                ui_helper.createLabel(T"Local Gateway",cur_ip[3].value,dhcp_attr)
              )
            end
          else
            ngx.print(ui_helper.createSimpleInputHidden("proto",content["proto"]))
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
          ngx.print([[</div>]])

          ngx.print(ui_helper.createSwitch(T"IPv6 Enabled on Boot","localIPv6",content["localIPv6"],switch_class_enable))
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
            local ipv6prefix = proxy.get("rpc.network.interface.@"..cur_intf..".ip6prefix_assignment")
            if ipv6prefix and ipv6prefix[1].value ~= "" then
              ngx.print(ui_helper.createLabel(T"Device IPv6 Prefix",ipv6prefix[1].value,ipv6_attr))
            end
          end

          if not (isBridgedMode and cur_intf == "lan") then
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
            --region attributes
            local number_attr = {
              group = { class = "monitor-localIPv6 monitor-1 monitor-hidden-localIPv6", },
              select = { style = "width:100px", }
            }
            local hex_attr = {
              group = { class = "monitor-localIPv6 monitor-1 monitor-hidden-localIPv6", },
              input = { style = "width:80px", }
            }
            --endregion attributes
            ngx.print(
              ui_helper.createInputSelect(T"IPv6 Prefix Size<span class='icon-question-sign' title='Delegate a prefix of the given length to this interface'></span>","ip6assign",req_prefix_map,content["ip6assign"],number_attr,helpmsg["ip6assign"]),
              ui_helper.createInputText(T"IPv6 Subprefix Hint<span class='icon-question-sign' title='Subprefix ID to be used if available'></span>","ip6hint",content["ip6hint"],hex_attr,helpmsg["ip6hint"])
            )
          end

          if not (isBridgedMode and cur_intf == "lan") then
            if cur_dhcp_intf == "lan" then
              if dns_int_ipv4 or dns_int_ipv6 then
                if dns_int_ipv4 and not dns_int_ipv4_ok then
                  ngx.print(ui_helper.createAlertBlock(T("DNS Hijacking is enabled, so IPv4 Primary DNS should be set to '"..dns_int_ipv4_dest_ip.."'."),warn_attr))
                end
                if dns_int_ipv6 and not dns_int_ipv6_ok then
                  ngx.print(ui_helper.createAlertBlock(T("DNS Hijacking is enabled, so IPv6 Primary DNS should be set to '"..dns_int_ipv6_dest_ip.."'."),warn_attr))
                end
              end
            end
          end

          --region LAN Ports
          local eth = vlan_helper.get_port_states()
          local vlan_enabled = proxy.get("uci.network.switch.@bcmsw_ext.enable_vlan") or {{value = "0"}}
          local ifnames = {}
          for ifname in gmatch(untaint(content.ifnames) or "","(%S+)") do
            ifnames[ifname] = true
          end
          local function add_port_checkbox(ifname)
            ngx.print('<td style="text-align:center;width:65px;"><input type="checkbox" name="lanport" value="',ifname,'" ',ifnames[ifname] and "checked" or "",'></td>')
          end
          local ban_circle = "<td class='red' style='font-family:FontAwesome;text-align:center;'>&#xf05e;</td>"
          ngx.print(
          '</fieldset>',
          '<fieldset>',
            '<legend style="border-bottom:0px;margin-bottom:0px;">',T"Port Assignment",'</legend>',
            ui_helper.createSimpleInputHidden("ifnames",content.ifnames),'\
          <table id="portmap" class="table table-striped">\
            <tr>\
              <th style="width:50%;text-align:right;vertical-align:bottom;">',vlan_enabled[1].value == "1" and T"VLAN ID" or "",'</th>\
              <th style="width:65px;">',ui_helper.createSwitchPort("1",eth.state_0,eth.speed_0),'</th>\
              <th style="width:65px;">',ui_helper.createSwitchPort("2",eth.state_1,eth.speed_1),'</th>\
              <th style="width:65px;">',ui_helper.createSwitchPort("3",eth.state_2,eth.speed_2),'</th>\
              <th style="width:65px;">',ui_helper.createSwitchPort("4",eth.state_3,eth.speed_3),'</th>')
              if isBridgedMode then
                ngx.print('<th>',ui_helper.createSwitchPort("WAN",eth.state_4,eth.speed_4,{socket={class="red"}}),'</th>')
              end
              ngx.print('\
              <th style="width:50%;"></th>\
            </tr>')
            local unused = 0
            local used = {eth0={[".count"]=0},eth1={[".count"]=0},eth2={[".count"]=0},eth3={[".count"]=0},eth4={[".count"]=0}}
            for _,v in ipairs(proxy.getPN("uci.network.interface.",true)) do
              local intf = match(v.path,"@([^%.]+)")
              for ifname in gmatch(untaint(proxy.get(v.path.."ifname")[1].value),"(%S+)") do
                local base = match(ifname,"(eth[0-4])")
                if base then
                  local vlan = match(ifname,"eth[0-4]_(%d+)$") or -1
                  used[base][".count"] = used[base][".count"] + 1
                  used[base][intf] = vlan
                end
              end
            end
            local lastport = isBridgedMode and 4 or 3
            if vlan_enabled[1].value == "1" then
              local switch_vlan = content_helper.getMatchedContent("uci.network.switch_vlan.",{device="bcmsw_ext"})
              table.sort(switch_vlan,function (a, b)
                return (tonumber(a.vlan) or -1) < (tonumber(b.vlan) or -1)
              end)
              local untagged = {}
              for _,v in ipairs(switch_vlan) do
                for i,state in gmatch(untaint(v.ports),"(%d)(%S*)") do
                  if state ~= "t" then
                    untagged[format("eth%d",i)] = untaint(v.vlan)
                  end
                end
              end
              for _,v in ipairs(switch_vlan) do
                local vlan = untaint(v.vlan)
                local ports = untaint(v.ports)
                ngx.print('<tr><td style="width:50%;text-align:right;">',T(vlan),'</td>')
                for i=0,lastport,1 do
                  local base = format("eth%d",i)
                  local vlan_ifname
                  for k,vid in pairs(devices[base] or {}) do
                    if vid == vlan then
                      vlan_ifname = k
                      break
                    end
                  end
                  if vlan_ifname then
                    local at = find(ports,tostring(i),nil,true)
                    if at then -- this port is used in this vlan
                      if sub(ports,at+1,at+1) == "t" then -- port is tagged, so can be multiple
                        add_port_checkbox(vlan_ifname)
                      elseif untagged[base] == vlan then -- port is untagged...
                        if used[base][cur_intf] == vlan then -- and used in the current interface
                          add_port_checkbox(vlan_ifname)
                        else
                          local untagged_used = false
                          for _,vid in pairs(used[base]) do
                            if vid == vlan then
                              untagged_used = true -- untagged port is used in another interface
                              break
                            end
                          end
                          if untagged_used then
                            ngx.print(ban_circle)
                          else
                            add_port_checkbox(vlan_ifname)
                          end
                        end
                      else
                        ngx.print(ban_circle)
                      end
                    else
                      ngx.print(ban_circle)
                    end
                  else
                    ngx.print(ban_circle)
                  end
                end
                ngx.print('<td style="width:50%;"></td></tr>')
              end
            else --VLANs disabled
              ngx.print('<tr><td style="width:50%;"></td>')
              for i=0,lastport,1 do
                local base = format("eth%d",i)
                if used[base][".count"] == 0 then
                  unused = unused + 1
                  add_port_checkbox(base)
                elseif used[base][cur_intf] then
                  add_port_checkbox(base)
                else
                  ngx.print(ban_circle)
                end
              end
              ngx.print('<td style="width:50%;"></td></tr>')
            end
            ngx.print('\
          </table>')
          if cur_intf ~= "lan" and unused > 0 then -- Removed in 20.4 by 165-VLAN
            ngx.print(ui_helper.createAlertBlock(T("You should enable VLANs to assign LAN ports to other interfaces!"),warn_attr))
          end
          --endregion LAN Ports
          --endregion Global
            ngx.print('\
        </fieldset>\
      </div>\
      <div class="span4">')
      if isBridgedMode and cur_intf == "lan" then
        local dhcp_warn_attr = {
          alert = {
            class = "monitor-proto monitor-dhcp alert-danger",
            style = "margin-top:58px;",
          },
        }
        ngx.print(ui_helper.createAlertBlock(T"DHCP Assigned IP address must <I>only</I> be used on LAN bridges. Do <B>NOT</B> use on a WAN bridge!",dhcp_warn_attr))
      else
      ngx.print('\
        <legend>',T"DHCP Settings",'</legend>\
        <fieldset>');
          --region DHCP
          if adguard and content["dhcpv4State"] == "disabled" then
            ngx.print(format(T'Configure DHCP through <a style="color:inherit;text-decoration:none;" target="_blank" href="http://%s:8008"><b>AdGuard Home</b></a>',content["localdevIP"]))
          else
            --region attributes
            local ignore_attr = { input = { class = "monitor-changes", }, }
            local switchDHCP = { values = { on = "server", off = "disabled", }, }
            local switchDHCPv4Log = { values = { on = "0", off = "1", }, }
            --endregion attributes
            ngx.print(
              ui_helper.createSwitch(T"DHCP Disabled","dhcpIgnore",content["dhcpIgnore"],ignore_attr),
              [[<div class="monitor-dhcpIgnore monitor-0 monitor-hidden-dhcpIgnore">]], --Ignore
              ui_helper.createSwitch(T"DHCPv4 Server","dhcpv4State",content["dhcpv4State"],switchDHCP),
              ui_helper.createLabel(T"Network Address",network or "",advanced),
              ui_helper.createInputText(T"DHCP Start Address","dhcpStartAddress",ipstart or "",advanced,helpmsg["dhcpStart"]),
              ui_helper.createInputText(T"DHCP End Address","dhcpEndAddress",ipend or "",advanced,helpmsg["dhcpLimit"]),
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
              [[<div class="monitor-localIPv6 monitor-1 monitor-hidden-localIPv6">]], --IPv6
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
            ngx.print([[</div>]]) --IPv6
            ngx.print([[</div>]]) --Ignore
          end
          --endregion DHCP
          ngx.print('\
        </fieldset>')
      end
      ngx.print('\
      </div>\
    </div>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
  <script src="/js/ethernet-modal.js"></script>\
  <script src="/js/srp-min.js"></script>\
  <script src="/js/bridgedmode.js"></script>\
</div>\
',ui_helper.createFooter())