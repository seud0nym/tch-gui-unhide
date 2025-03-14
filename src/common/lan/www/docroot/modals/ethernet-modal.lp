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
local proxy_helper = require("proxy_helper")
local ui_helper = require("web.ui_helper")
local vlan_helper = require("ethernet-vlan_helper")

local ngx = ngx
local session = ngx.ctx.session
local string = string
local find,format,gmatch,gsub,match,sub = string.find,string.format,string.gmatch,string.gsub,string.match,string.sub
local tonumber,tostring = tonumber,tostring
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local post_data
if ngx.var.request_method == "POST" then
  post_data = ngx.req.get_post_args(0)
end

if post_data and post_data.del_intf then
  if post_data.del_intf == "lan" then
    message_helper.pushMessage(T"Cannot remove the 'lan' LAN Interface!","error")
  else
    local errors,partial = require("ethernet-del-interface_helper").delete_interface(untaint(post_data.del_intf))
    if errors == 0 then
      message_helper.pushMessage(T(format("Successfully removed LAN Interface '%s'",post_data.del_intf)),"success")
    elseif partial then
      message_helper.pushMessage(T(format("Partially removed LAN Interface '%s' - Manually review network, dhcp and firewall configuration files!",post_data.del_intf)),"error")
    end
    ngx.print("<div class='loading-wrapper'><img src='/img/spinner.gif'/></div><script>tch.loadModal('/modals/ethernet-modal.lp?intf=lan');</script>")
    ngx.exit(ngx.HTTP_OK)
  end
end

local isBridgedMode = bridged.isBridgedMode()

local cur_intf,cur_dhcp_intf,all_intfs,lan_intfs,ppp_dev,ppp_intf,dnsmasq_path,wireless_radio = ethernet_helper.get_interfaces()
local switch_vlan,valid_vlan_ifnames,sys_vlans = vlan_helper.get_switch_vlans(isBridgedMode)
local local_dev_ip = post_data and post_data["localdevIP"] or proxy.get("rpc.network.interface.@"..cur_intf..".ipaddr")[1].value
local wanport = vlan_helper.wanport
local cur_intf_local_ipv6,cur_intf_local_ipv6_dashed = ethernet_helper.get_local_ipv6(cur_intf)

--We need to update IPv6 state only when user want to update the IPv6 state alone.
if post_data and post_data.iPv6StateOnlyChanged == "yes" then
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

local ipv42num = post_helper.ipv42num
local vB = post_helper.validateBoolean
local gOV = post_helper.getOptionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local gVNIR = post_helper.getValidateNumberInRange
local vNES = post_helper.validateNonEmptyString
local vSIIP4 = post_helper.validateStringIsIPv4
local vSIIP6 = post_helper.validateStringIsIPv6

local dnsIndex = 0
local ntpIndex = 0
local dns_v4_servers = {""}
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
  local dns_v6_pri = value == "" and "" or gsub(untaint(value),"-",":")
  if dns_v6_pri ~= "" and not vSIIP6(dns_v6_pri) then
    if mapParams[key] ~= nil then
      proxy.del(mapParams[key]:sub(1,-6))
      mapParams[key] = nil
    end
    return nil,"Invalid DNS server"
  end
  if key == "dns_v6_sec" then
    return true
  end

  if dns_v6_pri == "" and object["dns_v6_sec"] ~= "" then
    return nil,"Primary cannot be empty"
  end

  if dns_v6_pri == object["dns_v6_sec"] and dns_v6_pri ~= "" then
    return nil,"Primary and secondary DNS servers cannot be same"
  end

  if dns_v6_pri == "" then -- Localhost as DNS server
    if object["dns_v6_sec"] == "" then
     -- only localhost
      if mapParams["dns_v6_sec"] ~= nil then
        proxy.del(mapParams["dns_v6_sec"]:sub(1,-6))
        mapParams["dns_v6_sec"] = nil
      end
    end
    dns_v6_pri = cur_intf_local_ipv6
  end

  if dns_v6_pri ~= nil and dns_v6_pri ~= "" and mapParams["dns_v6_pri"] == nil then
    local index = proxy_helper.add(baseDHCPDNS)
    if index == "1" then
      mapParams["dns_v6_pri"] = baseDHCPDNS.."@1.value"
    end
  end
  if object["dns_v6_sec"] ~= nil and object["dns_v6_sec"] ~= "" and mapParams["dns_v6_sec"] == nil then
    local index = proxy_helper.add(baseDHCPDNS)
    if index == "2" then
      mapParams["dns_v6_sec"] = baseDHCPDNS.."@2.value"
    end
  end

  object[key] = dns_v6_pri
  return true
end

local function validateDNSv4(value,object,key)
  if value ~= "" and not vSIIP4(value) then
    return nil,"Invalid DNS server"
  end

  -- handle IPv4 dns setting
  dns_v4_servers[2] = value
  dns_v4_servers[3] = post_data.ipv4_dns_sec

  if value == "" and post_data.ipv4_dns_sec ~= "" then
    return nil,"Primary cannot not be empty"
  end

  if value == post_data.ipv4_dns_sec and value ~= "" then
    return nil,"Primary and secondary DNS servers cannot be same"
  end

  local dns_str = ""
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
      local index = proxy_helper.add(baseDHCPOptions)
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
    for k=1,#lanports do
      local v = lanports[k]
      local ifname = untaint(v)
      local base = match(ifname,"(eth[0-4])")
      if base ~= ifname then
        if not valid_vlan_ifnames[ifname] then
          message_helper.pushMessage("Unknown VLAN interface: "..ifname,"error")
          return nil,"Unknown VLAN interface: "..ifname
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
        ntpIndex = proxy_helper.add(baseDHCPOptions)
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
  dhcpLimit = ethernet_helper.validateDHCPLimit(cur_intf,local_dev_ip),
  dhcplog = gOV(vB),
  dhcpquiet = gOV(vB),
  dhcplogv6 = gOV(gVNIR(4,7)),
  dhcpStart = ethernet_helper.validateDHCPStart(cur_intf,local_dev_ip),
  dhcpv4State = ethernet_helper.validateDHCPState(),
  dhcpv6State = ethernet_helper.validateDHCPState(),
  dns_v4_pri = validateDNSv4,
  dns_v4_sec = gOV(vSIIP4),
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

local dhcp = {}
local dhcp_dns = {}
-- Check if there is a custom DNS server, if so, then retrieve its index
-- we'll update the parameters list to include it for retrieval
content_helper.addListContent(dhcp,{ options = baseDHCPOptions })
for i=1,#dhcp.options do
  local v = dhcp.options[i]
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

local content,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not helpmsg then helpmsg = {} end
--region Fix ethernet and QoS
if not next(helpmsg) then
  local ifnames = split(content.ifnames," ")
  local wan_in_lan_bridge = false
  for i=#ifnames,1,-1 do
    if not find(ifnames[i],"^wl") then
      proxy.set({
        ["uci.ethernet.port.@"..ifnames[i]..".wan"] = "0",
        ["uci.qos.device.@"..ifnames[i]..".classgroup"] = "TO_LAN",
      })
      if ifnames[i] == wanport then
        wan_in_lan_bridge = true
      end
    end
  end
  if not wan_in_lan_bridge then
    proxy.set({
      ["uci.ethernet.port.@"..wanport..".wan"] = "1",
      ["uci.qos.device.@"..wanport..".classgroup"] = "TO_WAN",
    })
  end
end
--endregion Fix ethernet and QoS
--region Set defaults
if not content.auto or content.auto == "" then
  content.auto = "1"
end
if not content.dhcpIgnore or content.dhcpIgnore == "" then
  content.dhcpIgnore = "0"
end
if not content.dhcpv4State or content.dhcpv4State == "" then
  content.dhcpv4State = "server"
end
if not content.localIPv6 or content.localIPv6 == "" then
  content.localIPv6 = "1"
end
if not content.dhcplog or content.dhcplog == "" then
  content.dhcplog = "0"
end
if not content.dhcpquiet or content.dhcpquiet == "" then
  content.dhcpquiet = "0"
end
if not content.dhcplogv6 or content.dhcplogv6 == "" then
  content.dhcplogv6 = "6"
elseif (tonumber(content.dhcplogv6) or 0) < 4 then
  content.dhcplogv6 = "4"
end
--endregion Set defaults

if post_data then
  if isBridgedMode and cur_intf == "lan" then
    local proto = session:retrieve("old_proto")
    session:store("old_proto",(content.proto))
    if content.proto ~= proto and not next(helpmsg) then
      local cur_ip
      repeat
        ngx.sleep(5)
        cur_ip = proxy.get("rpc.network.interface.@lan.ipaddr","rpc.network.interface.@lan.ipmask")
        content.localdevIP = untaint(cur_ip[1].value)
        content.localdevmask = bridged.maskCIDRToDottedDecimal(cur_ip[2].value)
      until content.localdevIP ~= ""
      if content.proto == "dhcp" then
        proxy_helper.set("uci.network.interface.@lan.reqopts","1 3 6 43 51 58 59")
      elseif content.proto == "static" then
        local uci_ipaddr = proxy.get("uci.network.interface.@lan.ipaddr")[1].value
        if uci_ipaddr == "" then
          proxy_helper.set("uci.network.interface.@lan.ipaddr",content.localdevIP)
        else
          content.localdevIP = untaint(uci_ipaddr)
        end
        local uci_mask = proxy.get("uci.network.interface.@lan.netmask")[1].value
        if uci_mask == "" then
          content.localdevmask = uci_mask
          proxy_helper.set("uci.network.interface.@lan.netmask",content.localdevmask)
        else
          content.localdevmask = untaint(uci_mask)
        end
      end
      proxy.apply()
    end
  end

  local gwIP = session:retrieve("old_ip")
  session:store("old_ip",(content.localdevIP))
  if cur_intf == "lan" and content.localdevIP ~= gwIP and not next(helpmsg) then
    ngx.print('\
    <script type="text/javascript">\
      window.location="http://',content.localdevIP,'";\
    </script>\
    ')
    session:logout()
  end

  if not next(helpmsg) then
    local dhcp_options_path = "uci.dhcp.dhcp.@"..cur_dhcp_intf..".dhcp_option."
    local dhcp_options = content_helper.convertResultToObject(dhcp_options_path,proxy.get(dhcp_options_path))
    for i=1,#dhcp_options do
      local v = dhcp_options[i]
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
  session:store("old_ip",(content.localdevIP))
  if content.localdomain then
    session:store("old_domain",(content.localdomain))
  end
  if isBridgedMode then
    session:store("old_proto",(content.proto))
  end
end

--region DNS
if #dns_v4_servers == 1 and content.dns_v4_pri ~= nil and content.dns_v4_pri ~= "" then
  dns_v4_servers = split(format("%s",content.dns_v4_pri),",")
end
if dns_v4_servers[2] ~= nil and dns_v4_servers[2] ~= "" and not ethernet_helper.find_dns(dns_v4_servers[2],dns_list[1]) then
  dns_list[1][#dns_list[1]+1]={dns_v4_servers[2],T("Custom ("..dns_v4_servers[2]..")")}
end
if dns_v4_servers[3] ~= nil and dns_v4_servers[3] ~= "" and not ethernet_helper.find_dns(dns_v4_servers[3],dns_list[1]) then
  dns_list[1][#dns_list[1]+1]={dns_v4_servers[3],T("Custom ("..dns_v4_servers[3]..")")}
end

content.dns_v4_pri = content.localdevIP
content.dns_v4_sec = ""
if dns_v4_servers[2] ~= nil and ethernet_helper.find_dns(dns_v4_servers[2],dns_list[1]) then
  content.dns_v4_pri = dns_v4_servers[2]
end
if dns_v4_servers[3] ~= nil and ethernet_helper.find_dns(dns_v4_servers[3],dns_list[1]) then
  content.dns_v4_sec = dns_v4_servers[3]
end

if content.dns_v6_pri ~= nil and content.dns_v6_pri ~= "" then
  if content.dns_v6_pri == cur_intf_local_ipv6 then
    content.dns_v6_pri = ""
  end
  content.dns_v6_pri = gsub(content.dns_v6_pri,":","-")
else
  content.dns_v6_pri = ""
end
if content.dns_v6_pri ~= nil and content.dns_v6_pri ~= "" and content.dns_v4_pri ~= cur_intf_local_ipv6_dashed and not ethernet_helper.find_dns(content.dns_v6_pri,dns_list[2]) then
  local customipv6dns = gsub(content.dns_v6_pri,"-",":")
  dns_list[2][#dns_list[2]+1]={ content.dns_v6_pri,T("Custom ("..customipv6dns..")")}
end

if content.dns_v6_sec ~= nil and content.dns_v6_sec ~= "" then
  if content.dns_v6_sec == cur_intf_local_ipv6 then
    content.dns_v6_sec = ""
  end
  content.dns_v6_sec = gsub(content.dns_v6_sec,":","-")
else
  content.dns_v6_sec = ""
end
if content.dns_v6_sec ~= nil and content.dns_v6_sec ~= "" and content.dns_v4_sec ~= cur_intf_local_ipv6_dashed and not ethernet_helper.find_dns(content.dns_v6_sec,dns_list[2]) then
  local customipv6dns = gsub(content.dns_v6_sec,"-",":")
  dns_list[2][#dns_list[2]+1]={ content.dns_v6_sec,T("Custom ("..customipv6dns..")")}
end
--endregion DNS

--region DHCP
-- because we do the localdevmask validation in localdevIP validation, we check if local ip starts with error msg "[netmask]".
-- If so move it to localdevmask
if helpmsg.localdevIP then
  local msg = match(helpmsg.localdevIP,'^%[netmask%]%s(.*)')
  if msg then
    helpmsg.localdevIP = nil
    helpmsg.localdevmask = msg
  end
end
if helpmsg.dhcpLimit then
    local msg = match(helpmsg.dhcpLimit,'^%[dhcpStart%]%s(.*)')
    if msg then
      helpmsg.dhcpLimit = nil
      helpmsg.dhcpStart = msg
    end
end

local ipstart,ipend,network
if content.leaseTime then
  if not match(content.leaseTime,"^0*([1-9]%d*[smhdw])$") then
    content.leaseTime = content.leaseTime.."s"
  end
  local netmask = ipv42num(content.localdevmask)
  local baseip = ipv42num(content.localdevIP)
  local start = tonumber(content.dhcpStart)
  local numips = tonumber(content.dhcpLimit)
  if start and numips and not helpmsg.localdevIP and not helpmsg.localdevmask and not helpmsg.dhcpStart and not helpmsg.dhcpLimit then
    ipstart,ipend,network = ethernet_helper.calculateDHCPStartAddress(baseip,netmask,start,numips)
    if ipstart then
      session:store("dhcpStartAddr",ipstart)
      session:store("dhcpEndAddr",ipend)
      session:store("networkAddr",network)
    end
  else
    ipstart = post_data.dhcpStartAddress
    ipend = post_data.dhcpEndAddress
    network = session:retrieve("networkAddr")
  end
  if helpmsg.leaseTime then
    content.leaseTime = post_data.leaseTime
  end
end

local adguard = false
if content.dhcpv4State == "disabled" and content.dhcpv6State == "disabled" and content.slaacState == "disabled" then
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
        --region Interfaces menu
        for k=1,#lan_intfs do
          local v = lan_intfs[k]
          if v.index ~= "loopback" and not (isBridgedMode and sub(v.index,-1) == "6") then
            local url = "modals/ethernet-modal.lp?intf="..v.index
            local active = v.index == cur_intf and "active" or ""
            ngx.print(format('<li class="%s"><a id="%s" href="#" data-remote="%s">',active,v.index,url))
            ngx.print(ui_helper.createSimpleLight(v.up,v.name,{span={style="display:inline-block;"}}))
            if v.index ~= "lan" and not match(v.index,"^Guest") and v.index ~= "fonopen" then
              ngx.print(format('<i title="Remove interface %s" id="remove-interface-%s" class="icon-remove-sign" style="float:right;cursor:pointer;"></i>',v.name,v.index))
            end
            ngx.print('</a></li>')
          end
        end
        --endregion Interfaces menu
        ngx.print('\
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
            ngx.print(ui_helper.createSimpleInputHidden("auto",content.auto))
          else
            ngx.print(ui_helper.createSwitch(T"IPv4 Enabled on Boot","auto",content.auto,switch_class_enable))
          end
          ngx.print([[<div class="monitor-auto monitor-1 monitor-hidden-auto">]])
          if isBridgedMode then
            local cur_ip = proxy.get("rpc.network.interface.@"..cur_intf..".ipaddr","rpc.network.interface.@"..cur_intf..".ipmask","rpc.network.interface.@"..cur_intf..".nexthop")
            --region attributes
            local proto_attr = {
              input = {
                class = "monitor-changes",
              },
            }
            --endregion attributes
            ngx.print(ui_helper.createInputRadio(T"IPv4 Address Source","proto",ethernet_helper.proto,content.proto,proto_attr,helpmsg.proto))
            if cur_ip and cur_ip[2].value ~= "" then
              if content.proto == "dhcp" then
                content.localdevIP = cur_ip[1].value
                content.localdevmask = bridged.maskCIDRToDottedDecimal(cur_ip[2].value)
                content.localgw = cur_ip[3].value
              end
              ngx.print(
                ui_helper.createLabel(T"Device IPv4 Address",cur_ip[1].value,dhcp_attr),
                ui_helper.createLabel(T"Local Network Subnet",bridged.maskCIDRToDottedDecimal(cur_ip[2].value),dhcp_attr),
                ui_helper.createLabel(T"Local Gateway",cur_ip[3].value,dhcp_attr)
              )
            end
          else
            ngx.print(ui_helper.createSimpleInputHidden("proto",content.proto))
          end
          ngx.print(
            ui_helper.createInputText(T"Device IPv4 Address","localdevIP",content.localdevIP,static_attr,helpmsg.localdevIP),
            ui_helper.createInputText(T"Local Network Subnet","localdevmask",content.localdevmask,static_attr,helpmsg.localdevmask)
          )
          if isBridgedMode then
            ngx.print(ui_helper.createInputText(T"Local Gateway","localgw",content.localgw,static_attr,helpmsg.localgw))
          else
            ngx.print(ui_helper.createInputText(T"Local Domain Name","localdomain",content.localdomain,advanced,helpmsg.localdomain))
          end
          ngx.print([[</div>]])

          ngx.print(ui_helper.createSwitch(T"IPv6 Enabled on Boot","localIPv6",content.localIPv6,switch_class_enable))
          if content.localIPv6 ~= "0" then
            local ipv6addr = proxy.get("rpc.network.interface.@"..cur_intf..".ip6addr")
            if ipv6addr and ipv6addr[1].value == "" and (cur_intf == "lan" or cur_intf == "Guest") then
              local ip6addr = proxy.get(format("rpc.network.interface.@%s6.ip6addr",cur_intf))
              if ip6addr then
                ipv6addr = ip6addr
              end
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
              ui_helper.createInputSelect(T"IPv6 Prefix Size<span class='icon-question-sign' title='Delegate a prefix of the given length to this interface'></span>","ip6assign",req_prefix_map,content.ip6assign,number_attr,helpmsg.ip6assign),
              ui_helper.createInputText(T"IPv6 Subprefix Hint<span class='icon-question-sign' title='Subprefix ID to be used if available'></span>","ip6hint",content.ip6hint,hex_attr,helpmsg.ip6hint)
            )
          end
          --endregion Global
          --region LAN Ports
          local eth = vlan_helper.get_port_states()
          local vlan_enabled = vlan_helper.get_vlan_enabled()
          local ban_circle = "<td class='red'style='font-family:FontAwesome;text-align:center;'>&#xf05e;</td>"
          local ticked = "<td style='font-family:FontAwesome;text-align:center;'>&#xf046;</td>"
          --region header
          ngx.print(
          '</fieldset>',
          '<fieldset>',
            '<legend style="border-bottom:0px;margin-bottom:0px;">',T"Port Assignment",'</legend>',
            ui_helper.createSimpleInputHidden("ifnames",content.ifnames),'\
          <style>.socket{margin-right:6px !important;}</style>\
          <table id="portmap" class="table table-striped">\
            <tr>\
              <th style="width:50%;text-align:right;vertical-align:bottom;">',(vlan_enabled[1].value == "1" or sys_vlans > 0) and T"VLAN ID" or "",'</th>\
              <th style="width:65px;">',ui_helper.createSwitchPort("1",eth.state_0,eth.speed_0),'</th>\
              <th style="width:65px;">',ui_helper.createSwitchPort("2",eth.state_1,eth.speed_1),'</th>\
              <th style="width:65px;">',ui_helper.createSwitchPort("3",eth.state_2,eth.speed_2),'</th>\
              <th style="width:65px;">',ui_helper.createSwitchPort("4",eth.state_3,eth.speed_3),'</th>\
              <th>',ui_helper.createSwitchPort("WAN",eth.state_4,eth.speed_4,{socket={class="red"}}),'</th>\
              <th style="width:50%;"></th>\
            </tr>')
            --endregion header
            local lastport = 4
            local unused = 0
            local untagged_flag = vlan_helper.get_untagged_flag()
            local used = {eth0={[".count"]=0},eth1={[".count"]=0},eth2={[".count"]=0},eth3={[".count"]=0},eth4={[".count"]=0}}
            local interfaces = proxy.getPN("uci.network.interface.",true)
            for k=1,#interfaces do
              local v = interfaces[k]
              local intf = match(v.path,"@([^%.]+)")
              for ifname in gmatch(untaint(proxy.get(v.path.."ifname")[1].value),"(%S+)") do
                local base = match(ifname,"(eth[0-4])")
                if base then
                  local vlan = match(ifname,"eth[0-4]%.(%d+)$") or "None"
                  used[base][intf] = vlan
                  if valid_vlan_ifnames[ifname] == untagged_flag then
                    used[base][".count"] = used[base][".count"] + 1
                  end
                end
              end
            end
            --region functions
            local ifnames = {} -- table of interface names assigned to the current interface
            for ifname in gmatch(untaint(content.ifnames) or "","(%S+)") do
              ifnames[ifname] = true
            end
            local function add_port_checkbox(ifname)
              ngx.print('<td style="text-align:center;width:65px;"><input type="checkbox" name="lanport" value="',ifname,'" ',ifnames[ifname] and "checked" or "",'></td>')
              return true
            end
            --endregion functions
            local none_vlan_label = vlan_helper.get_none_vlan_label(vlan_enabled[1].value)
            ngx.print('<tr><td style="width:50%;text-align:right;">',none_vlan_label,'</td>')
            for i=0,lastport,1 do
              local base = format("eth%d",i)
              if used[base][cur_intf] == "None" then
                add_port_checkbox(base)
              else
                ngx.print(ban_circle)
              end
            end
            ngx.print('<td style="width:50%;"></td></tr>')
            if vlan_enabled[1].value == "1" or sys_vlans > 0 then
              for k=1,#switch_vlan do
                local v = switch_vlan[k]
                local vlan = untaint(v.vlan)
                ngx.print('<tr><td style="width:50%;text-align:right;">',T(vlan),'</td>')
                for i=0,lastport,1 do
                  local filled = false
                  local base = format("eth%d",i)
                  local vlan_ifname = format("%s.%s",base,vlan)
                  if ifnames[vlan_ifname] then -- Used in the current interface, so just display the checkbox no matter what
                    filled = add_port_checkbox(vlan_ifname)
                  elseif v[base] == "t" then -- port is tagged (so can be multiple)...
                    if not v.type then -- FW 21.4 System VLANs
                      filled = add_port_checkbox(vlan_ifname)
                    elseif (v.type ~= "guest" and cur_intf == "lan") or (v.type == "guest" and cur_intf == "Guest") then
                      ngx.print(ticked)
                      filled = true
                    end
                  elseif v[base] == untagged_flag and used[base][".count"] == 0 then -- port is untagged (and unused)...
                    filled = add_port_checkbox(vlan_ifname)
                  end
                  if not filled then
                    ngx.print(ban_circle)
                  end
                end
                ngx.print('<td style="width:50%;"></td></tr>')
              end
            end
            ngx.print('\
          </table>')
          if cur_intf ~= "lan" and unused > 0 then -- Removed in 20.4 by 165-VLAN
            ngx.print(ui_helper.createAlertBlock(T("You should enable VLANs to assign LAN ports to other interfaces!"),warn_attr))
          end
          --endregion LAN Ports
            ngx.print('\
        </fieldset>\
      </div>\
      <div class="span4">\
        <legend>',T"DHCP Settings",'</legend>\
        <fieldset>');
          --region DHCP
          --region attributes
          local ignore_attr = { input = { class = "monitor-changes", }, }
          local switchDHCP = { values = { on = "server", off = "disabled", }, }
          local switchDHCPv4Log = { values = { on = "0", off = "1", }, }
          --endregion attributes
          ngx.print(
            ui_helper.createSwitch(T"DHCP Disabled","dhcpIgnore",content.dhcpIgnore,ignore_attr),
            [[<div class="monitor-dhcpIgnore monitor-0 monitor-hidden-dhcpIgnore">]], --Ignore
            ui_helper.createSwitch(T"DHCPv4 Server","dhcpv4State",content.dhcpv4State,switchDHCP),
            ui_helper.createLabel(T"Network Address",network or "",advanced),
            ui_helper.createInputText(T"DHCP Start Address","dhcpStartAddress",ipstart or "",advanced,helpmsg.dhcpStart),
            ui_helper.createInputText(T"DHCP End Address","dhcpEndAddress",ipend or "",advanced,helpmsg.dhcpLimit),
            ui_helper.createInputText(T"Lease Time","leaseTime",content.leaseTime,advanced,helpmsg.leaseTime),
            ui_helper.createSwitch(T"Standard DHCP Logging","dhcpquiet",content.dhcpquiet,switchDHCPv4Log),
            ui_helper.createSwitch(T"Detailed DHCP Logging","dhcplog",content.dhcplog)
          )
          local ntpServerEnabled = proxy.get("uci.system.ntp.enable_server")
          if ntpServerEnabled and ntpServerEnabled[1].value == "1" then
            local switchNTP = {
              values = {
                on = "42,"..content.localdevIP,
                off = ""
              }
            }
            ngx.print(ui_helper.createSwitch(T"Device as NTP Server","ntpServer",content.ntpServer,switchNTP))
          else
            ngx.print(ui_helper.createSimpleInputHidden("ntpServer",""))
          end
          ngx.print(ui_helper.createInputSelect(T"IPv4 Primary DNS","dns_v4_pri",dns_list[1],content.dns_v4_pri,stdattributes,helpmsg.dns_v4_pri))
          ngx.print(ui_helper.createInputSelect(T"IPv4 Secondary DNS","ipv4_dns_sec",dns_list[1],content.dns_v4_sec,stdattributes,helpmsg.dns_v4_sec))
          ngx.print(
            [[<div class="monitor-localIPv6 monitor-1 monitor-hidden-localIPv6">]], --IPv6
            ui_helper.createSwitch(T"DHCPv6 Server","dhcpv6State",content.dhcpv6State,switchDHCP),
            ui_helper.createSwitch(T"SLAAC + RA<span class='icon-question-sign' title='IPv6 Stateless Address Auto-Configuration + Router Advertisement'></span>","slaacState",content.slaacState,switchDHCP)
          )
          local select_attr = {
            select = { class = "span2", }
          }
          local dhcpv6LogLevel = {
            {"4",T"Warnings and Errors only"},
            {"5",T"Minimal"},
            {"6",T"Standard"},
            {"7",T"Detailed"},
          }
          ngx.print(ui_helper.createInputSelect(T"DHCPv6 Logging","dhcplogv6",dhcpv6LogLevel,content.dhcplogv6,select_attr,helpmsg.dhcplogv6))
          ngx.print(ui_helper.createInputSelect(T"IPv6 Primary DNS","dns_v6_pri",dns_list[2],content.dns_v6_pri,stdattributes,helpmsg.dns_v4_pri))
          ngx.print(ui_helper.createInputSelect(T"IPv6 Secondary DNS","dns_v6_sec",dns_list[2],content.dns_v6_sec,stdattributes,helpmsg.dns_v6_sec))
          ngx.print([[</div>]]) --IPv6
          ngx.print([[</div>]]) --Ignore
          --endregion DHCP
          ngx.print('\
        </fieldset>\
      </div>\
    </div>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
  <script src="/js/ethernet-modal.js"></script>\
  <script src="/js/srp-min.js"></script>\
  <script src="/js/bridgedmode.js"></script>\
</div>\
',ui_helper.createFooter())