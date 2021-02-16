--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")
local fwrules_helper = require("firewall-rules_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

if ngx.var.request_method == "POST" then
  local action = ngx.req.get_post_args()["action"]
  if action == "SAVE" or action == "TABLE-ADD" or action == "TABLE-MODIFY" or action == "TABLE-DELETE"  then
    proxy.set("rpc.gui.firewall.clearCache", "1")
  end
end

local userLevelEnabled = (proxy.get("rpc.network.firewall.mode")[1].value == "user")

local fwrule_options = {
  tableid = "fwrules",
  basepath = "rpc.network.firewall.userrule.@.",
  createMsg = T"Add new IPv4 firewall rule",
  canAdd = userLevelEnabled,
  canEdit = userLevelEnabled,
  canDelete = userLevelEnabled,
  sorted = fwrules_helper.fwrule_sort
}

local fwrule_defaultObject = {
  family = "ipv4",
}

local fwrule_columns, fwrule_data, fwrule_helpmsg = fwrules_helper.handleTableQuery(fwrule_options, fwrule_defaultObject)

local fwrule_v6_options = {
  tableid = "fwrules_v6",
  basepath = "rpc.network.firewall.userrule_v6.@.",
  createMsg = T"Add new IPv6 firewall rule",
  canAdd = userLevelEnabled,
  canEdit = userLevelEnabled,
  canDelete = userLevelEnabled,
  sorted = fwrules_helper.fwrule_sort
}

local fwrule_v6_defaultObject = {
  family = "ipv6",
}

local fwrule_v6_columns, fwrule_v6_data, fwrule_v6_helpmsg = fwrules_helper.handleTableQuery(fwrule_v6_options, fwrule_v6_defaultObject)

local info_attr = {
  alert = {
    class = "alert-info"
  },
}

ngx.print(ui_helper.createHeader(T"Firewall", true, false))   ngx.print('\
<div class="modal-body update">\
  '); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-user-rules-modal.lp">\
    '); 
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    if not userLevelEnabled then
      ngx.print(ui_helper.createAlertBlock(T"User Defined Rules are only editable and only applied when the Firewall Level is configured as User-Defined.", info_attr))
    end
    ngx.print('<legend>');  ngx.print( T"User Defined IPv4 Firewall Rules" ); ngx.print('</legend>');
    ngx.print(ui_helper.createTable(fwrule_columns, fwrule_data, fwrule_options, nil, fwrule_helpmsg))
    ngx.print('<legend>');  ngx.print( T"User Defined IPv6 Firewall Rules" ); ngx.print('</legend>');
    ngx.print(ui_helper.createTable(fwrule_v6_columns, fwrule_v6_data, fwrule_v6_options, nil, fwrule_v6_helpmsg))
    ngx.print('\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter()); ngx.print('\
<script>\
  (function() {\
    $(document).on("change focusin", \'.modal .line-edit\', function() {\
      var proto = $("#protocol option:selected").val();\
      if (typeof proto != \'undefined\') {\
        if (proto == "tcp" || proto == "udp" || proto == "tcpudp") {\
          $("#src_port").removeAttr("readonly");\
          $("#dest_port").removeAttr("readonly");\
        } else {\
          $("#src_port").attr("readonly", true);\
          $("#src_port").val("");\
          $("#dest_port").attr("readonly", true);\
          $("#dest_port").val("");\
        }\
      }\
      var proto_v6 = $("#protocol_v6 option:selected").val();\
      if (typeof proto_v6 != \'undefined\') {\
        if (proto_v6 == "tcp" || proto_v6 == "udp" || proto_v6 == "tcpudp") {\
          $("#src_port_v6").removeAttr("readonly");\
          $("#dest_port_v6").removeAttr("readonly");\
        } else {\
          $("#src_port_v6").attr("readonly", true);\
          $("#src_port_v6").val("");\
          $("#dest_port_v6").attr("readonly", true);\
          $("#dest_port_v6").val("");\
        }\
      }\
    });\
  }());\
</script>\
');
