local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")

local find,format,gmatch,match = string.find,string.format,string.gmatch,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

function M.add_vlan(index,vid,ports,cpu_port)
  local path -- index is _key, which is not exposed by transformer
  local switch_vlans = proxy.getPN("uci.network.switch_vlan.",true)
  for _,v in ipairs(switch_vlans) do
    if v.path == "uci.network.switch_vlan.@"..index.."." then
      path = v.path
      break
    end
  end
  if not path then
    for _,v in ipairs(switch_vlans) do
      local key = proxy.get(v.path.."_key")
      if key and key[1].value == "index" then
        path = v.path
        break
      end
    end
    if not path then
      path = switch_vlans[#switch_vlans].path
    end
  end
  proxy.set(path.."device","bcmsw_ext")
  proxy.set(path.."vlan",vid)
  proxy.set(path.."ports",ports or cpu_port)
  proxy.set(path.."_key","")
  for p=0,3,1 do
    local port = format("eth%s",p)
    local device = format("vlan_%s_%s",port,vid)
    local added,errmsg = proxy.add("uci.network.device.",device)
    if added then
      proxy.set("uci.network.device.@"..device..".ifname",port)
      proxy.set("uci.network.device.@"..device..".name",device)
      proxy.set("uci.network.device.@"..device..".type","8021q")
      proxy.set("uci.network.device.@"..device..".vid",vid)
    else
      message_helper.pushMessage("Error creating device '"..device.."' for VLAN ID "..vid..": "..errmsg,"error")
    end
  end
end

function M.get_device_map()
  local map = { eth0 = {}, eth1 = {}, eth2 = {}, eth3 = {} }
  for _,p in ipairs(proxy.getPN("uci.network.device.",true)) do
    local values = proxy.get(p.path.."ifname",p.path.."type",p.path.."enabled",p.path.."name",p.path.."vid")
    if values then
      local ifname = untaint(values[1].value)
      if match(ifname,"^eth[0-3]$") and values[2].value == "8021q" and values[3].value ~= "0" then
        local name = untaint(values[4].value)
        local vid = untaint(values[5].value)
        if name ~= "" then
          map[ifname][name] = vid
        end
      end
    end
  end
  return map
end

function M.get_port_states()
  local port = {
    state_0 = "sys.class.net.@eth0.operstate",
    speed_0 = "sys.class.net.@eth0.speed",
    state_1 = "sys.class.net.@eth1.operstate",
    speed_1 = "sys.class.net.@eth1.speed",
    state_2 = "sys.class.net.@eth2.operstate",
    speed_2 = "sys.class.net.@eth2.speed",
    state_3 = "sys.class.net.@eth3.operstate",
    speed_3 = "sys.class.net.@eth3.speed",
    state_4 = "sys.class.net.@eth4.operstate",
    speed_4 = "sys.class.net.@eth4.speed",
  }
  content_helper.getExactContent(port)
  return port
end

function M.fix_ifnames(enabled_was,enabled)
  local vlan_usage = {}
  local device_map = M.get_device_map()
  local apply = false
  for _,interface in pairs(proxy.getPN("uci.network.interface.",true)) do
    local value = proxy.get(interface.path.."ifname")
    if value then
      local ifnames = untaint(value[1].value)
      if find(ifnames,"eth[0-3]") then
        local ifnames_new
        for ifname in gmatch(ifnames,"(%S+)") do
          if enabled_was[1].value ~= enabled then
            if enabled == "0" then -- VLANs disabled, so move VLAN interfaces back to base interface
              local base = match(ifname,"(eth[0-3])_%d+$")
              if base then
                ifname = base
              end
              ifnames_new = (ifnames_new and ifnames_new.." " or "")..ifname
            elseif enabled == "1" then -- VLANs enabled, so move base interfaces to VLAN ID 1 interfaces
              if match(ifname,"^eth[0-3]$") then
                ifname = "vlan_"..ifname.."_1"
                if device_map[ifname] then
                  for k,v in pairs(device_map[ifname]) do
                    if v == "1" then
                      ifname = k
                      break
                    end
                  end
                end
              end
              ifnames_new = (ifnames_new and ifnames_new.." " or "")..ifname
            end
          end
          local index,vlan_id = match(ifname,"eth([0-3])_(%d+)$")
          if vlan_id then
            local key = match(interface.path,"@([^%.]+)")
            if not vlan_usage[vlan_id] then
              vlan_usage[vlan_id] = { count = 0, interfaces = "", ports = {} }
            end
            vlan_usage[vlan_id].count = vlan_usage[vlan_id].count + 1
            vlan_usage[vlan_id].ports[index] = ifname
            for v in gmatch(vlan_usage[vlan_id].interfaces,"(%S+)") do
              if v == key then
                key = nil
                break
              end
            end
            if key then
              vlan_usage[vlan_id].interfaces = vlan_usage[vlan_id].interfaces..key.." "
            end
          end
        end
        if ifnames_new and ifnames_new ~= ifnames then
          proxy.set(interface.path.."ifname",ifnames_new)
          apply = true
        end
      end
    end
  end
  if apply then
    proxy.apply()
  end
  return vlan_usage
end

return M
