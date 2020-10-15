--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local string, ngx, os = string, ngx, os
local tonumber = tonumber
local floor = math.floor
local concat = table.concat
local format, match = string.format, string.match
local content_helper = require("web.content_helper")
local lp = require("web.lp")
lp.setpath("/www/snippets/")

local devices_columns = {
  {--[1]
    header = T"Status",
    name = "state",
    param = "State",
    type = "light",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  {--[2]
    header = T"Hostname",
    name = "FriendlyName",
    param = "FriendlyName",
    type = "text",
    attr = { input = { class="span3" } },
  },
  {--[3]
    header = T"IPv4",
    name = "ipv4",
    param = "IPv4",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  {--[4]
    header = T"IPv6",
    name = "ipv6",
    param = "IPv6",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  {--[5]
    header = T"MAC",
    name = "macaddress",
    param = "MACAddress",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
    additional_class = 'data-type="mac"',
  },
  {--[6]
    header = T"Interface",
    name = "interfacetype",
    param = "InterfaceType",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  {--[7]
    header = T"Connected",
    name = "ConnectionTime",
    param = "ConnectedTime",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  {--[8]
    header = T"Lease",
    name = "dhcpLeaseTime",
    param = "DhcpLeaseTime",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
}

local getargs = ngx.req.get_uri_args()
local sortcol
if (getargs.sort) then
  sortcol = getargs.sort:untaint()
else
  sortcol = "FriendlyName"
end

local device_valid = {}
local devices_options = {
  canEdit = true,
  canAdd = false,
  canDelete = false,
  tableid = "devices",
  basepath = "rpc.hosts.host.",
  sorted = sortcol,
}

local devices_filter = function(data)
  if data["L2Interface"] == "" then
    return false
  end
  if match(data["L2Interface"], "^wl0") then
    data["InterfaceType"] = "Wireless - 2.4GHz"
  elseif match(data["L2Interface"], "^wl1") then
    data["InterfaceType"] = "Wireless - 5GHz"
  elseif match(data["L2Interface"], "eth*") then
    data["InterfaceType"] = format("Ethernet - %s",data["Port"])
  elseif match(data["L2Interface"], "moca*") then
    data["InterfaceType"] = "MoCA"
  end
  if data["IPv4"] == "" or data["IPv6"] == "" then
    if data["DhcpLeaseIP"] ~= "" then
      local chunks = {data["DhcpLeaseIP"]:match("(%d+)%.(%d+)%.(%d+)%.(%d+)")}
      if (#chunks == 4) then
        data["IPv4"] = data["DhcpLeaseIP"]
      else
        data["IPv6"] = data["DhcpLeaseIP"]
      end
    end 
  end
  return true
end

local devices_data, devices_helpmsg = post_helper.handleTableQuery(devices_columns, devices_options, devices_filter, nil, device_valid)

local function secondsToTime(time)
  local time_no = tonumber(time)
  if (time_no and time_no >= 0) then
    return format("%dd %02d:%02d:%02d",floor(time_no/86400),floor(time_no/3600)%24,floor(time_no/60)%60,floor(time_no)%60)
  end
  return nil, T"Positive number expected."
end

for k, v in pairs (devices_data) do
  devices_data[k][1] = format("%s",v[1])
  devices_data[k][5] = format("%s",v[5])
    if v[8] ~= "" and v[7] ~= "" then
      v[8] = secondsToTime(tonumber(v[8])- (os.time()- tonumber(v[7]))%tonumber(v[8])) or T"Infinite"
    else
      v[8]="-"
    end
    if v[7] ~= "" then
      v[7] = secondsToTime(os.time()- tonumber(v[7]))
    else
      v[7] = "-"
    end
end

ngx.print( ui_helper.createHeader(T"Devices", false, true) ); ngx.print('\
<div class="modal-body update">\
');  lp.include("tabs-devices.lp")   ngx.print('\
  <form class="form-horizontal" method="post" action="modals/device-modal.lp">\
    ');  
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('\
    <fieldset>\
      <legend>');  ngx.print( T"Devices" ); ngx.print('</legend>\
      ');  ngx.print( ui_helper.createTable(devices_columns, devices_data, devices_options, nil, devices_helpmsg) ); ngx.print('\
    </fieldset>\
  </form>\
</div>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
<script>\
var hdrs = document.getElementsByTagName("th");\
for(var i=0;i<hdrs.length;i++){\
  var text=hdrs[i].innerText;\
  var sort="";\
  switch(text){\
    case "Hostname":sort="FriendlyName";break;\
    case "MAC":sort="MACAddress";break;\
    case "IPv4":sort="IPv4";break;\
    case "IPv6":sort="IPv6";break;\
    case "Interface":sort="L2Interface";break;\
    case "Connected":sort="ConnectedTime";break;\
    default:sort="";break;\
  }\
  if (sort!="") {\
    hdrs[i].innerHTML=\'<a href="#" data-remote="modals/device-modal.lp?sort=\'+sort+\'">\'+text+\'</a>\';\
    hdrs[i].setAttribute("class", "nav");\
  }\
}\
</script>\
'); 
