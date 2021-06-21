--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local qosdevice_helper = require("qosdevice_helper")

local find, format, match, sub, untaint = string.find, string.format, string.match, string.sub, string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean

local ifs, ssid = qosdevice_helper.getNetworkDevices()
table.sort(ifs)

local options_if = {}
local device_max = 0
local device_min = 0
for k,v in pairs(ifs) do
  if not find(v, "Guest") then
    local backhaul = proxy.get(format("uci.wireless.wifi-iface.@%s.backhaul",k))
    if sub(k,1,1) ~= "@" and (sub(k,1,2) ~= "wl" or (backhaul and backhaul[1].value ~= "1")) then
      device_max = device_max + 1
      options_if[#options_if+1] = {k, k}
      if proxy.get(format("uci.qos.device.@%s.enable", k)) then
        device_min = device_min + 1
      end
    end
  end
end

local options_group = {
  { "TO_LAN", T"LAN" }, 
  { "TO_WAN", T"WAN" },
  { "TO_WLAN", T"WLAN" },
}

local device_columns = {
  { -- 1
    header = T"Device",
    name = "device",
    param = "paramindex",
    type = "select",
    values = options_if,
    attr = { input = { class="span1" } },
  },
  { -- 2
    header = T"Description",
    name = "description",
    type = "text",
    readonly = true,
  },
  { -- 3
    header = T"Network Interface",
    name = "networkif",
    type = "text",
    readonly = true,
  },
  { -- 4
    header = T"QoS Interface",
    name = "classgroup",
    param = "classgroup",
    type = "select",
    values = options_group,
    attr = { switch = { ["data-placement"] = "center" }},
  },
  {-- 5
    header = T"In Use",
    name = "state",
    param = "State",
    type = "light",
    readonly = true,
    attr = { light = { ["data-placement"] = "center" }},
  },
  { -- 6
    header = T"QoS Enabled?",
    name = "enable",
    param = "enable",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" }},
  },
}

local function onAddDevice(current_section_index,content)
  proxy.add("uci.qos.device.",current_section_index)
  proxy.set("uci.qos.device.@"..current_section_index..".classgroup", untaint(content["classgroup"]))
  proxy.set("uci.qos.device.@"..current_section_index..".enable", untaint(content["enable"]))
  proxy.apply()
end

local device_options = {
  tableid = "device",
  basepath = "uci.qos.device.@.",
  canEdit = true,
  canAdd = true,
  canDelete = false,
  canApply = true,
  sorted = "paramindex",
  minEntries = device_min,
  maxEntries = device_max,
  createMsg = T"Add Missing Device",
  addNamedObject = true,
  onAdd = onAddDevice,
}

local device_valid = {
  device = vNES,
  classgroup = gVIES(options_group),
  enable = vB
}

if ngx.req.get_method() == "POST" then
  local post_args = ngx.req.get_post_args()
  local action = post_args["action"]
  if action then
    if action == "TABLE-EDIT" then
      local tableid = post_args["tableid"]
      if tableid == "device" then
        device_columns[1]["name"] = "name"
        device_columns[1]["readonly"] = true
      end
    end    
  end  
end


local device_data, device_helpMsg = post_helper.handleTableQuery(device_columns, device_options, nil, nil, device_valid)
local script = {}
for _,v in pairs(device_data) do
  local portname
  local device = untaint(v[1])
  local classgroup = untaint(v[4])
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
  v[3] = ""
  if v[6] == "" then
    v[6] = "1"
  end
  if v[6] == "1" then
    if not ifs[device] or ifs[device] == "" or ifs[device] == "ppp" or ifs[device] == "ipoe" then
      v[5] = "0"
    elseif (classgroup == "TO_WAN" and not find(ifs[device], "wan")) or (classgroup == "TO_WLAN" and not find(ifs[device], "wlan")) or (classgroup == "TO_LAN" and not match(ifs[device], "^lan") and not find(ifs[device], ",lan")) then
      v[5] = "2"
      v[3] = "<i class='icon-warning-sign'></i>"
    else
      v[5] = "1"
    end
  else
    v[5] = "0"
  end
  v[3] = v[3] .. (ifs[device] or "")
  script[#script+1] = format("$(\"#device option[value='%s']\").remove();", device)
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
<script>\
');  ngx.print(script); ngx.print('\
$("#device option[value=\'lo\']").remove();\
</script>\
');  ngx.print( ui_helper.createFooter() ); 
