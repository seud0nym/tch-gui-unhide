--pretranslated: do not change this file

local ngx = ngx

-- Enable localization
gettext.textdomain('webui-mobiled')

local proxy = require("datamodel")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")
local ui_helper = require("web.ui_helper")
local utils = require("web.lte-utils")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local params = utils.get_params()

local vB = post_helper.validateBoolean
local gOV = post_helper.getOptionalValidation
local gVNIR = post_helper.getValidateNumberInRange

local mapParams = {
  autofailovermaxwait = "uci.wansensing.global.autofailovermaxwait",
  voiceonfailover = "uci.wansensing.global.voiceonfailover",
}

local mapValid = {
  autofailovermaxwait = gVNIR(1,30),
  voiceonfailover = vB,
}

local content,helpmsg = post_helper.handleQuery(mapParams,mapValid)

local l2state_columns = {
  { -- [1]
    header = T"Name",
    name = "name",
    param = "name",
    type = "text",
    readonly = true,
  },
  { -- [2]
    header = T"Mains",
    name = "mains",
    param = "mains",
    type = "text",
    readonly = true,
  },
  { -- [3]
    header = T"Entry/Exits",
    name = "entryexits",
    param = "entryexits",
    type = "text",
    readonly = true,
  },
  { -- [4]
    header = T"Timeout<br>(Seconds)",
    name = "timeout",
    param = "timeout",
    type = "text",
    attr = { input = { type = "number",min = "1",max = "300",class="span2",style = "width:50px" } },
  },
}

local l2state_options = {
  tableid = "l2state",
  basepath = "uci.wansensing.L2State.@.",
  canAdd = false,
  canDelete = false,
  sorted = "index"
}

local l2state_valid = {
  ["timeout"] = gVNIR(1,300),
}

local l2state_data,l2state_helpmsg = post_helper.handleTableQuery(l2state_columns,l2state_options,nil,nil,l2state_valid)

local l3state_columns = {}
for k,v in ipairs(l2state_columns) do
  l3state_columns[k] = v
end
l3state_columns[#l3state_columns+1] = { -- [5]
  header = T"Fast Timeout<br>(Seconds)",
  name = "fasttimeout",
  param = "fasttimeout",
  type = "text",
  attr = { input = { type = "number",min = "1",max = "300",class="span2",style = "width:50px" } },
}

local l3state_options = {
  tableid = "l3state",
  basepath = "uci.wansensing.L3State.@.",
  canAdd = false,
  canDelete = false,
  sorted = "index"
}

local l3state_valid = {
  ["timeout"] = gVNIR(1,300),
  ["fasttimeout"] = gOV(gVNIR(1,300)),
}

local l3state_data,l3state_helpmsg = post_helper.handleTableQuery(l3state_columns,l3state_options,nil,nil,l3state_valid)

if ngx.var.request_method == "POST" then
  proxy.apply()
end

local wait_attributes = {
  input = {
    type = "number",
    min = "1",
    max = "10",
    style = "width:50px",
  }
}

ngx.print(ui_helper.createHeader(params.modal_title,false,true),'\
<div class="modal-body update">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/lte-autofailover.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset><legend>',T"Auto-Failover Global Settings",'</legend>',
      ui_helper.createSwitch(T"Voice on Failover","voiceonfailover",content["voiceonfailover"],nil,helpmsg["voiceonfailover"]),
      ui_helper.createInputText(T"Max Wait (Seconds)","autofailovermaxwait",content["autofailovermaxwait"],wait_attributes,helpmsg["autofailovermaxwait"]),'\
    </fieldset>\
    <fieldset><legend>',T"WanSensing L2 State",'</legend>',
      ui_helper.createTable(l2state_columns,l2state_data,l2state_options,nil,l2state_helpmsg),'\
    </fieldset>\
    <fieldset><legend>',T"WanSensing L3 State",'</legend>',
      ui_helper.createTable(l3state_columns,l3state_data,l3state_options,nil,l3state_helpmsg),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter());
