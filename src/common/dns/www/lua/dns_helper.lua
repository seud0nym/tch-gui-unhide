local proxy = require("datamodel")
local post_helper = require("web.post_helper")
local match = string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

M.dnsmasq_path = nil
for _,dnsmidx in pairs(proxy.getPN("uci.dhcp.dnsmasq.",true)) do
  for _,dnsmif in pairs(proxy.get(dnsmidx.path.."interface.")) do
    if dnsmif.value == "lan" then
      M.dnsmasq_path = dnsmidx.path
      break
    end
  end
end

function M.handleDNSTableQuery(columns,options,filter,defaultObject,mapValidation)
  local data,helpmsg = post_helper.handleTableQuery(columns,options,filter,defaultObject,mapValidation)
  for k,v in pairs (data) do
    local domain,ip = M.toDomainAndIP(v[2])
    if domain and ip then
      data[k][1] = domain
      data[k][2] = ip
    end
  end
  return data,helpmsg
end

function M.handleRebindTableQuery(columns,options,filter,defaultObject,mapValidation)
  local data,helpmsg = post_helper.handleTableQuery(columns,options,filter,defaultObject,mapValidation)
  for k,v in pairs (data) do
    local domain = M.toDomainAndIP(v[1])
    if domain then
      data[k][1] = domain
    end
  end
  return data,helpmsg
end

function M.toDomainAndIP(value)
  local v = untaint(value)
  local domain,ip = match(v,"/([^/]+)/(.*)")
  return domain,ip or v
end

return M
