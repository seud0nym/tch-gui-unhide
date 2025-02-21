--pretranslated: do not change this file
gettext.textdomain('webui-qos')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint
local format,sub,upper = string.format,string.sub,string.upper

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local uci_qos_prefix = "uci.qos."
local uci_label_path = uci_qos_prefix.."label."

local gVIES = post_helper.getValidateInEnumSelect

local options_dscp_select = {
  {"",T""},
  {"0x00","BE"},--0
  {"0x08","CS1"},--8
  {"0x0A","AF11"},--10
  {"0x0C","AF12"},--12
  {"0x0E","AF13"},--14
  {"0x10","CS2"},--16
  {"0x12","AF21"},--18
  {"0x14","AF22"},--20
  {"0x16","AF23"},--22
  {"0x18","CS3"},--24
  {"0x1A","AF31"},--26
  {"0x1C","AF32"},--28
  {"0x1E","AF33"},--30
  {"0x20","CS4"},--32
  {"0x22","AF41"},--34
  {"0x24","AF42"},--36
  {"0x26","AF43"},--38
  {"0x28","CS5"},--40
  {"0x2C","<span style='font-size:7px;vertical-align:top'>VOICE-ADMIT</span>"},--44
  {"0x2E","EF"},--46
  {"0x30","CS6"},--48
  {"0x38","CS7"},----56
}

local options_trafficid = {}
for i=1,15,1 do
   local id = tostring(i)
   options_trafficid[#options_trafficid + 1] = { id,id }
end

local function dscpAsHex(value)
  if value and value ~= "" then
    local dscp = tostring(value)
    if #dscp > 2 and sub(dscp,1,2) == "0x" then
      return upper(dscp)
    else
      local decimal = tonumber(dscp)
      if decimal then
        return format("0x%02X",decimal)
      else
        for _,v in ipairs(options_dscp_select) do
          if v[2] == dscp then
            return v[1]
          end
        end
      end
    end
  end
  return value
end

local label_columns = {
  {
    header = T"Label Name",
    name = "qosLabel",
    param = "paramindex",
    type = "text",
    attr = { input = { class="span2 no-save"} },
  },
  {
    header = T"Traffic ID",
    name = "qosLabelTrafficid",
    param = "trafficid",
    type = "select",
    values = options_trafficid,
    attr = { select = { class="span1 no-save"} },
  },
  {
    header = T"DSCP",
    name = "qosLabelDSCP",
    param = "dscp",
    type = "select",
    values = options_dscp_select,
    attr = { input = { class="span1 no-save" } },
  },
}

local function onAddLabel(current_section_index,content)
	local trafficid = untaint(content["qosLabelTrafficid"])
	local trafficid_path = uci_label_path.."@"..current_section_index..".trafficid"
	local dscp = untaint(content["qosLabelDSCP"])
	local dscp_path = uci_label_path.."@"..current_section_index..".dscp"
	proxy.add(uci_label_path,current_section_index)
	proxy.set(trafficid_path,trafficid)
	proxy.set(dscp_path,dscp)
end

local label_options = {
  tableid = "label",
  basepath = uci_label_path.."@.",
  canEdit = false,
  canApply = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add Label",
  addNamedObject = true,
  sorted = "trafficid",
	onAdd = onAddLabel,
}

local label_filter = function(data)
  if data["trafficid"] == "" then
    return false
  end
  return true
end

local function validateLabelName(value)
  if not value:match("^[%w]+$") then
    return nil,"must not be empty and must only contain alphanumeric characters"
  end
  return true
end

local label_valid = {
  qosLabel = validateLabelName,
  qosLabelTrafficid = gVIES(options_trafficid),
  qosLabelDSCP = gVIES(options_dscp_select),
}

local label_data,label_helpMsg = post_helper.handleTableQuery(label_columns,label_options,label_filter,nil,label_valid)
for _,v in ipairs(label_data) do
  v[3] = dscpAsHex(untaint(v[3]))
end

ngx.print( ui_helper.createHeader(T"QoS",false) ); ngx.print('\
<div class="modal-body update">\
  ');  lp.include("tabs-qos.lp")   ngx.print('\
  <form class="form-horizontal" method="post" action="modals/qos-label-modal.lp">\
  ');  ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
	')
    local html = {}
    html[#html+1] = "<fieldset>"
    html[#html+1] =     "<legend>"..T"QoS Labels".."</legend>"
    html[#html+1] =     ui_helper.createTable(label_columns,label_data,label_options,nil,label_helpMsg)
    html[#html+1] = "</fieldset>"
    ngx.print(html)
    ngx.print('\
  </form>\
</div>\
')
ngx.print( ui_helper.createFooter() )
