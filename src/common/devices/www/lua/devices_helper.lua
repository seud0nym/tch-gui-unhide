local common = require("common_helper")
local dkjson = require('dkjson')
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local splitter = require("split")
local static_helper = require("ethernet-static-leases_helper")

local string,ngx,os = string,ngx,os
local sort = table.sort
local tonumber = tonumber
local find,format,gmatch,gsub,lower,match,sub = string.find,string.format,string.gmatch,string.gsub,string.lower,string.match,string.sub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local this_ip = proxy.get("rpc.network.interface.@lan.ipaddr")[1].value
local base_path = "Device.Services.X_TELSTRA_MultiAP.Agent."
local sta_param = "STA"
if not proxy.getPN(base_path,true) then
  base_path = "Device.WiFi.MultiAP.APDevice."
  sta_param = "AssociatedDevice"
end
local skip_path_length = #base_path + 2

local radios = {}
local values = proxy.getPN("rpc.wireless.radio.", true)
for k=1, #values do
  local v = values[k]
  local radio = match(v.path, "rpc%.wireless%.radio%.@([^%.]+)%.")
  if radio then
    local band = proxy.get(v.path.."band")
    if band then
      radios[radio] = untaint(band[1].value)
    end
  end
end

local function addIfNotExists(addr,list)
  for k=1,#list do
    if list[k] == addr then
      return false
    end
  end
  list[#list+1] = addr
  return true
end

local function asAnchor(v,addr,editing)
  if editing then
    return addr
  elseif v == 4 then
    return format("<a target='blank' href='http://%s/'>%s</a>",addr,addr)
  else
    return format("<a target='blank' href='http://[%s]/'>%s</a>",addr,addr)
  end
end

local function ipv4ToNumber(addr)
  local bits = {match(untaint(addr),"(%d+)%.(%d+)%.(%d+)%.(%d+)")}
  if #bits == 4 then
    return tonumber(format("%03d%03d%03d%03d",bits[1],bits[2],bits[3],bits[4]))
  else
    return 0
  end
end

local function ipv6ToString(addr)
  local a = splitter.first_and_rest(untaint(addr)," ")
  local emptyIdx = 0
  local bits = {}
  for bit in gmatch(a,"(%w*):*") do
    if bit == "" then
      emptyIdx = #bits+1
    end
    bits[#bits+1] = ('0'):rep(4-#bit)..bit
  end
  if emptyIdx > 0 then
    for _ = emptyIdx+1,8,1 do
      bits[emptyIdx] = bits[emptyIdx].."0000"
    end
  end
  local retval = ""
  for i=1,#bits do
    local b = bits[i]
    if retval == "" then
      retval = b
    else
      retval = retval..b
    end
  end
  return retval
end

local function joinAddresses(v,list,editing)
  local joined = ""
  if #list == 1 then
    joined = asAnchor(v,list[1])
  elseif #list > 1 then
    sort(list,function(a,b)
      if v == 4 then
        return ipv4ToNumber(a) < ipv4ToNumber(b)
      else
        return ipv6ToString(a) < ipv6ToString(b)
      end
    end)
    for k=1,#list do
      joined = joined..asAnchor(v,list[k],editing)
      if k < #list then
        joined = joined.."<br>"
      end
    end
  end
  return joined
end

local M = {}

function M.filter(wifi,connected,cache,editing)
  local mac_vendor = M.getMACVendors(cache)
  return function(data)
    if data.L2Interface == "" or (connected == "Active" and data.State == "0") then
      return false
    end
    data.InterfaceType = M.getInterfaceType(wifi,data)
    local ipv4 = splitter.split(untaint(data.IPv4)," ")
    local ipv6 = splitter.split(untaint(data.IPv6)," ")
    if #ipv4 == 0 or #ipv6 == 0 then
      local addresses = splitter.split(untaint(data.IPAddress)," ")
      for k=1, #addresses do
        local address = addresses[k]
        if find(address,":",1,true) then
          addIfNotExists(address,ipv6)
        else
          addIfNotExists(address,ipv4)
        end
      end
    end
    if #ipv4 == 0 or #ipv6 == 0 then
      if data.DhcpLeaseIP ~= "" then
        local dhcpLeaseIP = untaint(data.DhcpLeaseIP)
        local chunks = {match(dhcpLeaseIP,"(%d+)%.(%d+)%.(%d+)%.(%d+)")}
        if (#chunks == 4) then
          addIfNotExists(dhcpLeaseIP,ipv4)
        else
          local addresses = splitter.split(untaint(data.IPAddress)," ")
          for k=1, #addresses do
            addIfNotExists(addresses[k],ipv6)
          end
        end
      end
    end
    local mac = untaint(data.MACAddress)
    data.MACAddress = format("<span class='maccell' id='%s'><span class='macaddress'>%s</span><i class='devextinfo'>%s</i><span>",gsub(mac,":",""),mac,mac_vendor[mac] or "")
    data.ConnectedTime = common.secondsToTime(os.time() - untaint(data.ConnectedTime))
    data.DhcpLeaseTime = common.secondsToTime(untaint(data.LeaseTimeRemaining))
    if editing then
      data.LeaseType = format("<span class='typecell'>%s<span>",untaint(data.LeaseType))
      if #ipv4 == 0 then
        data.IPv4 = ""
      else
        data.IPv4 = format("<span class='ipv4cell'>%s<span>",ipv4[1])
      end
    else
      data.IPv4 = joinAddresses(4,ipv4,editing)
    end
    data.IPv6 = joinAddresses(6,ipv6,editing)
    return true
  end
end

function M.getInterfaceType(wifi,data)
  local std = ""
  if data.OperatingStandard and data.OperatingStandard ~= "" then
    std = "<span class='devextinfo'>802.11"..data.OperatingStandard.."</span>"
  end
  if data.Radio and data.Radio ~= "" and match(data.L2Interface,"^wl") then
    local key = untaint(data.Radio)
    local radio = radios[key]
    if radio then
      return untaint(wifi.prefix.." - "..radio..std)
    else
      return untaint(wifi.prefix.." - "..key..std)
    end
  elseif match(data.L2Interface,"^wl0") then
    return untaint(wifi.prefix.." - 2.4GHz"..std)
  elseif match(data.L2Interface,"^wl1") then
    return untaint(wifi.prefix.." - 5GHz"..std)
  elseif match(data.L2Interface,"^eth") then
    local agentWiFiBand = wifi.agentSTA[untaint(data.MACAddress)]
    if agentWiFiBand then
      return untaint(agentWiFiBand..std)
    else
      if data.Port and data.Port ~= "" then
        return "Ethernet - "..data.Port
      else
        return "Ethernet"
      end
    end
  elseif match(data.L2Interface,"moca*") then
    return "MoCA"
  elseif match(data.L2Interface,"^wds%d+") then
    return wifi.prefix
  else
    return data.L2Interface
  end
end

function M.getWiFi()
  local agentSTA = {}
  local bssid = {}
  local skipPath = {}
  local aliases = {}
  local bands = {}
  local macs = {}
  local remoteIP = {}
  local bandPrefixLength

  local multiap = proxy.get(base_path)
  if multiap then
    for k=#multiap,1,-1 do
      local v = multiap[k]
      local p = untaint(v.path)
      local prefix = sub(p,1,skip_path_length)
      if not skipPath[v.prefix] then
        if v.param == "IPAddress" then
          if sta_param == "AssociatedDevice" and (v.value == "" or v.value == this_ip) then
            skipPath[prefix] = true
          elseif v.value ~= "" then
            remoteIP[untaint(v.value)] = true
          end
        elseif v.param == "Alias" then
          if v.value == "" then
            skipPath[prefix] = true
          end
          aliases[prefix] = untaint(v.value)
        elseif v.param == "OperatingFrequencyBand" then
          bands[p] = v.value
          bandPrefixLength = #p
        elseif v.param == "BSSID2GHZ" then
          bands[untaint(v.value)] = "2.4GHz"
          bandPrefixLength = #p
        elseif v.param == "BSSID5GHZ" then
          bands[untaint(v.value)] = "5GHz"
          bandPrefixLength = #p
        elseif v.param == "BSSID" and find(p,"STA",skip_path_length,true) then
          bssid[p] = untaint(v.value)
        elseif v.param == "MACAddress" and find(p,sta_param,skip_path_length,true) then
          macs[#macs+1] = { mac = untaint(v.value), prefix = prefix, path = p }
        end
      end
    end
  end

  for k =1,#macs do
    local v = macs[k]
    if not skipPath[v.prefix] then
      if aliases[v.prefix] then
        local band = (sta_param == "AssociatedDevice") and bands[sub(v.path,1,bandPrefixLength)] or bands[bssid[v.path]]
        if not band then
          band = "???GHz"
          ngx.log(ngx.ERR,"Could not determine frequency band for ",v.mac," path=",v.path)
        end
        agentSTA[lower(v.mac)] = format("%s - %s",aliases[v.prefix],band)
      else
        agentSTA[lower(v.mac)] = "Unknown?"
      end
    end
  end

  local _,aps = static_helper.get_dhcp_tags()
  if aps > 0 then
    local hosts = proxy.getPN("uci.dhcp.host.",true)
    for k=1,#hosts do
      local p = hosts[k]
      local path = p.path
      local tag = proxy.get(path.."tag")[1].value
      if tag ~= "" then
        local ap = match(untaint(tag),"^AP_([%w_]+)$")
        if ap then
          local ipv4 = proxy.get(path.."ip")
          if ipv4 and ipv4[1].value ~= "" and not remoteIP[untaint(ipv4[1].value)] then
            local cmd = format("curl -qsklm1 --connect-timeout 1 http://%s:59595",ipv4[1].value)
            local curl = io.popen(cmd)
            if curl then
              local json = curl:read("*a")
              local devices = dkjson.decode(json)
              curl:close()
              if devices then
                ap = gsub(ap,"_"," ")
                for i=1,#devices do
                  local v = devices[i]
                  agentSTA[untaint(v.mac)] = format("%s - %s",ap,v.radio)
                end
              end
            else
              ngx.log(ngx.ERR,cmd)
            end
          end
        end
      end
    end
  end

  return {
    agentSTA = agentSTA,
    prefix = next(remoteIP) and "Gateway" or "Wireless"
  }
end

function M.getMACVendors(cache)
  local mac_vendor = {}
  if cache and cache == "clear" then
    proxy.set("rpc.gui.mac.clear_cache","1")
  else
    local file = proxy.get("rpc.gui.mac.cached")
    if file then
      local pattern = "([%x][%x]:[%x][%x]:[%x][%x]:[%x][%x]:[%x][%x]:[%x][%x]) (.*)"
      for line in gmatch(file[1].value,"([^\n]*)\n") do
        local mac,name = match(untaint(line),pattern)
        if mac and name and name ~= "" then
          mac_vendor[mac] = name
        end
      end
    end
  end
  return mac_vendor
end

function M.modify(index,data)
  if index == nil then
    return
  end
  local mac = data.MACAddress
  local name = data.FriendlyName
  local state = data.static
  local existing_lease_path
  local hosts = proxy.getPN("uci.dhcp.host.",true)
  for k=1,#hosts do
    local host = hosts[k]
    local static_mac = proxy.get(host.path.."mac")
    if static_mac and static_mac[1].value == mac then
      existing_lease_path = host.path
      break
    end
  end
  if state == "1" then
    if existing_lease_path then
      message_helper.pushMessage(T(format("Failed to create static lease for %s: Lease already exists?",name)),"error")
      ngx.log(ngx.ERR,format("Failed to create static lease for %s: Lease already exists?",name))
    else
      local key = post_helper.getRandomKey()
      local _,errmsg = proxy.add("uci.dhcp.host.",key)
      if errmsg then
        for i=1,#errmsg do
          local err = errmsg[i]
          message_helper.pushMessage(T(format("Failed to create static lease for %s: %s (%s)",name,err.errmsg,err.errcode)),"error")
          ngx.log(ngx.ERR,format("Failed to create static lease for %s: %s (%s)",name,err.errmsg,err.errcode))
        end
      else
        local ok,err = proxy.set({
          ["uci.dhcp.host.@"..key..".mac"] = mac,
          ["uci.dhcp.host.@"..key..".name"] = name,
          ["uci.dhcp.host.@"..key..".ip"] = data.IPv4,
          ["uci.dhcp.host.@"..key..".dns"] = "1",
        })
        if ok then
          proxy.apply()
        else
          for i=1,#err do
            local e = err[i]
            message_helper.pushMessage(T(format("Failed to correctly create static lease for %s: %s (%s)",name,e.errmsg,e.errcode)),"error")
            ngx.log(ngx.ERR,format("Failed to correctly create static lease for %s: %s (%s)",name,e.errmsg,e.errcode))
          end
          proxy.del("uci.dhcp.host.@"..key..".")
          proxy.apply()
        end
      end
    end
  elseif state == "0" then
    if existing_lease_path then
      local result,error = proxy.del(existing_lease_path)
      if result then
        proxy.apply()
      else
        message_helper.pushMessage(T(format("Failed to remove static lease for %s: %s",name,error),"error"))
        ngx.log(ngx.ERR,format("Failed to remove static lease for %s: %s",name,error),"error")
      end
    end
  end
end

function M.sorter(sortcol,wifi)
  local sortfunc
  if sortcol == "FriendlyName" then
    sortfunc = function(a,b)
      return lower(a.FriendlyName or "") < lower(b.FriendlyName or "")
    end
  elseif sortcol == "IPv4" then
    sortfunc = function(a,b)
      local a4 = (a.IPv4 == "") and a.DhcpLeaseIP or a.IPv4
      local b4 = (b.IPv4 == "") and b.DhcpLeaseIP or b.IPv4
      return ipv4ToNumber(a4) < ipv4ToNumber(b4)
    end
  elseif sortcol == "IPv6" then
    sortfunc = function(a,b)
      return ipv6ToString(a.IPv6) < ipv6ToString(b.IPv6)
    end
  elseif sortcol == "InterfaceType" then
    sortfunc = function(a,b)
      local aIface = M.getInterfaceType(wifi,a)
      local bIface = M.getInterfaceType(wifi,b)
      if aIface == bIface then
        return lower(a.FriendlyName or "") < lower(b.FriendlyName or "")
      else
        return aIface < bIface
      end
    end
  else
    sortfunc = sortcol
  end
  return sortfunc
end

return M
