local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")
local proxy_helper = require("proxy_helper")
local split = require("split").split

local find,format,gmatch,match,gsub = string.find,string.format,string.gmatch,string.match,string.gsub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

-- cpu_port and untagged_flag are updated by 165-VLAN
local cpu_port = "8t"
local untagged_flag = "*"

local function p2ports(values)
  local ports = cpu_port
  for p=4,0,-1 do
    local value = values["p"..p]
    if value and value ~= "" then
      ports = p..untaint(value).." "..ports
    end
  end
  return ports
end

local function update_ifnames(path,value)
  local interface = match(path,"uci%.network%.interface%.@(%S+)%.ifname")
  local type = ""
  local ifnames = split(value," ")
  for i=#ifnames,1,-1 do
    if find(ifnames[i],"^wl") then
      ifnames[i] = nil
    end
  end
  proxy_helper.set(path,value)
  if interface == "lan" or (find(interface,"Guest",nil,true) and #ifnames > 0) or #ifnames > 1 then
    type = "bridge"
  end
  return proxy_helper.set("uci.network.interface.@"..interface..".type",type)
end

local M = {}

function M.add_vlan(index,vid,ports)
  local path = "uci.network.switch_vlan.@"..index.."."
  proxy_helper.set(path.."device","bcmsw_ext")
  proxy_helper.set(path.."vlan",vid)
  proxy_helper.set(path.."ports",ports or cpu_port)
  proxy_helper.set(path.."_key","")
  for p=0,4,1 do
    local port = format("eth%s",p)
    local device = format("vlan_%s_%s",port,vid)
    local added,errmsg = proxy_helper.add("uci.network.device.",device)
    if added then
      proxy_helper.set("uci.network.device.@"..device..".ifname",port)
      proxy_helper.set("uci.network.device.@"..device..".name",device)
      proxy_helper.set("uci.network.device.@"..device..".type","8021q")
      proxy_helper.set("uci.network.device.@"..device..".vid",vid)
    else
      message_helper.pushMessage("Error creating device '"..device.."' for VLAN ID "..vid..": "..errmsg,"error")
    end
  end
end

function M.get_cpu_port()
  return cpu_port
end

function M.get_device_map()
  local map = { eth0 = {}, eth1 = {}, eth2 = {}, eth3 = {}, eth4 = {} }
  for _,p in ipairs(proxy.getPN("uci.network.device.",true)) do
    local values = proxy.get(p.path.."ifname",p.path.."type",p.path.."enabled",p.path.."name",p.path.."vid")
    if values then
      local ifname = untaint(values[1].value)
      if match(ifname,"^eth[0-4]$") and values[2].value == "8021q" and values[3].value ~= "0" then
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

function M.get_untagged_flag()
  return untagged_flag
end

function M.fix_ifnames(enabled_was,enabled)
  local vlan_usage = {}
  local device_map = M.get_device_map()
  local apply = false
  for _,interface in pairs(proxy.getPN("uci.network.interface.",true)) do
    local values = proxy.get(interface.path.."ifname",gsub(interface.path,"^uci","rpc",1).."type")
    if values and values[2].value == "lan" then
      local ifnames = untaint(values[1].value)
      if find(ifnames,"eth[0-4]") then
        local ifnames_new
        for ifname in gmatch(ifnames,"(%S+)") do
          if enabled_was[1].value ~= enabled then
            if enabled == "0" then -- VLANs disabled, so move VLAN interfaces back to base interface
              local base = match(ifname,"(eth[0-4])_%d+$")
              if base then
                ifname = base
              end
              ifnames_new = (ifnames_new and ifnames_new.." " or "")..ifname
            elseif enabled == "1" then -- VLANs enabled, so move base interfaces to VLAN ID 1 interfaces
              if match(ifname,"^eth[0-4]$") then
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
          local index,vlan_id = match(ifname,"eth([0-4])_(%d+)$")
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
          apply = update_ifnames(interface.path.."ifname",ifnames_new)
        end
      end
    end
  end
  if apply then
    proxy.apply()
  end
  return vlan_usage
end

function M.onAdd()
  return function(_,values)
    local vlans = proxy.getPN("uci.network.switch_vlan.",true) or {}
    for i=#vlans,1,-1 do
      local device = proxy.get(vlans[i].path.."device")
      if device and device[1].value == "" then
        local index = match(untaint(vlans[i].path),"@([^%.]+)")
        if index then
          M.add_vlan(index,untaint(values.vlan),p2ports(values))
          proxy.apply()
          break
        end
      end
    end
  end
end

function M.onDelete(vid)
  return function()
    if vid and vid ~= "" then
      local ifnames_map = {}
      for _,interface in pairs(proxy.getPN("uci.network.interface.",true)) do
        local path = interface.path.."ifname"
        local values = proxy.get(path,gsub(interface.path,"^uci","rpc",1).."type")
        if values and values[2].value == "lan" then
          local ifnames = untaint(values[1].value)
          if find(ifnames,"eth",0,true) then
            ifnames_map[#ifnames_map+1] = { path = path, ifnames = ifnames, updated_ifnames = ifnames }
          end
        end
      end
      for p=0,4,1 do
        for _,ifname in ipairs({format("eth%s_%s",p,vid),format("vlan_eth%s_%s",p,vid)}) do
          proxy.del(format("uci.network.device.@%s.",ifname))
          for i=1,#ifnames_map,1 do
            ifnames_map[i].updated_ifnames = gsub(ifnames_map[i].updated_ifnames,ifname,"")
          end
        end
      end
      for _,v in ipairs(ifnames_map) do
        if v.ifnames ~= v.updated_ifnames then
          update_ifnames(v.path,gsub(gsub(gsub(v.updated_ifnames,"^ +","")," +$",""),"  "," "))
        end
      end
      proxy.apply()
    end
  end
end

function M.onModify(vlan_usage)
  return function(index,values)
    local vlan_id = untaint(values.vlan)
    local ports = p2ports(values)
    if values.ports ~= ports then
      proxy_helper.set("uci.network.switch_vlan.@"..index..".ports",ports)
    end
    if vlan_usage[vlan_id] then
      for ifname in gmatch(vlan_usage[vlan_id].interfaces or "","(%S+)") do
        local ifpath = "uci.network.interface.@"..ifname..".ifname"
        local v = proxy.get(ifpath,"rpc.network.interface.@"..ifname..".type")
        if v and v[2].value == "lan" then
          local ifnames = untaint(v[1].value)
          local updated_ifnames = ifnames
          for p,interface in pairs(vlan_usage[vlan_id].ports) do
            if not find(ports,p,nil,true) then
              updated_ifnames = gsub(updated_ifnames,interface,"")
            end
          end
          if ifnames ~= updated_ifnames then
            updated_ifnames = gsub(gsub(gsub(updated_ifnames,"^ +","")," +$",""),"  "," ")
            update_ifnames(ifpath,updated_ifnames)
          end
        end
      end
    end
    proxy.apply()
  end
end

return M
