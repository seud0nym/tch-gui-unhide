local proxy = require("datamodel")
local ipairs,pairs,tonumber = ipairs,pairs,tonumber
local tsort = table.sort
local format,toupper = string.format,string.upper
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

function M.getBandwithStatisticsByMAC()
  local hosts,index,download,upload = {},{},{},{}

  local syshosts = proxy.getPN("rpc.hosts.host.",true)
  for _,host in pairs(syshosts) do
    local v = proxy.get(host.path.."FriendlyName",host.path.."MACAddress")
    local name,mac = untaint(v[1].value),untaint(v[2].value)
    if name == "" then
      name = "Unknown_"..mac
    end
    hosts[#hosts+1] = { name,mac,false }
    index[mac] = #hosts
    download[mac] = 0
    upload[mac] = 0
  end

  local stats = proxy.get("rpc.gui.bwstats.device.")
  local device = {}
  local ipv4_rx,ipv6_rx,ipv4_tx,ipv6_tx = "ipv4_rx","ipv6_rx","ipv4_tx","ipv6_tx"
  for _,item in pairs(stats) do
    device[item.param] = untaint(item.value)
    if device["mac"] and device[ipv4_rx] and device[ipv6_rx] and device[ipv4_tx] and device[ipv6_tx] then
      local mac = device["mac"]
      if index[mac] then
        hosts[index[mac]][3] = true
      end
      if download[mac] and upload[mac] then
        download[mac] = download[mac] + (tonumber(device[ipv4_tx]) or 0 ) + (tonumber(device[ipv6_tx]) or 0 )
        upload[mac] = upload[mac] + (tonumber(device[ipv4_rx]) or 0 ) + (tonumber(device[ipv6_rx]) or 0 )
      end
      device = {}
    end
  end

  return download,upload,hosts
end


function M.getBandwithStatistics()
  local download,upload,hosts = M.getBandwithStatisticsByMAC()
  local result = {
    labels = {},
    download = {},
    upload = {},
  }

  tsort(hosts,function (a,b)
    return toupper(a[1]) < toupper(b[1])
  end)

  for _,host in ipairs(hosts) do
    if host[3] == true then
      result["labels"][#result["labels"]+1] = host[1]
      result["download"][#result["download"]+1] = format("%.2f",download[host[2]]/1048576)
      result["upload"][#result["upload"]+1] = format("%.2f",upload[host[2]]/1048576)
    end
  end

  return result["labels"], result["download"], result["upload"]
end

return M