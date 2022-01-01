--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
local assistance = require 'web.assistance'
local admin = session:hasAccess("/modals/theme-modal.lp")
local assistant = assistance.getAssistant('remote')

if admin and assistant then
  local format = string.format
  local ui_helper = require("web.ui_helper")

 ngx.print('\
<div class="span3">\
    <div class="smallcard">\
        '); ngx.print( ui_helper.createCardHeader(T"Management","modals/theme-modal.lp") ); ngx.print('\
        <div class="content card_bg" data-bg-text="&#xf0c0;">\
          <div id="management-card-content">\
          ');
          ngx.print(ui_helper.createSimpleLight("3",'<span class="modal-link" data-toggle="modal" data-remote="/modals/theme-modal.lp" data-id="theme-modal">Checking Scheduled Theme Change</span>'))
          ngx.print(
            '<span class="simple-desc">',
              '<i class="icon-cog status-icon"></i>',
              '<strong class="modal-link" data-toggle="modal" data-remote="/modals/system-proc-modal.lp" data-id="system-proc-modal">0 processes</strong> running',
            '</span>',
            '<span class="simple-desc">',
              '<i class="icon-calendar status-icon"></i>',
              '<strong class="modal-link" data-toggle="modal" data-remote="/modals/system-cron-modal.lp" data-id="system-cron-modal">0 scheduled tasks</strong> active',
            '</span>',
            '<span class="simple-desc">',
              '<i class="icon-play-sign status-icon"></i>',
              '<strong class="modal-link" data-toggle="modal" data-remote="/modals/system-init-modal.lp" data-id="system-init-modal">0 init scripts</strong> found',
            '</span>',
          '</div>'
          );
          if assistant:enabled() then
            ngx.print(ui_helper.createSimpleLight("1",T"Remote assistance enabled"))
            if admin then
              local pattern = '<strong>%s</strong>'
              local url = assistant:URL()
              ngx.print('<p class="subinfos" style="font-size:x-small;line-height:normal;">' )
              ngx.print(T"URL: ")
              ngx.print(format(pattern,url or assistant:URL6() or "?"))
              ngx.print('<br/>')
              ngx.print(T"Username: ")
              ngx.print(format(pattern,assistant:username()))
              if assistant:isRandomPassword() then
                ngx.print(T" Password: ")
                ngx.print(format(pattern,assistant:password()))
              else
              end
            end
            ngx.print('</p>')
          else
            ngx.print('<span class="modal-link" data-toggle="modal" data-remote="/modals/assistance-modal.lp" data-id="assistance-modal">')
            ngx.print(ui_helper.createSimpleLight("0",T"Remote assistance disabled"))
            ngx.print('</span>')
          end
          ngx.print('\
        </div>\
    </div>\
    <script>\
    $().ready(function(){\
      $.post("/ajax/management-status.lua",[tch.elementCSRFtoken()],function(data){\
        $("#management-card-content").html(data["html"]);\
      },"json");\
    });\
    </script>\
</div>\
');  end   ngx.print('\
');