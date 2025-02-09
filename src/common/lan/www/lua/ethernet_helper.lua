local bit = require("bit")
local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local post_helper = require("web.post_helper")
local split = require("split").split
local tostring = tostring
local pairs,string,ipairs,ngx = pairs,string,ipairs,ngx
local find,format,match,sub,tolower = string.find,string.format,string.match,string.sub,string.lower
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local ipv42num = post_helper.ipv42num
local vB = post_helper.validateBoolean
local aIPV = post_helper.advancedIPValidation
local gOV = post_helper.getOptionalValidation
local gVIES = post_helper.getValidateInEnumSelect
local gVNIR = post_helper.getValidateNumberInRange
local vIAS6 = gOV(post_helper.validateIPAndSubnet(6))
local vSIIP = post_helper.validateStringIsIP
local vSILT = post_helper.validateStringIsLeaseTime
local vIP4N = post_helper.validateIPv4Netmask
local gAV = post_helper.getAndValidation

local dhcpStateSelect = {
  {"server"},
  {"disabled"},
}

local non_routable ={
  { ipv42num("10.0.0.0"),ipv42num("10.255.255.255") },
  { ipv42num("172.16.0.0"),ipv42num("172.31.255.255") },
  { ipv42num("192.168.0.0"),ipv42num("192.168.255.255") },
}

local function dhcpValidationNotRequired(cur_intf,local_dev_ip,dhcpIgnore)
  local v = proxy.get("uci.network.interface.@"..cur_intf..".ipaddr")
  return v and (v[1].value ~= local_dev_ip or dhcpIgnore == "1")
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
  { "static","Static IP&nbsp;&nbsp;"},
  { "dhcp","DHCP Assigned IP"}
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
    if v.type ~= "wan" and v.paramindex ~= "ppp" and v.paramindex ~= "ipoe" and v.paramindex ~= "wg0" and v.proto ~= "none" and v.proto ~= "wireguard" and (not find(v["ppp.ll_intf"],"wl") or wireless_radio[untaint(v["ppp.ll_intf"])]) then
      if v.name and v.name ~= "" then
        lan_intfs[#lan_intfs + 1] = {name=v.name,index=v.paramindex,up=v.up}
      else
        lan_intfs[#lan_intfs + 1] = {name=v.paramindex,index=v.paramindex,up=v.up}
      end
    end
    if v.paramindex == getintf then
      cur_intf = v.paramindex
      ppp_dev = v["ppp.ll_dev"]
      ppp_intf = v["ppp.ll_intf"]
    end
  end
  table.sort(lan_intfs,function(a,b)
    if a.name == "lan" then
      return true
    elseif b.name == "lan" then
      return false
    end
    return tolower(a.name) < tolower(b.name)
  end)

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

function M.get_local_ipv6(cur_intf)
  local ula = proxy.get("rpc.network.interface.@"..cur_intf..".ipv6uniquelocaladdr")
  if ula ~= nil and ula[1].value ~= "" then
    return split(format("%s",ula[1].value),"&")[1] or ""
  end
  local lla = proxy.get("rpc.network.interface.@"..cur_intf..".ipv6linklocaladdr")
  if lla == nil or lla[1].value == nil then
    return ""
  end
  return untaint(lla[1].value)
end

function M.validateByPass(_,_,_)
  return true
end

-- Validation is done for the dhcpLimit for the particular subnet
-- If different subnet mask is given other than 255.255.255.0,then the
-- DHCP Range limit has to be calculated from the new subnet and the validation
-- has to be done for the new limit.
function M.validateDHCPLimit(cur_intf,local_dev_ip)
  return function(value,object)
    if dhcpValidationNotRequired(cur_intf,local_dev_ip,object.dhcpIgnore) then
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
function M.validateDHCPStart(cur_intf,local_dev_ip)
  return function(value,object)
    if dhcpValidationNotRequired(cur_intf,local_dev_ip,object.dhcpIgnore) then
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

function M.validateGatewayIP(curintf,all_intfs,lan_intfs)
  -- This function will validate the Modem IP Address and check for
  -- Valid IP Format,Limited Broadcast Address,Public IP Range,Multicast Address Range
  return function(value,object,key)
    if object.proto ~= "static" then
      return true
    end
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

function M.validateGatewayMask(value,object,key)
  if object.proto ~= "static" then
    return true
  end
  return gAV(vIP4N,vSIIP)(value,object,key)
end

function M.validateHint(value,_,_)
  if value ~= "" and (#value > 4 or not match(value,"^[%x]+$")) then
    return nil,T"A hint can only contain 1 to 4 hexadecimal digits."
  end
  return true
end

function M.validateIPv6(curintf,wireless_radio,pppDev,pppIntf)
  local IP6assign_path = "uci.network.interface.@"..curintf..".ip6assign"
  local IP6assign_cache = "uci.network.interface.@"..curintf..".tch_ip6assign"  -- cache variable
  return function(value,_,_)
    local valid,msg = vB(value)
    if valid then
      local ip6assign = proxy.get(IP6assign_path)[1].value -- get current value and store in cache if we are switching state
      if not ip6assign or ip6assign == "" then
        ip6assign = "64"
      end
      local cached_ip6assign = proxy.get(IP6assign_cache)[1].value  -- fetch from cache
      if not cached_ip6assign or cached_ip6assign == "" then
        cached_ip6assign = ip6assign
      end
      local ipv6 = untaint(proxy.get("uci.network.interface.@"..curintf..".ipv6")[1].value) -- get current value in datamodel to know if we're switching state
      if value == "0" then -- disabled
        -- In case we disable IPv6, we must first invalidate the existing prefix so that local devices know not to use IPv6 anymore
        -- Do this here by set the ip6assign pref and only on ipv6 state change
        if ipv6 and ipv6 ~= "0" then -- default is enabled so anything non 0 is currently enabled
          -- set ra to 'disabled' in dhcp config
          proxy.set("uci.dhcp.dhcp.@" .. curintf .. ".ra","disabled")
          -- need to delete ip6assign entry
          proxy.set(IP6assign_cache,ip6assign) -- save current value to cache
          proxy.set(IP6assign_path,"") -- the value will be set back to its current value by process_query
          proxy.apply()
          ngx.sleep(3) -- ugly but need to give it the time to complete
        end
      else -- value == "1" (enabled)
        if ipv6 and ipv6 == "0" then -- currently disabled
          -- enable router advertisements and restore ip6assign value from cache
          proxy.set("uci.dhcp.dhcp.@" .. curintf .. ".ra","server")
          proxy.set(IP6assign_path,cached_ip6assign) -- restore value from cache
          proxy.set(IP6assign_cache,"") -- reset cache
          proxy.apply()
        end
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

function M.validateLeaseTime(value,object,key)
  if value == '-1' then -- included '-1' as a feasible set value as specified in TR 181
    object[key] = "infinite" -- included to ensure uci parameter is set as infinite
    return true
  else
    local isLeaseTime,msg = vSILT(value)
    if isLeaseTime then
      object["leaseTime"] = match(untaint(value),"^0*([1-9]%d*[smhdw]?)$")
      return true
    else
      return nil,msg
    end
  end
end

function M.validateULAPrefix(value,object,key)
  local valid,msg = vIAS6(value,object,key)
  if valid and value ~= "" and (sub(tolower(value),1,2) ~= "fd" or sub(value,-3,-1) ~= "/48") then
    return nil,"ULA Prefix must be within the prefix fd::/7, with a range of /48"
  end
  return valid,msg
end

return M
