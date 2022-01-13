--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ngx = ngx
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local qry_params = {
  WOL_enabled = "uci.wol.config.enabled",
  WOL_port = "uci.wol.config.src_dport",
  WOL_dest_ip = "uci.wol.config.dest_ip",
}

local qry_valid = {
  WOL_enabled = post_helper.validateBoolean,
  WOL_port = post_helper.validateStringIsPort,
  WOL_dest_ip = post_helper.validateStringIsIP,
}

local qry_data,qry_helpmsg = post_helper.handleQuery(qry_params,qry_valid)

ngx.print(ui_helper.createHeader(T"WAN Services",false,true)); ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-wanservices.lp");ngx.print('\
  <form class="form-horizontal" method="post" action="modals/wanservices-wol-modal.lp">\
  ');
  ngx.print(ui_helper.createMessages(message_helper.popMessages()))
  ngx.print('\
    <fieldset>\
      <legend>');  ngx.print( T"Wake on LAN over the Internet" ); ngx.print('</legend>\
      ');
      ngx.print(
        ui_helper.createSwitch(T"Enabled","WOL_enabled",qry_data["WOL_enabled"],nil,qry_helpmsg["WOL_enabled"]),
        ui_helper.createInputText(T"Destination IP","WOL_dest_ip",qry_data["WOL_dest_ip"],nil,qry_helpmsg["WOL_dest_ip"]),
        ui_helper.createInputText(T"WAN port","WOL_port",qry_data["WOL_port"],nil,qry_helpmsg["WOL_port"])
      )
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
')
ngx.print(ui_helper.createFooter())

