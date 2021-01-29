--pretranslated: do not change this file
gettext.textdomain('webui-qos')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local session = ngx.ctx.session

local match, format, sub, untaint, upper = string.match, string.format, string.sub, string.untaint, string.upper

local uniqueKey = post_helper.getRandomKey()

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local uci_qos_prefix = "uci.qos."
local uci_label_path = uci_qos_prefix .. "label."
local uci_classify_path = uci_qos_prefix .. "classify."
local uci_reclassify_path = uci_qos_prefix .. "reclassify."

local gVIC = post_helper.getValidateInCheckboxgroup
local gVIES = post_helper.getValidateInEnumSelect
local vSII = post_helper.validateStringIsIP
local vSIP = post_helper.validateStringIsPort
local gOV = post_helper.getOptionalValidation
local vNES = post_helper.validateNonEmptyString

local options_proto = {
  {"all", T"all"},
  {"tcp", T"tcp"},
  {"udp", T"udp"},
  {"udplite", T"udplite"},
  {"icmp", T"icmp"},
  {"esp", T"esp"},
  {"ah", T"ah"},
  {"sctp", T"sctp"},
  {"igmp", T"igmp"},
  {"gre", T"gre"},
}

local options_dscp = {
  {"0x00", "BE"},--0
  {"0x08", "CS1"},--8
  {"0x0A", "AF11"},--10
  {"0x0C", "AF12"},--12
  {"0x0E", "AF13"},--14
  {"0x10", "CS2"},--16
  {"0x12", "AF21"},--18
  {"0x14", "AF22"},--20
  {"0x16", "AF23"},--22
  {"0x18", "CS3"},--24
  {"0x1A", "AF31"},--26
  {"0x1C", "AF32"},--28
  {"0x1E", "AF33"},--30
  {"0x20", "CS4"},--32
  {"0x22", "AF41"},--34
  {"0x24", "AF42"},--36
  {"0x26", "AF43"},--38
  {"0x28", "CS5"},--40
  {"0x2C", "<span style='font-size:7px;vertical-align:top'>VOICE-ADMIT</span>"},--44
  {"0x2E", "EF"}, --46
  {"0x30", "CS6"},--48
  {"0x38", "CS7"}, ----56
}
local options_dscp_select = {
  {"", T""},
}
for i=1,22,1 do 
  options_dscp_select[#options_dscp_select+1] = options_dscp[i]
  local meaning = "!"..options_dscp[i][2]
  if sub(meaning,2,1) == "<" then
    local html1, text, html2 = match(meaning, "(.*>)([^<]+)(<.*)")
    meaning = html1.."!"..text..html2
  end
  options_dscp[#options_dscp+1] = { "!"..options_dscp[i][1], meaning }
end

local options_trafficid = {}
for i=1,15,1 do
   local id = tostring(i)
   options_trafficid[#options_trafficid + 1] = { id, id }
end

local function dscpAsHex(value) 
  if value and value ~= "" then
    local dscp = tostring(value)
    if #dscp > 2 and sub(1,2) == "0x" then
      return upper(dscp)
    else
      local decimal = tonumber(dscp)
      if decimal then
        return format("0x%02X", decimal)
      else
        for _,v in ipairs(options_dscp) do
          if v[2] == dscp then
            return v[1].value
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
    attr = { input = { class="span2"} },
  },
  {
    header = T"Traffic ID",
    name = "qosLabelTrafficid",
    param = "trafficid",
    type = "select",
    values = options_trafficid,
    attr = { select = { class="span1"} },
  },
  {
    header = T"DSCP",
    name = "qosLabelDSCP",
    param = "dscp",
    type = "select",
    values = options_dscp_select,
    attr = { input = { class="span1" } },
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
  basepath = uci_label_path .. "@.",
  canEdit = false,
  canApply = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add New Label",
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

local label_valid = {
  qosLabel = vNES,
  qosLabelTrafficid = gVIES(options_trafficid),
  qosLabelDSCP = gVIES(options_dscp_select),
}

local label_data, label_helpMsg = post_helper.handleTableQuery(label_columns, label_options, label_filter, nil, label_valid)
for _,v in ipairs(label_data) do
  v[3] = dscpAsHex(untaint(v[3]))
end

local options_labeltarget = {}
for _,v in ipairs(proxy.getPN(uci_label_path, true)) do
  local label = match(v.path, "uci%.qos%.label%.@([^%.]+)%.")
  if label then
    options_labeltarget[#options_labeltarget+1] = { label, label }
  end
end

local classify_columns = {
  { --1
    header = T"Label Target",
    name = "target",
    param = "target",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --2
    header = T"Source Host/IP",
    name = "qosClassifySrcHost",
    param = "srchost",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --3
    header = T"Dest Host/IP",
    name = "qosClassifyDstHost",
    param = "dsthost",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --4
    header = T"Protocol",
    name = "qosClassifyProto",
    param = "proto",
    default = "tcp",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --5
    header = T"Port(s)",
    name = "qosClassifyPorts",
    param = "ports",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --6
    header = T"Source Port(s)",
    name = "qosClassifySrcPorts",
    param = "srcports",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --7
    header = T"Dest Port(s)",
    name = "qosClassifyDstPorts",
    param = "dstports",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --8
    header = T"DSCP",
    name = "qosClassifyDSCP",
    param = "dscp",
    type = "text",
    readonly = true,
    attr = { input = { class="span1" } },
  },
  { --9
    header = "",
    legend = T"QoS Classification",
    name = "classify_entry",
    type = "aggregate",
    synthesis = nil, --tod_aggregate,
    subcolumns = {
      {
        header = T"Label Target",
        name = "qosClassifyLabelTarget",
        param = "target",
        type = "select",
        values = options_labeltarget,
        attr = { select = { class="span2"} },
      },
      {
        header = T"Source Host/IP",
        name = "qosClassifySrcHost",
        param = "srchost",
        type = "text",
        attr = { input = { class="span1"} },
      },
      {
        header = T"Dest Host/IP",
        name = "qosClassifyDstHost",
        param = "dsthost",
        type = "text",
        attr = { input = { class="span1"} },
      },
      {
        header = T"Protocol",
        name = "qosClassifyProto",
        param = "proto",
        default = "tcp",
        type = "select",
        values = options_proto,
        attr = { select = { class="span1" } },
      },
      {
        header = T"Port(s)",
        name = "qosClassifyPorts",
        param = "ports",
        type = "text",
        attr = { input = { class="span2"} },
      },
      {
        header = T"Source Port(s)",
        name = "qosClassifySrcPorts",
        param = "srcports",
        type = "text",
        attr = { input = { class="span2"} },
      },
      {
        header = T"Dest Port(s)",
        name = "qosClassifyDstPorts",
        param = "dstports",
        type = "text",
        attr = { input = { class="span2"} },
      },
      {
        header = T"DSCP",
        name = "qosClassifyDSCP",
        param = "dscp",
        default = "",
        type = "checkboxgroup",
        values = options_dscp,
        attr = { select = { class="span1" } },
      },
    },
  },
}

local classify_options = {
  tableid = "classify",
  basepath = uci_classify_path.."@.",
  canEdit = true,
  canApply = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add New Rule",
  sorted = "target",
	objectName = "classify"..uniqueKey,
	addNamedObject = true,
}

local function validateStringIsPorts(value)
  for port in string.gmatch(value, "([^%,]+)") do
    local ok, msg = vSIP(port)
    if not ok then
      return nil, "The ports should be between 1-65535 and seperated with ','."
    end
  end
  return true
end

local classify_valid = {
  qosClassifyLabelTarget = gVIES(options_labeltarget),
  qosClassifyProto = gVIES(options_proto),
  qosClassifyDSCP = gOV(gVIC(options_dscp)),
  qosClassifySrcHost = gOV(vSII),
  qosClassifyDstHost = gOV(vSII),
  qosClassifySrcPorts = gOV(validateStringIsPorts),
  qosClassifyDstPorts = gOV(validateStringIsPorts),
  qosClassifyPorts = gOV(validateStringIsPorts),
}

local function classify_filter(data)
  if data["srchost"] == "" and data["dsthost"] == "" and data["proto"] == "" and
    data["ports"] == "" and data["srcports"] == "" and data["dstports"] == "" and
    data["dscp"] == "" then
    return false
  end
  return true
end

local function dscpAsText(data)
  for _,v in ipairs(data) do
    if v[9][8] then
      v[8] = ""
      local dscp
      for _,dscp in pairs(v[9][8]) do
        if dscp then
          if v[8] ~= "" then
            v[8] = v[8]..","
          end
          for _,r in ipairs(options_dscp) do
            local meaning = r[2]
            if sub(meaning,1,1) == "<" then
              meaning = match(meaning, ".*>([^<]+)<.*")
            end
            if upper(dscp) == upper(r[1]) or dscp == meaning then
              v[8] = v[8]..meaning
              break
            end
          end
        end
      end
    end
  end
end

local classify_data, classify_helpMsg = post_helper.handleTableQuery(classify_columns, classify_options, classify_filter, nil, classify_valid)
dscpAsText(classify_data)

ngx.print( ui_helper.createHeader(T"QoS", false) ); ngx.print('\
<div class="modal-body update">\
  ');  lp.include("tabs-qos.lp")   ngx.print('\
  <form class="form-horizontal" method="post" action="modals/qos-classify-modal.lp">\
  ');  ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
	')
    local html = {}
    html[#html+1] = "<fieldset>"
    html[#html+1] =     "<legend>"..T"QoS Label Table".."</legend>"
    html[#html+1] =     ui_helper.createTable(label_columns, label_data, label_options, nil, label_helpMsg)
    html[#html+1] = "</fieldset>"
    html[#html+1] = "<fieldset>"
    html[#html+1] =	    "<legend>"..T"QoS Classification Table".."</legend>"
    html[#html+1] =     ui_helper.createTable(classify_columns, classify_data, classify_options, nil, classify_helpMsg)
    html[#html+1] = "</fieldset>"
    ngx.print(html)
    ngx.print('\
  </form>\
</div>\
')
ngx.print( ui_helper.createFooter() )
ngx.print('\
<script>\
$("#classify>tbody>tr>td:nth-child(7)").each(function() {\
  var content=$(this).text();\
  if(content.length>=20){\
    var split="";\
    var last=0;\
    for (var i=0; i<content.length; i++) {\
      var c=content.charAt(i);\
      split+=c;\
      if(i-last>20 && c==","){\
        split+="<br>";\
        last=i;\
      }\
    }\
    $(this).html(split);\
  }\
});\
</script>\
')