--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local function validateCronTimes(value,_,_)
  if not value:match("^%*$")
    and not value:match("^[0-9]+-[0-9]+$")
    and not value:match("^[0-9]+$")
    and not value:match("[0-9]+,")
    and not value:match("^%*/[0-9]+$")
    or value:match("%a")
    or value:match("%s")
  then
    return nil,"Invalid time entered. Check linux cron documentation for correct formats"
  end
  return true
end

local cron_valid = {
  enabled = post_helper.validateBoolean,
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
    header = T"Weekday<span class='icon-question-sign' title='0=Sun 1=Mon 2=Tue 3=Wed 4=Thu 5=Fri 6=Sat'></span>",
    name = "weekday",
    param = "weekday",
    type = "text",
    attr = { input = { class="span1"} },
  },
  { --[6]
    header = T"Command",
    name = "command",
    param = "command",
    type = "text",
  },
  { --[7]
    header = T"Enabled",
    name = "enabled",
    param = "enabled",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "left" }},
  },
}

local cron_options = {
  canEdit = true,
  canAdd = true,
  canDelete = true,
  tableid = "crontable",
  basepath = "rpc.gui.cron.entries.@.",
}

cron_options.onAdd = function(index,content)
  if content.command == "" then
    cron_options.errmsg = T"Command input can't be empty"
    return nil
  end

  local tbl = {
    enabled = content.enabled or "1",
    minute = content.minute or "*",
    hour = content.hour or "*",
    day = content.day or "*",
    month = content.month or "*",
    weekday = content.weekday or "*",
    command = content.command,
  }

  for key,v in pairs(tbl) do
    local value = untaint(v)
    local path = string.format("rpc.gui.cron.entries.%s.%s",index,key)
    local result,errors = proxy.set(path,value)
    if not result then
      for _,err in ipairs(errors) do
        message_helper.pushMessage(T(string.format("Failed to set %s to '%s': %s (%s)",err.path,value,err.errmsg,err.errcode)),"error")
      end
    end
  end

  proxy.set("rpc.gui.cron.writecrontab","1")
end

cron_options.onModify = function(_,_)
  proxy.set("rpc.gui.cron.writecrontab","1")
end

local cron_data,cron_helpmsg = post_helper.handleTableQuery(cron_columns,cron_options,nil,nil,cron_valid)

local alert_warn = {
  alert = {
    class = "alert alert-warn"
  },
}

local lp = require("web.lp")
lp.setpath("/www/snippets/")

ngx.print(ui_helper.createHeader(T"Management",false,true),'\
<div class="modal-body update">\
'); lp.include("tabs-management.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/system-cron-modal.lp">\
    <fieldset>\
      <legend>',T"Scheduled Tasks",'</legend>',
      ui_helper.createMessages(message_helper.popMessages()),
      ui_helper.createAlertBlock(T"WARNING: When you click the <i class='icon-remove-sign icon-large'/> button,the scheduled task will be deleted IMMEDIATELY!",alert_warn),
      ui_helper.createTable(cron_columns,cron_data,cron_options,nil,cron_helpmsg),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter(),'\
<script>\
$("#crontable td:contains(/usr/bin/thermalProtection)").siblings(":last").children(".btn-table-edit,.btn-table-delete").hide(); \
$("#crontable td:contains(/sbin/trafficmon.lua)").siblings(":last").children(".btn-table-delete").hide(); \
$("#crontable td:contains(/usr/sbin/traffichistory.lua)").siblings(":last").children(".btn-table-delete").hide(); \
$("#crontable td:contains(/root/.acme.sh/acme-ipsec.sh)").siblings(":last").children(".btn-table-edit,.btn-table-delete").hide(); \
$("#crontable td:contains(/usr/sbin/doh-ipsets-maintain)").siblings(":last").children(".btn-table-delete").hide(); \
</script>\
');
