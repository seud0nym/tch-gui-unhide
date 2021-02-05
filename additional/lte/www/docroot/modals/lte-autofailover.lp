--pretranslated: do not change this file
 
local table = { insert = table.insert }
local ngx, type = ngx, type

-- Enable localization
gettext.textdomain('webui-mobiled')

local content_helper = require("web.content_helper")
local proxy = require("datamodel")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")
local ui_helper = require("web.ui_helper")
local utils = require("web.lte-utils")

local params = utils.get_params()

local vB = post_helper.validateBoolean
local gVNIR = post_helper.getValidateNumberInRange

local mapParams = {
  autofailovermaxwait = "uci.wansensing.global.autofailovermaxwait",
}

local mapValid = {
  autofailovermaxwait = gVNIR(1,30),
}

local content, helpmsg = post_helper.handleQuery(mapParams, mapValid)

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
    attr = { input = { type = "number", min = "1", max = "300", class="span2", style = "width:50px" } },
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

local l2state_data, l2state_helpmsg = post_helper.handleTableQuery(l2state_columns, l2state_options, nil, nil, l2state_valid)

local l3state_columns = {}
for k,v in ipairs(l2state_columns) do
  l3state_columns[k] = v
end
l3state_columns[#l3state_columns+1] = { -- [5]
  header = T"Fast Timeout<br>(Seconds)",
  name = "fasttimeout",
  param = "fasttimeout",
  type = "text",
  attr = { input = { type = "number", min = "1", max = "300", class="span2", style = "width:50px" } },
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
  ["fasttimeout"] = gVNIR(1,300),
}

local l3state_data, l3state_helpmsg = post_helper.handleTableQuery(l3state_columns, l3state_options, nil, nil, l3state_valid)

local delay
local l3type = proxy.get("uci.wansensing.global.l3type")
if l3type and l3type[1] then
  local t = l3type[1].value
  for _,v in ipairs(l3state_data) do
    if v[1] == t and v[5] then
      delay = (v[5] * 11) + content["autofailovermaxwait"]
      break
    end
  end
end

ngx.print( ui_helper.createHeader(params.modal_title, false, true) ); ngx.print('\
<div class="modal-body update" id="lte-body">\
  <form class="form-horizontal" method="post" action="modals/lte-autofailover.lp">\
  ');  ngx.print(ui_helper.createMessages(message_helper.popMessages())); 
    local lp = require("web.lp")
    lp.setpath("/www/snippets/")
    lp.include("tabs-mobiled.lp")

    local alert_attributes = {
      alert = {
        class = "alert-info",
        style = "text-align: center;"
      },
    }
    local wait_attributes = {
      input = {
        type = "number",
        min = "1",
        max = "10",
        style = "width:70px",
      }
    }
  
    local html = {}
    table.insert(html, "<fieldset><legend>" .. T"Auto-Failover Global Settings" .. "</legend>")
    table.insert(html, ui_helper.createInputText(T"Max Wait (Seconds)", "autofailovermaxwait", content["autofailovermaxwait"], wait_attributes, helpmsg["autofailovermaxwait"]))
    table.insert(html, "</fieldset>")
    table.insert(html, "<fieldset><legend>" .. T"WanSensing L2 State" .. "</legend>")
    table.insert(html, ui_helper.createTable(l2state_columns, l2state_data, l2state_options, nil, l2state_helpmsg))
    table.insert(html, "</fieldset>")
    table.insert(html, "<fieldset><legend>" .. T"WanSensing L3 State" .. "</legend>")
    table.insert(html, ui_helper.createTable(l3state_columns, l3state_data, l3state_options, nil, l3state_helpmsg))
    table.insert(html, "</fieldset>")
    if delay then
      table.insert(html, ui_helper.createAlertBlock(T(string.format("Maximum estimated auto-failover delay is <strong>%s</strong> seconds after the connection drops.", delay)), alert_attributes))
    end
    ngx.print(html)
    ngx.print('\
  </form>\
</div>\
');  ngx.print( ui_helper.createFooter() );
