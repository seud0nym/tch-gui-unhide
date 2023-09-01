#!/usr/bin/env lua

local logger,posix,ubus,uloop = require("tch.logger"),require("tch.posix"),require('ubus'),require('uloop')
local log = logger.new("static-wan-routes-monitor",3,posix.LOG_DAEMON)
local ipairs = ipairs

local monitored = {}
for _,pair in ipairs(arg or {}) do
  local interface,gateway = string.match(pair,"([^,]+),(.+)")
  if interface then
    if not monitored[interface] then
      monitored[interface] = { }
    end
    monitored[interface][#monitored[interface]+1] = gateway
    log:notice("Adding monitoring for static WAN interface on "..interface.." (gateway = "..gateway..")")
  end
end

local conn = ubus.connect()
if conn then
  log:notice("Connected to ubusd")

  local function fix_default_routes(interface,gateway,action)
    local family = "4"
    local metric = "1"
    if string.find(gateway,":") then
      family = "6"
      metric = "512"
    end
    if action == "down" then
      local cmd,err = io.popen("ip -"..family.." route show default dev "..interface)
      if cmd then
        for line in cmd:lines() do
          local remove_cmd = "ip -"..family.." route del "..line
          log:notice(remove_cmd)
          os.execute(remove_cmd)
        end
        cmd:close()
      else
        log:error("Failed to retrieve IPv"..family.." routes for interface "..interface..": "..err)
      end
    else -- up
      local add_cmd = "ip -"..family.." route add default via "..gateway.." dev "..interface.." proto static metric "..metric
      log:notice(add_cmd)
      os.execute(add_cmd)
    end
  end

  uloop.init()

  local events = {}
  events['network.link'] = function(data)
    if data then
      if monitored[data.interface] then
        log:notice("Interface "..data.interface.." is "..data.action)
        for _,gateway in pairs(monitored[data.interface]) do
          if gateway then
            fix_default_routes(data.interface,gateway,data.action)
          end
        end
      else
        log:notice("Interface "..data.interface.." is "..data.action.." (Ignored)")
      end
    end
  end
  conn:listen(events)

  log:notice("Waiting for network.link events...")
  while true do
    uloop.run()
  end
else
  log:error("Failed to connect to ubusd - aborting")
  os.exit(2)
end

