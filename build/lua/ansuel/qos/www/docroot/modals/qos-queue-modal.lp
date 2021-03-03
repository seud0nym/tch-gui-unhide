--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-qos')

local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local uimessage_helper = require("web.uimessage_helper")
local proxy = require("datamodel")
local qosdevice_helper = require("qosdevice_helper")

local table, ipairs = table, ipairs
local find, format, gmatch, match, sub, untaint = string.find, string.format, string.gmatch, string.match, string.sub, string.untaint
local concat = table.concat

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local uci_classgroup_content = {
  wan_queues = "uci.qos.classgroup.@TO_WAN.classes",
  lan_queues = "uci.qos.classgroup.@TO_LAN.classes",
	wlan_queues = "uci.qos.classgroup.@TO_WLAN.classes",
}

content_helper.getExactContent(uci_classgroup_content)

local function get_queue_data(queues_string)
  local queues = format("%s", queues_string)
  local queue_table, queue_options = {}, {}
  for q in gmatch(queues, "([^%s]+)") do
    queue_table[q] = true
    queue_options[#queue_options+1] = { q, q }
  end
  return queue_table, queue_options
end

local wan_queue_table, wan_queue_options = get_queue_data(uci_classgroup_content.wan_queues)
local lan_queue_table, lan_queue_options = get_queue_data(uci_classgroup_content.lan_queues)
local wlan_queue_table, wlan_queue_options = get_queue_data(uci_classgroup_content.wlan_queues)

-- Shortcuts to validation helpers to make lines shorter
local vNES = post_helper.validateNonEmptyString
local vB = post_helper.validateBoolean
local gVIC = post_helper.getValidateInCheckboxgroup
local gVIES = post_helper.getValidateInEnumSelect
local vPN = post_helper.validatePositiveNum
local gOV = post_helper.getOptionalValidation
local gVCS = post_helper.getValidateCheckboxSwitch

local policy_options = {
  {"sp", T"Strict Priority"},
  {"wrr", T"Weighted Round Robin"},
  {"sp_wrr", T"Strict Priority and Weighted Round Robin"},
}

local trafficid_options = {}
for id=0,15,1 do
  trafficid_options[#trafficid_options+1] = { tostring(id), tostring(id) }
end

local mapParams = {
  wan_queues_policy = "uci.qos.classgroup.@TO_WAN.policy",
  wan_queues_default = "uci.qos.classgroup.@TO_WAN.default",
  lan_queues_policy = "uci.qos.classgroup.@TO_LAN.policy",
  lan_queues_default = "uci.qos.classgroup.@TO_LAN.default",
	wlan_queues_policy = "uci.qos.classgroup.@TO_WLAN.policy",
  wlan_queues_default = "uci.qos.classgroup.@TO_WLAN.default",
}

local mapValid = {
  wan_queues_policy = gVIES(policy_options),
  wan_queues_default = gVIES(wan_queue_options),
  lan_queues_policy = gVIES(policy_options),
	lan_queues_default = gVIES(lan_queue_options),
	wlan_queues_policy = gVIES(policy_options),
	wlan_queues_default = gVIES(wlan_queue_options),
}

local function isChecked(key, checked)
	if untaint(checked) == "" then
		return false
	end
	for _,v in ipairs(checked) do
		if v == key then
			return true
		end
	end
  return false
end

local ifs, ssid = qosdevice_helper.getNetworkDevices()

local wandevices = {}
local lanports = {}
local wldevices = {}

local qosDevices = proxy.getPN("uci.qos.device.", true)
for _,v in ipairs(qosDevices) do
  local portname
  local device = untaint(match(v.path, "uci%.qos%.device%.@([^%.]+)%."))
  if ifs[device] then
    if device == "atm_8_35" then
      portname = "aDSL WAN"
    elseif device == "ptm0" then
      portname = "vDSL WAN"
    elseif device == "eth4" then
      portname = "WAN Port"
    elseif sub(device,1,2) == "wl" then
      portname = ssid[device]
    else
      portname = "LAN Port" .. " " .. match(device, "%d") + 1
    end
    local group = proxy.get(v.path.."classgroup")[1].value
    if group == "TO_WAN" then
      table.insert(wandevices, {device, portname})
    elseif group == "TO_LAN" then
      table.insert(lanports, {device, portname})
    elseif group == "TO_WLAN" then
      table.insert(wldevices, {device, portname})
    end
    mapParams[device] = format("uci.qos.device.@%s.enable", device)
    mapValid[device] = gOV(vB)
  end
end

local function validateWanDevice(value, object, key)
  local getValidateEthports = gVIC(wandevices)
  local ok, msg = getValidateEthports(value, object, key)
  if not ok then
    return ok, msg
  end
  for k,v in ipairs(wandevices) do
    object[v[1]] = nil
    object[v[1]] = isChecked(v[1], value) and "1" or "0"
  end
  return true
end

local function validateLanPort(value, object, key)
  local getValidateEthports = gVIC(lanports)
  local ok, msg = getValidateEthports(value, object, key)
  if not ok then
    return ok, msg
  end
  for k,v in ipairs(lanports) do
    object[v[1]] = nil
    object[v[1]] = isChecked(v[1], value) and "1" or "0"
  end
  return true
end

local function validateWlanDevice(value, object, key)
  local getValidateWlDevices = gVIC(wldevices)
  local ok, msg = getValidateWlDevices(value, object, key)
  if not ok then
    return ok, msg
  end
  for k,v in ipairs(wldevices) do
    object[v[1]] = nil
    object[v[1]] = isChecked(v[1], value) and "1" or "0"
  end
  return true
end

mapValid.wandevices = validateWanDevice
mapValid.lanports = validateLanPort
mapValid.wldevices = validateWlanDevice
	
local content, helpmsg = post_helper.handleQuery(mapParams, mapValid)

local function setDefaultEnabled(devices)
  for _,v in pairs(devices) do
    if content[v[1]] == "" then
      content[v[1]] = "1"
    end
  end
end  

setDefaultEnabled(wandevices)
setDefaultEnabled(lanports)
setDefaultEnabled(wldevices)

-- queue table
local classes_columns = {
  { --1
    header = T"Name",
    name = "class_name",
    param = "paramindex",
    type = "text",
    readonly = true,
    attr = { input = { class="span2" } },
  },
  { --2
    header = T"Priority",
    name = "priority",
    param = "priority",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --3
    header = T"Weight",
    name = "weight",
    param = "weight",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --4
    header = T"Traffic ID",
    name = "trafficid",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --5
    header = "",
    legend = T"QoS Queue on Interface WAN",
    name = "wan_queues",
    type = "aggregate",
    synthesis = nil, --tod_aggregate,
    subcolumns = {
      {
        header = T"Priority",
        name = "priority",
        param = "priority",
        type = "text",
        attr = { input = { class="span2" } },
      },
      {
        header = T"Weight",
        name = "weight",
        param = "weight",
        type = "text",
        attr = { input = { class="span2" } },
      },
      {
        header = T"Traffic ID",
        name = "trafficid",
        param = "trafficid",
        type = "checkboxgroup",
        values = trafficid_options,
        attr = { checkbox = { class="inline" } },
      },
    },
  }, --[5]
  { --6
    header = T"Default",
    name = "default_queue",
    type = "text",
    readonly = true,
  },
}

local function validateWeight(value)
  if value == "" then
    return true
  end
  return vPN(value)
end

local classes_valid = {
  priority = vPN,
  weight = validateWeight,
  trafficid = gVIC(trafficid_options),
}

local wan_classes_options = {
  tableid = "wan_classes",
  basepath = "uci.qos.class.@.",
  createMsg = T"Add new queue",
  canAdd = false,
  canDelete = false,
  sorted = "paramindex",
}

local function wan_classes_filter(data)
  return wan_queue_table[data["paramindex"]]
end

local wan_classes_data, wan_classes_helpmsg = post_helper.handleTableQuery(classes_columns, wan_classes_options, wan_classes_filter, nil, classes_valid)

local lan_classes_options = {
  tableid = "lan_classes",
  basepath = "uci.qos.class.@.",
  createMsg = T"Add new queue",
  canAdd = false,
  canDelete = false,
  sorted = "paramindex",
}

local function lan_classes_filter(data)
  return lan_queue_table[data["paramindex"]]
end

local lan_classes_data, lan_classes_helpmsg = post_helper.handleTableQuery(classes_columns, lan_classes_options, lan_classes_filter, nil, classes_valid)

local wlan_classes_options = {
  tableid = "wlan_classes",
  basepath = "uci.qos.class.@.",
  createMsg = T"Add new queue",
  canAdd = false,
  canDelete = false,
  sorted = "paramindex",
}

local function wlan_classes_filter(data)
  return wlan_queue_table[data["paramindex"]]
end

local wlan_classes_data, wlan_classes_helpmsg = post_helper.handleTableQuery(classes_columns, wlan_classes_options, wlan_classes_filter, nil, classes_valid)

local function post_data_process(classes_data, queues_default)
  for _,v in ipairs(classes_data) do
    v[4] = concat(v[5][3],",")
    if v[1] == queues_default then
      v[6] = "Yes"
    else
      v[6] = "-"
    end
  end
end

local wandevices_checked = {}
if type(content) == "table" then
  for _,v in ipairs(wandevices) do
    if (content[v[1]] == "1") then
      wandevices_checked[#wandevices_checked + 1] = v[1]
    end
  end
end

local lanports_checked = {}
if type(content) == "table" then
  for _,v in ipairs(lanports) do
    if (content[v[1]] == "1") then
      lanports_checked[#lanports_checked + 1] = v[1]
    end
  end
end

local wldevices_checked = {}
if type(content) == "table" then
  for _,v in ipairs(wldevices) do
    if (content[v[1]] == "1") then
      wldevices_checked[#wldevices_checked + 1] = v[1]
    end
  end
end

post_data_process(wan_classes_data, content.wan_queues_default)
post_data_process(lan_classes_data, content.lan_queues_default)
post_data_process(wlan_classes_data, content.wlan_queues_default)

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri .. "?" .. ngx.var.args
end

ngx.print( ui_helper.createHeader(T"QoS", false) ); ngx.print('\
<div class="modal-body update">\
  ');  lp.include("tabs-qos.lp")   ngx.print('\
  <form class="form-horizontal" method="post" action="'); ngx.print(uri); ngx.print('">\
  '); ngx.print( ui_helper.createMessages(uimessage_helper.popMessages()) )
	local html = {}
	
  html[#html+1] = "<fieldset>"
  html[#html+1] =   "<legend>" ..  T"QoS Queue Scheduling on Interface WAN".."</legend>"

	if content["wan_queues_enabled"] then
		html[#html+1] =     ui_helper.createSwitch(T"Enabled", "wan_queues_enabled", content["wan_queues_enabled"])
	elseif proxy.get("uci.qos.device.@eth4.enable") then
		html[#html + 1] = ui_helper.createCheckboxGroup(T"QoS WAN Status", "wandevices", wandevices, wandevices_checked, {checkbox = { class="inline" }}, nil)
	end
  html[#html+1] =     ui_helper.createInputSelect(T"Algorithm", "wan_queues_policy", policy_options, content["wan_queues_policy"])
  html[#html+1] =     ui_helper.createInputSelect(T"Default Queue", "wan_queues_default", wan_queue_options, content["wan_queues_default"])

  html[#html+1] =   "<legend><small>" .. T"QoS Queues on Interface WAN".."</small></legend>"
	html[#html+1] =   	ui_helper.createTable(classes_columns, wan_classes_data, wan_classes_options, nil, wan_classes_helpmsg)
  html[#html+1] = "</fieldset>"

  html[#html+1] = "<fieldset>"
  html[#html+1] =   "<legend>"..T"QoS Queue Scheduling on Interface LAN".."</legend>"
	
	if content["lan_queues_enabled"] then
		html[#html+1] =     ui_helper.createSwitch(T"Enabled", "lan_queues_enabled", content["lan_queues_enabled"])
	elseif proxy.get("uci.qos.device.@eth0.enable") then
		html[#html + 1] = ui_helper.createCheckboxGroup(T"QoS LAN Status", "lanports", lanports, lanports_checked, {checkbox = { class="inline" }}, nil)
	end
  html[#html+1] =     ui_helper.createInputSelect(T"Algorithm", "lan_queues_policy", policy_options, content["lan_queues_policy"])
  html[#html+1] =     ui_helper.createInputSelect(T"Default Queue", "lan_queues_default", lan_queue_options, content["lan_queues_default"])

  html[#html+1] = "</fieldset>"
  html[#html+1] = "<fieldset>"
  html[#html+1] =   "<legend><small>".. T"QoS Queues on Interface LAN".."</small></legend>"

  --legend = T"QoS Queue Table on Interface WAN"
  --is hardcoded in 'classes_columns',but for LAN edit panel, should be "QoS Queue Table on Interface LAN", change here
  classes_columns[5]["legend"] = T"QoS Queue on Interface LAN"

  html[#html+1] =   	ui_helper.createTable(classes_columns, lan_classes_data, lan_classes_options, nil, lan_classes_helpmsg)
  html[#html+1] = "</fieldset>"

	html[#html+1] = "<fieldset>"
  html[#html+1] =   "<legend>".. T"QoS Queue Scheduling on Interface WLAN".."</legend>"
	
	html[#html+1] = 	ui_helper.createCheckboxGroup(T"QoS Wireless Status", "wldevices", wldevices, wldevices_checked, {checkbox = { class="inline" }}, nil)
  html[#html+1] =     ui_helper.createInputSelect(T"Algorithm", "wlan_queues_policy", policy_options, content["wlan_queues_policy"])
  html[#html+1] =     ui_helper.createInputSelect(T"Default Queue", "wlan_queues_default", wlan_queue_options, content["wlan_queues_default"])

  html[#html+1] = "</fieldset>"
  html[#html+1] = "<fieldset>"
  html[#html+1] =   "<legend><small>".. T"QoS Queues on Interface WLAN".."</small></legend>"

  --legend = T"QoS Queue Table on Interface WAN"
  --is hardcoded in 'classes_columns',but for LAN edit panel, should be "QoS Queue Table on Interface LAN", change here
  classes_columns[5]["legend"] = T"QoS Queue on Interface WLAN"

  html[#html+1] =   	ui_helper.createTable(classes_columns, wlan_classes_data, wlan_classes_options, nil, wlan_classes_helpmsg)
  html[#html+1] = "</fieldset>"
	
	ngx.print(html)
	
	ngx.print('\
  </form>\
</div>\
');

ngx.print( ui_helper.createFooter() )
