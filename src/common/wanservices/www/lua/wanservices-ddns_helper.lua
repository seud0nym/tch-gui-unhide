local proxy = require("datamodel")
local find,format,gmatch,gsub,match,upper = string.find,string.format,string.gmatch,string.gsub,string.match,string.upper
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local function get_services(name)
  -- open the supported services file that come with the ddns package
  local valid_services = {}
  local path = format("/etc/ddns/%s",name)
  local f = io.open(path,"r")
  if f then
    for line in f:lines() do
      --a service in this file is indicated as a url between quotes, we want a list with urls and name of service in capitals
      local service,url = match(line,'^(%b"")%s*(%b"")')
      if service then
        service = gsub(service,'"','')
        valid_services[#valid_services + 1] = { service,upper(service),url }
      end
    end
    f:close()
  end
  return valid_services
end

local function merge_services(ipv4,ipv6)
  local hash = {}
  local services = {}
  for k=1,#ipv4 do
    local service = ipv4[k][1]
    if not hash[service] then
      hash[service] = true
      services[#services+1] = { service,ipv4[k][2] }
    end
  end
  for k=1,#ipv6 do
    local service = ipv6[k][1]
    if not hash[service] then
      hash[service] = true
      services[#services+1] = { service,ipv6[k][2] }
    end
  end
  table.sort(services,function (a,b)
    return a[1] < b[1]
  end)
  return services
end

local M = {}

M.ddns_state_map = {
  disabled = T"Disabled",
  updating = T"Updating",
  updated = T"Updated",
  error = T"ERROR",
}

M.ddns_light_map = {
  disabled = "0",--"off",
  updating = "2",--"orange",
  updated = "1",--"green",
  error = "4",--"red",
}

M.intf_map = {
  { "wan", T"IPv4" },
}

M.unit_map = {
  { "seconds", T"seconds" },
  { "minutes", T"minutes" },
  { "hours", T"hours" },
  { "days", T"days" },
}

M.valid_ipv4_services = get_services("services")
M.valid_ipv6_services = get_services("services_ipv6")
M.valid_services = merge_services(M.valid_ipv4_services,M.valid_ipv6_services)

M.wan6 = false

local intfs_pn = proxy.getPN("uci.network.interface.",true)
if intfs_pn then
  for _,v in ipairs(intfs_pn) do
    local intf_name = match(untaint(v.path),"@([^@%.]-)%.")
    if intf_name and intf_name ~= "loopback" and intf_name ~= "lan" then
      local enabled = proxy.get(v.path.."enabled")[1].value
      if enabled ~= "0" then
        if intf_name == "wan6" then
          M.wan6 = true
          M.intf_map[#M.intf_map+1] = { "wan6",T"IPv6" }
          break
        end
      end
    end
  end
end

function M.get_services_status()
  local status = {}
  local result = proxy.get("rpc.ddns.status")
  if result then
    local all = untaint(result[1].value)
    for v in gmatch(all,'([^%]]+)') do 
      local paramindex,msg = match(v,'(.+)%[(.+)')
      status[paramindex] = msg
    end
  end
  return status
end

function M.to_status(services_status,paramindex)
  local ddns_message = "No error"
  local ddns_status = "disabled"
  local rpc_ddns_status = services_status[paramindex]
  if rpc_ddns_status then
    ddns_message = rpc_ddns_status
    if rpc_ddns_status == "Domain's IP updated" then
      ddns_status = "updated"
    elseif rpc_ddns_status == "No error received from server" then
      ddns_status = "updating"
    else
      ddns_status = "error"
    end
  end
  if ddns_status == "disabled" then
    ddns_message = T""
  end
  return ddns_status,ddns_message
end

function M.validate_username(value,object)
  local services = object.Interface == "wan" and M.valid_ipv4_services or M.valid_ipv6_services
  local service = untaint(object.Service)
  local url
  for k=1,#services do
    if service == services[k][1] then
      url = services[k][3]
      break
    end
  end
  if not url or find(url,"%[USERNAME%]") then
    local username = untaint(value)
    if not username or #username == 0 then
      return nil,"Username is required"
    end
  end
  return true
end

return M