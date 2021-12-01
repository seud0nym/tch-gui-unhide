--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
---@diagnostic disable-next-line: undefined-field
local untaint = string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

if ngx.req.get_method() == "POST" then
  local proxy = require("datamodel")
  local upgrade = ngx.req.get_post_args().upgrade_pkg
  local remove = ngx.req.get_post_args().remove_pkg
  if upgrade then
    if type(upgrade) == "string" or type(upgrade) == "userdata" then
      upgrade = { upgrade }
    end
    for _,pkg in pairs(upgrade) do
      proxy.set("rpc.gui.opkg.user.@"..untaint(pkg)..".action","upgrade")
    end
  end
  if remove then
    if type(remove) == "string" or type(remove) == "userdata" then
      remove = { remove }
    end
    for _,pkg in pairs(remove) do
      proxy.set("rpc.gui.opkg.user.@"..untaint(pkg)..".action","remove")
    end
  end
end

local opkg_data = {
  user_installed = "rpc.gui.opkg.userNumberOfEntries",
}
content_helper.getExactContent(opkg_data)

ngx.print(ui_helper.createHeader(T"Packages",true,false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-opkg.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/opkg-user-installed-modal.lp">\
    '); 
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>');  ngx.print( T"User Installed Packages" ); ngx.print('</legend>');
    if opkg_data["user_installed"] == "0" then
      local alert_info = {
        alert = {
          class = "alert alert-info"
        },
      }
      ngx.print(ui_helper.createAlertBlock(T"No user-installed packages found",alert_info))
    else 
      lp.include("opkg-form.lp");
    end
    ngx.print('\
  </form>\
  '); 
  lp.include("opkg-post.lp");
  ngx.print('\
</div>\
');  ngx.print(ui_helper.createFooter()); 
if opkg_data["user_installed"] ~= "0" then
  ngx.print('\
<script>\
  $.post("/ajax/opkg-list.lua?group=user",[tch.elementCSRFtoken()],function(data){\
    $("#opkgs_place_holder").html(data["html"]);\
    $("#searchControlGroup").removeClass("hide");\
    $(".btn-Remove").on("click",function(){\
      postAction("remove",$(this).data("index"),$(this).data("name"));\
    });\
    $(".btn-Upgrade").on("click",function(){\
      postAction("upgrade",$(this).data("index"),$(this).data("name"));\
    });\
    $(".opkg_cb").on("change",function(){\
      $("#modal-no-change").addClass("hide");\
      $("#modal-changes").removeClass("hide");\
    });\
  },"json")\
  .fail(function(response){\
    if(response.status==403||response.status==404){alert("Error retrieving packages?");}\
  });\
</script>\
  ')
end