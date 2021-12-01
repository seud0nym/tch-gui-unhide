--pretranslated: do not change this file

gettext.textdomain('webui-qos')

local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local uimessage_helper = require("web.uimessage_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local rule_columns = {
  { --[1]
    header = T"Target",
    name = "target",
    param = "target",
    type = "text",
  },
  { --[2]
    header = T"Protocol",
    name = "proto",
    param = "proto",
    type = "text",
  },
  { --[3]
    header = T"Destination Ports",
    name = "destports",
    param = "destports",
    type = "text",
  },
  { --[4]
    header = T"Priority",
    name = "priority",
    param = "priority",
    type = "text",
  },
}

local rule_options = {
  tableid = "rules",
  basepath = "uci.iqos.rule.@.",
  canAdd = false,
  canDelete = false,
  canEdit = false,
}

local rule_data,rule_helpmsg = post_helper.handleTableQuery(rule_columns,rule_options,nil,nil,nil)

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

ngx.print(ui_helper.createHeader(T"QoS",false)); ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-qos.lp");ngx.print('\
  <form class="form-horizontal" method="post" action="'); ngx.print(uri); ngx.print('">\
  '); ngx.print(ui_helper.createMessages(uimessage_helper.popMessages())); ngx.print('\
    <fieldset>\
      <legend>'); ngx.print(T"BCM Ingress QoS Control Rules"); ngx.print('</legend>\
      ')
      ngx.print(ui_helper.createTable(rule_columns,rule_data,rule_options,nil,rule_helpmsg))
      ngx.print('\
    <fieldset>\
  </form>\
</div>\
'); ngx.print( ui_helper.createFooter() );
