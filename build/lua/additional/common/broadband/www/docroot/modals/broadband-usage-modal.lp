--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")

local date,time = os.date,os.time
local match,sub = string.match,string.sub
local floor = math.floor
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local getargs = ngx.req.get_uri_args()
local wan_intf ="wan"
if getargs.wan_intf then
  wan_intf = untaint(getargs.wan_intf)
end

local content = {
  ifname = "rpc.network.interface.@"..wan_intf..".ifname",
}
content_helper.getExactContent(content)

local mapParams = {
  rollover_day = "rpc.gui.traffichistory."..wan_intf.."_rollover",
}

local mapValid = {
  rollover_day = post_helper.getValidateNumberInRange(1,31),
}

local content_rollover = post_helper.handleQuery(mapParams,mapValid)

local day = tonumber(untaint(content_rollover.rollover_day))
local this,last,prev,ignr
local today = date("*t")
if (today.day < day) then
  this = time({ year=today.year,month=today.month,  day=day-1 })
  last = time({ year=today.year,month=today.month-1,day=day-1 })
  prev = time({ year=today.year,month=today.month-2,day=day-1 })
  ignr = time({ year=today.year,month=today.month-3,day=day-1 })
else
  this = time({ year=today.year,month=today.month+1,day=day-1 })
  last = time({ year=today.year,month=today.month,  day=day-1 })
  prev = time({ year=today.year,month=today.month-1,day=day-1 })
  ignr = time({ year=today.year,month=today.month-2,day=day-1 })
end

local bb_usage_columns = {
  {
    name = "period",
    param = "period",
    type = "text",
    readonly = true,
  },
  {
    name = "date",
    param = "date",
    type = "text",
    readonly = true,
  },
  {
    header = T"Uploaded MB",
    name = "tx_bytes",
    param = "tx_bytes",
    type = "text",
    readonly = true,
  },
  {
    header = T"Downloaded MB",
    name = "rx_bytes",
    param = "rx_bytes",
    type = "text",
    readonly = true,
  },
  {
    header = T"Total MB",
    name = "total_bytes",
    param = "total_bytes",
    type = "text",
    readonly = true,
  },
}

local bb_usage_options = {
  tableid = "usage",
  basepath = "rpc.gui.traffichistory.usage.@.",
  canAdd = false,
  canEdit = false,
  canDelete = false,
  sorted = "-date"
}

local summary_options = {
  tableid = "summary",
  canAdd = false,
  canEdit = false,
  canDelete = false,
  sorted = "-date"
}

local summary_data = {
  { "This Month",date("%F",this),0,0,0 },
  { "Last Month",date("%F",last),0,0,0 },
  { "Previous Month",date("%F",prev),0,0,0 },
}

local function to_megabytes(val)
  local mb = tonumber(val) / 1048576
  local rounded = floor((mb * 10^2) + 0.5) / (10^2)
  local left,num,right = match(rounded,'^([^%d]*%d)(%d*)(.-)$')
  if (#right == 0) then
    right = ".00"
  elseif (#right < 3) then
    right = right..sub("000",1,3-#right)
  end
  return left..(num:reverse():gsub('(%d%d%d)','%1,'):reverse())..right
end

local bb_usage_filter = function(data)
  if data.interface == content.ifname then
    local yyyy,mm,dd = match(untaint(data.date),"(%d%d%d%d)-(%d%d)-(%d%d)")
    local date_time = os.time({ year=yyyy,month=mm,day=dd })
    if date_time <= ignr then
      return false
    else
      local date_str = os.date("%F",date_time)
      local idx
      if date_time <= prev then
        idx = 3
        data.period = "Previous Month"
      elseif date_time <= last then
        idx = 2
        data.period = "Last Month"
      else
        idx = 1
        data.period = "This Month"
      end
      if date_str < summary_data[idx][1] then
        summary_data[idx][1] = tostring(date_str)
      end
      summary_data[idx][3] = summary_data[idx][3] + data.tx_bytes
      summary_data[idx][4] = summary_data[idx][4] + data.rx_bytes
      summary_data[idx][5] = summary_data[idx][5] + data.total_bytes
      data.rx_bytes = to_megabytes(data.rx_bytes)
      data.tx_bytes = to_megabytes(data.tx_bytes)
      data.total_bytes = to_megabytes(data.total_bytes)
      return true
    end
  end
  return false
end

local bb_usage_data,bb_usage_helpmsg = post_helper.handleTableQuery(bb_usage_columns,bb_usage_options,bb_usage_filter,nil,nil)

if summary_data[3][5] == 0 then -- Previous Month Total
  table.remove(summary_data,3)
  if summary_data[2][5] == 0 then -- Last Month Total
    table.remove(summary_data,2)
  end
end

for _,data in ipairs(summary_data) do
  data[3] = to_megabytes(data[3])
  data[4] = to_megabytes(data[4])
  data[5] = to_megabytes(data[5])
end

local number_attr = {
  input = {
    type = "number",
    min = "1",
    max = "31",
    style = "width:100px",
  }
}

ngx.print(ui_helper.createHeader(T"Broadband",false,true,600),'\
<div class="modal-body update">\
'); lp.include("tabs-broadband.lp"); ngx.print('\
  <form class="traffichistory form-horizontal" method="post" action="modals/broadband-usage-modal.lp?wan_intf=',wan_intf,'">',
    ui_helper.createMessages(message_helper.popMessages()),
    '<legend>',T("Configuration"),'</legend>',
      ui_helper.createInputText(T"Roll-over Day","rollover_day",content_rollover["rollover_day"],number_attr),
      ui_helper.createLabel(T"Interface Device",content["ifname"]),
    '<legend>',T("Summary Usage"),'</legend>');
      bb_usage_columns[1].header = T"From Date"
      bb_usage_columns[2].header = T"End Date"
      ngx.print(ui_helper.createTable(bb_usage_columns,summary_data,summary_options),
    '<legend>',T("Daily Usage"),'</legend>');
      bb_usage_columns[1].header = T"Period"
      bb_usage_columns[2].header = T"Date"
      ngx.print(ui_helper.createTable(bb_usage_columns,bb_usage_data,bb_usage_options,nil,bb_usage_helpmsg),'\
  </form>\
</div>\
',ui_helper.createFooter());