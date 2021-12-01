--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")

local vB = post_helper.validateBoolean

local mapParams = {
  apd = "uci.power.ethernet.apd",
  dgm = "uci.power.ethernet.dgm",
  eee = "uci.power.ethernet.eee",
  net = "uci.power.ethernet.net",
  phy = "uci.power.ethernet.phy",
}

local mapValid = {
  apd = vB,
  eee = vB,
  dgm = vB,
  net = vB,
  phy = vB,
}

local content = post_helper.handleQuery(mapParams,mapValid)

ngx.print(ui_helper.createHeader(T"Eco Settings",false,false))
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
      local warn_box = {
        alert = {
          class = "alert alert-warn",
          style = "text-align:center",
        }
      }
      local html = {}
      html[#html + 1] = ui_helper.createAlertBlock(T"Turning on Eco features can cause latency.<br>Turning off Eco features can increase heat and reduce component lifespan.",info_box)
      html[#html + 1] = '<fieldset><legend>'..T"Ethernet Settings"..'</legend>'
      html[#html + 1] = ui_helper.createSwitch(T"Auto Power Down","apd",content["apd"])
      html[#html + 1] = ui_helper.createSwitch(T"Deep Green Mode","dgm",content["dgm"])
      html[#html + 1] = ui_helper.createSwitch(T"Energy Efficent Ethernet","eee",content["eee"])
      html[#html + 1] = ui_helper.createAlertBlock(T"The following options power down network interfaces. Use with care.",warn_box)
      html[#html + 1] = ui_helper.createSwitch(T"Network Device Down","net",content["net"])
      html[#html + 1] = ui_helper.createSwitch(T"PHY Power Down","phy",content["phy"])
      html[#html + 1] = '</fieldset>'
      ngx.print(html)
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
');  ngx.print(ui_helper.createFooter());
