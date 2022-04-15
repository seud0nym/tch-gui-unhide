--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local queue_columns = {
  {
    header = T"Queue Name",
    name = "X_000E50_QueueName",
    param = "X_000E50_QueueName",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Shaping Rate",
    name = "ShapingRate",
    param = "ShapingRate",
    type = "text",
    attr = { input = { type = "number",min = "-1",step = "1000",class = "span2" } },
  },
  {
    header = T"Shaping Burst Size",
    name = "ShapingBurstSize",
    param = "ShapingBurstSize",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Traffic Classes",
    name = "TrafficClasses",
    param = "TrafficClasses",
    type = "text",
    attr = { input = { class = "span2" } },
  },
  {
    header = T"Scheduler Algorithm",
    name = "SchedulerAlgorithm",
    param = "SchedulerAlgorithm",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Weight",
    name = "Weight",
    param = "Weight",
    type = "text",
    attr = { input = { class = "span2" } },
  },
  {
    header = T"Precedence",
    name = "Precedence",
    param = "Precedence",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Interface",
    name = "Interface",
    param = "Interface",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Status",
    name = "Status",
    param = "Status",
    type = "text",
    readonly = true,
  },
}

local queue_options = {
  tableid = "queue",
  basepath = "Device.QoS.Queue.",
  canEdit = false,
  canAdd = false,
  canDelete = false,
}

local queue_valid = {
}

local queue_data,queue_helpMsg = post_helper.handleTableQuery(queue_columns,queue_options,nil,nil,queue_valid)

for r,row in ipairs(queue_data) do
  for c,col in ipairs(row) do
    if col == "-1" then
      queue_data[r][c] = ""
    end
  end
end

ngx.print(ui_helper.createHeader(T"QoS",false)); ngx.print('\
<div class="modal-body">\
'); lp.include("tabs-qos.lp") ngx.print('\
  <form class="form-horizontal" method="post" action="modals/qos-queue-modal.lp">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>Queues</legend>\
      ',ui_helper.createTable(queue_columns,queue_data,queue_options,nil,queue_helpMsg),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter());
