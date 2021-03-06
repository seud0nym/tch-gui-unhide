--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")

local date, time = os.date, os.time
local match, untaint = string.match, string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local wan_intf ="wan"
local content_wan = {
  wwan_ipaddr = "rpc.network.interface.@wwan.ipaddr",
  wwan_ip6addr = "rpc.network.interface.@wwan.ip6addr",
}
content_helper.getExactContent(content_wan)
if content_wan.wwan_ipaddr:len() ~= 0 or content_wan.wwan_ip6addr:len() ~= 0 then
  wan_intf = "wwan"
end
local content = {
  ifname = "rpc.network.interface.@" .. wan_intf .. ".ifname",
}
content_helper.getExactContent(content)

local mapParams = {
  rollover_day = "rpc.gui.traffichistory." .. wan_intf .. "_rollover",
}

local mapValid = {
  rollover_day = post_helper.getValidateNumberInRange(1,31),
}

local content_rollover, helpmsg_rollover = post_helper.handleQuery(mapParams, mapValid)

local day = tonumber(untaint(content_rollover.rollover_day))
local this, last, prev, ignr
local today = date("*t")
if (today.day < day) then
  this = time({ year=today.year, month=today.month,   day=day-1 })
  last = time({ year=today.year, month=today.month-1, day=day-1 })
  prev = time({ year=today.year, month=today.month-2, day=day-1 })
  ignr = time({ year=today.year, month=today.month-3, day=day-1 })
else
  this = time({ year=today.year, month=today.month+1, day=day-1 })
  last = time({ year=today.year, month=today.month,   day=day-1 })
  prev = time({ year=today.year, month=today.month-1, day=day-1 })
  ignr = time({ year=today.year, month=today.month-2, day=day-1 })
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
  { "This Month", date("%F", this), 0, 0, 0 },
  { "Last Month", date("%F", last), 0, 0, 0 },
  { "Previous Month", date("%F", prev), 0, 0, 0 },
}

local function to_megabytes(val)
  local mb = tonumber(val) / 1048576
  local rounded = math.floor((mb * 10^2) + 0.5) / (10^2)
  local left,num,right = string.match(rounded,'^([^%d]*%d)(%d*)(.-)$')
  if (#right == 0) then
    right = ".00"
  elseif (#right < 3) then
    right = right..string.sub("000", 1, 3-#right)
  end
  return left..(num:reverse():gsub('(%d%d%d)','%1,'):reverse())..right
end

local bb_usage_filter = function(data)
  if data.interface == content.ifname then
    local yyyy, mm, dd = match(untaint(data.date), "(%d%d%d%d)-(%d%d)-(%d%d)")
    local date = os.time({ year=yyyy, month=mm, day=dd })
    if date <= ignr then
      return false
    else
      local date_str = os.date("%F", date)
      local idx
      if date <= prev then
        idx = 3
        data.period = "Previous Month"
      elseif date <= last then
        idx = 2
        data.period = "Last Month"
      else
        idx = 1
        data.period = "This Month"
      end
      if date_str < summary_data[idx][1] then
        summary_data[idx][1] = date_str
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

local bb_usage_data, bb_usage_helpmsg = post_helper.handleTableQuery(bb_usage_columns, bb_usage_options, bb_usage_filter, nil, nil)

if summary_data[3][5] == 0 then
  table.remove(summary_data, 3)
  if summary_data[2][5] == 0 then
    table.remove(summary_data, 2)
  end
end

local data
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


ngx.print(ui_helper.createHeader(T"Broadband", false, true, 600))
ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-broadband.lp"); ngx.print('\
  <form class="traffichistory form-horizontal" method="post" action="modals/broadband-usage-modal.lp">\
    '); 
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>');  ngx.print( T("Configuration") ); ngx.print('</legend>');
    ngx.print(ui_helper.createInputText(T"Roll-over Day", "rollover_day", content_rollover["rollover_day"], number_attr))
    ngx.print(ui_helper.createLabel(T"Interface", string.upper(wan_intf)))
    ngx.print(ui_helper.createLabel(T"Interface Device", content["ifname"]))
    ngx.print('<legend>');  ngx.print( T("Summary Usage") ); ngx.print('</legend>');
    bb_usage_columns[1].header = T"From Date"
    bb_usage_columns[2].header = T"End Date"
    ngx.print(ui_helper.createTable(bb_usage_columns, summary_data, summary_options))
    ngx.print('<legend>');  ngx.print( T("Daily Usage") ); ngx.print('</legend>');
    bb_usage_columns[1].header = T"Period"
    bb_usage_columns[2].header = T"Date"
    ngx.print(ui_helper.createTable(bb_usage_columns, bb_usage_data, bb_usage_options, nil, bb_usage_helpmsg))
    ngx.print('\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter());