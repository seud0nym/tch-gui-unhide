--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local vB = post_helper.validateBoolean
local vSII = post_helper.validateStringIsIP

local mapParams = {
  enabled = "uci.intercept.config.enabled",
  spoofip = "uci.intercept.dns.spoofip",
}
local mapValid = {
  enabled = vB,
  spoofip = vSII,
}
local content,content_helpmsg = post_helper.handleQuery(mapParams,mapValid)

ngx.print(ui_helper.createHeader(T"DNS",true,true))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-dns.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/dns-interceptd-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('\
    <fieldset>\
      <legend>'); ngx.print(T"Intercept Daemon"); ngx.print('</legend>\
      ');
      local info_attr = {
        alert = {
          class = "alert-info"
        },
      }
      ngx.print(ui_helper.createAlertBlock(T"<a href='https://whrl.pl/Rf8ZXP' target='_blank'>The Intercept Daemon is used for captive portals, initial setup wizards and offline alerts. It is capable of DNS spoofing to a dummy IP and L4 port interception with some rules.</a>",info_attr))
      ngx.print(ui_helper.createSwitch(T"Enabled","enabled",content["enabled"],nil,content_helpmsg["enabled"]))
      ngx.print(ui_helper.createInputText(T"DNS Spoof IP","spoofip",content["spoofip"],nil,content_helpmsg["spoofip"]))
      ngx.print('\
    </fieldset>\
  </form>\
</div>\
'); ngx.print(ui_helper.createFooter()); ngx.print('\
');