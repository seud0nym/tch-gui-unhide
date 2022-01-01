--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local content_helper = require("web.content_helper")
local format = string.format
local ui_helper = require("web.ui_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local session = ngx.ctx.session

local modal_link = ""
local devic_link = ""
local shape_link = "" -- NOT 17.2
local bqos_link  = ""
local iqos_link  = ""

if session:hasAccess("/modals/qos-queue-modal.lp") and session:hasAccess("/modals/qos-classify-modal.lp") then
  modal_link = "/modals/qos-bqos-modal.lp"
  devic_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-device-modal.lp" data-id="qos-modal"'
  shape_link = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-swshaper-modal.lp" data-id="qos-modal"' -- NOT 17.2
  bqos_link  = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-bqos-modal.lp" data-id="qos-modal"'
  iqos_link  = 'class="modal-link" data-toggle="modal" data-remote="modals/qos-iqos-modal.lp" data-id="qos-modal"'
end

local dataQoS = {
  qos_enabled = "rpc.gui.qos.qos_enabled",
  iqos_enabled = "rpc.gui.qos.iqos_enabled",
  bqos_enabled = "uci.bqos.global.enabled",
  bqos_devices = "uci.bqos.deviceNumberOfEntries",
}
content_helper.getExactContent(dataQoS)

local status = {
  ["0"] = "disabled",
  ["1"] = "enabled",
}

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
  '); ngx.print( ui_helper.createCardHeader(T"QoS",modal_link) ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf072;">\
      <div class="divtable">\
      ');
      ngx.print(ui_helper.createSimpleLight(dataQoS.bqos_enabled,format("<strong %1$s>Bandwidth Shaping</strong> %2$s",bqos_link,status[untaint(dataQoS.bqos_enabled)])))
      if dataQoS.bqos_enabled == "1" then
        ngx.print(
          '<p class="subinfos">',
          format(N("<strong %1$s>%2$d MAC Address</strong> shaped","<strong %1$s>%2$d MAC Addresses</strong> shaped",dataQoS.bqos_devices),bqos_link,dataQoS.bqos_devices),
          '</p>'
        )
      end
      ngx.print(ui_helper.createSimpleLight(dataQoS.qos_enabled,format("<strong %1$s>Egress QoS</strong> %2$s",devic_link,status[untaint(dataQoS.qos_enabled)])))
-- NOT 17.2 START
      if dataQoS.qos_enabled == "1" then
        ngx.print('\
          <p id="shaper_info_content" class="subinfos">\
          ')
          ngx.print(format("<strong %1$s>0 Shapers</strong>",shape_link).." enabled on "..format("<strong %1$s>0 interfaces</strong>",shape_link))
          ngx.print('\
          </p>\
          <script>\
          $().ready(function(){\
            $.post("/ajax/qos-swshaper-status.lua",[tch.elementCSRFtoken()],function(data){\
              $("#shaper_info_content").html(data["html"]);\
            },"json");\
          });\
          </script>\
        ')
      end
-- NOT 17.2 END
        ngx.print(ui_helper.createSimpleLight(dataQoS.iqos_enabled,format("<strong %1$s>Ingress QoS</strong> %2$s",iqos_link,status[untaint(dataQoS.iqos_enabled)])))
        ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
');
