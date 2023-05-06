--pretranslated: do not change this file

-- Enable localization
gettext.textdomain("webui-voice")

local content_helper = require ("web.content_helper")
local lp = require("web.lp")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")

local vB = post_helper.validateBoolean

local mapParams = {
  enabled = "Device.Services.X_TELSTRA_VOLTE.Enable",
  ims_pdn_autobringup = "uci.mobiled_device_specific.device.@device[0].ims_pdn_autobringup",
  session_activated = "uci.mobiled_sessions.session.@session[0].activated",
  session_autoconnect = "uci.mobiled_sessions.session.@session[0].autoconnect",
  session_optional = "uci.mobiled_sessions.session.@session[0].optional",
}
local mapValid = {
  enabled = vB,
  ims_pdn_autobringup = vB,
  session_activated = vB,
  session_autoconnect = vB,
  session_optional = vB,
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

lp.setpath("/www/snippets/")

ngx.print(ui_helper.createHeader(T"Telephony",false,true),'\
<div class="modal-body update">\
'); lp.include("tabs-voice.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="',uri,'">',
  ui_helper.createMessages(message_helper.popMessages()),'\
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
    <div>\
      <legend>',T"Mobile Configuration",'</legend>\
      <legend><small>',T"Device",'</small></legend>',
        ui_helper.createSwitch(T"IMS PDN Auto Bring Up","ims_pdn_autobringup",volte["ims_pdn_autobringup"]),'\
      <legend><small>',T"Session",'</small></legend>',
        ui_helper.createSwitch(T"Activated","session_activated",volte["session_activated"]),
        ui_helper.createSwitch(T"Auto Connect","session_autoconnect",volte["session_autoconnect"]),
        ui_helper.createSwitch(T"Optional","session_optional",volte["session_optional"]),'\
      </div>\
  </form>\
</div>\
',ui_helper.createFooter())
