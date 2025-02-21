--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local lp = require("web.lp")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local ssid_helper = require("ssid_helper")
local tod = require("parental_helper").getTodwifi()
local tod_helper = require("tod_helper")
local ui_helper = require("web.ui_helper")

local ngx = ngx
local string,table = string,table
local concat,insert = table.concat,table.insert
local ipairs = ipairs
local tonumber = tonumber
local gmatch,format,match = string.gmatch,string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local untaint_mt = require("web.taint").untaint_mt
local session = ngx.ctx.session

local validateTime = post_helper.validateTime
local validateBoolean = post_helper.validateBoolean
local getRandomKey = post_helper.getRandomKey
local gVIES = post_helper.getValidateInEnumSelect

local ap_status_map = setmetatable({
  ["0"] = T"Off",
  ["1"] = T"On",
  ["-1"] = T"Error. Re-set the rule!"
},untaint_mt)

local tod_columns = tod.columns
if #tod_columns[7].subcolumns == 5 then
  table.insert(tod_columns[7].subcolumns,2,{
    header = T"Access Point",
    name = "ap",
    param = "ap",
    attr = { input = { class="no-save" } },
  })
end
tod_columns[7].subcolumns[2].type = "checkboxgroup"
tod_columns[7].subcolumns[2].values = ssid_helper.getAccessPoints()


local tod_options = {
  tableid = "tod",
  basepath = "uci.tod.action.",
  createMsg = T"Add New Rule",
  canAdd = true,
  canDelete = true,
  canApply = true,
  canEdit = true,
  objectName = "action"..getRandomKey(),
  addNamedObject = true,
}

local function validate_wifitod(postcontent,timer_data)
  local todValidations = {
    enable = validateBoolean,
    mode = gVIES(({{"on"},{"off"}})),
    start_time = validateTime,
    stop_time = validateTime,
    weekdays = tod_helper.validateWeekdays,
  }
  local success,errmsg = content_helper.validateObject(postcontent,todValidations)
  if not success then
    for field,error in pairs(errmsg) do
      message_helper.pushMessage(T(field..": "..error),"error")
    end
    return nil
  end
  if postcontent.start_time >= postcontent.stop_time then
    message_helper.pushMessage(T"Stop time should be after Start time.","error")
    return nil
  end
  for i,v in ipairs(timer_data) do
    if i ~= postcontent.index then
      local result,error = tod_helper.validateScheduleOverlap(postcontent,v)
      if not result then
        message_helper.pushMessage(T(error),"error")
        return nil
      end
    end
  end
  return true
end

local function tod_filter(data)
  return data.script == "wifitodscript"
end

local function getWifiTodListFromIndex(index)
  if not index then
    return nil
  end
  return proxy.getPN(format("%s@wifitod_%s.ap.","uci.tod.wifitod.",index),true)
end

local function onAdd(success,content,onModify)
  local index = match(success,"action(.+)")
  local setObject = {}
  if not content.ap or content.ap == "" or content.ap == "all" or (type(content.ap) == "table" and #content.ap == (#tod_columns[7].subcolumns[2].values + 1)) then -- checkboxgroup always included an empty hidden checkbox
    content.ap = { "all" }
  elseif type(content.ap) == "string" then
    content.ap = { content.ap }
  end
  if not validate_wifitod(content,session:retrieve("timer_data")) then
    return nil
  end
  local wifitod_id = "wifitod_"..index
  local tod_wifi_mode
  content.mode = content.mode == "on" and "1" or "0"
  if onModify then
    local tod_ap = proxy.getPN("uci.tod.ap.",true)
    if tod_ap then
      for _,v in ipairs(tod_ap) do 
        tod_wifi_mode = proxy.get(v.path.."state")
        if match(v.path,"_"..index.."%.$") then
          proxy.del(v.path)
        end
      end
    end
    if tod_wifi_mode and content.mode ~= tod_wifi_mode[1].value then
      local tod_action_activeTimers = proxy.getPN(format("%s@action%s.activedaytime.","uci.tod.action.",index),true)
      if tod_action_activeTimers then
        for _,v in pairs(tod_action_activeTimers) do
          proxy.del(v.path)
        end
      end
    end
    proxy.del("uci.tod.wifitod.@"..wifitod_id..".")
  end
  if not onModify then
    proxy.add("uci.tod.action.@"..success..".timers.")
    proxy.add("uci.tod.timer.","timer"..index)
  end
  proxy.add("uci.tod.wifitod.",wifitod_id)
  for _,ap in pairs(content.ap) do
    if not (ap == "") then
      proxy.add("uci.tod.wifitod.@"..wifitod_id..".ap.")
      proxy.add("uci.tod.ap.",format("%s_%s",ap,index))
    end
  end
  content.weekdays = concat(content.weekdays,",")
  content.start_time = format("%s:%s",content.weekdays,content.start_time)
  content.stop_time = format("%s:%s",content.weekdays,content.stop_time)
  setObject[format("%s@action%s.timers.@1.value","uci.tod.action.",index)] = "timer"..index
  setObject[format("%s@action%s.enabled","uci.tod.action.",index)] = content.enabled
  setObject[format("%s@action%s.script","uci.tod.action.",index)] = "wifitodscript"
  setObject[format("%s@action%s.object","uci.tod.action.",index)] = "wifitod."..wifitod_id
  setObject[format("%s@timer%s.start_time","uci.tod.timer.",index)] = content.start_time
  setObject[format("%s@timer%s.stop_time","uci.tod.timer.",index)] = content.stop_time
  local count = 1
  for _,ap in pairs(content.ap) do
    if not (ap == "") then
      setObject[format("%s@%s.ap.@%s.value","uci.tod.wifitod.",wifitod_id,count)] = ap.."_"..index
      setObject[format("%s@%s_%s.state","uci.tod.ap.",ap,index)] = content.mode
      setObject[format("%s@%s_%s.ap","uci.tod.ap.",ap,index)] = ap
      count = count+1
    end
  end
  local ret = proxy.set(setObject)
  proxy.apply()
  return ret
end

local function onModify(index,content)
  local ret = onAdd(index,content,true)
  return ret
end

local function onDelete(success)
  local index = success:match("action(.+)")
   if not index then
    message_helper.pushMessage(T"Error on delete. Remove rule from config file manually.","error")
    return nil
  end
  local delObject = {}
  local ret = true
  local wifiTodList = getWifiTodListFromIndex(index)
  if wifiTodList then
    for _,v in ipairs(wifiTodList) do
      delObject[#delObject+1] = format("uci.tod.ap.@%s.",proxy.get(v.path.."value")[1].value)
    end
  end
  delObject[#delObject+1] = format("uci.tod.action.@action%s.",index)
  delObject[#delObject+1] = format("uci.tod.timer.@timer%s.",index)
  delObject[#delObject+1] = format("uci.tod.wifitod.@wifitod_%s.",index)
  for _,obj in pairs(delObject) do
    ret = ret and proxy.del(obj)
  end
  return ret
end

if ngx.var.request_method == "POST" then
  local postargs = ngx.req.get_post_args()
  postargs.index = tonumber(postargs.index) or -1
  local current_index = postargs.index
  local allowedIndexes = session:retrieve(tod_options.tableid..".allowedindexes") or {}
  current_index = allowedIndexes[current_index] and allowedIndexes[current_index].paramindex or -1
  if postargs.action == "TABLE-NEW" then
    tod_options.editing = -1
  elseif postargs.action == "TABLE-EDIT" then
    tod_options.editing = postargs.index
  elseif postargs.action == "TABLE-ADD" then
    current_index = proxy.add("uci.tod.action.",tod_options.objectName)
    local success = onAdd(current_index,postargs)
    if not success then
      proxy.del("uci.tod.action.@"..tod_options.objectName..".")
      tod_options.editing = -1
    end
    proxy.apply()
  elseif postargs.action == "TABLE-MODIFY" then
    local success = onModify(current_index,postargs)
    if not success then
      tod_options.editing = -1
    end
  elseif postargs.action == "TABLE-DELETE" then
    onDelete(current_index)
    proxy.apply()
  end
end

local tod_data,tod_index = content_helper.loadTableData(tod_options.basepath,tod_columns,tod_filter)

local tablesessionindexes = tod_options.tableid..".allowedindexes"
session:store(tablesessionindexes,tod_index)

local action_list = content_helper.convertResultToObject("uci.tod.action.",proxy.get("uci.tod.action."))
local timer_list = content_helper.convertResultToObject("uci.tod.timer.",proxy.get("uci.tod.timer."))
local timer_data = {}

local function getApSelected(index)
  local wifi_list,wifi_selected = {},{}
  local wifiTodList = getWifiTodListFromIndex(index)
  if not wifiTodList then
    return "",{}
  end
  for _,v in ipairs(wifiTodList) do
    local ap_index = proxy.get(v.path.."value")[1].value
    local value = proxy.get(format("%s@%s.ap","uci.tod.ap.",ap_index))[1].value
    if value == "all" then
      wifi_list[#wifi_list+1] = "All"
      for _,ap in ipairs(tod_columns[7].subcolumns[2].values) do
        wifi_selected[#wifi_selected+1] = ap[1]
      end
    else
      wifi_selected[#wifi_selected+1] = untaint(value)
      for _,ap in ipairs(tod_columns[7].subcolumns[2].values) do
        if ap[1] == value then
          wifi_list[#wifi_list+1] = ap[2]
        end
      end
    end
  end
  return table.concat(wifi_list,"<br/>"),wifi_selected
end

for i,v in ipairs(tod_data) do
  local days,start_time = timer_list[i].start_time:match("^(.-):(.+)$")
  local index = action_list[i].object:gsub("wifitod.wifitod_","")
  local wifi_list,wifi_selected = getApSelected(index)
  local ap_state = "-1"
  local ap_state_get
  if wifi_selected[1] then
    local path
    if wifi_list == "All" then
      path = format("%s@all_%s.state","uci.tod.ap.",index)
    else
      path = format("%s@%s_%s.state","uci.tod.ap.",wifi_selected[1],index)
    end
    ap_state_get = proxy.get(path)
    if ap_state_get then
      ap_state = untaint(ap_state_get[1].value)
    end
  end
  v[1] = v[1] -- status
  v[2] = wifi_list -- ap
  v[3] = start_time -- start_time
  v[4] = timer_list[i].stop_time:match("^.-:(.+)$") -- stop_time
  v[5] = ap_status_map[ap_state] -- apState
  v[6] = {} -- days
  if days then
    for day in gmatch(days,"[^,]+") do
      insert(v[6],untaint(day))
    end
  end
  -- Sub Columns
  v[7] = {
    [1] = v[1], -- status
    [2] = {}, -- ap
    [3] = ap_state == "1" and "on" or "off", -- apState
    [4] = v[3], -- start_time
    [5] = v[4], -- stop_time
    [6] = v[6] -- days
  }
  for _,ap in ipairs(wifi_selected) do
    insert(v[7][2],ap)
  end
  timer_data[i] = {
    start_time = v[3],
    stop_time = v[4],
    days = v[6],
  }
end

session:store("timer_data",timer_data)

local modal="/modals/tod_wireless-modal.lp"
local tabs = "tabs-wireless.lp"
local header = T"Wireless"
if ngx.var.args == "tod" then
  modal = modal.."?tod"
  tabs = "tabs-tod.lp"
  header = T"Time of Day"
end
lp.setpath("/www/snippets/")

ngx.print(ui_helper.createHeader(header,true,false),'\
<div class="modal-body update">\
'); lp.include(tabs); ngx.print('\
  <form class="form-horizontal" method="post" action="',modal,'">\
    <style>#tod .line-edit {display:none;}</style>',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <legend>',T"Wireless Control",'</legend>')
    if ssid_helper.isMultiAPEnabled() then
      ngx.print(ui_helper.createAlertBlock(T"Disabling wireless will disable the EasyMesh network",{ alert = { class = "alert-warning monitor-default-show" } }))
    end
    ngx.print(ui_helper.createTable(tod_columns,tod_data,tod_options,nil,nil),'\
  </form>\
</div>\
<script type="text/javascript" src="/js/tod-modal.js"></script>\
<script src="/js/modal-busy.js"></script>\
')
if ngx.var.request_method == "POST" and (ngx.req.get_post_args().action == "TABLE-NEW" or ngx.req.get_post_args().action == "TABLE-EDIT") then
  ngx.print('\
<script type="text/javascript">\
  $(function () {\
    var warning = \'<div class="control-group"><div style="padding-top: 12px;" class="controls"><span class="alert">',T"The Gateway will block/allow all the time if none of the days are selected",'</span></div></div>\';\
    $(\'#stoptime\').parent().parent().after(warning);\
    var warning_ap = \'<div class="control-group"><div style="padding-top: 12px; padding-bottom: 12px;" class="controls"><span class="alert">',T"The AP can be selected only once! To modify it you need to remove the rule.",'</span></div></div>\';\
    $("#ap").parent().parent().after(warning_ap);\
  });\
</script>\
');
end
ngx.print(ui_helper.createFooter())
