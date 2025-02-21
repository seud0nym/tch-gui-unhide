--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ngx,string = ngx,string
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local format = string.format

local gOV = post_helper.getOptionalValidation
local gVIPIL = post_helper.getValidationIfPropInList
local vB = post_helper.validateBoolean
local vSII = post_helper.validateStringIsIP
local vSIPR = post_helper.validateStringIsPortRange

local syslog_present,syslog_filter_sw = "0","0"
local syslog = proxy.get("uci.system.system.@system[0].log_ip","uci.system.system.@system[0].log_filter_ip")
if syslog[1].value ~= "" or syslog[2].value ~= "" then
  syslog_present = "1"
  if syslog[2].value ~= "" then
    syslog_filter_sw = "1"
  end
end

if ngx.var.request_method == "POST" then
  local vars = ngx.req.get_post_args()
  if vars["action"] == "SAVE" then
    syslog_present = vars["syslog_present"]
    syslog_filter_sw = vars["syslog_filter_sw"]
    proxy.set("uci.system.system.@system[0].log_ip","")
    proxy.set("uci.system.system.@system[0].log_filter_ip","")
    proxy.apply()
  end
end

local function validateFilter(_,_,_)
  return true
end

local filter_columns = {
  {
    header = T"Filter",
    name = "filter_string",
    param = "value",
    type = "text",
    attr = { input = { class="no-save" } },
  }
}
local filter_valid = {
  filter_string = validateFilter
}
local filter_options = {
  canEdit = syslog_present == "1",
  canAdd = syslog_present == "1",
  canDelete = syslog_present == "1",
  tableid = "syslog_filters",
  basepath = "uci.system.system.@system[0].log_filter.@.",
  createMsg = T"Add Syslog Filter",
  minEntries = 1,
  maxEntries = 10,
}
local filter_data,filter_helpmsg = post_helper.handleTableQuery(filter_columns,filter_options,nil,nil,filter_valid)

local syslog_params = {
  syslog_filter_ip = "uci.system.system.@system[0].log_filter_ip",
  syslog_port = "uci.system.system.@system[0].log_port",
  syslog_ip = "uci.system.system.@system[0].log_ip",
}
local syslog_valid = {
  syslog_present = vB,
  syslog_filter_sw = vB,
  syslog_ip = gVIPIL(gVIPIL(gOV(vSII),"syslog_filter_sw",{"0"}),"syslog_present",{"1"}),
  syslog_filter_ip = gVIPIL(gVIPIL(gOV(vSII),"syslog_filter_sw",{"1"}),"syslog_present",{"1"}),
  syslog_port = gVIPIL(vSIPR,"syslog_present",{"1"}),
}
local syslog_data,syslog_helpmsg = post_helper.handleQuery(syslog_params,syslog_valid)
if syslog_data.syslog_port == "" then
  syslog_data.syslog_port = "514"
end
if not syslog_helpmsg then
  syslog_helpmsg = {}
end

local switch_class_enable = {
  input = {
    class = "monitor-changes",
  }
}
local switch_class_filter = {
  group = {
    class ="monitor-syslog_present monitor-1",
  },
  input = {
    class = "monitor-changes",
  }
}
local sysloginput = {
  group = {
    class ="monitor-syslog_present monitor-1",
  },
  input = {
    maxlength= "15",
    class = "span2",
  },
}
local sysloginputip_fltr = {
  group = {
    class ="monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1",
  },
  input = {
    maxlength= "15",
    class = "span2",
  },
}
local sysloginputip_fltr_fltr = {
  group = {
    class ="monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1",
  },
  input = {
    maxlength= "50",
    class = "span5",
    },
}
local filter_warning = {
  alert = {
    class = "alert-info monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1"
  },
}

ngx.print(ui_helper.createHeader(T"System Extras",false,false),'\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-system-extras.lp")
ngx.print(
  ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal" method="post" action="modals/system-syslog-modal.lp">\
    <fieldset>\
      <legend>',T"System Log",'</legend>',
        ui_helper.createSwitch(T"Enable Syslog","syslog_present",syslog_present,switch_class_enable),'\
        <div class="monitor-syslog_filter_sw monitor-0 monitor-hide-syslog_filter_sw">',
          ui_helper.createInputText(T"Server IP","syslog_ip",syslog_data["syslog_ip"],sysloginput,syslog_helpmsg["syslog_ip"]),'\
        </div>',
        ui_helper.createInputText(T"Server IP","syslog_filter_ip",syslog_data["syslog_filter_ip"],sysloginputip_fltr,syslog_helpmsg["syslog_filter_ip"]),
        ui_helper.createSwitch(T"Enable Filter","syslog_filter_sw",syslog_filter_sw,switch_class_filter),
        ui_helper.createAlertBlock(T"Every filter is a regular expression string; Start with '!' to invert selection",filter_warning),
        format('<div class="control-group"><label class="control-label monitor-syslog_present monitor-1 monitor-syslog_filter_sw monitor-1">%s</label><div class="controls">',T"Syslog Filters"),
          ui_helper.createTable(filter_columns,filter_data,filter_options,sysloginputip_fltr_fltr,filter_helpmsg),
        '</div></div>',
        ui_helper.createInputText(T"Server Port","syslog_port",syslog_data["syslog_port"],sysloginput,syslog_helpmsg["syslog_port"]),'\
    </fieldset>\
  </form>\
</div>\
<script>\
(function() {\
  $("[name=syslog_present]").on("change",function(){\
    $("[name=syslog_ip]").val("");\
    $("[name=syslog_filter_ip]").val("");\
  });\
  $("[name=syslog_filter_sw]").on("change",function(){\
     if ($( this ).val() == "0"){\
       $("[name=syslog_ip]").val($("[name=syslog_filter_ip]").val());\
       $("[name=syslog_filter_ip]").val("");\
       $("[name=syslog_filter]").val("daemon");\
     }\
     else{\
       $("[name=syslog_filter_ip]").val($("[name=syslog_ip]").val());\
       $("[name=syslog_ip]").val("");\
       $("[name=syslog_filter]").val("daemon");\
     }\
  });\
}());\
</script>\
',ui_helper.createFooter())
