--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-printersharing')

local ngx = ngx
local ui_helper = require("web.ui_helper")
local post_helper = require("web.post_helper")
local attributes = { span = { class = "span4" } }
local message_helper = require("web.uimessage_helper")
local lp = require("web.lp")
lp.setpath("/www/snippets/")

local mapParams = {
  agent_enabled = "uci.multiap.agent.enabled",
  controller_enabled = "uci.multiap.controller.enabled",
  state5g = "uci.wireless.wifi-device.@radio_5G.state",
  ap6iface = "uci.wireless.wifi-ap.@ap6.iface",
  ap6state = "uci.wireless.wifi-ap.@ap6.state",
  wl1_2ssid = "uci.wireless.wifi-iface.@wl1_2.ssid",
}

local mapValid = {
  agent_enabled = post_helper.validateBoolean,
  controller_enabled = post_helper.validateBoolean,
  ap6state = post_helper.validateBoolean,
}

local content, helpmsg = post_helper.handleQuery(mapParams, mapValid)

  ngx.print('\
\
');  ngx.print( ui_helper.createHeader(T"Wi-Fi Boosters", false, true, nil)); ngx.print('\
<div class="modal-body update">\
');  lp.include("tabs-boosters.lp")   ngx.print('\
  <form class="form-horizontal" method="post" action="modals/wireless-boosters-modal.lp">\
    ');  ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
    ');  
      html = {}
      html[#html+1] = "<fieldset><legend>" .. T"Multi-AP Status" .. "</legend>"
      html[#html+1] = ui_helper.createSwitch(T"Controller", "controller_enabled", content["controller_enabled"], attributes)
      html[#html+1] = ui_helper.createSwitch(T"Agent", "agent_enabled", content["agent_enabled"], attributes)
      html[#html+1] = "</fieldset>"
      html[#html+1] = "<fieldset><legend>" .. T"Wireless Backhaul" .. "</legend>"
      html[#html+1] = ui_helper.createSwitch(T"SSID " .. content["wl1_2ssid"], "ap6state", content["ap6state"], attributes)
      html[#html+1] = "</fieldset>"
      ngx.print(html)
      ngx.print('\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter())   ngx.print('\
'); 