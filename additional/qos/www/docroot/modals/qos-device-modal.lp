--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

local find, match, sub, untaint = string.find, string.match, string.sub, string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean

local inUseDevices = ""
local ssid = {}
local wanifname = proxy.get("uci.network.interface.@wan.ifname")[1].value:untaint()
if wanifname and wanifname ~= "" and wanifname ~= "lo" and not match(wanifname, "^@") then
  inUseDevices = inUseDevices .. " " .. wanifname
end
local lanifname = proxy.get("uci.network.interface.@lan.ifname")[1].value:untaint()
if lanifname and lanifname ~= "" and lanifname ~= "lo" and not match(lanifname, "^@") then
  inUseDevices = inUseDevices .. " " .. lanifname
end
local wifi = proxy.getPN("uci.wireless.wifi-iface.", true)
for _,v in ipairs(wifi) do
  local ifname = match(v.path, "uci%.wireless%.wifi%-iface%.@([^%.]+)%.")
	local wlname = proxy.get(v.path .. "ssid")
  local radio_name = proxy.get("rpc.wireless.ssid.@"..ifname..".radio")[1].value
  if wlname then
    wlname = wlname[1].value
  else
    wlname = ifname
  end
  if radio_name == "radio_2G" then
    ssid[ifname] = wlname .. " (2.4G)"
  else
    ssid[ifname] = wlname .. " (5G)"
  end    
  inUseDevices = inUseDevices .. " " .. ifname
end

local shaper_options = {}
local swshapers = proxy.getPN("uci.qos.swshaper.",true)
shaper_options[#shaper_options+1] = { "", T("") }
for _,v in ipairs(swshapers) do
  local name = match(v.path, "uci%.qos%.swshaper%.@([^%.]+)%.")
  local desc = name
  if proxy.get(v.path .. "enable")[1].value:untaint() == "0" then
    desc = desc.." (Disabled)"
  end
  shaper_options[#shaper_options+1] = { name, T(desc) }
end

local group_options = {
  { "TO_LAN", T"LAN" }, 
  { "TO_WAN", T"WAN" },
  { "TO_WLAN", T"WLAN" },
}

local device_columns = {
  { -- 1
    header = T"Description",
    name = "description",
    type = "text",
    readonly = true,
  },
  { -- 2
    header = T"Device",
    name = "device",
    param = "paramindex",
    type = "text",
    readonly = true,
  },
  { -- 3
    header = T"QoS Interface",
    name = "classgroup",
    param = "classgroup",
    type = "select",
    values = group_options,
    attr = { switch = { ["data-placement"] = "center" }},
  },
  { -- 4
    header = T"Shaper",
    name = "swshaper",
    param = "swshaper",
    type = "select",
    values = shaper_options,
    attr = { input = { class = "span2" } },
  },
  { -- 5
    header = T"Device In Use?",
    name = "used",
    type = "text",
    readonly = true,
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
  swshaper = gVIES(shaper_options),
  enable = vB
}

local device_data, device_helpMsg = post_helper.handleTableQuery(device_columns, device_options, nil, nil, device_valid)
for _,v in pairs(device_data) do
  local portname
  local device = untaint(v[2])
  if device == "atm_8_35" then
    device = "atm"
    portname = "aDSL WAN"
  elseif device == "ptm0" then
    device = "ptm"
    portname = "vDSL WAN"
  elseif device == "eth4" then
    portname = "WAN Port"
  elseif sub(device,1,2) == "wl" then
    portname = ssid[device]
  else
    portname = "LAN Port" .. " " .. match(device, "%d") + 1
  end
  v[1] = portname
  if find(inUseDevices, device) then
    v[5] = "Yes"
  end
  if v[6] == "" then
    v[6] = "1"
  end
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
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
<script>\
$("#device>tbody>tr>td:nth-child(5)").each(function() {\
  var content=$(this).text();\
  if(content!="Yes"){\
    $(this).parent().css("color","gray");\
  }\
});\
</script>\
');
