--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean
local vNES = post_helper.validateNonEmptyString
local vPN = post_helper.validatePositiveNum

local find, format, gmatch, match, sub, untaint = string.find, string.format, string.gmatch, string.match, string.sub, string.untaint

local uci_qos_prefix = "uci.qos."
local uci_swshaper_path = uci_qos_prefix .. "swshaper."
local uci_device_path = uci_qos_prefix .. "device."

local shaper_columns = {
  { -- 1
    header = T"Shaper Name",
    name = "swshaper",
    param = "paramindex",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  { -- 2
    header = T"Max Upload Bit Rate",
    name = "max_bit_rate",
    param = "max_bit_rate",
    type = "text",
    default = "40000",
    attr = { input = { type = "number", min = "0", step = "1000", class = "span2" } },
  },
  { -- 3
    header = T"Enabled",
    name = "enable",
    param = "enable",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" }},
  },
  { -- 4
    header = T"Applied to Device?",
    name = "used",
    type = "text",
    readonly = true,
  },
}

local inShaperAdd = false
if ngx.req.get_method() == "POST" then
  local post_args = ngx.req.get_post_args()
  local action = post_args["action"]
  if action then
    if action == "TABLE-EDIT" then
      local tableid = post_args["tableid"]
      if tableid == "shaper" then
        shaper_columns[1]["name"] = "name"
        shaper_columns[1]["param"] = "name"
        shaper_columns[1]["readonly"] = true
      end
    elseif action == "TABLE-ADD" then
      inShaperAdd = true
    end    
  end  
end

local function onAddShaper(current_section_index,content)
  proxy.add(uci_swshaper_path,current_section_index)
  proxy.set(uci_swshaper_path.."@"..current_section_index..".max_bit_rate", untaint(content["max_bit_rate"]))
  proxy.set(uci_swshaper_path.."@"..current_section_index..".enable", untaint(content["enable"]))
  proxy.apply()
end

local function onDeleteShaper(current_section_index)
  local devlist = proxy.getPN("uci.qos.device.", true)
  local count = 0
  for _,v in pairs(devlist) do
    local path = v.path .. "swshaper"
    if untaint(proxy.get(path)[1].value) == current_section_index then
      proxy.set(path, "")
      count = count + 1
    end
  end
  if count > 0 then
    proxy.apply()
  end
end

local function validateShaperName(value, object, key)
  if inShaperAdd then
    if not value:match("^[%w]+$") then
      return nil, "must not be empty and must only contain alphanumeric characters"
    else
      if proxy.get(format("uci.qos.swshaper.@%s.enable", value)) then
        return nil, "duplicate name"
      end
    end
  end
  return true
end

local shaper_options = {
  tableid = "shaper",
  basepath = uci_swshaper_path .. "@.",
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add New Shaper",
  addNamedObject = true,
  onAdd = onAddShaper,
  onDelete = onDeleteShaper,
}

local shaper_valid = {
  swshaper = validateShaperName,
  max_bit_rate = vPN,
  enable = vB,
}

local shaper_filter = function(data)
  data["name"] = data["paramindex"]
  return true
end

local shaper_data, shaper_helpMsg = post_helper.handleTableQuery(shaper_columns, shaper_options, shaper_filter, nil, shaper_valid)

local shapers = {}
shapers[#shapers+1] = { "", T("None") }
for _,v in ipairs(shaper_data) do
  if v[3] == "0" then
    shapers[#shapers+1] = { v[1], T(v[1] .. " (Disabled)") }
  else
    shapers[#shapers+1] = { v[1], T(v[1]) }
  end
  v[4] = "No"
end

local group = {}
group["TO_LAN"] = {}
group["TO_WAN"] = {}
group["TO_WLAN"] = {}

local usageParams = {}
local usageValid = {}
local devices = {}

local ssid = {}
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
end

local qosDevices = proxy.getPN("uci.qos.device.", true)
for _,v in ipairs(qosDevices) do
  local portname
  local device = match(v.path, "uci%.qos%.device%.@([^%.]+)%.")
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
  local classgroup = untaint(proxy.get(v.path.."classgroup")[1].value)
  devices[device] = portname
  usageParams[device] = format("uci.qos.device.@%s.swshaper", device)
  usageValid[device] = gVIES(shapers)
  group[classgroup][#group[classgroup]+1] = device
end

local content, helpmsg = post_helper.handleQuery(usageParams, usageValid)

for _,s in pairs(content) do
  if s ~= "" then
    for _,v in ipairs(shaper_data) do
      if v[1] == s then
        v[4] = "Yes"
      end
    end
  end
end

local function makeHTMLfor(rows, heading)
  local sectionHTML = {}
  if #rows > 0 then
    sectionHTML[#sectionHTML+1] = "<fieldset>"
    sectionHTML[#sectionHTML+1] =   "<legend>"..T"QoS Shaper Application on Interface "..heading.."</legend>"
    table.sort(rows)
    for _,dev in pairs(rows) do
      if dev and devices[dev] and content[dev] then
        sectionHTML[#sectionHTML+1] = ui_helper.createInputSelect(T(devices[dev]), dev, shapers, content[dev])
      end
    end
    sectionHTML[#sectionHTML+1] = "<fieldset>"
  end
  if #sectionHTML > 3 then
    return sectionHTML
  end
  return {}
end

ngx.print( ui_helper.createHeader(T"QoS", false) ); ngx.print('\
<div class="modal-body">\
'); lp.include("tabs-qos.lp") ngx.print('\
  <form class="form-horizontal" method="post" action="modals/qos-swshaper-modal.lp">\
  ');  ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
  ')
    local alert_attr = {
      alert = {
        class = "alert-info"
      },
    }
    local html = {}
    html[#html+1] = "<fieldset>"
    html[#html+1] =   "<legend>"..T"QoS Shaper Profiles".."</legend>"
    html[#html+1] =   ui_helper.createAlertBlock(T("To calculate maximum bit rate, as a rule of thumb multiply your maximum upload speed by 1000. e.g. if your connection speed is 50/20, max bit rate would be 20000."), alert_attr)
    html[#html+1] =   ui_helper.createTable(shaper_columns, shaper_data, shaper_options, nil, shaper_helpMsg)
    html[#html+1] = "</fieldset>"
    ngx.print(html)
    ngx.print(makeHTMLfor(group.TO_WAN, "WAN"))
    ngx.print(makeHTMLfor(group.TO_LAN, "LAN"))
    ngx.print(makeHTMLfor(group.TO_WLAN, "WLAN"))
    ngx.print('\
  </form>\
</div>\
');  ngx.print( ui_helper.createFooter() ); ngx.print('\
<script>$("#shaper #btn-table-modify").parent().parent().find("#qosSWShaper").prop("readonly",true)</script>\
');
