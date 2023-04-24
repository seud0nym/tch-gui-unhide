local proxy = require("datamodel")
local message_helper = require("web.uimessage_helper")
local format = string.format

local M = {}

function M.delete_interface(intf)
  local paths = { "uci.network.interface.@"..intf..".", }
  local intf_ipaddr = proxy.get(paths[#paths].."ipaddr")[1].value

  for _,v in ipairs(proxy.getPN("uci.firewall.rule.",true)) do
    local fw_rule = proxy.get(v.path.."src",v.path.."dest",v.path.."dest_ip")
    if fw_rule[1].value == intf or fw_rule[2].value == intf or fw_rule[3].value == intf_ipaddr then
      paths[#paths+1] = v.path
    end
  end

  for _,v in ipairs(proxy.getPN("uci.firewall.forwarding.",true)) do
    local fw_fwd = proxy.get(v.path.."src")
    if fw_fwd[1].value == intf then
      paths[#paths+1] = v.path
    end
  end

  for _,zone in ipairs(proxy.getPN("uci.firewall.zone.",true)) do
    local nw = proxy.get(zone.path.."network.")
    for _,v in ipairs(nw) do
      if v.value == intf then
        if #nw == 1 then
          paths[#paths+1] = zone.path
        else
          paths[#paths+1] = v.path
        end
      end
    end
  end

  for _,v in ipairs(proxy.getPN("uci.dhcp.dhcp.",true)) do
    local dhcp = proxy.get(v.path.."interface")
    if dhcp[1].value == intf then
      paths[#paths+1] = v.path
    end
  end

  for _,dnsmasq in ipairs(proxy.getPN("uci.dhcp.dnsmasq.",true)) do
    local ifnames = proxy.get(dnsmasq.path.."interface.")
    for _,v in ipairs(ifnames) do
      if v.value == intf then
        if #ifnames == 1 then
          paths[#paths+1] = dnsmasq.path
        else
          paths[#paths+1] = v.path
        end
      end
    end
  end

  local errors = 0
  for i=#paths,1,-1 do
    local path = paths[i]
    ngx.log(ngx.ALERT,format("Deleting %s",path))
    local result,error = proxy.del(path)
    if not result then
      ngx.log(ngx.ERR,format("Failed to delete %s: %s",path,error))
      message_helper.pushMessage(format("Failed to remove %s: %s",path,error),"error")
      errors = errors + 1
    end
  end

  return errors,errors < #paths
end

return M