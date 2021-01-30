local proxy = require("datamodel")
local find, gfind, match, untaint = string.find, string.gfind, string.match, string.untaint

local M = {}

function M.getNetworkDevices() 
  local ssid = {}
  local ifs = {}

  local wired = proxy.getPN("uci.network.interface.", true)
  for _,interface in ipairs(wired) do
    local ifname = match(interface.path, "uci%.network%.interface%.@([^%.]+)%.")
    local devices = proxy.get(interface.path .. "ifname")[1].value:untaint()
    for device in gfind(devices, "[^ ]+") do
      if find(device,"atm") then
        device = "atm_8_35"
      elseif find(device,"ptm") then
        device = "ptm0"
      end
      if ifs[device] then
        ifs[device] = ifs[device] .. "," .. untaint(ifname)
      else
        ifs[device] = untaint(ifname)
      end
    end
  end
  
  local wifi = proxy.getPN("uci.wireless.wifi-iface.", true)
  for _,v in ipairs(wifi) do
    local device = match(v.path, "uci%.wireless%.wifi%-iface%.@([^%.]+)%.")
    local wlname = proxy.get(v.path .. "ssid")
    local radio_name = proxy.get("rpc.wireless.ssid.@"..device..".radio")[1].value
    local lan = proxy.get("rpc.wireless.ssid.@"..device..".lan")[1].value
    local ifname
    if lan == "1" then
      ifname = "wlan"
    else
      ifname = "wan"
    end
    if ifs[device] then
      ifs[device] = ifs[device] .. "," .. ifname
    else
      ifs[device] = ifname
    end
    if wlname then
      wlname = wlname[1].value
    else
      wlname = device
    end
    if radio_name == "radio_2G" then
      ssid[device] = wlname .. " (2.4G)"
    else
      ssid[device] = wlname .. " (5G)"
    end    
  end

  return ifs, ssid
end  

return M
