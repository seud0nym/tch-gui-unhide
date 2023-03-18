--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean
local vSIP = post_helper.validateStringIsPort

local proto_choices = {
  {"TCP",T"TCP"},
  {"UDP",T"UDP"},
}

local iperf_params = {
    iperf_enable = "uci.iperf.iperf.enabled",
    iperf_port = "uci.iperf.iperf.dest_port",
    iperf_proto = "uci.iperf.iperf.proto",
  }
local iperf_valid = {
    iperf_enable = vB,
    iperf_port = vSIP,
    iperf_proto = gVIES(proto_choices),
  }
local iperf_data,iperf_helpmsg = post_helper.handleQuery(iperf_params,iperf_valid)

local switch_class_enable = {
  input = {
    class = "monitor-changes",
  }
}

local iperf_input = {
  group = {
    class ="monitor-iperf_enable monitor-1",
  },
}
local port_attributes = {
  group = {
    class ="monitor-iperf_enable monitor-1",
  },
  input = {
    type = "number",
    min = "1",
    max = "65535",
    style = "width:70px",
  }
}

ngx.print(ui_helper.createHeader(T"System Extras",false,false),'\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-system-extras.lp")
ngx.print(
  ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal" method="post" action="modals/system-iperf-modal.lp">\
    <fieldset>\
      <legend>',T"iperf",'</legend>',
        ui_helper.createSwitch(T"iperf Server","iperf_enable",iperf_data["iperf_enable"],switch_class_enable),
        ui_helper.createInputText(T"Port","iperf_port",iperf_data["iperf_port"],port_attributes,iperf_helpmsg["iperf_port"]),
        ui_helper.createInputSelect(T"Protocol","iperf_proto",proto_choices,iperf_data["iperf_proto"],iperf_input,iperf_helpmsg["iperf_proto"]),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter())
