--pretranslated: do not change this file
gettext.textdomain('webui-voice')

local format = string.format
local ipairs = ipairs
local ngx = ngx
local session = ngx.ctx.session

local tabs = {
  {"mmpbx-info-modal.lp",T"Information"},
  {"mmpbx-global-modal.lp",T"Global"},
  {"mmpbx-profile-modal.lp",T"Phone Numbers"},
  {"mmpbx-volte-modal.lp",T"VoLTE"},
  {"mmpbx-inoutgoingmap-modal.lp",T"In/Outgoing Map"},
  {"mmpbx-codec-modal.lp",T"Codecs"},
  {"mmpbx-dialplan-modal.lp",T"Dial Plans"},
  {"mmpbx-service-modal.lp",T"Services"},
  {"tod_dnd-modal.lp",T"Do Not Disturb"},
  {"mmpbx-statistics-modal.lp",T"Statistics"},
  {"mmpbx-log-modal.lp",T"Call Log"},
  {"mmpbx-contacts-modal.lp",T"Phone Book"},
  {"mmpbx-dect-modal.lp",T"DECT"},
}

local uri = ngx.var.uri
if ngx.var.args and string.len(ngx.var.args) > 0 then
  uri = uri.."?"..ngx.var.args
end

ngx.print('<ul class="nav nav-tabs">')
for _,v in ipairs(tabs) do
  if session:hasAccess("/modals/"..v[1]) then
    local active = ""
    if uri == ("/modals/"..v[1]) then
      active = "active"
    end
    ngx.print(format('<li class="%s"><a id ="%s" href="#" class="open-modal" data-remote="/modals/%s">%s</a></li>',active,v[2],v[1],v[2]))
  end
end
ngx.print('</ul>','\
<div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>\
<script>\
  $(function(){\
    function waiting_action(self) {\
      var msg_dst = $(self);\
      var busy_msg = $(".loading-wrapper");\
      msg_dst.after(busy_msg);\
      busy_msg.removeClass("hide");\
      busy_msg[0].scrollIntoView();\
      $(".modal-body").scrollLeft(0);\
    };\
    $(document).on("click", "#save-config", function () {\
      waiting_action(this);\
    });\
    $(document).on("click", "table [class*=\'btn-table-\']:not(.disabled)", function () {\
      waiting_action(this);\
    });\
    $(document).on("change","table .switch input[type=\'hidden\']",function (e) {\
      var table = $(this).closest("table");\
      // Check that we are not editing a line, this is only for when the line is displayed\
      if (table.find(".btn-table-cancel").length === 0) {\
        waiting_action(this);\
      }\
    });\
  });\
</script>\
')
