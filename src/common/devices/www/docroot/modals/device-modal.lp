--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local devices_helper = require("devices_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local string,ngx = string,ngx
local match = string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

--region get args
local getargs = ngx.req.get_uri_args()

local sortcol
if getargs.sort then
  sortcol = untaint(getargs.sort)
else
  sortcol = "FriendlyName"
end

local connected
local connectedAction
if (getargs.connected) then
  connected = untaint(getargs.connected)
else
  connected = "Active"
end
if connected == "Active" then
  connectedAction = "All"
else
  connectedAction = "Active"
end

local tabs = "tabs-lan.lp"
local header = T"Local Network"
local modal_param=""
if getargs.dev and getargs.dev == "1" then
  modal_param = "&dev=1"
  tabs = "tabs-devices.lp"
  header = T"Devices"
end
--endregion

--region post args
local editing = false
local interval = 10
local postargs
if ngx.var.request_method == "POST" then
  postargs = ngx.req.get_post_args()
  if ngx.var.content_type and match(ngx.var.content_type,"application/x%-www%-form%-urlencoded") then
    local action = postargs.action
    if action == "TABLE-EDIT" then
      ---@diagnostic disable-next-line: cast-local-type
      interval = nil
      editing = true
    elseif action == "TABLE-MODIFY" then
      editing = true
    end
  end
end
--endregion

local wifi = devices_helper.getWiFi()

-- !!! These columns are styled in common-override.css !!!
local devices_columns = {
  {--[1]
    header = T"Status",
    name = "state",
    param = "State",
    type = "light",
    readonly = true,
  },
  {--[2]
    header = T"Hostname",
    name = "FriendlyName",
    param = "FriendlyName",
    type = "text",
    attr = { input = { class="span3 no-save" } },
  },
  {--[3]
    header = T"IPv4",
    name = "ipv4",
    param = "IPv4",
    type = "text",
    readonly = true,
  },
  {--[4]
    header = T"IPv6",
    name = "ipv6",
    param = "IPv6",
    type = "text",
    readonly = true,
  },
  {--[5]
    header = T"MAC",
    name = "macaddress",
    param = "MACAddress",
    type = "text",
    readonly = true,
    additional_class = 'data-type="mac"',
  },
  {--[6]
    header = T"Interface",
    name = "interfacetype",
    param = "InterfaceType",
    type = "text",
    readonly = true,
  },
  {--[7]
    header = T"Connected",
    name = "ConnectionTime",
    param = "ConnectedTime",
    type = "text",
    readonly = true,
  },
  {--[8]
    header = T"Lease Time",
    name = "dhcpLeaseTime",
    param = "DhcpLeaseTime",
    type = "text",
    readonly = true,
  },
  { --[9]
    header = T"Type",
    name = "LeaseType",
    param = "LeaseType",
    type = "text",
    readonly = true,
  },
  { --[10]
    header = T"Priority<span class='icon-question-sign' title='Prioritize outgoing traffic'></span>",
    name = "priority",
    param = "Priority",
    type = "switch",
    attr = { input = { class="span3 no-save" } },
  },
  {--[11]
    header = T"",
    name = "IPAddress",
    param = "IPAddress",
    type = "hidden",
    readonly = true,
  },
}
local dumaOS = proxy.get("uci.dumaos.tr69.dumaos_enabled")
if dumaOS and dumaOS[1].value == "1" then
  -- DumaOS controls device priority
  table.remove(devices_columns,10)
end

local devices_options = {
  canEdit = true,
  canAdd = false,
  canDelete = false,
  tableid = "devices",
  basepath = "rpc.hosts.host.",
  sorted = devices_helper.sorter(sortcol,wifi),
  onModify = devices_helper.modify,
}

local devices_data,devices_helpmsg = post_helper.handleTableQuery(devices_columns,devices_options,devices_helper.filter(wifi,connected,getargs.cache,editing),nil,nil)

ngx.print(ui_helper.createHeader(header,false,true,interval),'\
<div class="modal-body update">\
'); lp.include(tabs); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/device-modal.lp?sort=',sortcol,'&connected=',connected,modal_param,'">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>',
      ui_helper.createTable(devices_columns,devices_data,devices_options,nil,devices_helpmsg),'\
    </fieldset>\
  </form>\
</div>\
<script src="/js/modal-busy.js"></script>\
',ui_helper.createFooter(),'\
<script src="/js/device-modal.js"></script>\
<script>\
var hdrs = document.getElementsByTagName("th");\
for(var i=0;i<hdrs.length;i++){\
  var text=hdrs[i].innerText;\
  var sort="";\
  var title="";\
  var connected="',connected,'";\
  switch(text){\
    case "Status":sort="',sortcol,'";connected="',connectedAction,'";title="Show ',connectedAction,'";break;\
    case "Hostname":sort="FriendlyName";break;\
    case "IPv4":sort="IPv4";break;\
    case "IPv6":sort="IPv6";break;\
    case "MAC":sort="MACAddress";break;\
    case "Interface":sort="InterfaceType";break;\
    case "Connected":sort="ConnectedTime";break;\
    default:sort="";break;\
  }\
  if (sort!="") {\
    if (title=="") {\
      title="Sort by "+text;\
    }\
    hdrs[i].innerHTML=\'<a href="#" title="\'+title+\'" onclick="return showLoadingWrapper();" data-remote="modals/device-modal.lp?sort=\'+sort+\'&connected=\'+connected+\'',modal_param,'">\'+text+\'</a>\';\
    hdrs[i].setAttribute("class","nav");\
  }\
}\
</script>\
');
