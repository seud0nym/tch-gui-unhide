--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local bridged_mode = require("bridgedmode_helper")
local ethernet_helper = require("ethernet_helper")
local lp = require("web.lp")
local message_helper = require("web.uimessage_helper")
local post_helper = require("web.post_helper")
local ui_helper = require("web.ui_helper")

local mapParams = {
  ula_prefix = "uci.network.globals.ula_prefix",
}

local mapValid = {
  ula_prefix = ethernet_helper.validateULAPrefix,
}

local content,helpmsg = post_helper.handleQuery(mapParams,mapValid)
if not helpmsg then helpmsg = {} end

if ngx.var.request_method == "POST" then
  bridged_mode.configure(ngx.req.get_post_args(0)["action"])
end

local ula_attr = {
  controls = { style = "width:220px", },
  group = { class = "monitor-localIPv6 monitor-1 monitor-hidden-localIPv6", },
  input = { style = "width:180px", }
}

ngx.print(ui_helper.createHeader(T"Local Network",true,true),'\
<div class="modal-body update">\
'); lp.setpath("/www/snippets/"); lp.include("tabs-lan.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="',ngx.var.request_uri,'">\
  ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset>\
      <legend>',T"Global Configuration",'</legend>\
      ',ui_helper.createInputText(T"IPv6 ULA Prefix<span class='icon-question-sign' title='IPv6 equivalent of IPv4 private addresses. Must start with fd followed by 40 random bits and a /48 range (e.g. fd12:3456:789a::/48)'></span>", "ula_prefix", content["ula_prefix"], ula_attr, helpmsg["ula_prefix"]),'\
    </fieldset>\
    <fieldset>\
      <legend>',T"Network Mode",'</legend>\
        ');
        local html = {}
        if not bridged_mode.isBridgedMode() then
          bridged_mode.addBridgedModeButtons(html)
        else
          bridged_mode.addRoutedModeButtons(html)
        end
        ngx.print(html,'\
    </fieldset>\
  </form>\
  <script src="/js/modal-busy.js"></script>\
  <script src="/js/ethernet-modal.js"></script>\
  <script src="/js/bridgedmode.js"></script>\
</div>\
',ui_helper.createFooter())