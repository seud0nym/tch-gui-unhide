--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local lp = require("web.lp")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local proxy = require("datamodel")
local tod_helper = require("tod_helper")
local ui_helper = require("web.ui_helper")

local ngx = ngx
local string,table = string,table
local concat,insert = table.concat,table.insert
local ipairs = ipairs
local tonumber = tonumber
local match,gmatch,format,sub = string.match,string.gmatch,string.format,string.sub
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local untaint_mt = require("web.taint").untaint_mt
local session = ngx.ctx.session

local validateTime = post_helper.validateTime
local validateBoolean = post_helper.validateBoolean
local getRandomKey = post_helper.getRandomKey
local gVIES = post_helper.getValidateInEnumSelect

local ringing_status_map = setmetatable({
  ["off"] = T"Off",
  ["on"] = T"On"
},untaint_mt)

local profileCount = 0
local profileName = {
  {"All",T"All"},
}

--To get the number of profile and list of profile Number
local profileNumber = proxy.get("rpc.mmpbx.sip_profile.")
profileNumber = content_helper.convertResultToObject("rpc.mmpbx.sip_profile.",profileNumber)
for _,v in pairs(profileNumber) do
  if v.enabled == "1" then
    profileCount = profileCount + 1
    local name = sub(v.paramindex,2)
    local number
    if v.display_name == "" or v.display_name == v.uri then
      number = v.uri
    else
      number = v.display_name
    end
    profileName[#profileName+1] = { name,number }
  end
end

--when timer data is not defined in config, then whole rule will be deleted
for i=1,profileCount do
  local timer_path = "uci.tod.action.@dnd_profile"..i..".timers.@1.value"
  local timer_data = content_helper.convertResultToObject(timer_path,proxy.get(timer_path))
  if not next(timer_data) then
    proxy.del("uci.tod.action.@dnd_profile"..i..".")
    proxy.del("uci.tod.voicednd.@profile"..i..".")
  end
end

-- when the timer in the action section of dnd rule is empty, whole rule will be deleted.
local voice_dnd = content_helper.convertResultToObject("uci.tod.voicednd.",proxy.get("uci.tod.voicednd."))
for _,v in pairs (voice_dnd) do
  local index = match(v.paramindex,"^@voicednd(%S+)")
  local dnd_timer = proxy.get(format("uci.tod.action.@action%s.timers.",index))
  if dnd_timer[1].value == "" then
    proxy.del(format("uci.tod.action.@action%s.",index))
    proxy.del(format("uci.tod.voicednd.@voicednd%s.",index))
    proxy.apply()
  end
end

local tod_columns = {
  {
    header = T"Status",
    name = "enabled",
    param = "enabled",
    type = "light",
    readonly = true,
    attr = { input = { class="span1" } },
  }, --[1]
  {
    header = T"Number",
    name = "profile",
    param = "profile",
    type = "text",
    readonly = true,
    attr = { input = { class="span2" } },
  }, --[2]
  {
    header = T"Start Time",
    name = "start_time",
    param = "start_time",
    type = "text",
    readonly = true,
    attr = { input = { class="span2" } },
  }, --[3]
  {
    header = T"Stop Time",
    name = "stop_time",
    param = "stop_time",
    type = "text",
    readonly = true,
    attr = { input = { class="span2" } },
  }, --[4]
  {
    header = T"Ringing",
    name = "profile",
    param = "profile",
    type = "text",
    readonly = true,
    attr = { input = { class="span2" } },
  }, --[5]
  {
    header = T"Day of week",
    name = "weekdays",
    param = "weekdays",
    values = tod_helper.weekdays,
    type = "checkboxgroup",
    readonly = true,
    attr = { input = { class="span2" } },
  }, --[6]
{
    header = "", --T"ToD",
    legend = T"Add a New Ringing Schedule",
    name = "timeofday",
    type = "aggregate",
    synthesis = nil, --tod_aggregate,
    subcolumns = {
      {
        header = T"Enabled",
        name = "enabled",
        param = "enabled",
        type = "switch",
        default = "1",
        attr = { switch= { class="inline" } },
      },
      {
        header = T"Number",
        name = "profile",
        param = "profile",
        type = "select",
        values = profileName,
        default = "All",
        attr = { select = { class="span2" } },
      },
      {
        header = T"Start Time",
        name = "start_time",
        param = "start_time",
        type = "text",
        default = "00:00",
        attr = { input = { class="span2",id="starttime",style="cursor:pointer; background-color:white" } },
      },
      {
        header = T"Stop Time",
        name = "stop_time",
        param = "stop_time",
        type = "text",
        default = "23:59",
        attr = { input = { class="span2",id="stoptime",style="cursor:pointer; background-color:white" } },
      },
      {
        header = T"Day of week",
        name = "weekdays",
        param = "weekdays",
        type = "checkboxgroup",
        values = tod_helper.weekdays,
        attr = { checkbox = { class="inline" } },
      },
    }
  }, --[7]
}

local rngshdContent = {
  todStatus = "uci.tod.todvoicednd.enabled",
  profileStatus = "uci.tod.todvoicednd.ringing",
}

local rngshdValidation = {
  todStatus = post_helper.validateBoolean,
  profileStatus = post_helper.validateNonEmptyString,
}

local tod_options = {
  tableid = "tod",
  basepath = "uci.tod.voicednd.",
  createMsg = T"Add New Rule",
  canAdd = true,
  canDelete = true,
  canApply = true,
  canEdit = true,
  objectName = "action"..getRandomKey(),
  addNamedObject = true,
}

local function validate_voicednd(postcontent,timer_data)
  local todValidations = {
    enable = validateBoolean,
    profile = gVIES(profileName),
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
      if postcontent.profile == v.profile or v.profile == "All" or postcontent.profile == "All" then
        local result,error = tod_helper.validateScheduleOverlap(postcontent,v)
        if not result then
          message_helper.pushMessage(T(error),"error")
          return nil
        end
      end
    end
  end
  return true
end

local function onAdd(success,content,onModify)
  local index = success:match("action(.+)")
  if onModify then
    index = success:match("voicednd(.+)")
  end
  local setObject = {}
  if not validate_voicednd(content,session:retrieve("timer_data")) then
    return nil
  end
  if not onModify then
    proxy.add("uci.tod.action.@"..success..".timers.")
    proxy.add("uci.tod.timer.","timer"..index)
    proxy.add("uci.tod.voicednd.","voicednd"..index)
    proxy.add("uci.tod.voicednd.@voicednd"..index..".profile.")
  end
  content.weekdays = concat(content.weekdays,",")
  content.start_time = format("%s:%s",content.weekdays,content.start_time)
  content.stop_time = format("%s:%s",content.weekdays,content.stop_time)
  setObject[format("%s@action%s.timers.@1.value","uci.tod.action.",index)] = "timer"..index
  setObject[format("%s@action%s.enabled","uci.tod.action.",index)] = content.enabled
  setObject[format("%s@action%s.script","uci.tod.action.",index)] = "voicedndscript"
  setObject[format("%s@action%s.object","uci.tod.action.",index)] = "voicednd.voicednd"..index
  setObject[format("%s@timer%s.start_time","uci.tod.timer.",index)] = content.start_time
  setObject[format("%s@timer%s.stop_time","uci.tod.timer.",index)] = content.stop_time
  setObject[format("%s@timer%s.name","uci.tod.timer.",index)] = "voicednd"
  setObject[format("%s@voicednd%s.profile.@1.value","uci.tod.voicednd.",index)] = content.profile
  local ret = proxy.set(setObject)
  proxy.apply()
  return ret
end

local function onModify(index,content)
  proxy.set("uci.tod.todvoicednd.timerandactionmodified","1")
  onAdd(index,content,true)
  return true
end

local function onDelete(success)
  local index = success:match("voicednd(.+)")
  proxy.set(format("uci.tod.action.@action%s.timers.@1.value",index),"")
  proxy.set(format("uci.tod.voicednd.@voicednd%s.profile.@1.value",index),"")
  proxy.del(format("uci.tod.timer.@timer%s.",index))
  proxy.del(format("uci.tod.action.@action%s.",index))
  proxy.del(format("uci.tod.voicednd.@voicednd%s.",index))
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

local ring_data,ring_helpmsg = post_helper.handleQuery(rngshdContent,rngshdValidation)
if not ring_helpmsg then
  ring_helpmsg = {}
end

local tod_data,tod_index = content_helper.loadTableData(tod_options.basepath,tod_columns)

local tablesessionindexes = tod_options.tableid..".allowedindexes"
session:store(tablesessionindexes,tod_index)

local action_list = content_helper.convertResultToObject("uci.tod.action.",proxy.get("uci.tod.action."))
local timer_list = content_helper.convertResultToObject("uci.tod.timer.",proxy.get("uci.tod.timer."))
local voicednd_list = content_helper.convertResultToObject("uci.tod.voicednd.",proxy.get("uci.tod.voicednd."))
local ringingStatus = proxy.get("uci.tod.todvoicednd.ringing")[1].value
local timer_data = {}

--To filter out timer section created by ringing schedule rule
local dnd_timer = {}
for i in ipairs(timer_list) do
  if timer_list[i].name == "voicednd" then
    dnd_timer[#dnd_timer+1] = timer_list[i]
  end
end

--To filter out action section created by ringing schedule rule
local dnd_action = {}
--To filter out the action sections with no timer
local tod_without_timer = {}
for i in ipairs(action_list) do
  if action_list[i].script == "voicedndscript" then
    dnd_action[#dnd_action+1] = action_list[i]
  end
  if action_list[i]["timers.@1.value"] == "" then
    tod_without_timer[#tod_without_timer+1] = i
  end
end

for i = #tod_without_timer,1,-1 do
  table.remove(tod_data,tod_without_timer[i])
  table.remove(tod_index,tod_without_timer[i])
  table.remove(action_list,tod_without_timer[i])
end

for i,v in ipairs(tod_data) do
  local days,start_time = dnd_timer[i].start_time:match("^(.-):(.+)$")
  local profile = voicednd_list[i]["profile.@1.value"]
  for _,profileNum in pairs(profileName) do --To get the profile number from profileName table
    if profileNum[1] == profile then
      profile = profileNum[2]
      break
    end
  end
  v[1] = dnd_action[i].enabled -- status
  v[2] = profile  --profilenumber
  v[3] = start_time -- start_time
  v[4] = dnd_timer[i].stop_time:match("^.-:(.+)$") -- stop_time
  v[5] = ringing_status_map[ringingStatus] -- ringingState
  v[6] = {} -- days
  if days then
    for day in gmatch(days,"[^,]+") do
      insert(v[6],untaint(day))
    end
  end
  -- Sub Columns
  v[7] = {
    [1] = v[1], -- status
    [2] = voicednd_list[i]["profile.@1.value"], -- profilename
    [3] = v[3], -- start_time
    [4] = v[4], -- stop_time
    [5] = v[6] -- days
  }
  timer_data[i] = {
    start_time = v[3],
    stop_time = v[4],
    days = v[6],
    profile = voicednd_list[i]["profile.@1.value"],
  }
end

session:store("timer_data",timer_data)

local modal="/modals/tod_dnd-modal.lp"
local tabs = "tabs-voice.lp"
local header = T"Telephony"
if ngx.var.args == "tod" then
  modal = modal.."?tod"
  tabs = "tabs-tod.lp"
  header = T"Time of Day"
end
lp.setpath("/www/snippets/")

local switch_class_enable = {
  input = { class = "monitor-changes", }
}
local profileStatus_attr = {
  group = { class = "monitor-todStatus monitor-1 monitor-hidden-todStatus", },
  values = { on = "on", off = "off" }
}
local table_attr = {
  group = { class = "monitor-todStatus monitor-1 monitor-hidden-todStatus", },
}

ngx.print(ui_helper.createHeader(header,true,false),'\
<div class="modal-body update">\
'); lp.include(tabs); ngx.print('\
  <form class="form-horizontal" method="post" action="',modal,'">\
    <style>#tod .line-edit {display:none;}</style>',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <legend>',T"Do Not Disturb",'</legend>',
    ui_helper.createSwitch(T"Enabled","todStatus",ring_data["todStatus"],switch_class_enable,ring_helpmsg["todStatus"]))
    if ring_data["todStatus"] == "1" then ngx.print(
      ui_helper.createSwitch(T"In schedule, ringing is","profileStatus",ring_data["profileStatus"],profileStatus_attr,ring_helpmsg["profileStatus"]),
      ui_helper.createTable(tod_columns,tod_data,tod_options,table_attr,nil))
    end
    ngx.print('\
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
  });\
</script>\
');
end
ngx.print(ui_helper.createFooter())
