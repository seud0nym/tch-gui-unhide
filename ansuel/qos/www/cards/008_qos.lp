--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local session = ngx.ctx.session
local format = string.format
local proxy = require("datamodel")
local wan_ifname = proxy.get("uci.network.interface.@wan.ifname")

if wan_ifname and wan_ifname ~= "" and session:hasAccess("/modals/qos-queue-modal.lp") and session:hasAccess("/modals/qos-classify-modal.lp") then

local modal_link = "/modals/qos-queue-modal.lp"
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")

wan_ifname = wan_ifname[1].value:untaint()

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
local queue_link='class="modal-link" data-toggle="modal" data-remote="modals/qos-queue-modal.lp" data-id="device-modal"'
local class_link='class="modal-link" data-toggle="modal" data-remote="modals/qos-classify-modal.lp" data-id="device-modal"'

local qosState
if dataQoS.enabled == "1" then
  qosState = T"QoS on interface " .. wan_ifname .. " enabled"
else
  qosState = T"QoS on interface " .. wan_ifname .. " disabled"
end

ngx.print('\
<div class="span3">\
    <div class="smallcard">\
        '); ngx.print( ui_helper.createCardHeader(T"QoS", modal_link) ); ngx.print('\
        <div class="content card_bg" data-bg-text="&#xf072;">\
		<div class="divtable">\
            ');
            ngx.print(
                ui_helper.createSimpleLight(dataQoS.enabled, qosState),
                '<p class="subinfos">',
                format(N("<strong %1$s>%2$d queue</strong> active","<strong %1$s>%2$d queues</strong> active",dataQoS.numQueues),queue_link,dataQoS.numQueues),
                '</p>',
                '<p class="subinfos">',
                format(N("<strong %1$s>%2$d classify rule</strong> active","<strong %1$s>%2$d classify rules</strong> active",dataQoS.numClassify),class_link,dataQoS.numQueues),
                '</p>'
            )
            ngx.print('\
        </div>\
		</div>\
    </div>\
</div>\
');
end
  ngx.print('\
');
