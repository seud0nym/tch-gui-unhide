--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local interfaces = {}
for _,iface in ipairs(proxy.getPN("Device.IP.Interface.",true)) do
  local name = proxy.get(iface.path.."Name")
  if name then
    interfaces[#interfaces+1] = { iface.path:sub(1,-2), name[1].value}
  end
end

local classification_columns = {
  {
    header = T"Order",
    name = "Order",
    param = "Order",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Traffic Class",
    name = "TrafficClass",
    param = "TrafficClass",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Srce IP",
    name = "SourceIP",
    param = "SourceIP",
    type = "text",
    attr = { input = { class = "span2" } },
  },
  {
    header = T"Srce Port",
    name = "SourcePort",
    param = "SourcePort",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Dest IP",
    name = "DestIP",
    param = "DestIP",
    type = "text",
    attr = { input = { class = "span2" } },
  },
  {
    header = T"Dest Port",
    name = "DestPort",
    param = "DestPort",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Protocol",
    name = "Protocol",
    param = "Protocol",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"DSCP Check",
    name = "DSCPCheck",
    param = "DSCPCheck",
    type = "text",
    attr = { input = { class = "span2" } },
  },
  {
    header = T"DSCP Mark",
    name = "DSCPMark",
    param = "DSCPMark",
    type = "text",
    attr = { input = { class = "span2"} },
  },
  {
    header = T"DSCP Exclude",
    name = "DSCPExclude",
    param = "DSCPExclude",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" } },
  },
  {
    header = T"All Interfaces",
    name = "AllInterfaces",
    param = "AllInterfaces",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" } },
  },
  {
    header = T"Interface",
    name = "Interface",
    param = "Interface",
    type = "select",
    values = interfaces,
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Dest Interface",
    name = "X_000E50_DestInterface",
    param = "X_000E50_DestInterface",
    type = "select",
    values = interfaces,
    attr = { input = { class = "span2"} },
  },
  {
    header = T"Enabled",
    name = "Enable",
    param = "Enable",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" } },
  },
}

local classification_options = {
  tableid = "classification",
  basepath = "Device.QoS.Classification.",
  canEdit = false,
  canAdd = false,
  canDelete = false,
}

local classification_valid = {
}

local classification_data,classification_helpMsg = post_helper.handleTableQuery(classification_columns,classification_options,nil,nil,classification_valid)

for r,row in ipairs(classification_data) do
  for c,col in ipairs(row) do
    if col == "-1" then
      classification_data[r][c] = ""
    end
  end
end

ngx.print(ui_helper.createHeader(T"QoS",false)); ngx.print('\
<div class="modal-body">\
'); lp.include("tabs-qos.lp") ngx.print('\
  <form class="form-horizontal" method="post" action="modals/qos-classify-modal.lp">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>Classifications</legend>\
      ',ui_helper.createTable(classification_columns,classification_data,classification_options,nil,classification_helpMsg),'\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter());
