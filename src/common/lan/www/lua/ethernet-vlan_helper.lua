local content_helper = require("web.content_helper")
local log = require("tch.logger").new("ethernet-vlan_helper",7)
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")
local proxy_helper = require("proxy_helper")
local split = require("split").split

local find,format,gmatch,match,gsub = string.find,string.format,string.gmatch,string.match,string.gsub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

-- These constants are updated by 165-VLAN - DO NOT modify these lines without verifying that the substitutions will work correctly!
local allow_none_vlan = false
local cpu_port = "8t"
local untagged_flag = "*"
local switch_config = "bcmsw_ext"
local switch_device = "bcmsw_ext"
local switch_path = "uci.network.switch.@bcmsw_ext."

local function p2ports(values)
  local ports = cpu_port
  for p=4,0,-1 do
    local value = values["p"..p]
    if value and value ~= "X" then
      ports = p..untaint(value).." "..ports
    end
  end
  return ports
end

local function update_ifnames(path,updated_ifnames)
  updated_ifnames = gsub(gsub(gsub(updated_ifnames,"^ +","")," +$",""),"  "," ")
  local interface = match(path,"uci%.network%.interface%.@(%S+)%.ifname")
  local type = ""
  local ifnames = split(updated_ifnames," ")
  local count = 0
  for i=#ifnames,1,-1 do
    if not (find(ifnames[i],"^wl") or find(ifnames[i],"^dummy")) then
      count = count + 1
    end
  end
  log:notice("Setting %s = %s",path,updated_ifnames)
  proxy_helper.set(path,updated_ifnames)
  if interface == "lan" or (find(interface,"Guest",nil,true) and count > 0) or count > 1 then
    type = "bridge"
  end
  log:notice("Setting %s = %s","uci.network.interface.@"..interface..".type",type)
  return proxy_helper.set("uci.network.interface.@"..interface..".type",type)
end

local M = {}

function M.add_switch(jumbo)
  log:notice("Adding %s switch for device name %s (%s)",switch_config or 'unnamed',switch_device,switch_path)
  proxy_helper.add("uci.network.switch.",switch_config)
  proxy_helper.set(switch_path.."name",switch_device)
  proxy_helper.set(switch_path.."reset","1")
  proxy_helper.set(switch_path.."enable_vlan","1")
  proxy_helper.set(switch_path.."jumbo",jumbo)
  if switch_config then
    proxy_helper.set(switch_path.."qosimppauseenable","0")
    proxy_helper.set(switch_path.."type","bcmsw")
    proxy_helper.set(switch_path.."unit","1")
  end
  local added,errmsg = proxy_helper.add("uci.network.switch_vlan.")
  if added then
    M.add_vlan(added,"1","0"..untagged_flag.." 1"..untagged_flag.." 2"..untagged_flag.." 3"..untagged_flag.." "..cpu_port)
  else
    log:error("Failed to add switch_vlan: %s",errmsg)
    message_helper.pushMessage("Error creating VLAN ID 1: "..errmsg,"error")
  end
  proxy.apply()
end

function M.add_vlan(index,vid,ports)
  local path = "uci.network.switch_vlan.@"..index.."."
  log:notice("Adding VLAN # %s for ports %s on device name %s (%s)",vid,ports or cpu_port,switch_device,path)
  proxy_helper.set(path.."device",switch_device)
  proxy_helper.set(path.."vlan",vid)
  proxy_helper.set(path.."ports",ports or cpu_port)
  proxy_helper.set(path.."_key","")
end

function M.get_cpu_port()
  return cpu_port
end

function M.get_none_vlan_label(enabled)
  if enabled == "1" then
    if allow_none_vlan then
      return T"None"
    end
    return nil
  end
  return T""
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

function M.get_switch_device()
  return switch_device
end

function M.get_switch_path(option)
  return switch_path..option
end

function M.get_switch_vlans(include_no_vlan)
  local switch_vlan = content_helper.getMatchedContent("uci.network.switch_vlan.",{device=M.get_switch_device()})
  local vlan_ids = {}
  local valid_ifnames = {}
  for k,v in ipairs(switch_vlan) do
    local vlan_id = untaint(v.vlan)
    vlan_ids[vlan_id] = k
    for port in gmatch(gsub(untaint(v.ports),cpu_port,""),"(%d)") do
      valid_ifnames[format("eth%s.%s",port,vlan_id)] = true
    end
  end
  for _,interface in pairs(proxy.getPN("uci.network.interface.",true)) do
    local values = proxy.get(interface.path.."ifname",gsub(interface.path,"^uci","rpc",1).."type")
    if values and values[2].value == "lan" then
      local ifnames = untaint(values[1].value)
      if find(ifnames,"eth[0-4]") then
        for ifname in gmatch(ifnames,"(%S+)") do
          local vlan_id = nil
          local index = match(ifname,"^eth([0-4])$")
          if index and include_no_vlan then
            vlan_id = "None"
          elseif not index then
            index,vlan_id = match(ifname,"^eth([0-4])%.(%d+)$")
          end
          if vlan_id then
            local port
            if vlan_id == "None" then
              port = index..untagged_flag
            else
              port = index.."t"
            end
            valid_ifnames[format("eth%s.%s",index,vlan_id)] = true
            if not vlan_ids[vlan_id] then
              switch_vlan[#switch_vlan+1] = { vlan = vlan_id, ports = port }
              vlan_ids[vlan_id] = #switch_vlan
            else
              if not find(switch_vlan[vlan_ids[vlan_id]].ports,port) then
                switch_vlan[vlan_ids[vlan_id]].ports = switch_vlan[vlan_ids[vlan_id]].ports.." "..port
              end
            end
          end
        end
      end
    end
  end
  table.sort(switch_vlan,function (a, b)
    return (tonumber(a.vlan) or -1) < (tonumber(b.vlan) or -1)
  end)
  return switch_vlan,valid_ifnames
end

function M.get_untagged_flag()
  return untagged_flag
end

function M.fix_ifnames(enabled_was,enabled)
  local used = {}
  local vlan_usage = {}
  local vlan_ids = {}
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
              local base = match(ifname,"^(eth[0-4])%.%d+$")
              if base then
                ifname = base
              end
              if not used[ifname] then
                used[ifname] = true
                ifnames_new = (ifnames_new and ifnames_new.." " or "")..ifname
              end
            elseif not allow_none_vlan then -- VLANs enabled and none VLAN not allowed, so move base interfaces to VLAN 1
              local base = match(ifname,"^(eth[0-4])$")
              if base then
                ifname = base..".1"
              end
              ifnames_new = (ifnames_new and ifnames_new.." " or "")..ifname
            end
          end
          local index,vlan_id = match(ifname,"^eth([0-4])%.(%d+)$")
          if vlan_id then
            local key = match(interface.path,"@([^%.]+)")
            if not vlan_usage[vlan_id] then
              vlan_usage[vlan_id] = { count = 0, interfaces = "", ports = {} }
              vlan_ids[vlan_id] = true
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
  return vlan_usage,vlan_ids
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
        local ifname = format("eth%s.%s",p,vid)
        for i=1,#ifnames_map,1 do
          ifnames_map[i].updated_ifnames = gsub(ifnames_map[i].updated_ifnames,ifname,"")
        end
      end
      for _,v in ipairs(ifnames_map) do
        if v.ifnames ~= v.updated_ifnames then
          update_ifnames(v.path,v.updated_ifnames)
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
            update_ifnames(ifpath,updated_ifnames)
          end
        end
      end
    end
    proxy.apply()
  end
end

return M
