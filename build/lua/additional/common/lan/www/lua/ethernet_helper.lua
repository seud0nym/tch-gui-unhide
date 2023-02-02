local bit = require("bit")
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local split = require("split").split
local uinetwork = require("web.uinetwork_helper")
local hosts_ac = uinetwork.getAutocompleteHostsList()
local tostring = tostring
local pairs,string,ipairs,ngx = pairs,string,ipairs,ngx
local find,format,gmatch,gsub,match,toupper = string.find,string.format,string.gmatch,string.gsub,string.match,string.upper
---@diagnostic disable-next-line: undefined-field
local istainted,untaint = string.istainted,string.untaint

local ipv42num = post_helper.ipv42num
local vB = post_helper.validateBoolean
local aIPV = post_helper.advancedIPValidation
local gOV = post_helper.getOptionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local gVNIR = post_helper.getValidateNumberInRange
local sLIPV = post_helper.staticLeaseIPValidation
local vIAS6 = gOV(post_helper.validateIPAndSubnet(6))
local vSIIP = post_helper.validateStringIsIP
local vSILT = post_helper.validateStringIsLeaseTime
local vSIM = post_helper.validateStringIsMAC

local dhcpStateSelect = {
  {"server"},
  {"disabled"},
}

local non_routable ={
  { ipv42num("10.0.0.0"), ipv42num("10.255.255.255") },
  { ipv42num("172.16.0.0"), ipv42num("172.31.255.255") },
  { ipv42num("192.168.0.0"), ipv42num("192.168.255.255") },
}

local function dhcpValidationNotRequired(cur_intf,cur_dhcp_intf,local_dev_ip)
  local v = proxy.get("uci.network.interface.@"..cur_intf..".ipaddr","uci.dhcp.dhcp.@"..cur_dhcp_intf..".ignore")
  return v and (v[1].value ~= local_dev_ip or v[2].value == "1")
end

local function getDHCPData(object)
  -- Check the entered IP is valid IP and convert it to number
  local baseip = vSIIP(object["localdevIP"]) and ipv42num(object["localdevIP"])
  local netmask = vSIIP(object["localdevmask"]) and ipv42num(object["localdevmask"])
  local dhcpstart = vSIIP(object["dhcpStartAddress"]) and ipv42num(object["dhcpStartAddress"])
  local dhcpend = vSIIP(object["dhcpEndAddress"]) and ipv42num(object["dhcpEndAddress"])

  return baseip,netmask,dhcpstart,dhcpend
end

local function isQtnGuestWiFi(wireless_radio,intf)
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
  return false
end

local function num2ipv4(ip)
  local ret = tostring(bit.band(ip,255))
  ip = bit.rshift(ip,8)
  for _=1,3 do
    ret = bit.band(ip,255).."."..ret
    ip = bit.rshift(ip,8)
  end
  return ret
end

local M = {}

M.proto = {
  { "static", "Static IP&nbsp;&nbsp;"},
  { "dhcp", "DHCP Assigned IP"}
}

function M.calculateDHCPStartAddress(baseip,netmask,start,numips)
  if baseip and netmask and start and numips and baseip ~= "" and netmask ~= "" and start ~= "" and numips ~= "" then
    local network = bit.band(baseip,netmask)
    local ipmax = bit.bor(network,bit.bnot(netmask)) - 1
    local ipstart = bit.bor(network,bit.band(start,bit.bnot(netmask)))
    local ipend = ipstart+numips-1
    if ipend > ipmax then
      ipend = ipmax
    end
    return num2ipv4(ipstart),num2ipv4(ipend),num2ipv4(network)
  end
  return nil
end

function M.find_dns(ip,list)
  local dns = untaint(ip)
  for i,v in ipairs(list) do
    if i > 1 and dns == v[1] then
      return true
    end
  end
  return false
end

function M.get_dhcp_tags()
  local tags = { {"",T""},}
  for _,tag in pairs(proxy.getPN("uci.dhcp.tag.",true)) do
    local name = match(tag.path,"uci%.dhcp%.tag%.@([^%.]+)%.")
    tags[#tags+1] = { name,T(name) }
  end
  tags[#tags+1] = { ":custom;",T("(Add Custom Network ID)") }
  return tags
end

function M.get_hosts_mac()
  local hosts_mac = {}
  local unique = {}
  for k,_ in pairs(hosts_ac) do
    local hostname,mac = match(k,"([%w%s%p]+) %[([%x:]+)%]$")
    if not unique[mac] then
      hosts_mac[#hosts_mac+1] = {mac,T(mac.." ["..hostname.."]"),toupper(hostname)}
      unique[mac] = true
    end
  end
  table.sort(hosts_mac,function(k1,k2) return k1[3] < k2[3] end)
  hosts_mac[#hosts_mac+1] = {"custom",T"custom"}
  return hosts_mac
end

function M.get_interfaces()
  local getargs = ngx.req.get_uri_args()
  local getintf = getargs.intf

  local cur_intf = "lan"
  local cur_dhcp_intf = "lan"

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
  local ppp_intf,ppp_dev
  for _,v in ipairs(all_intfs) do
    if v.type == "lan" and v.name ~= "wg0" and v.proto ~= "wireguard" and (not find(v["ppp.ll_intf"],"wl") or wireless_radio[untaint(v["ppp.ll_intf"])]) then
      if v.name and v.name ~= "" then
        lan_intfs[#lan_intfs + 1] = {name = v.name,index = v.paramindex}
      else
        lan_intfs[#lan_intfs + 1] = {name = v.paramindex,index = v.paramindex}
      end
    end
    if v.paramindex == getintf then
      cur_intf = v.paramindex
      ppp_dev = v["ppp.ll_dev"]
      ppp_intf = v["ppp.ll_intf"]
    end
  end

  local dhcp_intfs_path = "uci.dhcp.dhcp."
  local all_dhcp_intfs = content_helper.convertResultToObject(dhcp_intfs_path.."@.",proxy.get(dhcp_intfs_path))
  for _,v in ipairs(all_dhcp_intfs) do
    if v.interface == cur_intf then
      cur_dhcp_intf = v.paramindex
      break
    end
  end

  local dnsmasq_path
  for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.",true)) do
    for _,dnsmif in pairs(proxy.get(dnsmidx.path.."interface.")) do
      if dnsmif.value == cur_intf then
        dnsmasq_path = dnsmidx.path
        break
      end
    end
    if dnsmasq_path then
      break
    end
  end

  return cur_intf,cur_dhcp_intf,all_intfs,lan_intfs,ppp_dev,ppp_intf,dnsmasq_path,wireless_radio
end

function M.get_lan_ula()
  local ula = proxy.get("rpc.network.interface.@lan.ipv6uniquelocaladdr")
  if ula == nil or ula[1].value == nil then
    return ""
  end

  return split(format("%s",ula[1].value),"&")[1] or ""
end

function M.onLeaseChange(index,data)
  if data.sleases_tag == ":custom;" then
    local tag = ""
    if data.sleases_ip == "" or data.sleases_name == "" then
      message_helper.pushMessage(T("Host Name and IP Address required to create custom Network ID"),"error")
    else
      local networkid = "";
      for octet in gmatch(untaint(data.sleases_ip),"%d+") do
        networkid = networkid..format("%X",octet)
      end
      tag = gsub(untaint(data.sleases_name),"%-","_")
      local key,errmsg = proxy.add("uci.dhcp.tag.",tag)
      if errmsg then
        message_helper.pushMessage(T(format("Failed to create Tag %s for custom Network ID: %s",tag,errmsg)),"error")
      else
        local success,errors = proxy.set("uci.dhcp.tag.@"..key..".networkid",networkid)
        if not success then
          for _,err in ipairs(errors) do
            message_helper.pushMessage(T(format("Failed to set %s to '%s': %s (%s)",err.path,networkid,err.errmsg,err.errcode)),"error")
          end
        end
      end
    end
    data.sleases_tag = tag
    local success,errors = proxy.set("uci.dhcp.host.@"..index..".tag",tag)
    if not success then
      for _,err in ipairs(errors) do
        message_helper.pushMessage(T(format("Failed to set %s to '%s': %s (%s)",err.path,tag,err.errmsg,err.errcode)),"error")
      end
    end
    proxy.apply()
  end
end

function M.onTagAdd(index,added)
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
      message_helper.pushMessage(T(format("Failed to add DHCP option to Tag %s: %s",index,errmsg)),"error")
    else
      local success,errors = proxy.set("uci.dhcp.tag.@"..index..".dhcp_option.@"..key..".value",value)
      if not success then
        for _,err in ipairs(errors) do
          message_helper.pushMessage(T(format("Failed to set %s to '%s': %s (%s)",err.path,value,err.errmsg,err.errcode)),"error")
        end
      end
    end
  end
end

function M.onTagModify(index,changed)
  local value = ""
  if changed.tags_dns1 ~= "" then
    value = "6,"..changed.tags_dns1
    if changed.tags_dns2 ~= "" then
      value = value..","..changed.tags_dns2
    end
  end
  ngx.log(ngx.ALERT,format("value=%s",value))
  local existing = proxy.get("uci.dhcp.tag.@"..index..".dhcp_option.@1.")
  local key,errmsg
  if existing then
    ngx.log(ngx.ALERT,format("existing[1].value=%s",untaint(existing[1].value)))
    key = "1"
  else
    ngx.log(ngx.ALERT,"no existing record found - adding")
    key,errmsg = proxy.add("uci.dhcp.tag.@"..index..".dhcp_option.")
    if errmsg then
      message_helper.pushMessage(T(format("Failed to add DHCP option to Tag %s: %s",index,errmsg)),"error")
      return
    end
  end
  ngx.log(ngx.ALERT,format("key=%s",key))
  if not existing or existing[1].value ~= value then
    local success,errors = proxy.set("uci.dhcp.tag.@"..index..".dhcp_option.@"..key..".value",value)
    if success then
      proxy.apply()
    else
      for _,err in ipairs(errors) do
        message_helper.pushMessage(T(format("Failed to set %s to '%s': %s (%s)",err.path,value,err.errmsg,err.errcode)),"error")
      end
    end
  end
end

function M.sort_static_leases(a,b)
  return toupper(a.name or "") < toupper(b.name or "")
end

function M.sort_tags(a,b)
  return toupper(a.paramindex or "") < toupper(b.paramindex or "")
end

function M.validateByPass(_,_,_)
    return true
end

function M.validateDHCPIgnore(value,object,_)
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

-- Validation is done for the dhcpLimit for the particular subnet
-- If different subnet mask is given other than 255.255.255.0,then the
-- DHCP Range limit has to be calculated from the new subnet and the validation
-- has to be done for the new limit.
function M.validateDHCPLimit(cur_intf,cur_dhcp_intf,local_dev_ip)
  return function(value,object)
    if dhcpValidationNotRequired(cur_intf,cur_dhcp_intf,local_dev_ip) then
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
end

-- Validation is done for the DHCP start Address for the particular subnet
-- For different subnets,validation for dhcpStart Address has to be done
-- from the new DHCP Range with respect to the subnet mask & Network Address
function M.validateDHCPStart(cur_intf,cur_dhcp_intf,local_dev_ip)
  return function(value,object)
    if dhcpValidationNotRequired(cur_intf,cur_dhcp_intf,local_dev_ip) then
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
end

function M.validateDHCPState()
  return gVIES(dhcpStateSelect)
end

function M.validateDUID(value,object,_)
  if value ~= "" and (#value < 20 or #value > 28 or not value:match("^[%x]+$")) then
    return nil, T"A Client DUID can only contain 28 hexadecimal digits."
  end
  if value == "" and object["hostid"] ~= "" then
    return nil, T"A Client DUID is required when setting the IPv6 Host ID."
  end
  return true
end

function M.validateGatewayIP(curintf,all_intfs,lan_intfs)
  -- This function will validate the Modem IP Address and check for
  -- Valid IP Format,Limited Broadcast Address,Public IP Range,Multicast Address Range
  return function(value,object,key)
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

    if ip >= non_routable[1][1] and ip <= non_routable[1][2] or
       ip >= non_routable[2][1] and ip <= non_routable[2][2] or
       ip >= non_routable[3][1] and ip <= non_routable[3][2] then
      return true
    else
      return nil,T"Public IP Range should not be used"
    end
  end
end

function M.validateHostID(value,_,_)
  if value ~= "" and (#value > 8 or not value:match("^[%x]+$")) then
    return nil, T"A Host ID can only contain 1 to 8 hexadecimal digits."
  end
  return true
end

function M.validateHint(value,_,_)
  if value ~= "" and (#value > 4 or not value:match("^[%x]+$")) then
    return nil, T"A hint can only contain 1 to 4 hexadecimal digits."
  end
  return true
end

function M.validateIPv6(curintf,wireless_radio,pppDev,pppIntf)
  return function(value,_,_)
    local valid,msg = vB(value)
    if valid then
      local IP6assign = "uci.network.interface.@"..curintf..".ip6assign"
      if value == "0" then
        -- In case we disable IPv6, we must first invalidate the existing prefix so that local devices know not to use IPv6 anymore
        -- Do this here by set the ip6assign pref and only on ipv6 state change
        local ipv6 = proxy.get("uci.network.interface.@"..curintf..".ipv6") -- get current value in datamodel to know if we're switching state
        if ipv6 and untaint(ipv6[1].value) ~= "0" then -- default is enabled so anything non 0 is enabled
          proxy.set(IP6assign,"0") -- the value will be set back to its current value by process_query
          proxy.apply()
          ngx.sleep(3) -- ugly but need to give it the time to complete
        end
      else
        proxy.set(IP6assign,"64")
        proxy.apply()
      end
      if isQtnGuestWiFi(wireless_radio,pppDev) then
        local ucipath = content_helper.getMatchedContent("uci.network.device.",{ifname = pppIntf})
        if ucipath and #ucipath > 0 then
          proxy.set(ucipath[1].path.."ipv6",value)
        end
      end
    end
    return valid,msg
  end
end

function M.validateLeaseTime(value,postdata,key)
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

function M.validateStaticLeaseIP(curintf)
  return function(value,object,_)
    if value == "" then
      return true
    end
    for _,host in pairs(proxy.getPN("uci.dhcp.host.",true)) do
      local existing = proxy.get(host.path.."ip", host.path.."mac")
      if value == existing[1].value and object.sleases_mac ~= existing[2].value then
        return nil,T("IP already in use for MAC "..untaint(existing[2].value))
      end
    end
    local contentdata = {
      localdevIP = "uci.network.interface.@"..curintf..".ipaddr",
      localdevmask = "uci.network.interface.@"..curintf..".netmask",
    }
    content_helper.getExactContent(contentdata)
    return sLIPV(value,contentdata)
  end
end

function M.validateStaticLeaseMAC(value,object,key)
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

function M.validateStaticLeaseName(value,_,_)
  if type(value) ~= "string" and not istainted(value) then
    return nil, T"not a string?"
  end
  if #value == 0 then
    return nil, T"cannot be empty"
  end
  if find(value,"^ReservedStatic") then
    return nil,T"cannot use reserved names as static lease name"
  end
  if #value > 255 then
    return nil, T"too long"
  end
  if not match(value, "^[a-zA-Z0-9%-]+$") then
    return nil, T"contains invalid characters"
  end
  return true
end

function M.validateTagName(value,_,_)
  if not value:match("^[%w]+$") then
    return nil,T"must not be empty and must only contain alphanumeric characters"
  end
  return true
end

function M.validateTags(data)
  local existing = proxy.getPN("uci.dhcp.tag.",true)
ngx.log(ngx.ALERT,format("#data=%d #existing=%d",#data,#existing))
  if #data == #existing+1 then
    local new = data[#data][1]
    ngx.log(ngx.ALERT,format("new=%s",untaint(new)))
    for _,tag in ipairs(existing) do
      local name = match(untaint(tag.path),"^uci%.dhcp%.tag%.@([^%.]+)%.")
      ngx.log(ngx.ALERT,format("new=%s name=%s",untaint(new),name))
      if name == new then
        return nil,{ tags_name = T"A tag with this name already exists" }
      end
    end
  end
  return true
end

function M.validateULAPrefix(value, object, key)
  local valid, msg = vIAS6(value, object, key)
  if valid and value ~= "" and (string.sub(string.lower(value),1,2) ~= "fd" or string.sub(value,-3,-1) ~= "/48") then
    return nil, "ULA Prefix must be within the prefix fd::/7, with a range of /48"
  end
  return valid, msg
end

return M
