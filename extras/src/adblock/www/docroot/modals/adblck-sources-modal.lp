--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-code')

local content_helper = require("web.content_helper")
local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local proxy = require("datamodel")
local format,match = string.format,string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

if ngx.req.get_method() == "POST" then
  local args = ngx.req.get_post_args()
  local apply_changes = false

  local enabled_source = {}
  local deleted_source = {}
  for _,source in ipairs(content_helper.convertResultToObject("uci.adblock.global.adb_sources.", proxy.get("uci.adblock.global.adb_sources."))) do
    enabled_source[untaint(source.value)] = format("uci.adblock.global.adb_sources.%s.",source.paramindex)
  end

  for k,v in pairs(args) do
    local source = match(untaint(k),"^adb_src_@(.+)_enabled$")
    if source then
      if v == "1" and not enabled_source[source] then
        local key,add_errmsg = proxy.add("uci.adblock.global.adb_sources.")
        if not key then
          for _,err in ipairs(add_errmsg) do
            message_helper.pushMessage(T(format("Failed to enable source %s: add failed with error %s (%s)",source,err.errmsg,err.errcode)),"error")
          end
        else
          local result,set_errmsg = proxy.set("uci.adblock.global.adb_sources.@"..key..".value",source)
          if result then
            message_helper.pushMessage(T("Enabled source "..source),"success")
            apply_changes = true
          else
            for _,err in ipairs(set_errmsg) do
              message_helper.pushMessage(T(format("Failed to enable source %s: set failed with error %s (%s)",source,err.errmsg,err.errcode)),"error")
            end
          end
        end
      elseif v == "0" and enabled_source[source] then
        deleted_source[#deleted_source+1] = { path = enabled_source[source], source = source }
      end
    end
  end

  if #deleted_source > 0 then
    table.sort(deleted_source,function(a,b) return b.path < a.path end)
    for _,p in ipairs(deleted_source) do
      local retval,errmsg = proxy.del(p.path)
      if not retval then
        message_helper.pushMessage(T(format("Failed to disable source %s : %s",p.source,errmsg)),"error")
      else
        message_helper.pushMessage(T("Disabled source "..p.source),"success")
        apply_changes = true
      end
    end
  end

  if apply_changes then
    proxy.apply()
  end
end

local sources_data = content_helper.convertResultToObject("rpc.adblock.source.",proxy.get("rpc.adblock.source."),"paramindex")
local enabled_source = {}
for _,source in ipairs(content_helper.convertResultToObject("uci.adblock.global.adb_sources.", proxy.get("uci.adblock.global.adb_sources."))) do
  enabled_source["@"..untaint(source.value)] = untaint(source.paramindex)
end

local warn_box = {
  alert = {
    class = "alert alert-warn",
    style = "text-align:center",
  }
}

ngx.print(ui_helper.createHeader("Adblock",false,true),'\
<div class="modal-body update">')
  local lp = require("web.lp")
  lp.setpath("/www/snippets/")
  lp.include("tabs-adblock.lp")
  ngx.print('\
  <form class="form-horizontal" method="post" action="modals/adblck-sources-modal.lp">',
    ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset><legend>',T"Black List Sources",'</legend>')
    if true then
      ngx.print(ui_helper.createAlertBlock(T"Do not enable too many lists,or you could run out or memory!",warn_box))
    end ngx.print('\
      <div class="control-group">\
        <table id="adb_sources" class="table table-striped">\
          <thead>\
            <tr>\
              <th></th>\
              <th>Name</th>\
              <th>URL</th>\
              <th>Size</th>\
              <th>Focus</th>\
              <th>Description URL</th>\
            </tr>\
          </thead>\
          <tbody>')
          for _,source in ipairs(sources_data) do
            ngx.print('<tr>')
            local paramindex = untaint(source.paramindex)
            local enabled = enabled_source[paramindex] and "1" or "0"
            ngx.print(
              '<td>',ui_helper.createSimpleSwitch("adb_src_"..paramindex.."_enabled",enabled),'</td>',
              '<td>',source.paramindex,'</td>',
              '<td style="whitespace:nowrap">',source.url,'</td>',
              '<td>',source.size,'</td>',
              '<td>',source.focus,'</td>',
              '<td style="whitespace:nowrap"><a target="_blank" href="',source.descurl,'">',source.descurl,'&nbsp;<i class="icon-external-link"></i></a></td>'
            )
            ngx.print('</tr>')
          end
          ngx.print('\
          </tbody>\
        </table>\
      </div>\
    </fieldset>\
  </form>\
</div>\
',ui_helper.createFooter(),'\
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
  });\
  $(document).off("change",\'table .switch input[type\\x3d"hidden"]\');\
  $(document).on("change",\'table .switch input[type\\x3d"hidden"]\', function(){\
    $("#modal-no-change").addClass("hide");\
    $("#modal-changes").removeClass("hide");\
  });\
</script>\
')

