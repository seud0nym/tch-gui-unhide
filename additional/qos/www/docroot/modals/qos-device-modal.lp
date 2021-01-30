--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local qosdevice_helper = require("qosdevice_helper")

local find, match, sub, untaint = string.find, string.match, string.sub, string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean

local ifs, ssid = qosdevice_helper.getNetworkDevices()

local group_options = {
  { "TO_LAN", T"LAN" }, 
  { "TO_WAN", T"WAN" },
  { "TO_WLAN", T"WLAN" },
}

local device_columns = {
  {-- 1
    header = T"Status",
    name = "state",
    param = "State",
    type = "light",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { -- 2
    header = T"Description",
    name = "description",
    type = "text",
    readonly = true,
  },
  { -- 3
    header = T"Device",
    name = "device",
    param = "paramindex",
    type = "text",
    readonly = true,
  },
  { -- 4
    header = T"Network Interface",
    name = "networkif",
    type = "text",
    readonly = true,
  },
  { -- 5
    header = T"QoS Interface",
    name = "classgroup",
    param = "classgroup",
    type = "select",
    values = group_options,
    attr = { switch = { ["data-placement"] = "center" }},
  },
  { -- 6
    header = T"QoS Enabled?",
    name = "enable",
    param = "enable",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" }},
  },
}

local device_options = {
  tableid = "device",
  basepath = "uci.qos.device.@.",
  canEdit = true,
  canAdd = false,
  canDelete = false,
  canApply = true,
  sorted = "paramindex",
}

local device_valid = {
  device = vNES,
  classgroup = gVIES(group_options),
  enable = vB
}

local device_data, device_helpMsg = post_helper.handleTableQuery(device_columns, device_options, nil, nil, device_valid)
for _,v in pairs(device_data) do
  local portname
  local device = untaint(v[3])
  local classgroup = untaint(v[5])
  if device == "atm_8_35" then
    portname = "aDSL WAN"
  elseif device == "ptm0" then
    portname = "vDSL WAN"
  elseif device == "eth4" then
    portname = "Ethernet WAN Port"
  elseif sub(device,1,2) == "wl" then
    portname = ssid[device]
  else
    portname = "Ethernet LAN Port" .. " " .. match(device, "%d") + 1
  end
  v[2] = portname
  v[4] = ""
  if v[6] == "" then
    v[6] = "1"
  end
  if v[6] == "1" then
    if not ifs[device] or ifs[device] == "" or ifs[device] == "ppp" or ifs[device] == "ipoe" then
      v[1] = "0"
    elseif (classgroup == "TO_WAN" and not find(ifs[device], "wan")) or (classgroup == "TO_WLAN" and not find(ifs[device], "wlan")) or (classgroup == "TO_LAN" and not match(ifs[device], "^lan") and not find(ifs[device], ",lan")) then
      v[1] = "2"
      v[4] = "<i class='icon-warning-sign'></i>"
    else
      v[1] = "1"
    end
  else
    v[1] = "0"
  end
  v[4] = v[4] .. (ifs[device] or "")
end

ngx.print( ui_helper.createHeader(T"QoS", false) ); ngx.print('\
<div class="modal-body">\
'); lp.include("tabs-qos.lp") ngx.print('\
  <form class="form-horizontal" method="post" action="modals/qos-device-modal.lp">\
  ');  ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
  ')
    local html = {}
    html[#html+1] = "<fieldset>"
    html[#html+1] =   "<legend>"..T"QoS Devices".."</legend>"
    html[#html+1] =   ui_helper.createTable(device_columns, device_data, device_options, nil, device_helpMsg)
    html[#html+1] = "</fieldset>"
    ngx.print(html)
    ngx.print('\
  </form>\
</div>\
');  ngx.print( ui_helper.createFooter() ); 
