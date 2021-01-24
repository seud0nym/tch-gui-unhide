--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local session = ngx.ctx.session
local format = string.format
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")

local wan_ifname = proxy.get("uci.network.interface.@wan.ifname")
local qosEnabled = "0"
local qosState
local modal_link

if wan_ifname and wan_ifname[1] and wan_ifname[1].value then
  wan_ifname = wan_ifname[1].value:untaint()
end

if wan_ifname and wan_ifname ~= "" then
  local content_helper = require("web.content_helper")

  if wan_ifname ~= "eth4" and string.find(wan_ifname, "eth4") then
    wan_ifname = "eth4"
  end

  -- Menu display information (policy classgroup name MUST match value in qos config)
  local dataQoS = {
    enabled = "uci.qos.device.@" .. wan_ifname .. ".enable",
    numQueues   = "uci.qos.classNumberOfEntries",
    numClassify = "uci.qos.classifyNumberOfEntries",
  }

  if not proxy.get(dataQoS.enabled) then
    proxy.set(dataQoS.enabled, "0")
    proxy.apply()
  end

  content_helper.getExactContent(dataQoS)

  if dataQoS.enabled == "1" then
    qosEnabled = "1"
    qosState = T"QoS enabled"
  else
    qosState = T"QoS disabled"
  end

  if session:hasAccess("/modals/qos-queue-modal.lp") and session:hasAccess("/modals/qos-classify-modal.lp") then
    modal_link = "/modals/qos-queue-modal.lp"
  end
else
  qosState = T"QoS disabled (No WAN interface!)"
end

ngx.print('\
<div class="span3">\
  <div class="smallcard">\
      '); ngx.print( ui_helper.createCardHeader(T"QoS", modal_link) ); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf072;">\
      <div class="divtable">\
        ');
        ngx.print(
          ui_helper.createSimpleLight(qosEnabled, qosState)
        )
        if qosEnabled == "1" then
          local queue_link='class="modal-link" data-toggle="modal" data-remote="modals/qos-queue-modal.lp" data-id="device-modal"'
          local class_link='class="modal-link" data-toggle="modal" data-remote="modals/qos-classify-modal.lp" data-id="device-modal"'
          local queues = tonumber(dataQoS.numQueues)
          local classify = tonumber(dataQoS.numClassify)
          if not queues then
            queues = 0
          end
          if not classify then
            classify = 0
          end
          ngx.print(
            '<p class="subinfos">',
            format(N("<strong %1$s>%2$d queue</strong> active","<strong %1$s>%2$d queues</strong> active",queues),queue_link,queues),
            '</p>',
            '<p class="subinfos">',
            format(N("<strong %1$s>%2$d classify rule</strong> active","<strong %1$s>%2$d classify rules</strong> active",classify),class_link,classify),
            '</p>'
          )
        end
        ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
');
ngx.print('\
');
