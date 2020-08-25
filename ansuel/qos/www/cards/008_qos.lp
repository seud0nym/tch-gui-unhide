--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local format = string.format
local modal_link = nil

local session = ngx.ctx.session
if session:hasAccess("/modals/qos-queue-modal.lp") and session:hasAccess("/modals/qos-classify-modal.lp") then
  modal_link = "/modals/qos-queue-modal.lp"
end

local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")

-- Menu display information (policy classgroup name MUST match value in qos config)
local dataQoS = {
  numQueues   = "uci.qos.classNumberOfEntries",
  numClassify = "uci.qos.classifyNumberOfEntries",
}
content_helper.getExactContent(dataQoS)
local queue_link='class="modal-link" data-toggle="modal" data-remote="modals/qos-queue-modal.lp" data-id="device-modal"'
local class_link='class="modal-link" data-toggle="modal" data-remote="modals/qos-classify-modal.lp" data-id="device-modal"'

ngx.print('\
<div class="span3">\
    <div class="smallcard">\
        '); ngx.print( ui_helper.createCardHeader(T"QoS", modal_link) ); ngx.print('\
        <div class="content card_bg" data-bg-text="&#xf072;">\
		<div class="divtable">\
            ');
            ngx.print(
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
