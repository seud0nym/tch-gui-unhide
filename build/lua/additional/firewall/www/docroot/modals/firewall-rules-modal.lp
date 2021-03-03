--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
local fwrules_helper = require("firewall-rules_helper")
local format, untaint = string.format, string.untaint
local allowEditRuleEnabled = true

local lp = require("web.lp")
lp.setpath("/www/snippets/")

if ngx.var.request_method == "POST" then
  local action = ngx.req.get_post_args()["action"]
  if action == "SAVE" or action == "TABLE-ADD" or action == "TABLE-MODIFY" or action == "TABLE-DELETE"  then
    proxy.set("rpc.gui.firewall.clearCache", "1")
  end
end

local fw_level = untaint(proxy.get("rpc.network.firewall.mode")[1].value)
local fw_levels = {
  lax = T"Low",
  normal = T"Normal",
  high = T"High",
  user = T"User Defined"
}

local fw_rule_options = {
  tableid = "fw_rules",
  basepath = "uci.firewall.rule.@.",
  canAdd = false,
  canEdit = allowEditRuleEnabled,
  canDelete = false,
}
local fw_dflt_rule_options = {
  tableid = "fw_dflt_rules",
  basepath = "uci.firewall.defaultrule.@.",
  canAdd = false,
  canEdit = allowEditRuleEnabled,
  canDelete = false,
}

local fw_rule_columns, fw_rule_data, fw_rule_helpmsg = fwrules_helper.handleTableQuery(fw_rule_options)
local fw_dflt_rule_columns, fw_dflt_rule_data, fw_dflt_rule_helpmsg = fwrules_helper.handleTableQuery(fw_dflt_rule_options)

local info_attr = {
  alert = {
    class = "alert-info"
  },
}

ngx.print(ui_helper.createHeader(T"Firewall", true, false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-rules-modal.lp">\
    '); 
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>');  ngx.print( T"Default Rules" ); ngx.print('</legend>');
    ngx.print(ui_helper.createTable(fw_dflt_rule_columns, fw_dflt_rule_data, fw_dflt_rule_options, nil, fw_dflt_rule_helpmsg))
    ngx.print('<legend>');  ngx.print( T"System Defined Rules" ); ngx.print('</legend>');
    ngx.print(ui_helper.createTable(fw_rule_columns, fw_rule_data, fw_rule_options, nil, fw_rule_helpmsg))
    ngx.print('<legend>'); ngx.print(fw_levels[fw_level]);  ngx.print( T" Level Rules" ); ngx.print('</legend>');
    if fw_level == "user" then
      ngx.print(ui_helper.createAlertBlock(T"User Defined rules can be modified on the <i>User Defined Rules</i> tab.", info_attr))
    else
      local fw_lvl_rule_options = {
        tableid = format("fw_%s_rules", fw_level),
        basepath = format("uci.firewall.%srule.@.", fw_level),
        canAdd = false,
        canEdit = allowEditRuleEnabled,
        canDelete = false,
      }
      local fw_lvl_rule_columns, fw_lvl_rule_data, fw_lvl_rule_helpmsg = fwrules_helper.handleTableQuery(fw_lvl_rule_options)
      ngx.print(ui_helper.createTable(fw_lvl_rule_columns, fw_lvl_rule_data, fw_lvl_rule_options, nil, fw_lvl_rule_helpmsg))
    end
    ngx.print('\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter());