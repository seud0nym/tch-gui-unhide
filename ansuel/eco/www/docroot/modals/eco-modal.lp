--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local content_helper = require("web.content_helper")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")

local vB = post_helper.validateBoolean

local mapParams = {
  cpuspeed = "uci.power.cpu.cpuspeed",
  wait = "uci.power.cpu.wait",
  ethapd = "uci.power.ethernet.ethapd",
  eee = "uci.power.ethernet.eee",
  autogreeen = "uci.power.ethernet.autogreeen",
  dgm = "uci.power.ethernet.dgm",
}

local mapValid = {
  cpuspeed = post_helper.validateNonEmptyString,
  wait = vB,
  ethapd = vB,
  eee = vB,
  autogreeen = vB,
  dgm = vB,
}

local content, helpmsg = post_helper.handleQuery(mapParams, mapValid)

local cpu_step = {
  {"256", T"&nbsp;&nbsp;CPU Frequency Auto-Scaling"},
  {"8", T"&nbsp;&nbsp;10% of max CPU Frequency in ASYNC mode"},
  {"4", T"&nbsp;&nbsp;25% of max CPU Frequency in ASYNC mode"},
  {"2", T"&nbsp;&nbsp;50% of max CPU Frequency in ASYNC mode"},
  {"1", T"&nbsp;&nbsp;100% of max CPU Frequency in ASYNC mode"},
  {"0", T"&nbsp;&nbsp;100% of max CPU Frequency in SYNC mode"},
}

ngx.print(ui_helper.createHeader(T"Eco Settings", false, false))
ngx.print('\
<div class="modal-body update">\
  <form class="form-horizontal" method="post" action="modals/eco-modal.lp">\
  '); ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
    <fieldset>\
      ');
      local info_box = {
        alert = {
          class = "alert alert-info",
          style = "text-align:center",
        }
      }
      local html = {}
      html[#html + 1] = ui_helper.createAlertBlock(T"Turning on Eco features can cause major latency.", info_box)
      html[#html + 1] = '<fieldset><legend>' .. T"CPU Settings" .. '</legend>'
      html[#html + 1] = ui_helper.createSliderSelect(T"CPU Speed", "cpuspeed", cpu_step , content["cpuspeed"])
      html[#html + 1] = ui_helper.createSwitch(T"CPU Wait", "wait", content["wait"])
      html[#html + 1] = '</fieldset>'
      html[#html + 1] = '<fieldset><legend>' .. T"Ethernet Settings" .. '</legend>'
      html[#html + 1] = ui_helper.createSwitch(T"Auto Power Down", "ethapd", content["ethapd"])
      html[#html + 1] = ui_helper.createSwitch(T"Energy Efficent Ethernet", "eee", content["eee"])
      html[#html + 1] = ui_helper.createSwitch(T"AutoGrEEEn", "autogreeen", content["autogreeen"])
      html[#html + 1] = ui_helper.createSwitch(T"Deep Green Mode", "dgm", content["dgm"])
      html[#html + 1] = '</fieldset>'
      ngx.print(html)
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter());
