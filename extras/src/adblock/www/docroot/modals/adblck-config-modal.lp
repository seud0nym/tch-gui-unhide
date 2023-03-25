--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-code')

local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")

local vB = post_helper.validateBoolean

local mapParams = {
  adb_enabled = "uci.adblock.global.adb_enabled",
  adb_backup = "uci.adblock.extra.adb_backup",
  adb_debug = "uci.adblock.extra.adb_debug",
}

local mapValid = {
  adb_enabled = vB,
  adb_backup = vB,
  adb_debug = vB,
}

local config,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not helpmsg then
  helpmsg = {}
end

ngx.print('\
');  ngx.print(ui_helper.createHeader("Adblock",false,true) ); ngx.print('\
<div class="modal-body update">\
  <ul class="nav nav-tabs">\
    <li class="active"><a href="#" data-remote="/modals/adblck-config-modal.lp">Configuration</a></li>\
    <li><a href="#" data-remote="/modals/adblck-lists-modal.lp">Lists</a></li>\
  </ul>\
  <form class="form-horizontal" method="post" action="modals/adblck-config-modal.lp">\
    ');
      local html = {}
      html[#html+1] = "<fieldset><legend>" .. T"Configuration" .. "</legend>"
      html[#html+1] = ui_helper.createSwitch(T"Enable Adblock","adb_enabled",config["adb_enabled"],nil,helpmsg["adb_enabled"])
      html[#html+1] = ui_helper.createSwitch(T"Enable Backup","adb_backup",config["adb_backup"],nil,helpmsg["adb_backup"])
      html[#html+1] = ui_helper.createSwitch(T"Enable Debugging","adb_debug",config["adb_debug"],nil,helpmsg["adb_debug"])
      html[#html+1] = "</fieldset>"
      ngx.print(html)
      ngx.print('\
  </form>\
</div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
');
