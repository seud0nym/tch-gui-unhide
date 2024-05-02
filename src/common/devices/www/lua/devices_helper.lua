local common = require("common_helper")
local dkjson = require('dkjson')
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local splitter = require("split")

local string,ngx,os = string,ngx,os
local concat,sort = table.concat,table.sort
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

local function ipSort(a,b)
  return a < b
end

local M = {}

function M.filter(wifi,connected,cache,editing)
  local mac_vendor = M.getMACVendors(cache)
  return function(data)
    if data.L2Interface == "" or (connected == "Active" and data.State == "0") then
      return false
    end
    data.InterfaceType = M.getInterfaceType(wifi,data)
    if not data.IPv4 and not data.IPv6 then
      local addresses = splitter.split(untaint(data.IPAddress)," ")
      for k=1, #addresses do
        local address = addresses[k]
        if find(address,":",1,true) then
          if not data.IPv6 or data.IPv6 == "" then
            data.IPv6 = address
          else
            data.IPv6 = data.IPv6.."<br>"..address
          end
        else
          if not data.IPv4 or data.IPv4 == "" then
            data.IPv4 = address
          else
            data.IPv4 = data.IPv4.."<br>"..address
          end
        end
      end
    end
    if data.IPv4 == "" or data.IPv6 == "" then
      if data.DhcpLeaseIP ~= "" then
        local dhcpLeaseIP = untaint(data.DhcpLeaseIP)
        local chunks = {match(dhcpLeaseIP,"(%d+)%.(%d+)%.(%d+)%.(%d+)")}
        if (#chunks == 4) then
          data.IPv4 = dhcpLeaseIP
        else
          data.IPv6 = dhcpLeaseIP
        end
      end
    end
    if data.IPv6 and find(untaint(data.IPv6)," ") then
      local addresses = splitter.split(untaint(data.IPv6)," ")
      sort(addresses,ipSort)
      data.IPv6 = concat(addresses,"<br>")
    end
    local mac = untaint(data.MACAddress)
    data.MACAddress = format("<span class='maccell' id='%s'><span class='macaddress'>%s</span><i class='devextinfo'>%s</i><span>",gsub(mac,":",""),mac,mac_vendor[mac] or "")
    data.ConnectedTime = common.secondsToTime(os.time() - untaint(data.ConnectedTime))
    data.DhcpLeaseTime = common.secondsToTime(untaint(data.LeaseTimeRemaining))
    if editing then
      data.LeaseType = format("<span class='typecell'>%s<span>",untaint(data.LeaseType))
      if data.IPv4 and data.IPv4 ~= "" then
        local addresses = splitter.split(untaint(data.IPv4),"[<br> ]+")
        sort(addresses,ipSort)
        addresses[1] = format("<span class='ipv4cell'>%s<span>",addresses[1])
        data.IPv4 = concat(addresses,"<br>")
      end
    end
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

  for k =1,#macs do
    local v = macs[k]
    if not skipPath[v.prefix] then
      agentSTA[lower(v.mac)] = format("%s - %s",aliases[v.prefix],((sta_param == "AssociatedDevice") and bands[sub(v.path,1,bandPrefixLength)] or bands[bssid[v.path]]))
    end
  end

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
    local ipv4ToNumber = function(a)
      local ipv4
      if (a.IPv4 == "") then
        ipv4 = a.DhcpLeaseIP
      else
        ipv4 = a.IPv4
      end
      local bits = {ipv4:match("(%d+)%.(%d+)%.(%d+)%.(%d+)")}
      if #bits == 4 then
        return tonumber(format("%03d%03d%03d%03d",bits[1],bits[2],bits[3],bits[4]))
      else
        return 0
      end
    end
    sortfunc = function(a,b)
      return ipv4ToNumber(a) < ipv4ToNumber(b)
    end
  elseif sortcol == "IPv6" then
    local ipv6ToString = function(addr)
      local a = splitter.first_and_rest(untaint(addr)," ")
      local emptyIdx = 0
      local bits = {}
      for bit in string.gmatch(a,"(%w*):*") do
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
