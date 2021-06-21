--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local content_helper = require("web.content_helper")
local match, untaint = string.match, string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

if ngx.req.get_method() == "POST" then
  local proxy = require("datamodel")
  local install = untaint(ngx.req.get_post_args().install_pkg)
  if install then
    local pkg
    if type(install) == "string" or type(install) == "userdata" then
      install = { install }
    end
    for _,pkg in pairs(install) do
      proxy.set("rpc.gui.opkg.available.@".. untaint(pkg) .. ".action", "install")
    end
  end
end

local opkg_data = {
  last_update = "rpc.gui.opkg.last_update",
}
content_helper.getExactContent(opkg_data)

ngx.print(ui_helper.createHeader(T"Packages", true, false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-opkg.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/opkg-not-installed-modal.lp">\
    '); 
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>');  ngx.print( T"Available Packages" ); ngx.print('</legend>');
    if opkg_data["last_update"] == "-1" then
      local alert_info = {
        alert = {
          class = "alert alert-info"
        },
      }
      ngx.print(ui_helper.createAlertBlock(T"Available package list is unavailable until an update has been executed",alert_info))
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
if opkg_data["last_update"] ~= "-1" then
  ngx.print('\
<script>\
  $.post("/ajax/opkg-list.lua?group=available", [tch.elementCSRFtoken()], function(data){\
    $("#opkgs_place_holder").html(data["html"]);\
    $(".btn-Install").on("click", function(){\
      postAction("install",$(this).data("index"),$(this).data("name"));\
    });\
    $(".opkg_cb").on("change", function(){\
      $("#modal-no-change").addClass("hide");\
      $("#modal-changes").removeClass("hide");\
    });\
  }, "json")\
  .fail(function(response){\
    if(response.status==403||response.status==404){alert("Error retrieving packages?");}\
  });\
</script>\
  ')
end