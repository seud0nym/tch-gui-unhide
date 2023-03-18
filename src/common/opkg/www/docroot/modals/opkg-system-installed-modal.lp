--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local match = string.match
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

if ngx.req.get_method() == "POST" then
  local proxy = require("datamodel")
  local action = untaint(ngx.req.get_post_args().action)
  local cmd,pkg = match(action,"(%w+)_(.*)")
  proxy.set("rpc.gui.opkg.system.@"..pkg..".action",cmd)
end

ngx.print(ui_helper.createHeader(T"Packages",true,false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-opkg.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/opkg-system-installed-modal.lp">\
    ');
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>');  ngx.print( T"System Installed Packages" ); ngx.print('</legend>');
    local alert_warn = {
      alert = {
        class = "alert alert-warn"
      },
    }
    ngx.print(ui_helper.createAlertBlock(T"WARNING! Removing or upgrading system packages is NOT recommended. You may end up with an unusable system!",alert_warn))
    lp.include("opkg-form.lp");
    ngx.print('\
  </form>\
  <div class="loading-wrapper hide"><img src="/img/spinner.gif" /></div>\
  <script>\
    function postAction(action,idx,name) {\
      if (confirm("Are you really, really, really sure that you want to "+action+" "+name+"?")) {\
        var busy_msg = $(".loading-wrapper");\
        busy_msg.removeClass("hide");\
        busy_msg[0].scrollIntoView();\
        $(".modal-body").scrollLeft(0);\
        var t = $(".modal form"),e = t.serializeArray();\
        e.push({\
          name: "action",\
          value: action+"_"+idx\
        },{\
          name: "CSRFtoken",\
          value: $("meta[name=CSRFtoken]").attr("content")\
        });\
        tch.loadModal(t.attr("action"),e)\
      }\
    }\
  </script>\
  ');
  ngx.print('\
</div>\
');  ngx.print(ui_helper.createFooter()); ngx.print('\
<script>\
  $.post("/ajax/opkg-list.lua?group=system",[tch.elementCSRFtoken()],function(data){\
    $("#opkgs_place_holder").html(data["html"]);\
    $("#searchControlGroup").removeClass("hide");\
    $(".btn-Upgrade").on("click",function(){\
      postAction("upgrade",$(this).data("index"),$(this).data("name"));\
    });\
    $(".btn-Remove").on("click",function(){\
      postAction("remove",$(this).data("index"),$(this).data("name"));\
    });\
  },"json")\
  .fail(function(response){\
    if(response.status==403||response.status==404){alert("Error retrieving packages?");}\
  });\
</script>\
')