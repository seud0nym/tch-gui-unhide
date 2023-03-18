--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
---@diagnostic disable-next-line: undefined-field
local gsub,untaint = string.gsub,string.untaint

local proc_columns = {
  {
    header = T"Process ID",
    name = "process",
    param = "process",
    type = "text",
  },
  {
    header = T"Parent",
    name = "parent",
    param = "parent",
    type = "text",
  },
  {
    header = T"User",
    name = "user",
    param = "user",
    type = "text",
  },
  {
    header = T"State",
    name = "state",
    param = "state",
    type = "text",
  },
  {
    header = T"Resident<br>Mem <small>KB</small>",
    name = "resident_set_size",
    param = "resident_set_size",
    type = "text",
  },
  {
    header = T"Resident<br>Mem %",
    name = "resident_set_percent",
    param = "resident_set_percent",
    type = "text",
  },
  {
    header = T"Virtual<br>Mem <small>KB</small>",
    name = "virtual_memory_size",
    param = "virtual_memory_size",
    type = "text",
  },
  {
    header = T"Virtual<br>Mem %",
    name = "virtual_memory_percent",
    param = "virtual_memory_percent",
    type = "text",
  },
  {
    header = T"CPU%",
    name = "cpu",
    param = "cpu",
    type = "text",
  },
  {
    header = T"Command",
    name = "command",
    param = "command",
    type = "text",
  },
  {
    header = "",
    name = "depth_indicator",
    param = "depth_indicator",
    type = "hidden",
  },
}
local proc_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "proctable",
  basepath = "rpc.gui.proc.processes.",
}
local function proc_filter(data)
  data.command = gsub(untaint(data.depth_indicator)," ","&emsp;")..untaint(data.command)
  return true
end
local proc_data,proc_helpmsg = post_helper.handleTableQuery(proc_columns,proc_options,proc_filter,nil,nil)

local lp = require("web.lp")
lp.setpath("/www/snippets/")

ngx.print(ui_helper.createHeader(T"Management",false,true,10),'\
<div class="modal-body update">\
'); lp.include("tabs-management.lp"); ngx.print('\
<form class="form-horizontal" method="post" action="modals/system-proc-modal.lp">\
  <fieldset>\
    <legend>',T"Processes",'</legend>\
    <div class="control-group"><label class="control-label">',T"Filter",'</label>\
      <div class="controls">\
        <input data-type="search" name="Filter" class="edit-input span3 no-save" placeholder="Search For Processes..." id="searchInput" type="text">\
      </div>\
    </div>\
  </fieldset>',
  ui_helper.createTable(proc_columns,proc_data,proc_options,nil,proc_helpmsg),'\
</form>\
</div>\
',ui_helper.createFooter(),'\
<script src="/js/system-proc-modal.js"></script>\
');
