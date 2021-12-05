--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local proxy = require("datamodel")
local content_helper = require("web.content_helper")
local format,match = string.format,string.match
local ui_helper = require("web.ui_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local session = ngx.ctx.session

local modal_link = ""
local devic_link = ""
local shape_link = "" -- NOT 17.2
local mqos_link  = ""
local iqos_link  = ""

if session:hasAccess("/modals/qos-queue-modal.lp") and session:hasAccess("/modals/qos-classify-modal.lp") then
  modal_link = "/modals/qos-mqos-modal.lp"
  devic_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-device-modal.lp" data-id="qos-modal"'
  shape_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-swshaper-modal.lp" data-id="qos-modal"' -- NOT 17.2
  mqos_link  = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-mqos-modal.lp" data-id="qos-modal"'
  iqos_link  = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-iqos-modal.lp" data-id="qos-modal"'
end

local dataQoS = {
  qos_enabled = "rpc.gui.qos.qos_enabled",
  iqos_enabled = "rpc.gui.qos.iqos_enabled",
  mqos_enabled = "uci.mqos.global.enabled",
  mqos_devices = "uci.mqos.deviceNumberOfEntries",
  numShapers = "uci.qos.swshaperNumberOfEntries",-- NOT 17.2
}
content_helper.getExactContent(dataQoS)

local status = {
  ["0"] = "disabled",
  ["1"] = "enabled",
}

-- NOT 17.2 START
local shapers = {
  count = tonumber(dataQoS.numShapers),
  enabled = 0,
  active = 0,
  light = "0",
  names = {},
}
if not shapers.count then
  shapers.count = 0
else
  local swshapers = proxy.getPN("uci.qos.swshaper.",true)
  for _,v in ipairs(swshapers) do
    local name = match(v.path,"uci%.qos%.swshaper%.@([^%.]+)%.")
    shapers.names[name] = untaint(proxy.get(v.path.."enable")[1].value)
    if shapers.names[name] == "1" then
      shapers.enabled = shapers.enabled + 1
    end
  end
end

local ifs = require("qosdevice_helper").getNetworkDevices()
local data = proxy.getPN("uci.qos.device.",true)
for _,v in ipairs(data) do
  local device = untaint(match(v.path,"uci%.qos%.device%.@([^%.]+)%."))
  if ifs[device] and ifs[device] ~= "" and ifs[device] ~= "ppp" and ifs[device] ~= "ipoe" then
    local shaper = proxy.get(v.path.."swshaper")[1].value:untaint()
    if shapers.names[shaper] == "1" then
      shapers.active = shapers.active + 1
      shapers.light = "1"
    end
  end
end
-- NOT 17.2 END

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
  '); ngx.print( ui_helper.createCardHeader(T"QoS",modal_link) ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf072;">\
      <div class="divtable">\
      ');
      ngx.print(ui_helper.createSimpleLight(dataQoS.mqos_enabled,format("<strong %1$s>Bandwidth Shaping</strong> %2$s",mqos_link,status[untaint(dataQoS.mqos_enabled)])))
      if dataQoS.mqos_enabled == "1" then
        ngx.print(
          '<p class="subinfos">',
          format(N("<strong %1$s>%2$d MAC Address</strong> shaped","<strong %1$s>%2$d MAC Addresses</strong> shaped",dataQoS.mqos_devices),mqos_link,dataQoS.mqos_devices),
          '</p>'
        )
      end
      ngx.print(ui_helper.createSimpleLight(dataQoS.qos_enabled,format("<strong %1$s>Egress QoS</strong> %2$s",devic_link,status[untaint(dataQoS.qos_enabled)])))
-- NOT 17.2 START
      if dataQoS.qos_enabled == "1" then
        ngx.print(
          '<p class="subinfos">',
          format(N("<strong %1$s>%2$d Shaper</strong>","<strong %1$s>%2$d Shapers</strong>",shapers.enabled),shape_link,shapers.enabled).." enabled on "..format(N("<strong %1$s>%2$d interface</strong>","<strong %1$s>%2$d interfaces</strong>",shapers.active),shape_link,shapers.active),
          '</p>'
        )
      end
-- NOT 17.2 END
        ngx.print(ui_helper.createSimpleLight(dataQoS.iqos_enabled,format("<strong %1$s>Ingress QoS</strong> %2$s",iqos_link,status[untaint(dataQoS.iqos_enabled)])))
        ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
');
