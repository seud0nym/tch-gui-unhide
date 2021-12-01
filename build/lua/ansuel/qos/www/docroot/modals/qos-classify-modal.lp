--pretranslated: do not change this file
----------------------------------------------------------------------------------
-- Enable localization
----------------------------------------------------------------------------------
gettext.textdomain('webui-qos')

----------------------------------------------------------------------------------
-- Dependencies
----------------------------------------------------------------------------------
local match,format,sub,upper = string.match,string.format,string.sub,string.upper

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

----------------------------------------------------------------------------------
-- UCI Paths (Do not add '@',arrays taken care of later)
----------------------------------------------------------------------------------
local uci_qos_prefix = "uci.qos."
local uci_label_path = uci_qos_prefix.."label."
local uci_classify_path = uci_qos_prefix.."classify."

----------------------------------------------------------------------------------
-- UCI Parameters
----------------------------------------------------------------------------------
local uci_param_srcif = "srcif"
local uci_param_dstif = "dstif"
local uci_param_srchost = "srchost"
local uci_param_dsthost = "dsthost"
local uci_param_protocol = "proto"
local uci_param_ports = "ports"
local uci_param_srcports = "srcports"
local uci_param_dstports = "dstports"
local uci_param_dscp = "dscp"

----------------------------------------------------------------------------------
-- Table Parameters (POST messages)
----------------------------------------------------------------------------------
local labelTargetName = "qosClassifyLabelTarget"
local srcInterfaceName = "qosClassifySrcIF"
local dstInterfaceName = "qosClassifyDstIF"
local srcHostName = "qosClassifySrcHost"
local dstHostName = "qosClassifyDstHost"
local protocolName = "qosClassifyProto"
local portsName = "qosClassifyPorts"
local srcPortsName = "qosClassifySrcPorts"
local dstPortsName = "qosClassifyDstPorts"
local dscpClassifyName = "qosClassifyDSCP"

---------------------------------------------------------------------------------
-- Shortcuts to validation helpers to make lines shorter
local gVIC = post_helper.getValidateInCheckboxgroup
local gVIES = post_helper.getValidateInEnumSelect
local vSII = post_helper.validateStringIsIP
local vSIP = post_helper.validateStringIsPort
local gOV = post_helper.getOrValidation

---------------------------------------------------------------------------------
-- handle qos labels
---------------------------------------------------------------------------------
local options_trafficid = {}
for i =1,15,1 do
  local id = tostring(i)
  options_trafficid[#options_trafficid + 1] = { id,id }
end

----------------------------------------------------------------------------------
-- Function:  Setup mapped label keys to be handled whenever the label is
--            manipulated.
----------------------------------------------------------------------------------
local options_labeltarget = {}
for _,v in ipairs(proxy.getPN(uci_label_path,true)) do
  local label = match(v.path,"uci%.qos%.label%.@([^%.]+)%.")
  if label then
    options_labeltarget[#options_labeltarget+1] = { label,label }
  end
end

local options_proto = {
  {"all","all"},
  {"tcp","tcp"},
  {"udp","udp"},
  {"udplite","udplite"},
  {"icmp","icmp"},
  {"esp","esp"},
  {"ah","ah"},
  {"sctp","sctp"},
  {"igmp","igmp"},
  {"gre","gre"},
}

local options_dscp = {
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
for i=1,22,1 do 
  local meaning = "!"..options_dscp[i][2]
  if sub(meaning,2,1) == "<" then
    local html1,text,html2 = match(meaning,"(.*>)([^<]+)(<.*)")
    meaning = html1.."!"..text..html2
  end
  options_dscp[#options_dscp+1] = { "!"..options_dscp[i][1],meaning }
end

local options_if = {}
for _,v in ipairs(proxy.getPN("rpc.network.interface.",true)) do
  local ifname = match(v.path,"rpc%.network%.interface%.@([^%.]+)%.")
  if ifname then
    options_if[#options_if+1] = { ifname,ifname }
  end
end

-- NOT 17.2 START
local rules = proxy.getPN("uci.qos.classify.",true)
local next_rule_num = 0
for _,v in ipairs(rules) do
  local number = tonumber(match(v.path,"@classify_(%d+)%."))
  if number then
    next_rule_num = number
  end
end
next_rule_num = next_rule_num + 1
-- NOT 17.2 END

local classify_columns = {
  { --[1]
    header = T"Label Target",
    name = labelTargetName,
    param = "target",
    type = "text",
    readonly = true,
  },
  { --[2]
    header = T"Source Interface",
    name = srcInterfaceName,
    param = uci_param_srcif,
    type = "text",
    readonly = true,
  },
  { --[3]
    header = T"Dest Interface",
    name = dstInterfaceName,
    param = uci_param_dstif,
    type = "text",
    readonly = true,
  },
  { --[4]
    header = T"Source Host/IP",
    name = srcHostName,
    param = uci_param_srchost,
    type = "text",
    readonly = true,
  },
  { --[5]
    header = T"Dest Host/IP",
    name = dstHostName,
    param = uci_param_dsthost,
    type = "text",
    readonly = true,
  },
  { --[6]
    header = T"Protocol",
    name = protocolName,
    param = uci_param_protocol,
    default = "tcp",
    type = "text",
    readonly = true,
  },
  { --[7]
    header = T"Port(s)",
    name = portsName,
    param = uci_param_ports,
    type = "text",
    readonly = true,
  },
  { --[8]
    header = T"Source Port(s)",
    name = srcPortsName,
    param = uci_param_srcports,
    type = "text",
    readonly = true,
  },
  { --[9]
    header = T"Dest Port(s)",
    name = dstPortsName,
    param = uci_param_dstports,
    type = "text",
    readonly = true,
  },
  { --[10]
    header = T"DSCP",
    name = dscpClassifyName,
    param = uci_param_dscp,
    type = "text",
    readonly = true,
  },
  { --11
    header = "",
    legend = T"QoS Classify Rule",
    name = "classify_entry",
    type = "aggregate",
    synthesis = nil,--tod_aggregate,
    subcolumns = {
      { --[1]
        header = T"Label Target",
        name = labelTargetName,
        param = "target",
        type = "select",
        values = options_labeltarget,
        attr = { select = { class ="span2"} },
      },
      { --[2]
        header = T"Source Interface",
        name = srcInterfaceName,
        param = uci_param_srcif,
        type = "checkboxgroup",
        values = options_if,
      },
      { --[3]
        header = T"Dest Interface",
        name = dstInterfaceName,
        param = uci_param_dstif,
        type = "checkboxgroup",
        values = options_if,
      },
      { --[4]
        header = T"Source Host/IP",
        name = srcHostName,
        param = uci_param_srchost,
        type = "text",
        attr = { input = { class ="span1"} },
      },
      { --[5]
        header = T"Dest Host/IP",
        name = dstHostName,
        param = uci_param_dsthost,
        type = "text",
        attr = { input = { class ="span1"} },
      },
      { --[6]
        header = T"Protocol",
        name = protocolName,
        param = uci_param_protocol,
        default = "tcp",
        type = "select",
        values = options_proto,
        attr = { select = { class ="span1" } },
      },
      { --[7]
        header = T"Port(s)",
        name = portsName,
        param = uci_param_ports,
        type = "text",
        attr = { input = { class ="span2"} },
      },
      { --[8]
        header = T"Source Port(s)",
        name = srcPortsName,
        param = uci_param_srcports,
        type = "text",
        attr = { input = { class ="span2"} },
      },
      { --[9]
        header = T"Dest Port(s)",
        name = dstPortsName,
        param = uci_param_dstports,
        type = "text",
        attr = { input = { class ="span2"} },
      },
      { --[10]
        header = T"DSCP",
        name = dscpClassifyName,
        param = uci_param_dscp,
        type = "checkboxgroup",
        values = options_dscp,
      },
    },
  },
}

----------------------------------------------------------------------------------
-- Classify Table Configuration
----------------------------------------------------------------------------------
local classify_options = {
  tableid = "classify",
  basepath = uci_classify_path.."@.",
  canEdit = true,
  canApply = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add Rule",
  addNamedObject = true,-- NOT 17.2
  objectName = format("classify_%d",next_rule_num),-- NOT 17.2
}

----------------------------------------------------------------------------------
-- Classify Table Configuration
----------------------------------------------------------------------------------
local function validateStringIsPorts(value)
  for port in string.gmatch(value,"([^%,]+)") do
    local ok = vSIP(port)
    if not ok then
      return nil,"The ports should be between 1-65535 and seperated with ','."
    end
  end
  return true
end

local function validateEmptyString(value)
  if value == "" then
    return true
  end
  return nil,"Empty string is allowed."
end

local classify_valid = {
  qosClassifyLabelTarget = gVIES(options_labeltarget),
  qosClassifyProto = gVIES(options_proto),
  qosClassifyDSCP = gVIC(options_dscp),
  qosClassifySrcIF = gVIC(options_if),
  qosClassifyDstIF = gVIC(options_if),
  qosClassifySrcHost = gOV(vSII,validateEmptyString),
  qosClassifyDstHost = gOV(vSII,validateEmptyString),
  qosClassifySrcPorts = gOV(validateStringIsPorts,validateEmptyString),
  qosClassifyDstPorts = gOV(validateStringIsPorts,validateEmptyString),
  qosClassifyPorts = gOV(validateStringIsPorts,validateEmptyString),
}

----------------------------------------------------------------------------------
-- Populate Table
----------------------------------------------------------------------------------
local classify_data,classify_helpMsg = post_helper.handleTableQuery(classify_columns,classify_options,nil,nil,classify_valid)
for _,v in ipairs(classify_data) do
  if v[11][2] and type(v[11][2]) == "table" then
    v[2] = table.concat(v[11][2],",")
  end
  if v[11][3] and type(v[11][3]) == "table" then
    v[3] = table.concat(v[11][3],",")
  end
  if v[11][10] then
    v[10] = ""
    for _,dscp in pairs(v[11][10]) do
      if dscp then
        if v[10] ~= "" then
          v[10] = v[10]..","
        end
        for _,r in ipairs(options_dscp) do
          local meaning = r[2]
          if sub(meaning,1,1) == "<" then
            meaning = match(meaning,".*>([^<]+)<.*")
          end
          if upper(dscp) == upper(r[1]) or dscp == meaning then
            v[10] = v[10]..meaning
            break
          end
        end
      end
    end
  end
end

ngx.print( ui_helper.createHeader(T"QoS",false) ); ngx.print('\
<div class ="modal-body update">\
  ');  lp.include("tabs-qos.lp")   ngx.print('\
  <form class ="form-horizontal" method ="post" action ="modals/qos-classify-modal.lp">\
  ');  ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
	')
  local alert_attr = {
    alert = {
      class = "alert-info"
    },
  }
  local html = {}
    html[#html+1] = "<fieldset>"
    html[#html+1] =	  "<legend>"..T"QoS Classify Rules".."</legend>"
    html[#html+1] =   ui_helper.createAlertBlock(T("Rules are applied in the order they appear in /etc/config/qos,which is NOT necessarily the order listed here!"),alert_attr)
    html[#html+1] =   ui_helper.createTable(classify_columns,classify_data,classify_options,nil,classify_helpMsg)
    html[#html+1] = "</fieldset>"
    ngx.print(html)
    ngx.print('\
  </form>\
</div>\
')
ngx.print( ui_helper.createFooter() )
ngx.print('\
<script>\
var maxlen=10;\
$("tr:not(.additional-edit) td:not(.btn-col-OK)").each(function() {\
  var content=$(this).text();\
  if(content.length>=maxlen){\
    var split="";\
    var last=0;\
    for (var i=0; i<content.length; i++) {\
      var c=content.charAt(i);\
      split+=c;\
      if(i-last>=maxlen && c==","){\
        split+="<br>";\
        last=i;\
      }\
    }\
    $(this).html(split);\
  }\
});\
</script>\
')