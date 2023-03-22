--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local content_helper = require("web.content_helper")
local parental_helper = require("parental_helper")
local proxy = require("datamodel")

local ngx = ngx
local string,table = string,table
local concat,insert = table.concat,table.insert
local ipairs = ipairs
local tonumber = tonumber
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local match,gmatch,format = string.match,string.gmatch,string.format

local tod = parental_helper.getTodwifi()
local tod_columns = tod.columns
local untaint_mt = require("web.taint").untaint_mt
local session = ngx.ctx.session

local validateTime = post_helper.validateTime
local validDays = post_helper.getValidateInEnumSelect(tod.days)
local validateBoolean = post_helper.validateBoolean
local validMode = post_helper.getValidateInEnumSelect({{"on"},{"off"}})
local optionallyValidate = post_helper.getOptionalValidation
local getRandomKey = post_helper.getRandomKey

local isMerged = "0"

--To check whether multiAP is enabled or not
local multiap_enabled = false
local multiap_state = {
  agent = "uci.wireless.map-agent.@agent.state",
  controller = "uci.mesh_broker.meshapp.@mesh_common.controller_enabled",
  meshbrokerState = "uci.mesh_broker.global.@mesh_broker.enable"
}
content_helper.getExactContent(multiap_state)
multiap_enabled = multiap_state.agent == "1" and multiap_state.controller == "1" and multiap_state.meshbrokerState == "1"
if multiap_state.agent == "0" and  multiap_state.meshbrokerState == "1" and multiap_state.controller == "1" then
  multiap_enabled = true
end

local ap_status_map = setmetatable({
  ["0"] = T"Off",
  ["1"] = T"On",
  ["-1"] = T"Error. Re-set the rule!"
},untaint_mt)

local tod_timer_path = "uci.tod.timer."
local tod_action_path = "uci.tod.action."
local tod_wifitod_path = "uci.tod.wifitod."
local tod_ap_path = "uci.tod.ap."

-- validates days (Mon...Sun)
-- @param #table value have the value of corresponding key
-- @return #boolean or nil+error message
local function validateWeekdays(value)
  if not value then
    return nil,T"Invalid day"
  end
  for _,v in ipairs(value) do
    if not validDays(v) then
      return nil,T"Invalid day"
    end
  end
  return true
end

local tod_options = {
  tableid     = "tod",
  basepath    = "uci.tod.action.",
  createMsg   = T"Add New Rule",
  canAdd      = true,
  canDelete   = true,
  canApply    = true,
  canEdit     = true,
  objectName  = "action"..getRandomKey(),
  addNamedObject  = true
}

local function validate_wifitod(index, postcontent, timer_data)
  local todData = {
    enable      = postcontent.enable,
    mode        = postcontent.mode,
    start_time  = postcontent.start_time,
    stop_time   = postcontent.stop_time,
    weekdays    = postcontent.weekdays,
  }
  local todValidations = {
    enable      = validateBoolean,
    mode        = validMode,
    start_time  = validateTime,
    stop_time   = validateTime,
    weekdays    = optionallyValidate(validateWeekdays), -- weekdays can be empty if user didn't select any day
  }

  local success,errmsg = content_helper.validateObject(todData,todValidations)
  if not success then
    return nil,errmsg
  end

  if postcontent.start_time >= postcontent.stop_time then
    return nil,T"Stop time should be greater than Start time."
  end

  -- day/time overlap validation
  for indexVal in ipairs(timer_data) do
    if indexVal ~= postcontent.index then
      local isOverlap, errMsg = parental_helper.daysTimeOverlap(index, postcontent)
      if not isOverlap then
        return nil, errMsg
      end
    end
  end

  return true
end

local function tod_filter(data)
  return data.script == "wifitodscript"
end

-- function to add a tod rule and apply the rule
-- @param #success have the index value of the adding tod action
-- @param #content have the posted values which contains ToD table data
-- @param #onModify has the boolean value,false by default, true in rule modification state
-- @param #onActive has the boolean value,false by default, true in active TOD rule modification state
-- if active rule is edited the new action index will be added with its corresponding timers, wifitod, ap paths and finally
-- applied if no overlap or duplicate error found with old rule. old edited rule will be deleted in onModify function
local function onAdd(success,content,onModify,onActive)
  local index = success:match("action(.+)")
  if onModify or onActive then
    index = success:match("wifitod(.+)")
  end
  local setObject = {}
  content.weekdays = parental_helper.formatWeekdays(content)
  local valid, errmsg = validate_wifitod(index, content, session:retrieve("timer_data"))
  if not valid then
    tod_options.errmsg = errmsg
    return nil
  end

  if not onModify then
    proxy.add(tod_action_path .. "@" .. success .. ".timers.")
    proxy.add(tod_timer_path, "timer"..index)
    proxy.add(tod_wifitod_path, "wifitod"..index)
  end

  content.weekdays = concat(content.weekdays, ",")

  proxy.add(tod_wifitod_path .. "@wifitod"..index..".ap.")
  proxy.add(tod_ap_path, "ap" ..index)

  content.start_time = format("%s:%s",content.weekdays,content.start_time)
  content.stop_time = format("%s:%s",content.weekdays,content.stop_time)

  setObject[format("%s@action%s.timers.@1.value",tod_action_path,index)] = "timer"..index
  setObject[format("%s@action%s.enabled",tod_action_path,index)] = content.enabled
  setObject[format("%s@action%s.script",tod_action_path,index)] = "wifitodscript"
  setObject[format("%s@action%s.object",tod_action_path,index)] = "wifitod.wifitod"..index

  setObject[format("%s@timer%s.start_time",tod_timer_path,index)] = content.start_time
  setObject[format("%s@timer%s.stop_time",tod_timer_path,index)] = content.stop_time
  setObject[format("%s@timer%s.name",tod_timer_path, index)] = "wifitod"

  setObject[format("%s@wifitod%s.ap.@1.value",tod_wifitod_path,index)] = "ap"..index

  setObject[format("%s@ap%s.state",tod_ap_path,index)] = content.mode == "on" and "1" or "0"
  setObject[format("%s@ap%s.ap",tod_ap_path, index)] = "all"
  proxy.set(setObject)
  return true
end

local function onDelete(success)
  local index = success:match("wifitod(.+)")
--As per the TOD functionality Ap state will revert only when the remaining configuration is present in the tod config.
-- Only the timer list value defined in the action section has been modified as empty and the timer section has been deleted in the config.
  proxy.set(format("%s@action%s.timers.@1.value", tod_action_path, index),"")
  proxy.del(format("uci.tod.timer.@timer%s.", index))
  return true
end

local function onModify(index, content)
  local days = ""
  if type(content.weekdays) == "table" then
    for _, v in ipairs(content.weekdays) do
      days = days..untaint(v)
    end
  else
    days = untaint(content.weekdays)
  end
  local currDay, currTime = parental_helper.getCurrentDayAndTime()
--Check whether the selected rule is enabled and it is in active timeslot.
  if index ~= "" and content.enabled == "1" and match(days, currDay) and currTime >= content.start_time and currTime <= content.stop_time then
    local new_index = proxy.add(tod_action_path, tod_options.objectName)
    local success = onAdd(new_index, content)
    if not success then
      proxy.del(tod_action_path .. "@" .. tod_options.objectName .. ".")
      tod_options.editing = -1
    else
      onDelete(index)
    end
  else
    onAdd(index, content, true)
  end
  proxy.apply()
  return true
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
    current_index = proxy.add(tod_action_path,tod_options.objectName)
    local success = onAdd(current_index,postargs)
    if not success then
      proxy.del(tod_action_path.."@"..tod_options.objectName..".")
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
local action_list = content_helper.convertResultToObject(tod_action_path,proxy.get(tod_action_path))
local ap_list = content_helper.convertResultToObject(tod_ap_path,proxy.get(tod_ap_path))

local newlist_index = {}
local dnd_val = 0
for  i , v in ipairs(action_list) do
  local param_index = v["paramindex"]:match("action(.+)")
  dnd_val = v["paramindex"]:match("dnd(.+)") and dnd_val+1 or dnd_val
  if param_index then
    if v["timers.@1.value"] == "" then
      newlist_index[#newlist_index + 1] = i-dnd_val
    end
  end
end

--Section to remove the TOD rule displaying in GUI which doesn't have timer section in config.
for i = #newlist_index , 1 , -1 do
  table.remove(tod_index, newlist_index[i])
  table.remove(ap_list, newlist_index[i])
  table.remove(tod_data, newlist_index[i])
end

local tablesessionindexes = tod_options.tableid..".allowedindexes"
session:store(tablesessionindexes,tod_index)

local timer_list = content_helper.convertResultToObject(tod_timer_path, proxy.get(tod_timer_path))
local timer_data = {}

--To filter out timer section created by WiFi Tod rule
local wifitod_timer = {}
for i in ipairs(timer_list) do
  if timer_list[i].name == "wifitod" then
    wifitod_timer[#wifitod_timer+1] = timer_list[i]
  end
end

--To filter out action section created by ringing schedule rule
local wifi_action = {}
for i in ipairs(action_list) do
  if action_list[i].script == "wifitodscript" then
    wifi_action[#wifi_action+1] = action_list[i]
  end
end

for i, v in ipairs(tod_data) do
  v[1] = wifi_action[i].enabled -- status
  v[2] = ap_list[i].ap -- ap
  local days, start_time = wifitod_timer[i].start_time:match("^(.-):(.+)$")
  v[3] = start_time -- start_time
  v[4] = wifitod_timer[i].stop_time:match("^.-:(.+)$") -- stop_time
  v[5] = ap_status_map[ap_list[i].state] -- apState
  v[6] = {} -- days
  for day in gmatch(days, "[^,]+") do
    insert(v[6], untaint(day))
  end
  -- Sub Columns
  v[7] = {
    [1] = v[1], -- status
    [2] = ap_list[i].state == "1" and "on" or "off", -- apState
    [3] = v[3], -- start_time
    [4] = v[4], -- stop_time
    [5] = v[6] -- days
  }
  timer_data[i] = {
    start_time = v[3],
    stop_time = v[4],
    days = v[6]
  }
end

session:store("timer_data",timer_data)

local modal="/modals/tod_wireless-modal.lp"

ngx.print(ui_helper.createHeader(T"Time of Day"),'\
<div class="modal-body update">\
');
local lp = require("web.lp")
lp.setpath("/www/snippets/")
if ngx.var.args == "tod" then
  lp.include("tabs-tod.lp")
  modal = modal.."?tod"
else
  lp.include("tabs-wireless.lp")
end
ngx.print('\
  <form class="form-horizontal" method="post" action="',modal,'">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
    <legend>',T"Wireless Control",'</legend>\
    ');  if multiap_enabled then
      local multiap_warning = {
        alert = {
          class = "alert-warning monitor-default-show"
        }
      }
      ngx.print(ui_helper.createAlertBlock(T"Disabling wireless will disable the EasyMesh network", multiap_warning))
    end  ngx.print('\
    ');  if isMerged == "1" then
       local mergedssid_warning = {
         alert = {
           class = "alert-warning monitor-default-show"
         }
       }
       ngx.print(ui_helper.createAlertBlock(T"Existing 5 GHz ToD rules are removed and replaced by 2.4 ones.", mergedssid_warning))
    end  ngx.print('\
    ');  ngx.print(ui_helper.createTable(tod_columns,tod_data,tod_options,nil,nil)); ngx.print('\
    ');  if ngx.var.request_method == "POST" and (ngx.req.get_post_args().action == "TABLE-NEW" or ngx.req.get_post_args().action == "TABLE-EDIT") then  ngx.print('\
      <script type="text/javascript">\
        var warning = \'<div class="control-group"><div style="padding-top: 12px;" class="controls"><span class="alert">',T"The Gateway will block/allow all the time if none of the days are selected",'</span></div></div>\';\
        $(\'#stoptime\').parent().parent().after(warning);\
      </script>\
    ');  end  ngx.print('\
  </form>\
</div>\
<script type="text/javascript">\
  $(function () {\
    var opt = {\
      theme: \'android-ics light\',\
      display: \'bubble\',\
      mode: \'scroller\',\
      headerText: false,\
      timeFormat: \'HH:ii\',\
      stepMinute: 15\
    };\
    $("#starttime").mobiscroll().time(opt);\
    $("#stoptime").mobiscroll().time(opt);\
  });\
  $(".btn-table-add,.btn-table-modify").click(function() {\
    // ui_helper adds dummy hidden checkbox if no checkboxes are checked. we don\'t need that.\
    if ($("form .additional-edit input[name=weekdays]:visible:checked").length > 0) {\
      $("form input[name=weekdays]:hidden").removeAttr("name");\
    } else {\
      // we need one checkbox when no checkboxes are checked.\
      $("form input[name=weekdays]:hidden").removeAttr("name");\
      $("form input[type=checkbox]:hidden:last").attr("name","weekdays");\
    }\
  });\
</script>\
',ui_helper.createFooter())