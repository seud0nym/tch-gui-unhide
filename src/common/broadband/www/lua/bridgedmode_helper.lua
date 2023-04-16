local require, ipairs = require, ipairs
local find,format = string.find,string.format
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local M = {}

local function logErrors(source,errors)
  if errors then
    for _,e in ipairs(errors) do
      ngx.log(ngx.ERR,format("%s: [%s] %s",source,e.errcode,e.errmsg))
    end
  end
end

local function configGuestFirewall(value)
  local success = true
  local errors
  local rules = proxy.get("uci.firewall.rule.")
  for _,v in ipairs(rules) do
    if success and v.param == "name" and find(untaint(v.value),"Guest") then
      success,errors = proxy.set(v.path.."enabled", value)
      ngx.log(ngx.WARN,format("configGuestFirewall: Setting %senabled = %s (Result=%s)",v.path,value,tostring(success)))
      if not success then
        logErrors(errors)
        success = false
        break
      end
    end
  end
  return success
end

local function failed(message)
  ngx.header.content_type = "application/json"
  ngx.print('{ "success":false, "message":"'..message..'" }')
  ngx.timer.at(0,function(_)
    proxy.apply()
  end)
  ngx.exit(ngx.HTTP_OK)
end

local function reboot()
  proxy.set("rpc.system.reboot","GUI")
  ngx.header.content_type = "application/json"
  ngx.print('{ "success":true }')
  ngx.timer.at(0,function(_)
    proxy.apply()
  end)
  ngx.exit(ngx.HTTP_OK)
end

function M.maskCIDRToDottedDecimal(cidr)
  local arrayIP = {0,0,0,0}
  local n = tonumber(untaint(cidr))
  if n and n >= 0 and n <= 32 then
    local int = math.floor(n+0.5)
    local divisionWhole = math.floor((int-1)/8)
    local cidrFraction = int-(8*divisionWhole)
    for i = 0,3,1 do
      if divisionWhole == i then
        arrayIP[i+1] = 256-math.pow(2,(8-cidrFraction))
      elseif divisionWhole > i then
        arrayIP[i+1] = 255
      else
        arrayIP[i+1] = 0
      end
    end
  end
  return table.concat(arrayIP,".")
end

function M.addBridgedModeButtons(html)
  local bridged_rebooting = {
    alert = {
      class = "alert hide",
      id = "bridged-rebooting-msg"
    }
  }
  local bridged_confirming = {
    alert = {
      class = "alert hide",
      id = "bridged-confirming-msg"
    }
  }
  local bridged_button = {
    button = {
      id = "btn-bridged"
    }
  }
  html[#html + 1] = ui_helper.createButton("Change Mode","Bridged","icon-cog",bridged_button)
  html[#html + 1] = '<div class="control-group controls">'
  html[#html + 1] = ui_helper.createAlertBlock(T"Switching to <strong>Bridged Mode</strong> and restarting. Please wait...",bridged_rebooting)
  html[#html + 1] = ui_helper.createAlertBlock(T"Are you sure you want to switch to <strong>Bridged Mode</strong>?",bridged_confirming)
  html[#html + 1] = string.format([[
    <div id="bridged-changes" class="hide">
      <div id="bridged-confirm" class="btn btn-primary" data-dismiss="modal">%s</div>
      <div id="bridged-cancel" class="btn">%s</div>
    </div>
  </div>
  ]],T"Confirm",T"Cancel")
end

function M.addRoutedModeButtons(html)
  local routed_rebooting = {
    alert = {
      class = "alert hide",
      id = "routed-rebooting-msg"
    }
  }
  local routed_confirming = {
    alert = {
      class = "alert hide",
      id = "routed-confirming-msg"
    }
  }
  local routed_button = {
    button = {
      id = "btn-routed"
    }
  }
  html[#html + 1] = ui_helper.createButton("Change Mode","Routed","icon-cog",routed_button)
  html[#html + 1] = '<div class="control-group controls">'
  html[#html + 1] = ui_helper.createAlertBlock(T"Switching to <strong>Routed Mode</strong> and restarting. Please wait...",routed_rebooting)
  html[#html + 1] = ui_helper.createAlertBlock(T"Are you sure you want to switch back to <strong>Routed Mode</strong>?",routed_confirming)
  html[#html + 1] = string.format([[
    <div id="routed-changes" class="hide">
      <div id="routed-confirm" class="btn btn-primary" data-dismiss="modal">%s</div>
      <div id="routed-cancel" class="btn">%s</div>
    </div>
  </div>
  ]],T"Confirm",T"Cancel")
end

function M.isBridgedMode()
  local wan_mode = proxy.get("uci.network.config.wan_mode")
  return wan_mode and wan_mode[1].value == "bridge"
end

function M.configBridgedMode()
  local success,errors = proxy.set({
    ["uci.wansensing.global.enable"] = "0",
    ["uci.network.interface.@lan.ifname"] = "eth0 eth1 eth2 eth3 eth4 atm_8_35 ptm0",
    ["uci.network.config.wan_mode"] = "bridge",
    ["uci.dhcp.dhcp.@lan.ignore"] = "1",
  })

  ngx.log(ngx.WARN,format("configBridgedMode: Configuring WAN Sensing, LAN, WAN Mode and DHCP (Result=%s)",tostring(success)))

  if success then
    if not proxy.getPN("uci.network.interface.@lan6.",true) then
      local added,errs = proxy.add("uci.network.interface.","lan6")
      if added then
        ngx.log(ngx.WARN,format("configBridgedMode: Adding LAN6 (Result=%s)",tostring(added)))
        local ok,err = proxy.set({
          ["uci.network.interface.@lan6.forceprefix"] = "0",
          ["uci.network.interface.@lan6.iface_464xlat"] = "0",
          ["uci.network.interface.@lan6.ifname"] = "br-lan",
          ["uci.network.interface.@lan6.noslaaconly"] = "1",
          ["uci.network.interface.@lan6.peerdns"] = "1",
          ["uci.network.interface.@lan6.proto"] = "dhcpv6",
          ["uci.network.interface.@lan6.reqaddress"] = "force",
          ["uci.network.interface.@lan6.reqopts"] = "23 17",
          ["uci.network.interface.@lan6.reqprefix"] = "no",
        })
        if ok then
          ngx.log(ngx.WARN,format("configBridgedMode: Configuring LAN6 (Result=%s)",tostring(success)))
        else
          logErrors("configBridgedMode[lan6]",err)
        end
      else
        logErrors("configBridgedMode[lan6]",errs)
      end
    end
  end

  if success then
    local delnames = { -- populated by 095-Network
    }

    for _,v in ipairs(delnames) do
      local deleted,errmsg,errcode = proxy.del(v)
      if not deleted then
        ngx.log(ngx.WARN,format("configBridgedMode: Failed to delete %s ([%s]: %s)",v,errcode,errmsg))
      end
    end

    success = success and configGuestFirewall("0")
    if success then
      local applied,errmsg,errcode = proxy.apply()
      if not applied then
        success = applied
        ngx.log(ngx.WARN,format("configBridgedMode: Failed to apply changes ([%s]: %s)",errcode,errmsg))
      end
    end
  else
    logErrors("configBridgedMode",errors)
  end

  ngx.log(ngx.WARN,format("configBridgedMode: Returning %s)",tostring(success)))

  if not success then
    M.configRoutedMode()
  end

  return success
end

function M.configRoutedMode()
  local success,errors = true,nil

  local landns = proxy.getPN("uci.network.interface.@lan.dns.", true)
  if landns then
    for _,dns in pairs(landns) do
      local deleted,errmsg,errcode = proxy.del(dns.path)
      if not deleted then
        ngx.log(ngx.WARN,format("configRoutedMode: Failed to delete %s ([%s]: %s)",dns.path,errcode,errmsg))
      end
    end
  end
  local deleted,delerrmsg,delerrcode = proxy.del("uci.network.interface.@lan6.")
  if not deleted then
    ngx.log(ngx.WARN,format("configRoutedMode: Failed to delete uci.network.interface.@lan6. ([%s]: %s)",delerrcode,delerrmsg))
  end

  local ifnames = { -- populated by 095-Network
  }

  for ifname,config in pairs(ifnames) do
    if success and not proxy.get("uci.network.interface.@" .. ifname .. ".") then
      local added,errmsg,errcode = proxy.add("uci.network.interface.", ifname)
      if not added then
        success = added
        ngx.log(ngx.WARN,format("configRoutedMode: Failed to add %s to uci.network.interface. ([%s]: %s)",ifname,errcode,errmsg))
      else
        for k,v in pairs(config) do
          local set,err = proxy.set(k,v)
          if not set then
            ngx.log(ngx.WARN,format("configRoutedMode: Failed to set %s to %s",k,v))
            logErrors("configRoutedMode",err)
          end
        end
      end
    end
  end

  if success then
    local settings = {
      ["uci.wansensing.global.enable"] = "1",
      ["uci.network.interface.@lan.ifname"] = "eth0 eth1 eth2 eth3",
      ["uci.network.interface.@lan.gateway"] = "",
      ["uci.network.config.wan_mode"] = "dhcp",
      ["uci.dhcp.dhcp.@lan.ignore"] = "0",
    }
    if proxy.get("uci.network.interface.@lan.proto")[1].value == "dhcp" then
      settings["uci.network.interface.@lan.proto"] = "static"
      local ipaddr = proxy.get("uci.network.interface.@lan.ipaddr")
      if not ipaddr or ipaddr[1].value == "" then
        settings["uci.network.interface.@lan.ipaddr"] = proxy.get("rpc.network.interface.@lan.ipaddr")[1].value
        settings["uci.network.interface.@lan.netmask"] = M.maskCIDRToDottedDecimal(proxy.get("rpc.network.interface.@lan.ipmask")[1].value)
      end
    end
    success,errors = proxy.set(settings)

    ngx.log(ngx.WARN,format("configRoutedMode: Configuring WAN Sensing, LAN, WAN Mode and DHCP (Result=%s)",tostring(success)))

    if success then
      success = success and configGuestFirewall("1")
      if success then
        local applied,errmsg,errcode = proxy.apply()
        if not applied then
          success = applied
          ngx.log(ngx.WARN,format("configRoutedMode: Failed to apply changes ([%s]: %s)",errcode,errmsg))
        end
      end
    else
      logErrors("configRoutedMode",errors)
    end
  end

  ngx.log(ngx.WARN,format("configRoutedMode: Returning %s)",tostring(success)))

  if not success then
    M.configBridgedMode()
  end

  return success
end

function M.configure(mode)
  local isBridgedMode = M.isBridgedMode()
  if mode == "bridged" then
    if not isBridgedMode then
      if M.configBridgedMode() then
        reboot()
      else
        failed("Check system log: Failed to configure bridged mode!")
      end
    else
      failed("Bridged mode already configured?")
    end
  elseif mode == "routed" then
    if isBridgedMode then
      if M.configRoutedMode() then
        reboot()
      else
        failed("Check system log: Failed to configure routed mode!")
      end
    else
      failed("Routed mode already configured!")
    end
  end
end

return M
