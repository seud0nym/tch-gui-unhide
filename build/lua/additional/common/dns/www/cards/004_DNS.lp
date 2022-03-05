--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
local ui_helper = require("web.ui_helper")
local proxy = require("datamodel")
local modalPath = "/modals/dns-modal.lp"
local adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")
if adguard and adguard[1].value == "1" then
  modalPath = "/modals/dns-interceptd-modal.lp"
end

if session:hasAccess(modalPath) then
  ngx.print('\
  <div class="span3">\
    <div class="smallcard">\
      ');  ngx.print( ui_helper.createCardHeader(T"DNS",modalPath) ); ngx.print('\
      <div class="content card_bg mirror" data-bg-text="&#xf002">\
        <div id="dns-card-content">\
          <span class="simple-desc">Getting DNS Status...</span>\
        </div>\
      </div>\
    </div>\
    <script>\
    $().ready(function(){\
      $.post("/ajax/dns-status.lua",[tch.elementCSRFtoken()],function(data){\
        $("#dns-card-content").html(data["html"]);\
      },"json");\
    });\
    </script>\
  </div>\
  ');
end
