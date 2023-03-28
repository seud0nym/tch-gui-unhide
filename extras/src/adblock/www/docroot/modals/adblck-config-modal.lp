--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-code')

local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")

local vB = post_helper.validateBoolean

local mapParams = {
  adb_enabled = "uci.adblock.global.adb_enabled",
  adb_safesearch = "uci.adblock.global.adb_safesearch",
  adb_backup = "uci.adblock.global.adb_backup",
  adb_debug = "uci.adblock.global.adb_debug",
}

local mapValid = {
  adb_enabled = vB,
  adb_safesearch = vB,
  adb_backup = vB,
  adb_debug = vB,
}

local config,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not helpmsg then
  helpmsg = {}
end

ngx.print('\
');  ngx.print(ui_helper.createHeader("Adblock",false,true) ); ngx.print('\
<div class="modal-body update">')
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-adblock.lp")
  ngx.print('\
  <form class="form-horizontal" method="post" action="modals/adblck-config-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset><legend>',T"Configuration",'</legend>',
      ui_helper.createSwitch(T"Enable Adblock","adb_enabled",config["adb_enabled"],nil,helpmsg["adb_enabled"]),
      ui_helper.createSwitch(T"Enable Safe Search<span class='icon-question-sign' title='Enable to enforce SafeSearch for google, bing, duckduckgo, yandex, youtube and pixabay'></span>","adb_safesearch",config["adb_safesearch"],nil,helpmsg["adb_safesearch"]),
      ui_helper.createSwitch(T"Enable Backup","adb_backup",config["adb_backup"],nil,helpmsg["adb_backup"]),
      ui_helper.createSwitch(T"Enable Debugging","adb_debug",config["adb_debug"],nil,helpmsg["adb_debug"]),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter())
