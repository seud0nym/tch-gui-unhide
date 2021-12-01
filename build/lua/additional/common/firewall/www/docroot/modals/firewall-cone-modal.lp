--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local fw_cone_columns = {
  {
    header = T"Name",
    name = "name",
    param = "name",
    type = "text",
    readonly = true,
  },
  {
    header = T"Source",
    name = "src",
    param = "src",
    type = "text",
    readonly = true,
  },
  {
    header = T"Source Port",
    name = "src_port",
    param = "src_port",
    type = "text",
    readonly = true,
  },
  {
    header = T"Destination Port",
    name = "dest_port",
    param = "dest_port",
    type = "text",
    readonly = true,
  },
  {
    header = T"Destination IP",
    name = "destinationip",
    param = "dest_ip",
    type = "text",
    readonly = true,
  },
 }

local fw_cone_options = {
  tableid = "fw_cones",
  basepath = "uci.firewall.cone.@.",
  canAdd = false,
  canEdit = false,
  canDelete = false,
}

local fw_cone_data,fw_cone_helpmsg = post_helper.handleTableQuery(fw_cone_columns,fw_cone_options,nil,nil,nil)

ngx.print(ui_helper.createHeader(T"Firewall",true,false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-firewall.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/firewall-cones-modal.lp">\
    '); 
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>');  ngx.print( T"Cone NAT Rules" ); ngx.print('</legend>');
    ngx.print(ui_helper.createTable(fw_cone_columns,fw_cone_data,fw_cone_options,nil,fw_cone_helpmsg))
    ngx.print('\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter());