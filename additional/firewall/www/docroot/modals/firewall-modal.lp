--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
-- Process POST query

local ngx = ngx
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
local portslist = require("portslist_helper")
local hosts_ac, hosts_ac_v6 = require("web.uinetwork_helper").getAutocompleteHostsList()
local remove = table.remove
local str_format, str_match, str_untaint = string.format, string.match, string.untaint

local outgoingpolicy = {
  {"ACCEPT", T"ACCEPT"},
  {"DROP", T"DROP"},
  {"REJECT", T"REJECT"},
}

local incomingpolicy = {
  {"DROP", T"DROP"},
  {"REJECT", T"REJECT"},
}

local fw_levels = {
  {"lax", T"<strong>&nbsp;&nbsp;Low</strong>"},
  {"normal", T"<strong>&nbsp;&nbsp;Normal</strong>"},
  {"high", T"<strong>&nbsp;&nbsp;High</strong>"},
  {"user", T"<strong>&nbsp;&nbsp;User Defined</strong>"}
}

local fw_params = {
  fw_ping = "rpc.network.firewall.internetpingipv4",
  fw_ping_ipv6 = "rpc.network.firewall.internetpingipv6",
  fw_enable = "rpc.network.firewall.enable",
  fw_level = "rpc.network.firewall.mode",
  fw_outgoing_policy = "rpc.network.firewall.useroutgoingdefault",
  fw_incoming_policy = "rpc.network.firewall.userincomingdefault",
}

local rpc_fw_zone_path = "uci.firewall.zone."
local rpc_fw_zone_content = content_helper.getMatchedContent (rpc_fw_zone_path)
local dst_intfs = {
  {"", T"<i>Incoming Rule</i>"},
}
local src_intfs = {
  {"", T"<i>Outgoing Rule</i>"},
}
for _, v in ipairs (rpc_fw_zone_content) do
  dst_intfs[#dst_intfs+1] = { v.name, T(v.name) }
  src_intfs[#src_intfs+1] = { v.name, T(v.name) }
end

-- Retrieve GW IP + netmask for use by validation function
local ipdetails = {
    gw = "uci.network.interface.@lan.ipaddr",
    netmask = "uci.network.interface.@lan.netmask"
}
content_helper.getExactContent(ipdetails)

-- Shortcuts to validation helpers to make lines shorter
local vB = post_helper.validateBoolean
local vSII = post_helper.validateStringIsIP
local gVIES = post_helper.getValidateInEnumSelect
local vSIPR = post_helper.validateStringIsPortRange
local gVSIDIP = post_helper.getValidateStringIsDeviceIPv4
local vSIDIP = gVSIDIP(ipdetails.gw, ipdetails.netmask)
local vSIIv6 = post_helper.validateStringIsIPv6
local gAV = post_helper.getAndValidation
local vNES = post_helper.validateNonEmptyString
local gOV = post_helper.getOptionalValidation
local vIP4AS = post_helper.validateIPAndSubnet(4)
local vIP6AS = post_helper.validateIPAndSubnet(6)
local gDSM = post_helper.getDefaultSubnetMask
local netMaskToMask = post_helper.cidr2mask
local isNA = post_helper.isNetworkAddress

local fw_valid = {
  fw_ping = post_helper.validateBoolean,
  fw_ping_ipv6 = post_helper.validateBoolean,
  fw_enable = post_helper.validateBoolean,
  fw_level = gVIES(fw_levels),
  fw_outgoing_policy = gVIES(outgoingpolicy),
  fw_incoming_policy = gVIES(incomingpolicy),
}

local content = post_helper.handleQuery(fw_params, fw_valid)

-- Firewall rules
local function fwrule_sort(rule1, rule2)
  return tonumber(rule1.paramindex) < tonumber(rule2.paramindex)
end

local fwrules_targets = {
  { "ACCEPT", "ACCEPT"},
  { "DROP", "DROP"},
  { "REJECT", "REJECT"},
}

local fwrules_protocols = {
    { "tcp", T"TCP"},
    { "udp", T"UDP"},
    { "tcpudp", T"TCP/UDP"},
    { "icmp", T"ICMP"},
    { "esp", T"ESP"},
    { "ah", T"AH"},
    { "sctp", T"SCTP"},
    { "all", T"all"},
}

local fwrule_columns = {
  {
    header = "",
    name = "enabled",
    param = "enabled",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }}
  },
  {
    header = T"Name",
    name = "name",
    param = "name",
    type = "text",
    attr = { input = { class="span1", style="width:75px" } },
  },
  {
    header = T"Action",
    name = "target",
    param = "target",
    default = "DROP",
    type = "select",
    values = fwrules_targets,
    attr = { select = { class="span1", style="width:95px" } },
  },
  {
    header = T"Protocol",
    name = "protocol",
    param = "proto",
    default = "tcp",
    type = "select",
    values = fwrules_protocols,
    attr = { select = { class="span1", style="width:80px" } },
  },
  {
    header = T"Src Zone",
    name = "src",
    param = "src",
    default = "wan",
    type = "select",
    values = src_intfs,
    attr = { select = { class="span1", style="width:100px" } },
  },
  {
    header = T"Src IP/Subnet",
    name = "src_ip",
    param = "src_ip",
    type = "text",
    attr = { input = { class="span2", maxlength="18", style="width:130px" } },
  },
  {
    header = T"Src port",
    name = "src_port",
    param = "src_port",
    type = "text",
    attr = { input = { class="span1", maxlength="11", style="width:50px" }, autocomplete = portslist },
  },
  {
    header = T"Dest Zone",
    name = "dest",
    param = "dest",
    default = "",
    type = "select",
    values = dst_intfs,
    attr = { select = { class="span1", style="width: 100px" } },
  },
  {
    header = T"Dst IP/Subnet",
    name = "dest_ip",
    param = "dest_ip",
    type = "text",
    attr = { input = { class="span2", maxlength="18", style="width:130px" }, autocomplete = hosts_ac },
  },
  {
    header = T"Dst port",
    name = "dest_port",
    param = "dest_port",
    type = "text",
    attr = { input = { class="span1", maxlength="11", style="width:50px" }, autocomplete = portslist },
  },
}

local fwrule_options = {
    tableid = "fwrules",
    basepath = "rpc.network.firewall.userrule.@.",
    createMsg = T"Add new IPv4 firewall rule",
    sorted = fwrule_sort
}

local duplicatedErrMsg = nil
local session = ngx.ctx.session
--[[
   The following function used to validate the duplicate entries while adding or editing on firewall table.
   We will throw the duplicate error if any rows containing all 6 values below are duplicated
   Sample:- Protocol    Src IP       Src port      Dst IP         Dst port    DSCP
            TCP         192.168.1.1  1000          192.168.1.4    2000        AF23
            TCP         192.168.1.1  1000          192.168.1.5    2000        CS7
            UDP         192.168.1.1  1000          192.168.1.5    2001        AF23
            TCP         192.168.1.1  1000          192.168.1.5    2000        EF
   In above example the 4th and 2nd rows are exactly duplicated. In this case we throw the error message as duplicated.
   If anyone of the value of row is different then we dont consider as duplicated row.
]]
local function rulesDuplicateCheck(basepath,tableid,columns)
  return function(value, postdata, key)
  local sucess,msg
    if value and value ~= "" then
      sucess,msg = vSIPR(value, postdata, key)
    else
      sucess = true
    end
    if sucess then
      -- specify column range to check for duplicates
      local startIndex, endIndex= 4,10
      local fullpath = nil
      if postdata.action =="TABLE-MODIFY" then
        local index = tonumber(postdata.index)
        local tablesessionindexes = tableid..".allowedindexes"
        local allowedIndexes = session:retrieve(tablesessionindexes) or {}
        if allowedIndexes[index] then
          index = allowedIndexes[index].paramindex
        end
        -- fullpath => The UCI path which is going to be modifed, Ex: rpc.network.firewall.userrule.@4.
        fullpath = basepath .. "@"..index.."."
      end
      local paths=nil
      for i=startIndex, endIndex do
        local value = string.untaint(postdata[columns[i].name])
        local cmatch = content_helper.getMatchedContent(basepath, {[columns[i].param] = value })
        if fullpath then
          for u,v in ipairs(cmatch) do
            if v.path == fullpath then
            --The rpc.network.firewall.userrule.@4. will be removed
            --because we no need to validate with the path which we need to modify
              remove(cmatch, u)
              break
            end
          end
        end
        -- If cmatch is empty then there will be no duplicated rows in UCI.
        if #cmatch > 0 then
        -- The below condition will be true at first iteration.
          if i == startIndex then
            -- At the first iteration the duplicate paths will be stored in a temp table
            -- which can be used to validate with duplicate path of subsequest columns
            paths={}
            for _,v in ipairs(cmatch) do
              paths[v.path]=true
            end
          -- If path is empty then no duplicates in previous columns. So we can break the loop and can say no duplicates
          elseif paths then
            local duplicate = {}
            local flag = false
            for _,v in ipairs(cmatch) do
              if paths[v.path] then
                duplicate[v.path] = true
                flag = true
              end
            end
            -- if current duplicated path is not matching with previouse duplicatd paths.
            -- Then there is no exact duplicated rows.
            if flag then
              paths = duplicate
            else
               paths = nil
               break
            end
          else
            paths = nil
            break
          end
        else
          paths=nil
          break
        end
      end
      --Finally if you get one or more paths which contain all
      --the 4 values are duplicated (sr ip,port and dest ip, port) are duplicated
      if paths then
          sucess = nil
          msg = T"duplicate value"
          duplicatedErrMsg = msg
      end
    end
    return sucess, msg
  end
end

local function validateLanIP(value, object, key)
  local retVal, msg
  local ipAddress, netMask = str_match(value,"^([^/]+)/?(%d*)$")
  retVal, msg = gOV(vIP4AS(value, object, key))
  if retVal and netMask == "" and ((key == "src_ip" and object.src == "lan") or (key == "dest_ip" and object.dest == "lan")) then
    --To add default subnet mask to the IPv4 Network Address if not explicitly mentioned.
    netMask = gDSM(ipAddress)
    if netMask then
      local isNetworkAddress = isNA(ipAddress, netMaskToMask(netMask))
      if isNetworkAddress then
        object[key] = value .. "/" .. netMask
      end
    end
    return true
  end
  return retVal, msg
end

local fwrule_valid = {
    enabled = vB,
    target = gVIES(fwrules_targets),
    protocol = gVIES(fwrules_protocols),
    src = gVIES(src_intfs),
    src_ip = validateLanIP,
    src_port = gOV(vSIPR),
    dest = gVIES(dst_intfs),
    dest_ip = validateLanIP,
    dest_port = rulesDuplicateCheck("rpc.network.firewall.userrule.", "fwrules",fwrule_columns),
}

local fwrule_defaultObject = {
    family = "ipv4",
}

local fwrule_data, fwrule_helpmsg = post_helper.handleTableQuery(fwrule_columns, fwrule_options, nil, fwrule_defaultObject, fwrule_valid)

if duplicatedErrMsg then
  fwrule_helpmsg.protocol = duplicatedErrMsg
  fwrule_helpmsg.src_ip = duplicatedErrMsg
  fwrule_helpmsg.src_port = duplicatedErrMsg
  fwrule_helpmsg.dest_ip = duplicatedErrMsg
  fwrule_helpmsg.dest_port = duplicatedErrMsg
  fwrule_helpmsg.dscp = duplicatedErrMsg
  duplicatedErrMsg = nil
end

local fwrules_v6_protocols = {
    { "tcp", T"TCP"},
    { "udp", T"UDP"},
    { "tcpudp", T"TCP/UDP"},
    { "icmpv6", T"ICMPv6"},
    { "esp", T"ESP"},
    { "ah", T"AH"},
    { "sctp", T"SCTP"},
    { "all", T"all"},
}

local fwrule_v6_columns = {
  {
    header = "",
    name = "enabled_v6",
    param = "enabled",
    type = "switch",
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }}
  },
  {
    header = T"Name",
    name = "name_v6",
    param = "name",
    type = "text",
    attr = { input = { class="span1", style="width:75px" } },
  },
  {
    header = T"Action",
    name = "target_v6",
    param = "target",
    default = "DROP",
    type = "select",
    values = fwrules_targets,
    attr = { select = { class="span1", style="width:95px" } },
  },
  {
    header = T"Protocol",
    name = "protocol_v6",
    param = "proto",
    default = "tcp",
    type = "select",
    values = fwrules_v6_protocols,
    attr = { select = { class="span1", style="width:80px" } },
  },
  {
    header = T"Src Zone",
    name = "src_v6",
    param = "src",
    default = "wan",
    type = "select",
    values = src_intfs,
    attr = { select = { class="span1", style="width:100px" } },
  },
  {
    header = T"Src IP/Subnet",
    name = "src_ip_v6",
    param = "src_ip",
    type = "text",
    attr = { input = { class="span2", maxlength="39", style="width:130px" } },
  },
  {
    header = T"Src port",
    name = "src_port_v6",
    param = "src_port",
    type = "text",
    attr = { input = { class="span1", maxlength="11", style="width:50px" }, autocomplete = portslist },
  },
  {
    header = T"Dest Zone",
    name = "dest_v6",
    param = "dest",
    default = "",
    type = "select",
    values = dst_intfs,
    attr = { select = { class="span1", style="width: 100px" } },
  },
  {
    header = T"Dst IP/Subnet",
    name = "dest_ip_v6",
    param = "dest_ip",
    type = "text",
    attr = { input = { class="span2", maxlength="39", style="width:130px" }, autocomplete = hosts_ac_v6 },
  },
  {
    header = T"Dst port",
    name = "dest_port_v6",
    param = "dest_port",
    type = "text",
    attr = { input = { class="span1", maxlength="11", style="width:50px" }, autocomplete = portslist },
  },
}

local fwrule_v6_options = {
    tableid = "fwrules_v6",
    basepath = "rpc.network.firewall.userrule_v6.@.",
    createMsg = T"Add new IPv6 firewall rule",
    sorted = fwrule_sort
}

local fwrule_v6_valid = {
    enabled_v6 = vB,
    target_v6 = gVIES(fwrules_targets),
    protocol_v6 = gVIES(fwrules_v6_protocols),
    src_v6 = gVIES(src_intfs),
    src_ip_v6 = gOV(vIP6AS),
    src_port_v6 = gOV(vSIPR),
    dest_v6 = gVIES(dst_intfs),
    dest_ip_v6 = gOV(vIP6AS),
    dest_port_v6 = rulesDuplicateCheck("rpc.network.firewall.userrule_v6.", "fwrules_v6",fwrule_v6_columns),
}

local fwrule_v6_defaultObject = {
  family = "ipv6",
}

local fwrule_v6_data, fwrule_v6_helpmsg = post_helper.handleTableQuery(fwrule_v6_columns, fwrule_v6_options, nil, fwrule_v6_defaultObject, fwrule_v6_valid)

if duplicatedErrMsg then
  fwrule_v6_helpmsg.protocol_v6 = duplicatedErrMsg
  fwrule_v6_helpmsg.src_ip_v6 = duplicatedErrMsg
  fwrule_v6_helpmsg.src_port_v6 = duplicatedErrMsg
  fwrule_v6_helpmsg.dest_ip_v6 = duplicatedErrMsg
  fwrule_v6_helpmsg.dest_port_v6 = duplicatedErrMsg
  fwrule_v6_helpmsg.dscp_v6 = duplicatedErrMsg
end
  ngx.print('\
\
');  ngx.print(ui_helper.createHeader(T"Firewall", true, false))   ngx.print('\
\
<div class="modal-body update">\
<form class="form-horizontal" method="post" action="modals/firewall-modal.lp">\
');
ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.print('\
<fieldset>\
<legend>');  ngx.print( T"Firewall Status" ); ngx.print('</legend>');
ngx.print(ui_helper.createSwitch(T"Enabled", "fw_enable", content["fw_enable"])); ngx.print('\
');
local enable_attr = { alert = { class = "alert-info" }, }
ngx.print(ui_helper.createAlertBlock(T("WARNING: Setting Enabled to OFF causes the firewall to ACCEPT all incoming connections from the WAN!<br>NOTE: You can only disable the firewall when the current level is <strong>Normal</strong>."), enable_attr)); ngx.print('\
</fieldset>\
<fieldset>\
<legend>');  ngx.print( T"Firewall Level" ); ngx.print('</legend>\
\
');
    local html = {}

    local fwlevel_attr = {
        group = {
            class = ""
        },
        select = {
            class = "monitor-changes"
        }
    }
    html[#html+1] = ui_helper.createInputSelect(T"Level", "fw_level", fw_levels, content["fw_level"], fwlevel_attr)

    local lax_attr = {
        alert = {
            class = "alert-info monitor-fw_level monitor-lax"
        },
    }
    html[#html+1] = ui_helper.createAlertBlock(T("In <strong>Low mode</strong>, the firewall will allow all outgoing connections. Unless the firewall is disabled, it will reject unknown incoming connections but acknowledge the presence of the firewall."), lax_attr)

    local normal_attr = {
        alert = {
            class = "alert-info monitor-fw_level monitor-normal"
        },
    }
    html[#html+1] = ui_helper.createAlertBlock(T"In <strong>Normal mode</strong>, the firewall will allow all outgoing connections. Unless the firewall is disabled, it will silently drop unknown incoming connections.", normal_attr)

    local high_attr = {
        alert = {
            class = "alert-info monitor-fw_level monitor-high"
        },
    }
    html[#html+1] = ui_helper.createAlertBlock(T"In <strong>High mode</strong>, the firewall will allow outgoing connections to the following services: HTTP, HTTPS, SMTP, POP3, IMAP, SSH. Unless the firewall is disabled, it will silently drop unknown incoming connections. This may impact on web services like Internet Speed Tests.", high_attr)

    local user_attr = {
        alert = {
            class = "alert-info monitor-fw_level monitor-user"
        },
    }
    html[#html+1] = ui_helper.createAlertBlock(T"\
    In <strong>User Defined mode</strong>, you can configure each individual rule of the firewall as well as the default behavior.<br>\
    <ul>\
    <li>If Src and Dest Zones are specified, the rule matches <i>forwarded</i> traffic</li>\
    <li>If only Src Zone is specified, the rule matches <i>incoming</i> traffic</li>\
    <li>If only Dest Zone is specified, the rule matches <i>outgoing</i> traffic</li>\
    <li>If neither Src Zone nor Dest Zone are specified, the rule defaults to an <i>outgoing</i> traffic rule</li>\
    </ul>\
    <b>NOTE:</b> You must save the Level change before you can add rules.", user_attr)
    ngx.print(html)
  ngx.print('\
</fieldset>\
\
<fieldset>\
<legend>');  ngx.print( T"Firewall Default Behavior" ); ngx.print('</legend>\
\
');
    local html = {}
    html[#html+1] = "<label class=\"control-label\">Internet ping response:</label><div class=\"controls\">"
    html[#html+1] = ui_helper.createSwitch(T"Answer IPv4 ping?", "fw_ping", content["fw_ping"])
    html[#html+1] = ui_helper.createSwitch(T"Answer IPv6 ping?", "fw_ping_ipv6", content["fw_ping_ipv6"])
    html[#html+1] = "</div>"

    local outgoing_attr = {
        group = {
            class = "monitor-fw_level monitor-user"
        }
    }

    local info_attr = {
        alert = {
            class = "alert-info monitor-fw_level monitor-user"
        },
    }

    html[#html+1] = ui_helper.createInputSelect(T"Outgoing default policy", "fw_outgoing_policy", outgoingpolicy, content["fw_outgoing_policy"], outgoing_attr)
    html[#html+1] = ui_helper.createAlertBlock(T"The <strong>outgoing policy</strong> defines what is done with packets coming from the LAN devices toward the internet. Setting it to REJECT or DROP will forbid any internet traffic from the LAN unless explicitely allowed by a firewall rule.", info_attr)

    html[#html+1] = ui_helper.createInputSelect(T"Incoming default policy", "fw_incoming_policy", incomingpolicy, content["fw_incoming_policy"], outgoing_attr)
    html[#html+1] = ui_helper.createAlertBlock(T("The <strong>incoming policy</strong> defines what is done with packets destined to the firewall. They can be either REJECTED (the firewall will notify the sender they were rejected) or DROPPED (the firewall will silently discard those packets)."), info_attr)

    ngx.print(html)
  ngx.print('\
</fieldset>\
<div class="monitor-fw_level monitor-user">\
<legend>');  ngx.print( T"IPv4 Firewall Rules" ); ngx.print('</legend>\
');
    ngx.print(ui_helper.createTable(fwrule_columns, fwrule_data, fwrule_options, nil, fwrule_helpmsg))
  ngx.print('\
<legend>');  ngx.print( T"IPv6 Firewall Rules" ); ngx.print('</legend>\
');
    ngx.print(ui_helper.createTable(fwrule_v6_columns, fwrule_v6_data, fwrule_v6_options, nil, fwrule_v6_helpmsg))
  ngx.print('\
</div>\
</form>\
</div>\
');  ngx.print(ui_helper.createFooter())   ngx.print('\
<script>\
(function() {\
    //by disabling the add new rule button, we prevent that the user start adding a rule, before the adapted firewall level user is saved.\
    //as this is a special case, we implement this here and not in actions.js\
    $(document).on("change", \'.modal select:not(.no-save):not(.disabled)\', function() {\
        $(".btn-table-new").addClass("disabled");\
    });\
    $(document).on("change focusin", \'.modal .line-edit\', function() {\
        var proto = $("#protocol option:selected").val();\
        if (typeof proto != \'undefined\') {\
            if (proto == "tcp" || proto == "udp" || proto == "tcpudp") {\
                $("#src_port").removeAttr("readonly");\
                $("#dest_port").removeAttr("readonly");\
            } else {\
                $("#src_port").attr("readonly", true);\
                $("#src_port").val("");\
                $("#dest_port").attr("readonly", true);\
                $("#dest_port").val("");\
            }\
        }\
        var proto_v6 = $("#protocol_v6 option:selected").val();\
        if (typeof proto_v6 != \'undefined\') {\
            if (proto_v6 == "tcp" || proto_v6 == "udp" || proto_v6 == "tcpudp") {\
                $("#src_port_v6").removeAttr("readonly");\
                $("#dest_port_v6").removeAttr("readonly");\
            } else {\
                $("#src_port_v6").attr("readonly", true);\
                $("#src_port_v6").val("");\
                $("#dest_port_v6").attr("readonly", true);\
                $("#dest_port_v6").val("");\
            }\
        }\
    });\
}());\
</script>\
');
