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
    ["uci.wansensing.global.enable"] = '0',
    ["uci.network.interface.@lan.ifname"] = 'eth0 eth1 eth2 eth3 eth4 wl0 wl0_1 wl1 wl1_1 atm_8_35 ptm0',
    ["uci.network.config.wan_mode"] = 'bridge',
    ["uci.dhcp.dhcp.@lan.ignore"] = '1',
  })

  ngx.log(ngx.WARN,format("configBridgedMode: Configuring WAN Sensing, LAN, WAN Mode and DHCP (Result=%s)",tostring(success)))

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
    success,errors = proxy.set({
      ["uci.wansensing.global.enable"] = '1',
      ["uci.network.interface.@lan.ifname"] = 'eth0 eth1 eth2 eth3',
      ["uci.network.interface.@lan.gateway"] = "",
      ["uci.network.config.wan_mode"] = "dhcp",
      ["uci.dhcp.dhcp.@lan.ignore"] = "0",
    })

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
  return success
end

function M.resetreboot(path,value)
  proxy.set(path,value)
  ngx.header.content_type = "application/json"
  ngx.print('{ "success":"true" }')
  ngx.timer.at(0,function()
    proxy.apply()
  end)
  ngx.exit(ngx.HTTP_OK)
end

return M
