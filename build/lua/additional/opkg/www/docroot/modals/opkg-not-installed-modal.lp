--pretranslated: do not change this file

-- Localization
gettext.textdomain('webui-core')

local ui_helper = require("web.ui_helper")
local message_helper = require("web.uimessage_helper")
local match, untaint = string.match, string.untaint

local lp = require("web.lp")
lp.setpath("/www/snippets/")

if ngx.req.get_method() == "POST" then
  local proxy = require("datamodel")
  local action = untaint(ngx.req.get_post_args().action)
  local cmd, pkg = match(action, "(%w+)_(.*)")
  proxy.set("rpc.gui.opkg.available.@".. pkg .. ".action", cmd)
end

ngx.print(ui_helper.createHeader(T"Packages", true, false))   ngx.print('\
<div class="modal-body update">\
'); lp.include("tabs-opkg.lp"); ngx.print('\
  <form class="form-horizontal" method="post" action="modals/opkg-not-installed-modal.lp">\
    '); 
    ngx.print(ui_helper.createMessages(message_helper.popMessages()))
    ngx.print('<legend>');  ngx.print( T"Available Packages" ); ngx.print('</legend>');
    lp.include("opkg-form.lp");
    ngx.print('\
  </form>\
  '); 
  lp.include("opkg-post.lp");
  ngx.print('\
</div>\
');  ngx.print(ui_helper.createFooter()); ngx.print('\
<script>\
  $.post("/ajax/opkg-list.lua?group=available", [tch.elementCSRFtoken()], function(data){\
    $("#opkgs_place_holder").html(data["html"]);\
    $(".btn-Install").on("click", function(){\
      postAction("install",$(this).data("index"),$(this).data("name"));\
    });\
  }, "json")\
  .fail(function(response){\
    if(response.status==403||response.status==404){alert("Error retrieving packages?");}\
  });\
</script>\
')