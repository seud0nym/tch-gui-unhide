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

local M = {}

local wanport = proxy.get("sys.eth.port.@eth4.status")
if wanport and wanport[1].value then
  M.wanport = "eth4"
else
  M.wanport = "eth3"
end

local function find_vlan_1_path()
  local vlan_1
  local values = proxy.getPN("uci.network.switch_vlan.",true)
  for k=1,#values do
    local v = values[k]
    local vlan = proxy.get(v.path.."vlan")
    if vlan and vlan[1].value == "1" then
      vlan_1 = v.path
      break
    end
  end
  return vlan_1
end

local function make_vlan_1(vlan_1_path)
  local ports
  if M.wanport == "eth4" then
    ports = "0"..untagged_flag.." 1"..untagged_flag.." 2"..untagged_flag.." 3"..untagged_flag.." "..cpu_port
  else
    ports = "0"..untagged_flag.." 1"..untagged_flag.." 2"..untagged_flag.." "..cpu_port
  end
  if vlan_1_path then
    proxy_helper.set(vlan_1_path.."ports",ports)
  else
    local added,errmsg = proxy_helper.add("uci.network.switch_vlan.")
    if added then
      M.add_vlan(added,"1",ports)
    else
      log:error("Failed to add switch_vlan: %s",errmsg)
      message_helper.pushMessage("Error creating VLAN ID 1: "..errmsg,"error")
    end
  end
  return ports
end

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

function M.add_switch(jumbo)
  if not proxy.getPN(switch_path,true) then
    log:notice("Adding %s switch for device name %s (%s)",switch_config or 'unnamed',switch_device,switch_path)
    proxy.add("uci.network.switch.",switch_config)
  end
  proxy_helper.set(switch_path.."name",switch_device)
  proxy_helper.set(switch_path.."reset","1")
  proxy_helper.set(switch_path.."enable_vlan","1")
  proxy_helper.set(switch_path.."jumbo",jumbo)
  if switch_config then
    proxy_helper.set(switch_path.."qosimppauseenable","0")
    proxy_helper.set(switch_path.."type","bcmsw")
    proxy_helper.set(switch_path.."unit","1")
  end
  local vlan_1 = find_vlan_1_path()
  make_vlan_1(vlan_1)
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
  if not allow_none_vlan and enabled == "1" then
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
  if M.wanport == "eth3" then
    port["state_4"] = nil
    port["speed_4"] = nil
  end
  content_helper.getExactContent(port)
  return port
end

function M.get_switch_device()
  return switch_device
end

function M.get_switch_path(option)
  return switch_path..option
end

function M.get_switch_vlans(include_no_vlan,is_bridged_mode)
  local vlan_ids = {}
  local valid_ifnames = {}
  local switch_vlan
  local enabled = M.get_vlan_enabled()
  if enabled[1].value == "0" then
    switch_vlan = {}
  else
    switch_vlan = content_helper.getMatchedContent("uci.network.switch_vlan.",{device=M.get_switch_device()})
    for k=1,#switch_vlan do
      local v = switch_vlan[k]
      local vlan_id = untaint(v.vlan)
      vlan_ids[vlan_id] = k
      for port,state in gmatch(gsub(untaint(v.ports),cpu_port,""),"(%d)(%S*)") do
        valid_ifnames[format("eth%s.%s",port,vlan_id)] = state or untagged_flag
        v[format("eth%s",port)] = state or untagged_flag
      end
    end
  end
  if include_no_vlan then
    local interfaces = proxy.getPN("uci.network.interface.",true)
    for i=1,#interfaces do
      local interface = interfaces[i]
      local values = proxy.get(interface.path.."ifname",gsub(interface.path,"^uci","rpc",1).."type")
      if values and values[2].value == "lan" then
        local ifnames = untaint(values[1].value)
        if find(ifnames,"eth[0-4]") then
          for ifname in gmatch(ifnames,"(%S+)") do
            local p = match(ifname,"^eth([0-4])$")
            if p then
              local base = format("eth%s",p)
              local port = p..untagged_flag
              valid_ifnames[base] = true
              if not vlan_ids["None"] then
                switch_vlan[#switch_vlan+1] = {vlan="None",ports=port}
                vlan_ids["None"] = #switch_vlan
              elseif not find(switch_vlan[vlan_ids["None"]].ports,port,1,true) then
                switch_vlan[vlan_ids["None"]].ports = switch_vlan[vlan_ids["None"]].ports.." "..port
              end
              switch_vlan[vlan_ids["None"]][base] = untagged_flag
            end
          end
        end
      end
    end
  end
  local sys_vlans = 0
  local mesh_cred = content_helper.getMatchedContent("uci.mesh_broker.controller_credentials.")
  if mesh_cred or #mesh_cred > 0 then
    local wan_ifname = proxy.get("uci.network.interface.@wan.ifname")
    local ports = (is_bridged_mode or (wan_ifname and wan_ifname[1].value ~= M.wanport)) and "0t 1t 2t 3t 4t" or "0t 1t 2t 3t"
    for i=1,#mesh_cred do
      local credentials = mesh_cred[i]
      local vlan_id = untaint(credentials.vlan_id)
      if not vlan_ids[vlan_id] then
        switch_vlan[#switch_vlan+1] = {vlan=vlan_id,ports=ports,type=credentials.type}
        vlan_ids[vlan_id] = #switch_vlan
        sys_vlans = sys_vlans + 1
        for port,state in gmatch(ports,"(%d)(%S*)") do
          valid_ifnames[format("eth%s.%s",port,vlan_id)] = true
          switch_vlan[vlan_ids[vlan_id]][format("eth%s",port)] = state or untagged_flag
        end
      end
    end
  end
  table.sort(switch_vlan,function (a,b)
    return (tonumber(a.vlan) or -1) < (tonumber(b.vlan) or -1)
  end)
  return switch_vlan,valid_ifnames,sys_vlans
end

function M.get_untagged_flag()
  return untagged_flag
end

function M.get_vlan_enabled()
  return proxy.get(M.get_switch_path("enable_vlan")) or {{value = "0"}}
end

function M.fix_ifnames(enabled_was,enabled)
  local used = {}
  local vlan_usage = {}
  local vlan_ids = {}
  local apply = false
  local vlan_1_path = find_vlan_1_path()
  local vlan_1_ports
  if vlan_1_path then
    vlan_1_ports = untaint(proxy.get(vlan_1_path.."ports")[1].value)
  end
  local paths = proxy.getPN("uci.network.interface.",true)
  for k=1,#paths do
    local interface = paths[k]
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
              if not vlan_1_ports then
                vlan_1_ports = make_vlan_1(vlan_1_path)
              end
              local base = match(ifname,"^(eth[0-4])$")
              if base then
                if find(vlan_1_ports,match(ifname,"eth(%d)"),1,true) then
                  ifname = base..".1"
                  ifnames_new = (ifnames_new and ifnames_new.." " or "")..ifname
                else
                  log.warning("Dropping %s from %s.ifname - not found in VLAN ID 1 ports (%s)",base,interface.path,vlan_1_ports)
                end
              end
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
