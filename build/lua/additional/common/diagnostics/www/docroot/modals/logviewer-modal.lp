--pretranslated: do not change this file

gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local logdownload_helper = require("logdownload_helper")
local proxy = require("datamodel")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local match = string.match
local tinsert = table.insert
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local getargs = ngx.req.get_uri_args()
local current_process = getargs["process"]
if current_process == "" then
  current_process = nil
end

local log_array = {}
local processes_select = {}

local log_data = proxy.get("sys.log.devicelog")
if log_data and log_data[1] and log_data[1].value then
  log_data = untaint(log_data[1].value)

  local processes_list = {}
  local function filter_log_data(date,facility,process,message)
    local p = match(process,"[^%[]+")
    if not current_process or p == current_process then
      tinsert(log_array,1,{ date,facility,process,message })
    end
    if p then
      processes_list[p] = true
    end
  end

  log_data:gsub("([^%s]+%s+%d+ %d+:%d+:%d+) [^%s]+ ([^%s]+) ([^%s]+): ([^\n]+)",filter_log_data)

  for k in pairs(processes_list) do
    processes_select[#processes_select+1] = { k,T(k) }
  end
  table.sort(processes_select,function(a,b)
    return a[1] < b[1]
  end)
end

tinsert(processes_select,1,{"",T"Everything"})

local log_columns = {
  {
    header = T"Date",
    name = "date",
    param = "date",
    type = "text",
    attr = { input = {  class="span1" } },
  },
  {
    header = T"Facility",
    name = "facility",
    param = "facility",
    type = "text",
    attr = { input = { class="span1" } },
  },
  {
    header = T"Process",
    name = "process",
    param = "process",
    type = "text",
    attr = { select = { class="span1" } },
  },
  {
    header = T"Message",
    name = "message",
    param = "message",
    type = "text",
    attr = { select = { class="span6" } },
  },
}
local log_options = {
  tableid = "logviewer",
  basepath = "sys.log.",
  canAdd = false,
  canEdit = false,
  canDelete = false,
}

if ngx.req.get_method() == "POST" then
  -- now process non-file POSTs
  local action = ngx.req.get_post_args().action
  if action == "export_log" then
    return logdownload_helper.export_log("logread")
  end
end

local filterclass = {
  span = {
    style = "float:left;",
  },
}
local exporting_alert = {
  alert = {
    class = "alert-info hide",
    id = "exporting-msg"
  }
}
local exporting_fail = {
  alert = {
    class = "alert-error hide",
    id = "export-failed-msg"
  }
}

ngx.print(ui_helper.createHeader(T"Event Log",false,true,5),'\
<div class="modal-body update no-save">\
'); lp.include("tabs-diagnostics.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="',ngx.var.request_uri,'">\
    <fieldset>\
      <table>\
        <tr>\
          <td>',ui_helper.createInputSelect(T"Filter","process",processes_select,current_process or "",filterclass),'</td>\
          <td>&nbsp;&nbsp;</td>\
          <td><div id="Export" class="btn export-conntracklog custom-handler" style="margin-bottom:8px;"><i class="icon-download-alt"></i>',T"Export All",'</div></td>\
        </tr>\
      </table>',
      ui_helper.createAlertBlock(T"Exporting log, please wait ...",exporting_alert),
      ui_helper.createAlertBlock(T"Exporting failed; please try again.",exporting_fail),
      ui_helper.createMessages(message_helper.popMessages()),
      ui_helper.createTable(log_columns,log_array,log_options,nil,nil),'\
    </fieldset>\
  </form>\
</div>\
<div class="modal-footer">\
  <div id="modal-no-change">\
    <div class="btn btn-primary btn-large" data-dismiss="modal">',T"Close",'</div>\
  </div>\
</div>\
<script type="text/javascript" src="/js/logviewer-modal.js"></script>\
');