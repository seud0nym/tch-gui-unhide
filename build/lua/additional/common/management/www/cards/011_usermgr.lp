--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local session = ngx.ctx.session
local assistance = require 'web.assistance'
local admin = session:hasAccess("/modals/theme-modal.lp")
local assistant = assistance.getAssistant('remote')

if admin and assistant then
  local tinsert = table.insert
  local format = string.format
  local ui_helper = require("web.ui_helper")
  local content_helper = require("web.content_helper")
  local theme_helper = require("theme-schedule_helper")
  local proxy = require("datamodel")

  local counts = {
    cron = "rpc.gui.cron.ActiveNumberOfEntries",
    init = "rpc.gui.init.InitNumberOfEntries",
    proc = "rpc.gui.proc.ProcNumberOfEntries",
  }
  content_helper.getExactContent(counts)

  local function toTime(hh,mm)
    local m
    if hh >= 12 then
      hh = hh - 12
      m = "p"
    else
      m = "a"
    end
    return format("%d:%02d%sm", hh, mm, m)
  end

  local light, night = theme_helper.getThemeSchedule()
  local on_schedule, theme_status = "0", "disabled"
  if light.hh and light.mm and night.hh and night.mm and light.enabled and night.enabled then
    on_schedule = "1"
    local now = tonumber(os.date("%H")) * 60 + tonumber(os.date("%M"))
    local light_time = light.hh * 60 + light.mm
    local night_time = night.hh * 60 + night.mm
    if light_time <= night_time then
      if now >= light_time and now <= night_time then
        theme_status = toTime(night.hh, night.mm)
      else
        theme_status = toTime(light.hh, light.mm)
      end
    else
      if now >= night_time and now <= light_time then
        theme_status = toTime(light.hh, light.mm)
      else
        theme_status = toTime(night.hh, night.mm)
      end
    end
  end

 ngx.print('\
<div class="span3">\
    <div class="smallcard">\
        '); ngx.print( ui_helper.createCardHeader(T"Management", "modals/theme-modal.lp") ); ngx.print('\
        <div class="content card_bg" data-bg-text="&#xf0c0;">\
          ');  
          ngx.print(ui_helper.createSimpleLight(on_schedule, '<span class="modal-link" data-toggle="modal" data-remote="/modals/theme-modal.lp" data-id="theme-modal">Scheduled Theme Change</span> '..theme_status))
          ngx.print( 
            '<span class="simple-desc">',
            '<i class="icon-cog status-icon"></i>',
            format(N("<strong %s>%d process</strong> running",
                    "<strong %s>%d processes</strong> running", counts.proc),
                  'class="modal-link" data-toggle="modal" data-remote="/modals/system-proc-modal.lp" data-id="system-proc-modal"',
                  counts.proc),
            '</span>',
            '<span class="simple-desc">',
              '<i class="icon-calendar status-icon"></i>',
              format(N("<strong %s>%d scheduled task</strong> active",
                      "<strong %s>%d scheduled tasks</strong> active", counts.cron),
                    'class="modal-link" data-toggle="modal" data-remote="/modals/system-cron-modal.lp" data-id="system-cron-modal"',
                    counts.cron),
            '</span>',
            '<span class="simple-desc">',
              '<i class="icon-play-sign status-icon"></i>',
              format(N("<strong %s>%d init script</strong> found",
                      "<strong %s>%d init scripts</strong> found", counts.init),
                    'class="modal-link" data-toggle="modal" data-remote="/modals/system-init-modal.lp" data-id="system-init-modal"',
                    counts.init),
            '</span>'
          ); 
          if assistant:enabled() then
            ngx.print(ui_helper.createSimpleLight("1", T"Remote assistance enabled"))
            if admin then
              ngx.print('<p class="subinfos" style="font-size:x-small;line-height:normal;">' )
              ngx.print(T"Port:")
              ngx.print(format(' <strong id="port">%s</strong>', assistant:port()))
              ngx.print('<br/>')
              ngx.print(T"Username:")
              ngx.print(format(' <strong id="username">%s</strong>', assistant:username()))
              if assistant:isRandomPassword() then
                local tremove = table.remove
                for i = 1, 4, 1 do
                  tremove(content, 1)
                end
                ngx.print("<br/>")
                ngx.print(T"Password:")
                ngx.print(format(' <strong id="password">%s</strong>', assistant:password()))
              end
            end
            ngx.print('</p>')
          else
            ngx.print('<span class="modal-link" data-toggle="modal" data-remote="/modals/assistance-modal.lp" data-id="assistance-modal">')
            ngx.print(ui_helper.createSimpleLight("0", T"Remote assistance disabled"))
            ngx.print('</span>')
          end
          ngx.print('\
        </div>\
    </div>\
</div>\
');  end   ngx.print('\
');