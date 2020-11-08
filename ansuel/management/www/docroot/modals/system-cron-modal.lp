--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")

local function validateCronTimes(value, object, key)
  if not value:match("^%*$") 
    and not value:match("^[0-9]+-[0-9]+$")
    and not value:match("^[0-9]+$")
    and not value:match("[0-9]+,")
    and not value:match("^%*/[0-9]+$")
    or value:match("%a")
    or value:match("%s")
  then
    return nil, "Invalid time entered. Check linux cron documentation for correct formats"
  end
  return true
end

local cron_valid = {
  minute = validateCronTimes,
  hour = validateCronTimes,
  day = validateCronTimes,
  month = validateCronTimes,
  week = validateCronTimes,
  command = post_helper.validateNonEmptyString,
}

local cron_columns = {
  { --[1]
    header = T"Minute",
    name = "minute",
    param = "minute",
    type = "text",
    attr = { input = { class="span1"} },
  },
  { --[2]
    header = T"Hour",
    name = "hour",
    param = "hour",
    type = "text",
    attr = { input = { class="span1"} },
  },
  { --[3]
    header = T"Day",
    name = "day",
    param = "day",
    type = "text",
    attr = { input = { class="span1"} },
  },
  { --[4]
    header = T"Month",
    name = "month",
    param = "month",
    type = "text",
    attr = { input = { class="span1"} },
  },
  { --[5]
    header = T"Week",
    name = "week",
    param = "week",
    type = "text",
    attr = { input = { class="span1"} },
  },
  { --[6]
    header = T"Command",
    name = "command",
    param = "command",
    type = "text",
  },
}

-- TODO: Failed to make edits/adds/deletes work reliably, so turn off until fixed
local cron_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "crontable",
  basepath = "rpc.gui.cron.entries.",
}

cron_options.onAdd = function(index, content)
  --We increment as the add function write a line
  index = index+1

  if content.command == "" then
    cron_options.errmsg = T"Command input can't be empty"
    return nil
  end


  local tbl = {
    minute = content.minute or "*",
    hour = content.hour or "*",
    day = content.day or "*",
    month = content.month or "*",
    week = content.week or "*",
    command = content.command,
  }

  for key,value in pairs(tbl) do
    local path = string.format("%s%s.%s",cron_options.basepath,index,key)
    proxy.set(path,value)
  end

  proxy.set("rpc.gui.cron.editCronRuleNumber",index)
end

cron_options.onModify = function(index, content)
  proxy.set("rpc.gui.cron.editCronRuleNumber",index)
end

local cron_data, cron_helpmsg = post_helper.handleTableQuery(cron_columns, cron_options, nil, nil, cron_valid)

  ngx.print('\
');  ngx.print(ui_helper.createHeader(T"Management", false, true) ); ngx.print('\
<div class="modal-body update">\
');  
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-management.lp")
  ngx.print('\
');  
-- dummy form so that refresh button knows which page to reload, keep it
  ngx.print('\
  <form class="form-horizontal" method="post" action="modals/system-cron-modal.lp">\
  </form>\
  <fieldset id="system-cron-tab">\
    <legend>');  ngx.print( T"Cron Tab Viewer" ); ngx.print('</legend>')
    ngx.print(ui_helper.createTable(cron_columns, cron_data, cron_options, nil, cron_helpmsg))
    ngx.print('\
  </fieldset>\
</div>\
');  ngx.print(ui_helper.createFooter() ); ngx.print('\
'); 
