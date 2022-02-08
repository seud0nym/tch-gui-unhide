#!/usr/bin/env lua

local logger,ubus,uloop = require("tch.logger"),require('ubus'),require('uloop')
local log = logger.new("bwstats",tonumber(arg[1]))

local conn = ubus.connect()
if conn then
  log:notice("Connected to ubusd")
  local format,match,substr = string.format,string.match,string.sub
  local os = os
  local run = os.execute

  local events = {}
  local actions = {
    add = { "||", "A" },
    delete = { "&&", "D" },
  }
  local chains = {
    BWSTATSRX = "s",
    BWSTATSTX = "d",
  }

  local function update_rules(family,action,mac,ip)
    if run('iptables -t mangle -nL BWSTATSRX >/dev/null 2>&1') ~= 0 then
      log:warning("IPv4 Chain BWSTATSRX not found - exiting")
      os.exit()
    end
    if action and actions[action] and mac and mac ~= "" and ip and ip.address and ip.address ~= "" then
      local cmd
      if family == "4" then
        cmd = "iptables -t mangle"
      elseif family == "6" then
        if not (substr(ip.address,1,4) == "fe80" or substr(ip.address,1,2) == "fd") then
          cmd = "ip6tables -t mangle"
        else
          log:notice("%s %s (%s) IGNORED: Local IPv6 address",action,ip.address,mac)
        end
      else
        log:warning("%s %s (%s) IGNORED: Unknown family '%s'",action,ip.address,mac,family)
      end
      if cmd then
        log:notice("%s %s (%s)",action,ip.address,mac)
        for chain,option in pairs(chains) do
          local options = format('%s -%s %s -j RETURN -m comment --comment "%s MAC: %s"',chain,option,ip.address,chain,mac)
          local command = format('%s -C %s 2>/dev/null %s %s -%s %s',cmd,options,actions[action][1],cmd,actions[action][2],options)
          log:debug(options)
          run(command)
        end
      end
    end
  end

  local function add_existing(family)
    local cmd = io.popen("ip -"..family.." neigh show dev br-lan nud reachable")
    if cmd then
      for line in cmd:lines() do
        local ip,mac = match(line,"^([^ ]+) lladdr ([^ ]+) .*$")
        update_rules(family,"add",mac,{ address = ip })
      end
      cmd:close()
    end
  end

  uloop.init()

  events['network.neigh'] = function(data)
    if data and data['mac-address'] and data["interface"] == "br-lan" and data['action'] == "add" then
      if data['ipv4-address'] then
        update_rules('4',data['action'],data['mac-address'],data['ipv4-address'])
      end
      if data['ipv6-address'] then
        update_rules('6',data['action'],data['mac-address'],data['ipv6-address'])
      end
    end
  end
  conn:listen(events)

  log:notice("Adding existing devices")
  add_existing('4')
  add_existing('6')

  log:notice("Waiting for device changes...")
  while true do
    uloop.run()
  end
else
  log:error("Failed to connect to ubusd - aborting")
  os.exit(2)
end

