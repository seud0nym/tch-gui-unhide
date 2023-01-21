--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ngx = ngx
local session = ngx.ctx.session
local format = string.format
local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local modalPath = "/modals/system-modal.lp"
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local isBridgedMode = require("bridgedmode_helper").isBridgedMode()

local syslog = proxy.get("uci.system.system.@system[0].log_ip","uci.system.system.@system[0].log_filter_ip")
local ssh_lan = proxy.get("uci.dropbear.dropbear.@lan.enable")
local ssh_afg = proxy.get("uci.dropbear.dropbear.@afg.enable")
local iperf = proxy.get("uci.iperf.iperf.enabled")

local state_map = {
  ["0"] = "disabled",
  ["1"] = "enabled",
}

local function makeLight(setting,default,prefix)
  if setting then
    local value
    if type(setting) == "table" then
      value = untaint(setting[1].value)
    else
      value = setting
    end
    if value == "" then
      value = default
    end
    return ui_helper.createSimpleLight(value,T(format("%s %s",prefix,state_map[value])))
  end
end

if session:hasAccess("/modals/system-modal.lp") then
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"System Extras",modalPath,nil,nil,{icon={class="icon-cogs"}}),'\
    <div class="content card_bg" data-bg-text="&#xf085;">\
      ');
      local html = {}
      if ssh_lan then
        html[#html+1] = makeLight(ssh_lan,"1","SSH LAN")
      end
      if ssh_afg then
        local ssh_afg_if = proxy.get("uci.dropbear.dropbear.@afg.Interface")
        if ssh_afg_if then
          html[#html+1] = makeLight(ssh_afg,"1",format("SSH AFG (%s)",string.upper(ssh_afg_if[1].value)))
        else
          html[#html+1] = makeLight(ssh_afg,"1","SSH AFG")
        end
      end
      if not isBridgedMode then
        local ssh_wan = proxy.get("uci.dropbear.dropbear.@wan.enable")
        html[#html+1] = makeLight(ssh_wan,"1","SSH WAN")
      end
      if syslog then
        if (syslog[1].value ~= "" or syslog[2].value ~= "") then
          html[#html+1] = makeLight("1","0","syslog")
        else
          html[#html+1] = makeLight("0","0","syslog")
        end
      end
      if iperf then
        html[#html+1] = makeLight(iperf,"0","iperf")
      end
      ngx.print(html)
      ngx.print('\
    </div>\
  </div>\
</div>\
');
end
  ngx.print('\
');