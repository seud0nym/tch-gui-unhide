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

  local counts = {
    cron = "rpc.gui.cron.active",
    init = "rpc.gui.init.InitNumberOfEntries",
    proc = "rpc.gui.proc.ProcNumberOfEntries",
  }
  content_helper.getExactContent(counts)
    
  local state
  local content = {
    '<span class="simple-desc">',
      '<i class="icon-tint status-icon">&nbsp;</i>',
      '<span class="modal-link" data-toggle="modal" data-remote="/modals/theme-modal.lp" data-id="theme-modal">Theme</span> enabled',
    '</span>',
    '<span class="simple-desc">',
      '<i class="icon-cog status-icon">&nbsp;</i>',
      format(N("<strong %s>%d process</strong> running",
               "<strong %s>%d processes</strong> running", counts.proc),
            'class="modal-link" data-toggle="modal" data-remote="/modals/system-proc-modal.lp" data-id="system-proc-modal"',
            counts.proc),
    '</span>',
    '<span class="simple-desc">',
      '<i class="icon-calendar status-icon">&nbsp;</i>',
      format(N("<strong %s>%d scheduled task</strong> active",
               "<strong %s>%d scheduled tasks</strong> active", counts.cron),
            'class="modal-link" data-toggle="modal" data-remote="/modals/system-cron-modal.lp" data-id="system-cron-modal"',
            counts.cron),
    '</span>',
    '<span class="simple-desc">',
      '<i class="icon-play-sign status-icon">&nbsp;</i>',
      format(N("<strong %s>%d init script</strong> found",
               "<strong %s>%d init scripts</strong> found", counts.init),
            'class="modal-link" data-toggle="modal" data-remote="/modals/system-init-modal.lp" data-id="system-init-modal"',
            counts.init),
    '</span>',
  }
  if assistant:enabled() then
    state = "1"
    tinsert(content, ui_helper.createSimpleLight(state, T"Remote assistance enabled"))
    if admin then
      tinsert(content, '<p class="subinfos" style="font-size:x-small;line-height:normal;">' )
      tinsert(content, T"Port:")
      tinsert(content, format(' <strong id="port">%s</strong>', assistant:port()))
      tinsert(content, '<br/>')
      tinsert(content, T"Username:")
      tinsert(content, format(' <strong id="username">%s</strong>', assistant:username()))
      if assistant:isRandomPassword() then
        local tremove = table.remove
        for i = 1, 4, 1 do
          tremove(content, 1)
        end
        tinsert(content, "<br/>")
        tinsert(content, T"Password:")
        tinsert(content, format(' <strong id="password">%s</strong>', assistant:password()))
      end
    end
    tinsert(content, '</p>')
  else
    state = "0"
    tinsert(content, ui_helper.createSimpleLight(state, T"Remote assistance disabled"))
  end
  ngx.print('\
<div class="span3">\
    <div class="smallcard">\
        ');  ngx.print( ui_helper.createCardHeader(T"Management", "modals/theme-modal.lp") ); ngx.print('\
        <div class="content card_bg" data-bg-text="&#xf0c0;">\
            ');  ngx.print( content ); ngx.print('\
        </div>\
    </div>\
</div>\
');  end   ngx.print('\
'); 