gettext.textdomain('webui-core')

local bridged = require("bridgedmode_helper")
local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local match,sub = string.match,string.sub

local content = {
  sfp_enabled = "uci.env.rip.sfp",
}
content_helper.getExactContent(content)
local sfp = content["sfp_enabled"]

local ethname = proxy.get("sys.eth.port.@eth4.status")
if ethname and ethname[1].value then
  ethname =  "eth4"
else
  ethname =  "eth3"
end

local function get_wansensing() 
  local ws = proxy.get("uci.wansensing.global.enable")
  if ws then
    return ws[1].value
  end
  return ""
end

local function findwan(interface)
  local pathmatch = "uci%.network%.device%.@.*"..interface..".*%."
  local found = {}

  for _,v in ipairs(proxy.getPN("uci.network.device.",true)) do
    local result = match(v.path,pathmatch)
    if result then
      found[#found+1] = result:gsub("uci%.network%.device%.",""):gsub("%.","")
    end
  end

  if #found == 1 then
    return found[1]
  elseif #found == 0 then
    return nil
  end

  local ifmatch = "uci%.network%.device%.@"..interface..".*%.ifname"
  local base
  local vlan
  for _,v in ipairs(found) do
    local path = "uci.network.device."..v..".ifname"
    if match(path,ifmatch) then
      base = v
    else
      local ifname = proxy.get(path)
      if ifname then
        if sub(ifname[1].value,1,#interface) == interface then
          vlan = v
        end
      end
    end
  end

  if vlan then
    return vlan
  end
  return base
end

local tablecontent = {}
tablecontent[#tablecontent + 1] = {
  name = "adsl",
  default = false,
  description = "ADSL2+",
  view = "broadband-adsl-advanced.lp",
  card = "002_broadband_xdsl.lp",
  check = function()
    if get_wansensing() == "1" then
      local L2 = proxy.get("uci.wansensing.global.l2type")[1].value
      if L2 == "ADSL" then
        return true
      end
    else
      local ifname = proxy.get("uci.network.interface.@wan.ifname")
      if ifname then
        local iface = match(ifname[1].value,"atm")
        if iface then
          return true
        end
      end
    end
  end,
  operations = function()
    local interface = findwan("atm") or "@wanatmwan"
    local difname = proxy.get("uci.network.device."..interface..".ifname")
    if difname then
      local dname = proxy.get("uci.network.device."..interface..".name")[1].value
      difname = proxy.get("uci.network.device."..interface..".ifname")[1].value
      if difname ~= "" and difname ~= nil then
        proxy.set("uci.network.interface.@wan.ifname",dname)
        proxy.set("uci.network.interface.@wan6.ifname",dname)
      else
        proxy.set("uci.network.interface.@wan.ifname","atmwan")
        proxy.set("uci.network.interface.@wan6.ifname","atmwan")
      end
    else
      proxy.set("uci.network.interface.@wan.ifname","atmwan")
      proxy.set("uci.network.interface.@wan6.ifname","atmwan")
    end
    if sfp == 1 then
      proxy.set("uci.ethernet.globals.eth4lanwanmode","1")
    end
    if ethname == "eth3" then
      local ifnames = proxy.get("uci.network.interface.@lan.ifname")[1].value
      proxy.set({
        ["uci.network.interface.@lan.ifname"] = ifnames..' '..ethname,
        ["uci.ethernet.port.@eth3.wan"] = "0"
      })
    end
    proxy.set("uci.wansensing.global.l2type","ADSL")
  end,
}
tablecontent[#tablecontent + 1] = {
  name = "vdsl",
  default = true,
  description = "VDSL2",
  view = "broadband-vdsl-advanced.lp",
  card = "002_broadband_xdsl.lp",
  check = function()
    if get_wansensing() == "1" then
      local L2 = proxy.get("uci.wansensing.global.l2type")[1].value
      if L2 == "VDSL" then
        return true
      end
    else
      local ifname = proxy.get("uci.network.interface.@wan.ifname")
      if ifname then
        local iface = match(ifname[1].value,"ptm0")
        if iface then
          return true
        end
      end
    end
  end,
  operations = function()
    local interface = findwan("ptm") or "@wanptm0"
    local difname = proxy.get("uci.network.device."..interface..".ifname")
    if difname then
      local dname = proxy.get("uci.network.device."..interface..".name")[1].value
      difname = proxy.get("uci.network.device."..interface..".ifname")[1].value
      if difname ~= "" and difname ~= nil then
        proxy.set("uci.network.interface.@wan.ifname",dname)
        proxy.set("uci.network.interface.@wan6.ifname",dname)
      else
        proxy.set("uci.network.interface.@wan.ifname","ptm0")
        proxy.set("uci.network.interface.@wan6.ifname","ptm0")
      end
    else
      proxy.set("uci.network.interface.@wan.ifname","ptm0")
      proxy.set("uci.network.interface.@wan6.ifname","ptm0")
    end
    if sfp == 1 then
      proxy.set("uci.ethernet.globals.eth4lanwanmode","1")
    end
    if ethname == "eth3" then
      local ifnames = proxy.get("uci.network.interface.@lan.ifname")[1].value
      proxy.set({
        ["uci.network.interface.@lan.ifname"] = ifnames..' '..ethname,
        ["uci.ethernet.port.@eth3.wan"] = "0"
      })
    end
    proxy.set("uci.wansensing.global.l2type","VDSL")
  end,
}
tablecontent[#tablecontent + 1] = {
  name = "bridge",
  default = false,
  description = "Bridged Mode",
  view = "broadband-bridge.lp",
  card = "002_broadband_bridge.lp",
  check = function()
    return bridged.isBridgedMode()
  end,
  operations = function()
  end,
}
tablecontent[#tablecontent + 1] = {
  name = "ethernet",
  default = false,
  description = "Ethernet",
  view = "broadband-ethernet-advanced.lp",
  card = "002_broadband_ethernet.lp",
  check = function()
    if get_wansensing() == "1" then
      local L2 = proxy.get("uci.wansensing.global.l2type")[1].value
      if L2 == "ETH" then
        return true
      end
    else
      local ifname = proxy.get("uci.network.interface.@wan.ifname")
      if ifname then
        local iface = match(ifname[1].value,ethname) or match(ifname[1].value,"lan") --the or is in case wan iface is br-lan
        if sfp == 1 then
          local lwmode = proxy.get("uci.ethernet.globals.eth4lanwanmode")[1].value
          if iface and lwmode == "0" then
            return true
          end
        else
          if iface then
            return true
          end
        end
      end
    end
  end,
  operations = function()
    local interface = findwan(ethname) or ("@wan"..ethname)
    local difname = proxy.get("uci.network.device."..interface..".ifname")
    if difname then
      local dname = proxy.get("uci.network.device."..interface..".name")[1].value
      difname = proxy.get("uci.network.device."..interface..".ifname")[1].value
      if difname ~= "" and difname ~= nil then
        proxy.set("uci.network.interface.@wan.ifname",dname)
        proxy.set("uci.network.interface.@wan6.ifname",dname)
      else
        proxy.set("uci.network.interface.@wan.ifname",ethname)
        proxy.set("uci.network.interface.@wan6.ifname",ethname)
      end
    else
      proxy.set("uci.network.interface.@wan.ifname",ethname)
      proxy.set("uci.network.interface.@wan6.ifname",ethname)
    end
    if sfp == 1 then
      proxy.set("uci.ethernet.globals.eth4lanwanmode","0")
    end
    if ethname == "eth3" then
      local ifnames = proxy.get("uci.network.interface.@lan.ifname")[1].value
      proxy.set({
        ["uci.network.interface.@lan.ifname"] = string.gsub(string.gsub(ifnames,ethname,""),"%s$",""),
        ["uci.ethernet.port.@eth3.wan"] = "1"
      })
    end
    proxy.set("uci.wansensing.global.l2type","ETH")
  end,
}

if sfp == 1 then
  tablecontent[#tablecontent + 1] = {
    name = "gpon",
    default = false,
    description = "GPON",
    view = "broadband-gpon-advanced.lp",
    card = "002_broadband_gpon.lp",
    check = function()
      if get_wansensing() == "1" then
        local L2 = proxy.get("uci.wansensing.global.l2type")[1].value
        if L2 == "SFP" then
          return true
        end
      else
        local ifname = proxy.get("uci.network.interface.@wan.ifname")
        if ifname then
          local iface = match(ifname[1].value,ethname)
          if sfp == 1 then
            local lwmode = proxy.get("uci.ethernet.globals.eth4lanwanmode")[1].value
            if iface and lwmode == "1" then
              return true
            end
          else
            if iface then
              return true
            end
          end
        end
      end
    end,
    operations = function()
      local interface = findwan(ethname) or "@waneth4"
      local difname = proxy.get("uci.network.device."..interface..".ifname")
      if difname then
        local dname = proxy.get("uci.network.device."..interface..".name")[1].value
        difname = proxy.get("uci.network.device."..interface..".ifname")[1].value
        if difname ~= "" and difname ~= nil then
          proxy.set("uci.network.interface.@wan.ifname",dname)
          proxy.set("uci.network.interface.@wan6.ifname",dname)
        else
          proxy.set("uci.network.interface.@wan.ifname",ethname)
          proxy.set("uci.network.interface.@wan6.ifname",ethname)
        end
      else
        proxy.set("uci.network.interface.@wan.ifname",ethname)
        proxy.set("uci.network.interface.@wan6.ifname",ethname)
      end
      proxy.set("uci.ethernet.globals.eth4lanwanmode","1")
      proxy.set("uci.wansensing.global.l2type","SFP")
    end,
  }
end
return tablecontent