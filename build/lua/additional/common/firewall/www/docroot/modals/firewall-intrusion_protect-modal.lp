--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local toupper = string.upper

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local vB = post_helper.validateBoolean
local vSII4 = post_helper.validateStringIsIPv4

local cfgParams = {
  config_enabled = "uci.intrusion_protect.config.enabled",
  whitelist_enabled = "uci.intrusion_protect.whitelist.@WhiteList.enabled",
  blacklist_enabled = "uci.intrusion_protect.blacklist.@BlackList.enabled",
  blacklist_logging = "uci.intrusion_protect.blacklist.@BlackList.logging",
}
local cfgValid = {
  config_enabled = vB,
  whitelist_enabled = vB,
  blacklist_enabled = vB,
  blacklist_logging = vB,
}

local config,config_helpmsg = post_helper.handleQuery(cfgParams,cfgValid)

local ipv4pattern = "^(((([1]?\\d)?\\d|2[0-4]\\d|25[0-5])\\.){3}(([1]?\\d)?\\d|2[0-4]\\d|25[0-5]))$"

local whitelist_columns = {
  {
    header = T"White-Listed IPv4 Address",
    name = "whitelist_ip",
    param = "value",
    type = "text",
    attr = { input = { class="span2",maxlength="15",pattern = ipv4pattern } },
  },
}
local whitelist_options = {
  canEdit = false,
  canAdd = true,
  canDelete = true,
  tableid = "whitelist",
  basepath = "uci.intrusion_protect.whitelist.@WhiteList.ip.@.",
  createMsg = T"Add White Listed IPv4 Address",
  sorted = function(a,b)
    return a.paramindex < b.paramindex
  end
}
local whitelist_valid = {
  whitelist_ip = vSII4,
}
local whitelist_data,whitelist_helpmsg = post_helper.handleTableQuery(whitelist_columns,whitelist_options,nil,nil,whitelist_valid)

local blacklist_columns = {
  {
    header = T"Black-Listed IPv4 Address",
    name = "blacklist_ip",
    param = "value",
    type = "text",
    attr = { input = { class="span2",maxlength="15",pattern = ipv4pattern } },
  },
}
local blacklist_options = {
  canEdit = false,
  canAdd = true,
  canDelete = true,
  tableid = "blacklist",
  basepath = "uci.intrusion_protect.blacklist.@BlackList.ip.@.",
  createMsg = T"Add Black Listed IPv4 Address",
  sorted = function(a,b)
    return a.paramindex < b.paramindex
  end
}
local blacklist_valid = {
  blacklist_ip = vSII4,
}
local blacklist_data,blacklist_helpmsg = post_helper.handleTableQuery(blacklist_columns,blacklist_options,nil,nil,blacklist_valid)

local blocklist_columns = {
  {
    header = T"Enabled",
    name = "enabled",
    param = "enabled",
    type = "switch",
  },
  {
    header = T"Logging",
    name = "logging",
    param = "logging",
    type = "switch",
  },
  {
    header = T"Name",
    name = "paramindex",
    param = "paramindex",
    readonly = true,
  },
  {
    header = T"URL",
    name = "url",
    param = "url",
    readonly = true,
  },
  {
    header = T"Description",
    name = "description",
    param = "description",
    readonly = true,
  },
}
local blocklist_options = {
  canEdit = true,
  canAdd = false,
  canDelete = false,
  tableid = "blocklists",
  basepath = "uci.intrusion_protect.blocklist.@.",
  sorted = function(a,b)
    return toupper(a.paramindex or "") > toupper(b.paramindex or "")
  end,
}
local blocklist_valid = {
  enabled = vB,
  logging = vB,
}
local blocklist_data,blocklist_helpmsg = post_helper.handleTableQuery(blocklist_columns,blocklist_options,nil,nil,blocklist_valid)

local monitor_changes = {
  input = {
    class = "monitor-changes"
  }
}

ngx.print(ui_helper.createHeader(T"Firewall",true,true))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-intrusion_protect-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('\
    <fieldset>\
      <legend>'); ngx.print(T"Intrusion Protection"); ngx.print('</legend>\
      ');
      ngx.print(
        ui_helper.createSwitch(T"Enabled","config_enabled",config["config_enabled"],nil,config_helpmsg["config_enabled"]),
        '<div class="monitor-config_enabled monitor-1 monitor-hidden-config_enabled">',
        ui_helper.createSwitch(T"White List Enabled","whitelist_enabled",config["whitelist_enabled"],monitor_changes,config_helpmsg["whitelist_enabled"]),
        ui_helper.createSwitch(T"Black List Enabled","blacklist_enabled",config["blacklist_enabled"],monitor_changes,config_helpmsg["blacklist_enabled"]),
        '</div>')
      ngx.print('\
    </fieldset>\
    <fieldset class="monitor-whitelist_enabled monitor-1 monitor-hidden-whitelist_enabled">\
      <legend>'); ngx.print(T"Custom White List"); ngx.print('</legend>\
      ');
      ngx.print(ui_helper.createTable(whitelist_columns,whitelist_data,whitelist_options,nil,whitelist_helpmsg))
      ngx.print('\
    </fieldset>\
    <fieldset class="monitor-blacklist_enabled monitor-1 monitor-hidden-blacklist_enabled">\
      <legend>'); ngx.print(T"Custom Black List"); ngx.print('</legend>\
      ');
      ngx.print(
        ui_helper.createSwitch(T"Logging","blacklist_logging",config["blacklist_logging"],nil,config_helpmsg["blacklist_logging"]),
        ui_helper.createTable(blacklist_columns,blacklist_data,blacklist_options,nil,blacklist_helpmsg))
      ngx.print('\
    </fieldset>\
    <fieldset>\
      <legend>'); ngx.print(T"Block Lists"); ngx.print('</legend>\
      ');
      ngx.print(ui_helper.createTable(blocklist_columns,blocklist_data,blocklist_options,nil,blocklist_helpmsg))
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
'); ngx.print(ui_helper.createFooter());