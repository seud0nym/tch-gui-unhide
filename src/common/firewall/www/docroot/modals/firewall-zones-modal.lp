--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")

local concat = table.concat
local tolower = string.lower

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local zone_columns = {
  { -- [1]
    header = "",
    name = "status",
    param = "enabled",
    type = "switch",
    readonly = true,
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }},
  },
  { -- [2]
    header = T"Zone<br>Name",
    name = "name",
    param = "name",
    type = "text",
    attr = { input = { class="span2"} },
  },
  { -- [3]
    header = T"Incoming Traffic<br>Default Policy",
    name = "input",
    param = "input",
    type = "text",
    attr = { input = { class="span2"} },
  },
  { -- [4]
    header = T"Forwarded Traffic<br>Default Policy",
    name = "forward",
    param = "forward",
    type = "text",
    attr = { input = { class="span2"} },
  },
  { -- [5]
    header = T"Outgoing Traffic<br>Default Policy",
    name = "output",
    param = "output",
    type = "text",
    attr = { input = { class="span2"} },
  },
  { -- [6]
    header = T"Network<br>Interfaces",
    name = "network",
    param = "network",
    type = "checkboxgroup",
    values = {},
    attr = { input = { class="span2"} },
  },
  { -- [7]
    header = "Masqueraded<br>(NAT)",
    name = "masq",
    param = "masq",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" }}
  },
  { -- [8]
    header = "TCP MSS<br>Clamping",
    name = "mtu_fix",
    param = "mtu_fix",
    type = "switch",
    attr = { switch = { ["data-placement"] = "center" }}
  },
}

local zone_sort = function(zone1,zone2)
  return tolower(zone1.name) > tolower(zone2.name)
end

local zone_options = {
  tableid = "zone",
  basepath = "uci.firewall.zone.@.",
  canEdit = false,
  canAdd = false,
  canDelete = false,
  sorted = zone_sort,
}

local zone_data,zone_helpMsg = post_helper.handleTableQuery(zone_columns,zone_options,nil,nil,nil)
for _,v in ipairs(zone_data) do
  if v[1] == "" then
    v[1] = "1"
  end
  if v[6] and type(v[6]) == "table" then
    v[6] = concat(v[6],",")
    zone_columns[6].type = "text"
  end
end

local fw_forwarding_columns = {
  { -- [1]
    header = "",
    name = "status",
    param = "enabled",
    type = "switch",
    readonly = true,
    default = "1",
    attr = { switch = { ["data-placement"] = "right" }},
  },
  {
    header = T"Name",
    name = "name",
    param = "name",
    type = "text",
    readonly = true,
  },
  { -- [3]
    header = T"Family",
    name = "family",
    param = "family",
    type = "text",
    readonly = true,
  },
  {
    header = T"Source Zone",
    name = "src",
    param = "src",
    type = "text",
    readonly = true,
  },
  {
    header = T"Destination Zone",
    name = "dest",
    param = "dest",
    type = "text",
    readonly = true,
  },
}

local fw_forwarding_options = {
  tableid = "fw_forwardings",
  basepath = "uci.firewall.forwarding.@.",
  canAdd = false,
  canEdit = false,
  canDelete = false,
}

local fw_forwarding_data,fw_forwarding_helpmsg = post_helper.handleTableQuery(fw_forwarding_columns,fw_forwarding_options,nil,nil,nil)
for _,v in ipairs(fw_forwarding_data) do
  if v[1] == "" then
    v[1] = "1"
  end
end

ngx.print(ui_helper.createHeader(T"Firewall",true,false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-zones-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>');  ngx.print( T"Firewall Zones" ); ngx.print('</legend>');
    ngx.print(ui_helper.createTable(zone_columns,zone_data,zone_options,nil,zone_helpMsg))
    ngx.print('<legend>');  ngx.print( T"Zone Forwarding Traffic Control Rules" ); ngx.print('</legend>');
    ngx.print(ui_helper.createTable(fw_forwarding_columns,fw_forwarding_data,fw_forwarding_options,nil,fw_forwarding_helpmsg))
    ngx.print('\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter());