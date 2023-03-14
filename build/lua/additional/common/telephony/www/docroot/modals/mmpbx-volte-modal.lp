--pretranslated: do not change this file

-- Enable localization
gettext.textdomain("webui-voice")

local content_helper = require ("web.content_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")

local mapParams = {
  enabled = "Device.Services.X_TELSTRA_VOLTE.Enable",
}
local mapValid = {
  enabled = post_helper.validateBoolean,
}
local volte = post_helper.handleQuery(mapParams,mapValid)

local content = {
  wsens = "uci.wansensing.global.enable",
  registration_status = "rpc.mobiled.device.@1.voice.info.volte.registration_status",
  cs_emergency = "rpc.mobiled.device.@1.voice.network_capabilities.cs.emergency",
  volte_emergency = "rpc.mobiled.device.@1.voice.network_capabilities.volte.emergency",
}
content_helper.getExactContent(content)

local status_message = ""
local apn = ""
if content["registration_status"] == "registered" then
  status_message = "Your phone service is connected and working normally."
  apn = "ims"
else
  if content["cs_emergency"] == "true" or content["volte_emergency"] == "true" then
    status_message = "Your phone service is available for emergency calls only."
    apn = "sos"
  else
    status_message = "No signal, your phone service is not connected."
  end
end

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

local switch_class_enable = {
  input = {
    class = "monitor-changes",
  },
}
local enabled_monitor = {
  group = {
    class = "monitor-enabled monitor-1",
  },
  span = {
    class = "span5"
  },
}

ngx.print(ui_helper.createHeader(T"Telephony",false,true),'\
<div class="modal-body update">\
');
local lp = require("web.lp")
lp.setpath("/www/snippets/")
lp.include("tabs-voice.lp")
ngx.print('\
  <form class="form-horizontal" method="post" action="',uri,'">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal">\
    <fieldset>\
      <legend>',T"VoLTE Service",'</legend>')
        if content.wsens == "1" and content["registration_status"] ~= "registered" then
          local danger_box = {
            group = {
              class = "monitor-enabled monitor-1",
            },
            alert = {
              class = "alert alert-danger",
            },
          }
          ngx.print(ui_helper.createAlertBlock(T"WARNING: VoLTE may be incompatible with WAN Sensing! To use VoLTE, you should enable either Permanent WWAN or Mobile Mode Only.",danger_box))
        end
        ngx.print(
        ui_helper.createSwitch(T"VoLTE Enabled","enabled",volte["enabled"],switch_class_enable),
        ui_helper.createLabel(T"VoLTE Service Status",status_message,enabled_monitor),
        ui_helper.createLabel(T"VoLTE APN",apn,enabled_monitor),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter())
