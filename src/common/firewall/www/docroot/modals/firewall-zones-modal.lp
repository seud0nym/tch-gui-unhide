--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local lp = require("web.lp")
local fw_rules_helper = require("firewall-rules_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local zones_helper = require("firewall-zones_helper")

local concat = table.concat
local match,tolower = string.match,string.lower
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local vB = post_helper.validateBoolean
local gVIC = post_helper.getValidateInCheckboxgroup
local gVIES = post_helper.getValidateInEnumSelect
local gOV = post_helper.getOptionalValidation

local network_rpc_content = content_helper.getMatchedContent("rpc.network.interface.")
local intfs = {}
for _,v in ipairs (network_rpc_content) do
  local path = match(v.path,"rpc%.network%.interface%.@([^%.]+)%.")
  if path and not (path == "ppp" or path == "ipoe" or path == "fonopen" or path == "loopback") then
    intfs[#intfs+1] = { path,T(path) }
  end
end

local post_tableid,post_action
if ngx.var.request_method == "POST" then
  local post_data = ngx.req.get_post_args(0)
  post_tableid = post_data.tableid
  post_action = post_data.action
end

local zone_columns = {
  { -- [1]
    header = "",
    name = "enabled",
    param = "enabled",
    type = "switch",
    default = "1",
    readonly = true,
  },
  { -- [2]
    header = T"Zone<br>Name",
    name = "name",
    param = "name",
    type = "text",
    readonly = true,
    attr = { input = { class="span2"} },
  },
  { -- [3]
    header = T"Incoming Traffic<br>Default Policy",
    name = "input",
    param = "input",
    type = "select",
    values = fw_rules_helper.fwrules_targets,
    readonly = true,
  },
  { -- [4]
    header = T"Forwarded Traffic<br>Default Policy",
    name = "forward",
    param = "forward",
    type = "select",
    values = fw_rules_helper.fwrules_targets,
    readonly = true,
},
  { -- [5]
    header = T"Outgoing Traffic<br>Default Policy",
    name = "output",
    param = "output",
    type = "select",
    values = fw_rules_helper.fwrules_targets,
    readonly = true,
  },
  { -- [6]
    header = T"Network<br>Interfaces",
    name = "network",
    param = "network",
    type = "checkboxgroup",
    values = intfs,
    readonly = true,
  },
  { -- [7]
    header = "Masqueraded<br>(NAT)",
    name = "masq",
    param = "masq",
    type = "switch",
    readonly = true,
  },
  { -- [8]
    header = "TCP MSS<br>Clamping",
    name = "mtu_fix",
    param = "mtu_fix",
    type = "switch",
    readonly = true,
  },
  { -- [9]
    header = "",
    legend = T"Firewall Zone",
    name = "zone_entry",
    type = "aggregate",
    synthesis = nil,
    subcolumns = {
      { -- [1]
        header = "Enabled",
        name = "enabled",
        param = "enabled",
        type = "switch",
        default = "1",
      },
      { -- [2]
        header = T"Zone Name",
        name = "name",
        param = "name",
        type = "text",
        readonly = true,
        attr = { input = { class="span2"} },
        unique = true,
      },
      { -- [3]
        header = T"Incoming Default Policy",
        name = "input",
        param = "input",
        type = "select",
        values = fw_rules_helper.fwrules_targets,
        attr = { select = { class="span2" } },
        default = "DROP",
      },
      { -- [4]
        header = T"Forward Default Policy",
        name = "forward",
        param = "forward",
        type = "select",
        values = fw_rules_helper.fwrules_targets,
        attr = { select = { class="span2" } },
        default = "REJECT",
    },
      { -- [5]
        header = T"Outgoing Default Policy",
        name = "output",
        param = "output",
        type = "select",
        values = fw_rules_helper.fwrules_targets,
        attr = { select = { class="span2" } },
        default = "ACCEPT",
      },
      { -- [6]
        header = T"Network Interfaces",
        name = "network",
        param = "network",
        type = "checkboxgroup",
        values = intfs,
      },
      { -- [7]
        header = "Masqueraded",
        name = "masq",
        param = "masq",
        type = "switch",
        default = "0",
      },
      { -- [8]
        header = "TCP MSS Clamping",
        name = "mtu_fix",
        param = "mtu_fix",
        type = "switch",
        default = "1",
      },
    },
  },
}
if post_tableid == "zone" then
  if post_action == "TABLE-ADD" then
    zone_columns[9].subcolumns[2].readonly = false
  end
end

local zone_valid = {
  enabled = vB,
  name = zones_helper.validateName,
  input = gVIES(fw_rules_helper.fwrules_targets),
  forward = gVIES(fw_rules_helper.fwrules_targets),
  output = gVIES(fw_rules_helper.fwrules_targets),
  network = gVIC(intfs),
  masq = vB,
  mtus_fix = vB,
}

local zone_options = {
  tableid = "zone",
  basepath = "uci.firewall.zone.@.",
  canEdit = true,
  canAdd = true,
  canDelete = true,
  createMsg = T"Add Zone",
  sorted = function(zone1,zone2)
    return tolower(zone1.name) < tolower(zone2.name)
  end,
  onAdd = zones_helper.onZoneAdded,
  onDelete = zones_helper.onZoneDeleted,
}

local function zone_filter(data)
  if data.name == "wan" then
    return { canEdit = false,canDelete = false }
  end
  return true
end

local zone_data,zone_helpMsg = post_helper.handleTableQuery(zone_columns,zone_options,zone_filter,nil,zone_valid)
zone_columns[6].type = "text"
for _,v in ipairs(zone_data) do
  if v[1] == "" then
    v[1] = "1"
  end
  if v[9][1] == "" then
    v[9][1] = "1"
  end
  if type(v[6]) == "table" then
    for i=1,#v[6],1 do
      if type(v[6][i]) == "userdata" then
        v[6][i] = untaint(v[6][i])
      end
    end
    v[6] = concat(v[6],",")
  end
end
local zones = fw_rules_helper.getZones()

local fw_forwarding_columns = {
  { -- [1]
    header = "",
    name = "enabled",
    param = "enabled",
    type = "switch",
    default = "1",
  },
  {
    header = T"Name",
    name = "name",
    param = "name",
    type = "text",
  },
  { -- [3]
    header = T"Family",
    name = "family",
    param = "family",
    type = "select",
    values = fw_rules_helper.fwrules_family,
  },
  {
    header = T"Source Zone",
    name = "src",
    param = "src",
    type = "select",
    values = zones,
  },
  {
    header = T"Destination Zone",
    name = "dest",
    param = "dest",
    type = "select",
    values = zones,
  },
}
if post_tableid == "fw_forwardings" then
  if post_action == "TABLE-ADD" then
    zone_columns[2].readonly = false
  end
end

local fw_forwarding_valid = {
  enabled = vB,
  name = gOV(zones_helper.validateName),
  family = gVIES(fw_rules_helper.fwrules_family),
  src = gVIES(zones),
  dest = gVIES(zones),
}

local fw_forwarding_options = {
  tableid = "fw_forwardings",
  basepath = "uci.firewall.forwarding.@.",
  canAdd = true,
  canEdit = false,
  canDelete = true,
  createMsg = T"Add Forwarding Rule",
}
local fwd_column_index = {}
for k,v in ipairs(fw_forwarding_columns) do
  fwd_column_index[v.name] = k
end

local fw_forwarding_data,fw_forwarding_helpmsg = post_helper.handleTableQuery(fw_forwarding_columns,fw_forwarding_options,nil,nil,fw_forwarding_valid)
for _,v in ipairs(fw_forwarding_data) do
  if v[fwd_column_index.enabled] == "" then
    v[fwd_column_index.enabled] = "1"
  end
end

ngx.print(ui_helper.createHeader(T"Firewall",true,false),'\
<div class="modal-body update">\
'); lp.setpath("/www/snippets/"); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-zones-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <legend>',T"Firewall Zones",'</legend>\
      <style>#zone .line-edit {display:none;}</style>',
      ui_helper.createTable(zone_columns,zone_data,zone_options,nil,zone_helpMsg),'\
    <legend>',T"Zone Forwarding Traffic Control Rules",'</legend>',
      ui_helper.createTable(fw_forwarding_columns,fw_forwarding_data,fw_forwarding_options,nil,fw_forwarding_helpmsg),'\
  </form>\
</div>\
<script>\
  $("#zone .line-edit input[type=hidden],#zone label.hide.checkbox input[type=checkbox]").prop("disabled",true);\
</script>\
',ui_helper.createFooter())