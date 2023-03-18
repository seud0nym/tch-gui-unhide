--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

local getargs = ngx.req.get_uri_args()
if getargs.clear_leases and getargs.clear_leases == "1" then
  require("datamodel").set("rpc.gui.upnpd.clear_leases","1")
end

local gVIES = post_helper.getValidateInEnumSelect
local vB = post_helper.validateBoolean

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local igd_select = {
  { "igdv1",T'1'},
  { "igdv2",T'2'},
}

local qry_params = {
  upnp_status = "uci.upnpd.config.enable_upnp",
  upnp_natpmp = "uci.upnpd.config.enable_natpmp",
  upnp_secure_mode = "uci.upnpd.config.secure_mode",
  upnp_igd_version = "uci.upnpd.config.datamodel_version",
}

local qry_valid = {
  upnp_status = vB,
  upnp_natpmp = vB,
  upnp_secure_mode = vB,
  upnp_igd_version = gVIES(igd_select),
}

local qry_data,qry_helpmsg = post_helper.handleQuery(qry_params,qry_valid)

local upnp_columns = {
  {
    header = T"Protocol",
    name = "protocol",
    param = "proto",
    default = "tcp",
    type = "select",
    values = {
      { "tcp","TCP"},
      { "udp","UDP"},
      { "tcpudp","TCP+UDP"}
    },
    attr = { select = { class="span2" } },
  },
  {
    header = T"WAN port",
    name = "wanport",
    param = "src_dport",
    type = "text",
    attr = { input = { class="span1",maxlength="5" } },
  },
  {
    header = T"LAN port",
    name = "lanport",
    param = "dest_port",
    type = "text",
    attr = { input = { class="span1",maxlength="5" } },
  },
  {
    header = T"Destination",
    name = "destinationip",
    param = "dest_ip",
    type = "text",
    attr = { input = { class="span2",maxlength="15"} },
  },
  {
    header = T"Description",
    name = "description",
    param = "description",
    type = "text",
    attr = { input = { class="span2",maxlength="15"} },
  },
}

local upnp_options = {
  canEdit = false,
  canAdd = false,
  canDelete = false,
  tableid = "upnpportforwarding",
  basepath = "sys.upnp.redirect.",
}

local upnp_data,upnp_helpmsg = post_helper.handleTableQuery(upnp_columns,upnp_options,nil,nil,nil)

local button_attr = { button = { id = "btn-clear-leases" }, group = { class = #upnp_data > 0 and nil or "hide", } }

ngx.print(ui_helper.createHeader(T"WAN Services",false,true)); ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-wanservices.lp");ngx.print('\
  <form class="form-horizontal" method="post" action="modals/wanservices-upnp-modal.lp">',
  ui_helper.createMessages(message_helper.popMessages()),'\
  <fieldset>\
    <legend>',T"UPnP",'</legend>',
      ui_helper.createSwitch(T"UPnP Enabled","upnp_status",qry_data["upnp_status"],nil,qry_helpmsg["upnp_status"]),
      ui_helper.createSwitch(T"NAT-PMP Enabled","upnp_natpmp",qry_data["upnp_natpmp"],nil,qry_helpmsg["upnp_natpmp"]),
      ui_helper.createSwitch(T"Secure Mode Enabled","upnp_secure_mode",qry_data["upnp_secure_mode"],nil,qry_helpmsg["upnp_secure_mode"]),
      ui_helper.createInputSelect(T"Datamodel Version","upnp_igd_version",igd_select,qry_data.upnp_igd_version,nil),
      ui_helper.createLabel(T"",T"Note: Datamodel version <b>1</b> may be required for older applications.",{span = {class = "span12 advanced hide"},}),
      ui_helper.createTable(upnp_columns,upnp_data,upnp_options,nil,upnp_helpmsg),
      ui_helper.createButton("Clear Leases","Restart Service","icon-refresh",button_attr),'\
      <script>$("#btn-clear-leases").click(function(){tch.loadModal($(".modal form").attr("action")+"?clear_leases=1");})</script>\
  </fieldset>\
  </form>\
</div>\
')
ngx.print(ui_helper.createFooter())
