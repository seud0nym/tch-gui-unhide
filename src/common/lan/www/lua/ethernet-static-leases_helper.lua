local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")
local post_helper = require("web.post_helper")
local pairs,string,ipairs,ngx = pairs,string,ipairs,ngx
local find,format,gmatch,gsub,match,tolower,toupper = string.find,string.format,string.gmatch,string.gsub,string.match,string.lower,string.upper
---@diagnostic disable-next-line: undefined-field
local istainted,untaint = string.istainted,string.untaint

local aIPV = post_helper.advancedIPValidation
local sLIPV = post_helper.staticLeaseIPValidation
local vSIM = post_helper.validateStringIsMAC

local M = {}

function M.get_dhcp_tags()
  local tags = { {"",T""},}
  local ap = 0 -- count of access point tags
  for _,tag in pairs(proxy.getPN("uci.dhcp.tag.",true)) do
    local name = match(tag.path,"uci%.dhcp%.tag%.@([^%.]+)%.")
    tags[#tags+1] = { name,T(name) }
    if match(name,"^AP_([%w_]+)$") then
      ap = ap+1
    end
  end
  tags[#tags+1] = { ":custom;",T("(Add Custom Network ID)") }
  return tags,ap
end

function M.get_mac_list(hosts_ac)
  local mac_list = {}
  local hosts_mac = {}
  for k,_ in pairs(hosts_ac) do
    local hostname,mac = match(k,"([%w%s%p]+) %[([%x:]+)%]$")
    if not hosts_mac[mac] then
      local description = T(mac.." ["..hostname.."]")
      mac_list[#mac_list+1] = {mac,description,toupper(hostname)}
      hosts_mac[mac] = description
    end
  end
  table.sort(mac_list,function(k1,k2) return k1[3] < k2[3] end)
  mac_list[#mac_list+1] = {"custom",T"custom"}
  return mac_list,hosts_mac
end

function M.onLeaseChange(index,data)
  if data.sleases_tag == ":custom;" then
    local tag = ""
    if data.sleases_ip == "" or data.sleases_name == "" then
      message_helper.pushMessage(T("Host Name and IP Address required to create custom Network ID"),"error")
    else
      local networkid = "";
      for octet in gmatch(untaint(data.sleases_ip),"%d+") do
        networkid = networkid..format("%02X",octet)
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

function M.sort_tags(a,b)
  return toupper(a.paramindex or "") < toupper(b.paramindex or "")
end

function M.validateDUID(value,object,_)
  if value ~= "" and (#value < 20 or #value > 28 or not match(value,"^[%x]+$")) then
    return nil, T"A Client DUID can only contain 28 hexadecimal digits."
  end
  if value == "" and object["hostid"] ~= "" then
    return nil, T"A Client DUID is required when setting the IPv6 Host ID."
  end
  return true
end

function M.validateHostID(value,_,_)
  if value ~= "" and (#value > 8 or not match(value,"^[%x]+$")) then
    return nil, T"A Host ID can only contain 1 to 8 hexadecimal digits."
  end
  return true
end

function M.validateStaticLeaseIP()
  return function(value,object,_)
    if value == "" then
      return true
    end
    local valid,errmsg = aIPV(value,object)
    if not valid then
      return nil, errmsg
    end
    for _,h in ipairs(proxy.getPN("uci.dhcp.host.",true)) do
      local existing = proxy.get(h.path.."ip",h.path.."mac")
      if value == existing[1].value and object.sleases_mac ~= existing[2].value then
        return nil,T("IP already in use for MAC "..untaint(existing[2].value))
      end
    end
    for _,i in ipairs(proxy.getPN("uci.network.interface.",true)) do
      local type = proxy.get(gsub(i.path,"^uci","rpc").."type")
      if type and type[1].value == "lan" then
        local iface = {
          localdevIP = i.path.."ipaddr",
          localdevmask = i.path.."netmask",
        }
        content_helper.getExactContent(iface)
        if iface.localdevIP ~= "" and iface.netmask ~= "" and iface.localdevIP ~= "127.0.0.1" then
          local result = sLIPV(value,iface)
          if result then
            return result
          end
        end
      end
    end
    return nil,"IP is not in the same network as any LAN or Guest interface"
  end
end

function M.validateStaticLeaseMAC(value,object,key)
  local r1,r2 = vSIM(value)
  if r1 then
    if tolower(value) == "ff:ff:ff:ff:ff:ff" then
      return nil,T"The requested MAC Address can't be the broadcast MAC"
    else
      value = match(value,"^%x%x%-%x%x%-%x%x%-%x%x%-%x%x%-%x%x$") and value:gsub("-",":") or value
      object[key] = tolower(value)
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
  if not match(value,"^[%w_]+$") then
    return nil,T"must not be empty and must only contain alphanumeric characters"
  end
  return true
end

function M.validateTags(data)
  local existing = proxy.getPN("uci.dhcp.tag.",true)
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

return M
