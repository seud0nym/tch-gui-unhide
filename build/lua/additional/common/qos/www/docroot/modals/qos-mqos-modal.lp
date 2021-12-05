--pretranslated: do not change this file

gettext.textdomain('webui-qos')

local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local proxy = require ("datamodel")
local message_helper = require("web.uimessage_helper")
local uinetwork = require("web.uinetwork_helper")
local hosts_ac = uinetwork.getAutocompleteHostsList()
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local match = string.match

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local vB = post_helper.validateBoolean
local vPN = post_helper.validatePositiveNum
local vSIM = post_helper.validateStringIsMAC

local hosts_mac = {}
for k,_ in pairs(hosts_ac) do
  local mac = match(k,"%[(.*)%]")
  hosts_mac[#hosts_mac+1] = {mac,mac}
end
hosts_mac[#hosts_mac+1] = {"custom",T"custom"}

local mapParam = {
  mqos_enabled = "uci.mqos.global.enabled",
  mqos_download = "uci.mqos.global.download",
  mqos_upload = "uci.mqos.global.upload",
  mqos_r2q = "uci.mqos.global.r2q",
}

local mapValid = {
  mqos_enabled = vB,
  mqos_download = vPN,
  mqos_upload = vPN,
  mqos_r2q = vPN,
}

local content,helpmsg = post_helper.handleQuery(mapParam,mapValid)
if content["mqos_enabled"] == "" then
  content["mqos_enabled"] = "0"
end
if content["mqos_download"] == "" then
  content["mqos_download"] = "50"
end
if content["mqos_upload"] == "" then
  content["mqos_upload"] = "20"
end
if content["mqos_r2q"] == "" then
  content["mqos_r2q"] = "35"
end

local device_columns = {
  { --[1]
    header = T"Name",
    name = "device_name",
    param = "paramindex",
    type = "text",
    attr = { input = { class="span2" } },
    unique = true,
  },
  { --[2]
    header = T"MAC Address",
    name = "mac",
    param = "mac",
    type = "select",
    values = hosts_mac,
    attr = { input = { class="span2",maxlength="17",id="lease_mac" },autocomplete=hosts_mac },
    unique = true,
  },
  { --[3]
    header = T"Max Download (Mbit/s)",
    name = "download",
    param = "download",
    type = "text",
    attr = { input = { type = "number",min = "1",max = content["mqos_download"],class="span2",style = "width:50px" } },
  },
  { --[4]
    header = T"Max Upload (Mbit/s)",
    name = "upload",
    param = "upload",
    type = "text",
    attr = { input = { type = "number",min = "1",max = content["mqos_upload"],class="span2",style = "width:50px" } },
  },
  { --[5]
    header = T"Enabled",
    name = "enabled",
    param = "enabled",
    type = "switch",
    default = "1",
  },
}
local device_valid = {
  device_name = function(value)
    if not value:match("^[%w]+$") then
      return nil,"must not be empty and must only contain alphanumeric characters"
    end
    return true
  end,
  mac = vSIM,
  download = vPN,
  upload = vPN,
  enabled = vB,
}
local device_options = {
  tableid = "devices",
  basepath = "uci.mqos.device.@.",
  createMsg = T"Add Device",
  addNamedObject = true,
  canEdit = true,
  onAdd = function(index,values)
    local base="uci.mqos.device.@"..index
    proxy.set(base..".enabled",untaint(values["enabled"]))
    proxy.set(base..".mac",untaint(values["mac"]))
    proxy.set(base..".download",untaint(values["download"]))
    proxy.set(base..".upload",untaint(values["upload"]))
    proxy.apply()
  end,
}

local post_data
if ngx.var.request_method == "POST" then
  post_data = ngx.req.get_post_args()
  if post_data.tableid == "devices" and post_data.action == "TABLE-EDIT" then
    device_columns[1].readonly = true
  end
end

local device_data,device_helpmsg = post_helper.handleTableQuery(device_columns,device_options,nil,nil,device_valid)

if post_data and post_data.tableid == "devices" and (post_data.action == "TABLE-MODIFY" or post_data.action == "TABLE-EDIT" or post_data.action == "TABLE-ADD") then
  local text_mac
  if post_data.action == "TABLE-ADD" then
    text_mac = post_data["mac"]
  else
    text_mac = device_data[tonumber(post_data.index)][2]
  end
  local flag = true
  for _,j in ipairs(hosts_mac) do
    if text_mac == j[1] then
      flag = false
      break
    end
  end
  if flag then
    device_columns[2].type = "text"
    device_columns[2].attr = { input = { class="span2",maxlength="17",id="lease_mac",value=text_mac } }
  end
end

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

local switch_attr = {
  input = {
    class ="monitor-changes",
  },
}
local rate_attr = {
  group = {
    class ="monitor-mqos_enabled monitor-1",
  },
  input = {
    type = "number",
    min = "1",
    style = "width:100px",
  }
}
local r2q_attr = {
  group = {
    class ="monitor-mqos_enabled monitor-1",
  },
  input = {
    type = "number",
    min = "35",
    step = "5",
    style = "width:100px",
  }
}
local info_box = {
  alert = {
    class = "alert alert-info monitor-mqos_enabled monitor-1",
    style = "text-align:center",
  }
}
local warn_box = {
  alert = {
    class = "alert alert-warn",
    style = "text-align:center",
  }
}

ngx.print(ui_helper.createHeader(T"QoS",false)); ngx.print('\
<div class="modal-body update">\
  '); lp.include("tabs-qos.lp");ngx.print('\
  <form class="form-horizontal" method="post" action="'); ngx.print(uri); ngx.print('">\
  '); ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
    <fieldset>\
      <legend>'); ngx.print(T"Bandwidth Shaping"); ngx.print('</legend>\
      ')
      ngx.print(
        ui_helper.createAlertBlock(T"Bandwidth shaping will cause <B>ALL</B> traffic between LAN clients and the Gateway to be limited to the specified Global Download/Upload rates, so should not be used if the Gateway is being for services such as <B>File Sharing</B>. Traffic <I>between</I> LAN clients is unaffected and will still run at normal LAN speeds.<BR>Bandwidth shaping can also increase CPU utilisation, and may not be suitable for connections above 100Mbit/s.",warn_box),
        ui_helper.createSwitch(T"Enabled","mqos_enabled",content["mqos_enabled"],switch_attr,helpmsg["mqos_enabled"]),
        ui_helper.createInputText(T"Global Download Mbit/s","mqos_download",content["mqos_download"],rate_attr,helpmsg["mqos_download"]),
        ui_helper.createInputText(T"Global Upload Mbit/s","mqos_upload",content["mqos_upload"],rate_attr,helpmsg["mqos_upload"]),
        ui_helper.createInputText(T"Global r2q<span class='icon-question-sign' title='If you see kernel messages like \"HTB: quantum of class 20001 is big. Consider r2q change.\", increase this value by 5 and restart until the message stops. The default value is 35.'></span>","mqos_r2q",content["mqos_r2q"],r2q_attr,helpmsg["mqos_r2q"]),
        ui_helper.createAlertBlock(T"To eliminate buffer bloat, set the Global Download/Upload rates slightly lower than your maximum.",info_box)
      )
      ngx.print('\
    </fieldset>\
    ')
    if content["mqos_enabled"] == "1" then
      ngx.print('\
      <fieldset class="control-group monitor-mqos_enabled monitor-1">\
        <legend>'); ngx.print( T"Devices" ); ngx.print('</legend>\
        ')
        ngx.print(ui_helper.createTable(device_columns,device_data,device_options,nil,device_helpmsg))
        ngx.print('\
      <fieldset>\
      ')
    end
    ngx.print('\
  </form>\
</div>\
<script>\
$("[name =\'mac\']").change(function () {\
  if ((this.value) == "custom") {\
    $(this).replaceWith($(\'<input/>\',{\'type\':\'text\',\'name\':\'mac\'}));\
  }\
});\
</script>\
'); 
ngx.print( ui_helper.createFooter() );
