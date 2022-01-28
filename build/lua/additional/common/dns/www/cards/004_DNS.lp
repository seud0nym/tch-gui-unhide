--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
local modalPath = "/modals/dns-modal.lp"

if session:hasAccess(modalPath) then
  local proxy = require("datamodel")
  local adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")

  if not adguard or adguard[1].value ~= "1" then
    local ui_helper = require("web.ui_helper")
    ngx.print('\
    <div class="span3">\
      <div class="smallcard">\
        ');  ngx.print( ui_helper.createCardHeader(T"DNS",modalPath) ); ngx.print('\
        <div class="content card_bg mirror" data-bg-text="&#xf002">\
          <div id="dns-card-content">\
            ')
            ngx.print('\
            <span class="simple-desc">\
              <i>&nbsp</i><strong>0</strong><span> DNS Servers</span>\
            </span>\
            <p class="subinfos" style="letter-spacing:-1px;font-size:12px;font-weight:bold;">\
            </p>\
            <span class="simple-desc">\
              <i>&nbsp</i><strong>0</strong><span> DNS Rewrites</span>\
            </span>\
            ')
            ngx.print(ui_helper.createSimpleLight("3","Checking DNS Hijacking status..."))
            ngx.print(ui_helper.createSimpleLight("3","Checking Intercept Daemon status..."))
            ngx.print('\
          </div>\
          ')
          ngx.print('\
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
end