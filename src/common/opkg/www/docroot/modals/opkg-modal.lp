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

local gVCS = post_helper.getValidateCheckboxSwitch()

local lp = require("web.lp")
lp.setpath("/www/snippets/")

local mapParams = {
  run_update = "rpc.gui.opkg.update",
}
local mapValid = {
  run_update = gVCS,
}
local content = post_helper.handleQuery(mapParams,mapValid)

local opkg_data = {
  customfeeds = "rpc.gui.opkg.customfeeds",
  last_update = "rpc.gui.opkg.last_update",
  log = "rpc.gui.opkg.log",
}
content_helper.getExactContent(opkg_data)

local last_update
if opkg_data["last_update"] == "-1" then
  last_update = T"Not since last reboot"
else
  last_update = T(os.date("%d/%m/%Y %H:%M:%S",tonumber(opkg_data["last_update"])))
end

ngx.print(ui_helper.createHeader(T"Packages",false,true,nil),'\
<div class="modal-body update">\
'); lp.include("tabs-opkg.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/opkg-modal.lp">\
    ',ui_helper.createMessages(message_helper.popMessages()),'\
    <fieldset><legend>',T"Configuration",'</legend> \
      ');
      local html = {}
      if opkg_data["customfeeds"] ~= "" then
        local opkg_pre_attr = {
          span = {
            style = "display:block;white-space:pre;",
          },
        }
        local opkg_log_attr = {
          span = {
            id = "opkg_log",
            style = "display:block;white-space:pre;",
          },
        }
        local opkg_last_update_attr = {
          span = {
            id = "opkg_last_update",
          },
        }
        html[#html+1] = ui_helper.createLabel(T"Custom Feeds",untaint(opkg_data["customfeeds"]),opkg_pre_attr)
        html[#html+1] = ui_helper.createLabel(T"Last Update",last_update,opkg_last_update_attr)
        html[#html+1] = ui_helper.createCheckboxSwitch(T"Update Now","run_update",content["run_update"])
        html[#html+1] = ui_helper.createLabel(T"Log Messages",untaint(opkg_data["log"]),opkg_log_attr)
      else
        local warn_box = {
          alert = {
            class = "alert alert-warn",
            style = "text-align:center",
          }
        }
        html[#html+1] = ui_helper.createAlertBlock(T"No custom feeds configured. You can configure them using de-telstra with the -o option.",warn_box)
        html[#html+1] = ui_helper.createSimpleInputHidden("run_update","_DUMMY_")
      end
      ngx.print(html,'\
    </fieldset>\
  </form>\
</div>\
<script src="/js/modal-busy.js"></script>\
',ui_helper.createFooter(),'\
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
  $().ready(function(){opkgLogFuncID=setInterval(updateAcmeLog,2000);});\
</script>\
');