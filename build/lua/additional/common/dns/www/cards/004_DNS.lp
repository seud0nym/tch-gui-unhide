--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
local modalPath = "/modals/dns-modal.lp"

if session:hasAccess(modalPath) then
  local proxy = require("datamodel")
  local adguard = proxy.get("rpc.gui.init.files.@AdGuardHome.active")

  if not adguard or adguard[1].value ~= "1" then
    local dyntab_helper = require("web.dyntab_helper")
    local imh = require("internetmode_helper")
    local mode_active = dyntab_helper.process(imh).current.name
    if mode_active == "" then
      for _,v in ipairs(imh) do
        if v.default == true then
          mode_active = v.name
          break
        end
      end
    end

    if mode_active ~= "bridge" then
      local ui_helper = require("web.ui_helper")
      local format = string.format
      local interceptd = proxy.get("uci.intercept.config.enabled","uci.intercept.dns.spoofip")
      local interceptd_enabled
      local interceptd_status
      if interceptd and interceptd[1].value == "0" then
        interceptd_enabled = "0"
        interceptd_status = "disabled"
      else
        interceptd_enabled = "1"
        interceptd_status = "enabled"
      end

      ngx.print('\
      <div class="span3">\
        <div class="smallcard">\
          ');  ngx.print( ui_helper.createCardHeader(T"DNS",modalPath) ); ngx.print('\
          <div class="content card_bg mirror" data-bg-text="&#xf002">\
            <div id="dns-card-content">\
              ')
              ngx.print(ui_helper.createSimpleLight("3","Checking Custom DNS status..."))
              ngx.print('\
              <span class="simple-desc">\
                <i>&nbsp</i>\
                ');
                ngx.print(format(N('<strong>%d</strong><span> DNS Server:</span>','<strong>%d</strong><span> DNS Servers:</span>',0),0))
                ngx.print('\
              </span>\
              <p class="subinfos" style="letter-spacing:-1px;font-size:12px;font-weight:bold;">\
              </p>\
              <span class="simple-desc">\
                <i>&nbsp</i>\
                ');
                ngx.print(format(N('<strong>%d</strong><span> DNS Rewrite</span>','<strong>%d</strong><span> DNS Rewrites</span>',0),0))
                ngx.print('\
              </span>\
            </div>\
            ')
            ngx.print(ui_helper.createSimpleLight(interceptd_enabled,T(format('<span class="modal-link" data-toggle="modal" data-remote="/modals/dns-interceptd-modal.lp" data-id="dns-interceptd-modal">Intercept Daemon %s</span>',interceptd_status))))
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
end