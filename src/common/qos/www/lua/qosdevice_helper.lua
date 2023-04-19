local proxy = require("datamodel")
local find,gmatch,match = string.find,string.gmatch,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

function M.getNetworkDevices()
  local ssid = {}
  local ifs = {}

  local wired = proxy.getPN("uci.network.interface.",true)
  for _,interface in ipairs(wired) do
    local ifname = match(interface.path,"uci%.network%.interface%.@([^%.]+)%.")
    local devices = proxy.get(interface.path.."ifname")[1].value:untaint()
    for device in gmatch(devices,"[^ ]+") do
      if find(device,"atm") then
        device = "atm_8_35"
      elseif find(device,"ptm") then
        device = "ptm0"
      end
      if ifs[device] then
        ifs[device] = ifs[device]..","..untaint(ifname)
      else
        ifs[device] = untaint(ifname)
      end
    end
  end

  local wifi = proxy.getPN("uci.wireless.wifi-iface.",true)
  for _,v in ipairs(wifi) do
    local device = match(v.path,"uci%.wireless%.wifi%-iface%.@([^%.]+)%.")
    local ifname = proxy.get("rpc.wireless.ssid.@"..device..".network")
    if ifname and ifname[1].value ~= "fonopen" then
      ifname = ifname[1].value
      if ifname == "lan" then
        ifname = "wlan"
      end
      local wlname = proxy.get(v.path.."ssid")
      if ifs[device] then
        ifs[device] = ifs[device]..","..untaint(ifname)
      else
        ifs[device] = untaint(ifname)
      end
      if wlname then
        wlname = wlname[1].value
      else
        wlname = device
      end
      local radio_name = untaint(proxy.get("rpc.wireless.ssid.@"..device..".radio")[1].value)

      local radio_band = match(untaint(proxy.get("rpc.wireless.radio.@"..radio_name..".band")[1].value),"^([245%.]+G)Hz$")
      if radio_band then
        ssid[device] = wlname.." ("..radio_band..")"
      end
    end
  end

  return ifs,ssid
end

return M
