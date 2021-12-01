local proxy = require("datamodel")
local find,format,match = string.find,string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

function M.getThemeSchedule()
  local light = {}
  local night = {}
  local data = proxy.getPN("rpc.gui.cron.entries.",true)
  for _,v in ipairs(data) do
    local cmd = proxy.get(v.path.."command")
    if cmd and find(untaint(cmd[1].value),"rpc%.gui%.theme%.THEME") then
      local time = proxy.get(v.path.."minute",v.path.."hour",v.path.."enabled")
      local mm = untaint(time[1].value)
      local hh = untaint(time[2].value)
      if mm ~= "0" and mm ~= "15" and mm ~= "30" and mm ~= "45" then
        mm = "0"
      end
      if find(untaint(cmd[1].value),"light") then
        light.path = v.path
        light.hh = tonumber(hh)
        light.mm = tonumber(mm)
        light.time = format("%s:%s",hh,mm)
        light.enabled = untaint(time[3].value) == "1"
      elseif find(untaint(cmd[1].value),"night") then
        night.path = v.path
        night.hh = tonumber(hh)
        night.mm = tonumber(mm)
        night.time = format("%s:%s",hh,mm)
        night.enabled = untaint(time[3].value) == "1"
      end
    end
  end
  return light,night
end

local pattern = "lua -e \"require('datamodel').set('rpc.gui.theme.THEME','%s')\""
function M.updateCronEntry(theme,path,time,enabled)
  local hh,mm = match(time,"(%d+):(%d+)")
  local index
  if path then
    index = match(path,"rpc%.gui%.cron%.entries%.(%d+)%.")
  else
    index = proxy.add("rpc.gui.cron.entries.")
    path = "rpc.gui.cron.entries."..index.."."
    proxy.set(path.."command",format(pattern,theme))
  end
  proxy.set(path.."minute",mm)
  proxy.set(path.."hour",hh)
  proxy.set(path.."enabled",enabled)
  return time,tonumber(hh),tonumber(mm)
end

return M
