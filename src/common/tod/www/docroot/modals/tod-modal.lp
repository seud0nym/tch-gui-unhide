--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local lp = require("web.lp")
local message_helper = require("web.uimessage_helper")
local parental_helper = require("parental_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")

local ngx = ngx
local string,table = string,table
local insert = table.insert
local ipairs = ipairs
local tonumber = tonumber
local format = string.format
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local tod = parental_helper.getTod()
local tod_columns,tod_default,tod_valid,tod_sort_func,tod_mac_to_hostname =
      tod.columns,tod.default,tod.valid,tod.sort_func,tod.mac_to_hostname
local uniqueKey = post_helper.getRandomKey()
local session = ngx.ctx.session

tod_columns[7].subcolumns[2].type = "text"
tod_columns[7].subcolumns[2].attr = { input = { class="span2", maxlength="17"}, autocomplete=parental_helper.get_hosts_ac() }

if not tod.days then
  tod.days = tod_columns[6].values
end

local tod_options = {
  tableid = "tod",
  basepath = "uci.tod.host.@.",
  createMsg = T"Add New Rule",
  canAdd = true,
  canDelete = true,
  canApply = true,
  canEdit = true,
  sorted = tod_sort_func,
  objectName = uniqueKey,
  addNamedObject = true,
}

-- function that can be used to validate tod rule
-- @param #value have the value of corresponding key
-- @param #object have the POST data
-- @param #key validation key name
-- @return #boolean or nil+error message
local function todRuleVerification(value,object,key)
  if object.action == "TABLE-MODIFY" or object.action == "TABLE-EDIT" then
    local current_index = tonumber(object.index) or -1
    local allowedIndexes = session:retrieve(tod_options.tableid .. ".allowedindexes") or {}
    current_index = allowedIndexes[current_index] and allowedIndexes[current_index].paramindex or -1
    if current_index ~= -1 then
      object.index = current_index
    end
  end
  if type(object.weekdays) ~= "table" then
    if object.weekdays == "" then
      object.weekdays = {}
      for _,v in ipairs(tod.days) do
        insert(object.weekdays,v[1])
      end
    else
      object.weekdays = {untaint(object.weekdays)}
    end
  else
    for i,v in ipairs(object.weekdays) do
      object.weekdays[i] = untaint(v)
    end
  end
  local retVal,errorMsg = parental_helper.validateTodRule(value,object,key,"AccessControl")
  if not retVal then
    message_helper.pushMessage(format("%s",errorMsg),"error")
  end
  return retVal
end

tod_valid["weekdays"] = todRuleVerification

local tod_data,tod_helpmsg = post_helper.handleTableQuery(tod_columns,tod_options,nil,tod_default,tod_valid)
tod_mac_to_hostname(tod_data)

ngx.print(ui_helper.createHeader(T"Time of Day",true,false),'\
<div class="modal-body update">\
'); lp.setpath("/www/snippets/"); lp.include("tabs-tod.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="/modals/tod-modal.lp">\
    <style>#tod .line-edit {display:none;}</style>',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <legend>',T"Device Access",'</legend>',
    ui_helper.createTable(tod_columns,tod_data,tod_options,nil,tod_helpmsg),'\
  </form>\
</div>\
<script type="text/javascript" src="/js/tod-modal.js"></script>\
<script src="/js/modal-busy.js"></script>\
')
if ngx.var.request_method == "POST" and (ngx.req.get_post_args().action == "TABLE-NEW" or ngx.req.get_post_args().action == "TABLE-EDIT") then
  ngx.print('\
<script type="text/javascript">\
  $(function () {\
    var warning = \'<div class="control-group"><div style="padding-top:10px;padding-bottom:12px;" class="controls"><span class="alert">',T"Enter the host name, MAC address or IP address of the device.",'</span></div></div>\';\
    $(\'#id\').parent().parent().after(warning);\
  });\
</script>')
end
ngx.print(ui_helper.createFooter())