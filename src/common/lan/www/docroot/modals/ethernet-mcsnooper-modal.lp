--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")

local vB = post_helper.validateBoolean

local params = {
  igmp_snooping = "uci.mcastsnooping.interface.@lan.igmp_snooping",
  mld_snooping = "uci.mcastsnooping.interface.@lan.mld_snooping",
  mcast6_flooding = "uci.mcastsnooping.interface.@lan.mcast6_flooding",
  mcast_flooding = "uci.mcastsnooping.interface.@lan.mcast_flooding",
  igmplan2lan_snooping = "uci.mcastsnooping.interface.@lan.igmplan2lan_snooping",
  mldlan2lan_snooping = "uci.mcastsnooping.interface.@lan.mldlan2lan_snooping",
}

local valid = {
  igmp_snooping = vB,
  mld_snooping = vB,
  mcast6_flooding = vB,
  mcast_flooding = vB,
  igmplan2lan_snooping = vB,
  mldlan2lan_snooping = vB,
}

local data,helpmsg = post_helper.handleQuery(params,valid)
if not helpmsg then
  helpmsg = {}
end

ngx.print(ui_helper.createHeader(T"Local Network",false,false),'\
<div class="modal-body update">\
');
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-lan.lp")
ngx.print(
  ui_helper.createMessages(message_helper.popMessages()),'\
  <form class="form-horizontal" method="post" action="modals/ethernet-mcsnooper-modal.lp">\
    <fieldset>\
      <legend>',T"LAN Multicast Snooping",'</legend>',
        ui_helper.createSwitch(T"IGMP Snooping","igmp_snooping",data["igmp_snooping"]),
        ui_helper.createSwitch(T"MLD Snooping","mld_snooping",data["mld_snooping"]),
        ui_helper.createSwitch(T"IGMP LAN-LAN Snooping","igmplan2lan_snooping",data["igmplan2lan_snooping"]),
        ui_helper.createSwitch(T"MLD LAN-LAN Snooping","mldlan2lan_snooping",data["mldlan2lan_snooping"]),
        ui_helper.createSwitch(T"Multicast Flooding","mcast_flooding",data["mcast_flooding"]),
        ui_helper.createSwitch(T"Multicast 6 Flooding","mcast6_flooding",data["mcast6_flooding"]),
        '\
    </fieldset>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
</div>\
',ui_helper.createFooter())
