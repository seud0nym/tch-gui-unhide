--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ngx = ngx
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local post_helper = require("web.post_helper")
local message_helper = require("web.uimessage_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local vB = post_helper.validateBoolean
local gVCS = post_helper.getValidateCheckboxSwitch()

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local mapParams = {
  feeds = "rpc.gui.opkg.feeds_configured",
  scheduled = "rpc.gui.opkg.scheduled_update",
  run_update = "rpc.gui.opkg.update",
}
local mapValid = {
  feeds = vB,
  scheduled = vB,
  run_update = gVCS,
}
local content = post_helper.handleQuery(mapParams,mapValid)

local opkg_data = {
  last_update = "rpc.gui.opkg.last_update",
  log = "rpc.gui.opkg.log",
  user_installed = "rpc.gui.opkg.user_installed",
  user_upgradable = "rpc.gui.opkg.user_upgradable",
}
content_helper.getExactContent(opkg_data)

local last_update
if opkg_data["last_update"] == "-1" then
  last_update = T"Not since last reboot"
else
  last_update = T(os.date("%d/%m/%Y %H:%M:%S",tonumber(opkg_data["last_update"])))
end

ngx.print('\
\
');  ngx.print( ui_helper.createHeader(T"Packages",false,true,nil)); ngx.print('\
<div class="modal-body update">\
');  lp.include("tabs-opkg.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/opkg-modal.lp">\
    ');  ngx.print(ui_helper.createMessages(message_helper.popMessages())); ngx.print('\
    <fieldset><legend>'); ngx.print( T"Configuration" ); ngx.print('</legend> \
      ');
      local switch_class_enable = {
        input = {
          class = "monitor-changes",
        }
      }
      local switch_class_monitor = {
        group = {
          class = "monitor-feeds monitor-1 monitor-hidden-feeds",
        },
      }
      local opkg_last_update_attr = {
        span = {
          id = "opkg_last_update",
        },
      }
      local opkg_log_attr = {
        group = {
          class = "monitor-feeds monitor-1 monitor-hidden-feeds",
        },
        span = {
          id = "opkg_log",
          style = "display:block;white-space:pre;",
        },
      }

      local html = {}
      html[#html+1] = ui_helper.createSwitch(T"Custom Feeds","feeds",content["feeds"],switch_class_enable)
      html[#html+1] = ui_helper.createLabel(T"Last Update",last_update,opkg_last_update_attr)
      if content["feeds"] == "1" then
        html[#html+1] = ui_helper.createSwitch(T"Scheduled Update","scheduled",content["scheduled"],switch_class_monitor)
        html[#html+1] = ui_helper.createCheckboxSwitch(T"Update Now","run_update",content["run_update"])
      else
        html[#html+1] = ui_helper.createSimpleInputHidden("scheduled",content["scheduled"])
        html[#html+1] = ui_helper.createSimpleInputHidden("run_update","_DUMMY_")
      end
      html[#html+1] = ui_helper.createLabel(T"Log Messages",untaint(opkg_data["log"]),opkg_log_attr)
      ngx.print(html)
      ngx.print('\
    </fieldset>\
  </form>\
  <div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>\
</div>\
');  ngx.print(ui_helper.createFooter())   ngx.print('\
<script>\
  var opkgLogFuncID;\
  function updateAcmeLog(){\
    if ($("#opkg_log").length) {\
      $.post("/ajax/opkg-cfg.lua",[tch.elementCSRFtoken()],function(data){\
        $("#opkg_log").html(data["text"]);\
        $("#opkg_last_update").html(data["updated"]);\
      },"json")\
      .fail(function(response){\
        if(response.status==403||response.status==404){clearInterval(opkgLogFuncID);}\
      });\
    }\
  }\
  updateAcmeLog();\
  $("#save-config").on("click",function(){\
    var busy_msg = $(".loading-wrapper");\
    busy_msg.removeClass("hide");\
    busy_msg[0].scrollIntoView();\
    $(".modal-body").scrollLeft(0);\
    return true;\
  });\
  $().ready(function(){opkgLogFuncID=setInterval(updateAcmeLog,2000);});\
</script>\
');