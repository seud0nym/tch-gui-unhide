--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ngx = ngx
local session = ngx.ctx.session
local format = string.format
local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local modalPath = "/modals/system-reboot-modal.lp"
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local isBridgedMode = require("bridgedmode_helper").isBridgedMode()

local state_map = {
  ["0"] = "disabled",
  ["1"] = "enabled",
}

local function makeLight(setting,default,prefix,modal)
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
    return ui_helper.createSimpleLight(value,T(format('<span class="modal-link" data-toggle="modal" data-remote="/modals/system-%s-modal.lp" data-id="routes-modal">%s</span> %s',modal,prefix,state_map[value])))
  end
  return nil
end

if session:hasAccess(modalPath) then
  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"System Extras",modalPath,nil,nil,{icon={class="icon-cogs"}}),'\
    <div class="content card_bg" data-bg-text="&#xf085;">\
      ');
      local html = {}
      html[#html+1] = makeLight(proxy.get("uci.dropbear.dropbear.@lan.enable"),"1","SSH LAN","ssh")
      local ssh_afg = proxy.get("uci.dropbear.dropbear.@afg.enable","uci.dropbear.dropbear.@afg.Interface")
      if ssh_afg then
        html[#html+1] = makeLight(ssh_afg,"1",format("SSH AFG (%s)",string.upper(ssh_afg[2].value)),"ssh")
      end
      if not isBridgedMode then
        local ssh_wan = proxy.get("uci.dropbear.dropbear.@wan.enable","uci.dropbear.dropbear.@wan.Interface")
        if ssh_wan then
          if ssh_wan[2].value == "wwan" then
            html[#html+1] = makeLight(ssh_wan,"1","SSH WWAN","ssh")
          else
            html[#html+1] = makeLight(ssh_wan,"1","SSH WAN","ssh")
          end
        end
      end
      html[#html+1] = makeLight(proxy.get("uci.dhcp.dnsmasq.@main.enable_tftp"),"0","TFTP Server","tftp")
      local syslog = proxy.get("uci.system.system.@system[0].log_ip","uci.system.system.@system[0].log_filter_ip")
      if syslog then
        if syslog[1].value ~= "" then
          html[#html+1] = makeLight("1","0",format("syslog (%s)",syslog[1].value),"syslog")
        elseif syslog[2].value ~= "" then
          html[#html+1] = makeLight("1","0",format("syslog (%s)",syslog[2].value),"syslog")
        else
          html[#html+1] = makeLight("0","0","syslog","syslog")
        end
      end
      html[#html+1] = makeLight(proxy.get("uci.iperf.iperf.enabled"),"0","iperf","iperf")
      ngx.print(html)
      ngx.print('\
    </div>\
  </div>\
</div>\
');
end
