--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")

local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local content = {
  activebank = "rpc.system.fwupgrade.nextbootbank",
  bootedbank = "rpc.system.fwupgrade.activebank",
  bootedversion = "rpc.system.fwupgrade.activeversion",
  notbootedbank = "rpc.system.fwupgrade.passivebank",
  notbootedversion = "rpc.system.fwupgrade.passiveversion",
  optimal = "rpc.system.fwupgrade.optimalbankplan",
  bootedversionsuffix = "uci.versioncusto.override.fwversion_suffix",
}

content_helper.getExactContent(content)
content.bootedversion = T(match(untaint(content.bootedversion),"^(%d[%a%d%.]+)%-.*") or "Unknown")
content.notbootedversion = T(match(untaint(content.notbootedversion),"^(%d[%a%d%.]+)%-.*") or "Unknown")

local function bank_status(bank)
  local number = match(bank,"bank_([%d])")
  local state,text
  if content.bootedbank == bank then
    if content.optimal == "1" then
      state = "1"
    else
      state = "2"
    end
    text = "Booted"
  else
    state = "0"
    text = "Not Booted"
  end
  if content.activebank == bank then
    text = "Active and "..text
  end
ngx.print(ui_helper.createSimpleLight(state,T(format("Bank %s: %s",number,text))))
  ngx.print('<p class="subinfos">')
  if content.bootedbank == bank then
    ngx.print('<span class="simple-desc"><i class="icon-star status-icon"></i>Firmware: <strong>',content.bootedversion,content.bootedversionsuffix,'</strong></span>')
  else
    ngx.print('<span class="simple-desc"><i class="icon-star-empty status-icon"></i>Firmware: ',content.notbootedversion,'</span>')
  end
  ngx.print('</p>')
end

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ',ui_helper.createCardHeader(T"Firmware Banks",nil,nil,nil,{icon={class="icon-building"}}),'\
    <div class="content card_bg" data-bg-text="&#xf0f7;">\
      <div class="divtable">')
        if content.optimal == "1" then
          ngx.print(ui_helper.createSimpleLight(content.optimal,T"Bank Plan is <strong>OPTIMAL</strong>"))
        else
          ngx.print(ui_helper.createSimpleLight(content.optimal,T"Bank Plan is <strong>SUB-OPTIMAL</strong>"))
        end
        bank_status("bank_1")
        bank_status("bank_2")
        ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
');
