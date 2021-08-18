--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local adblock_helper = require("adblock_helper")
local ui_helper = require("web.ui_helper")
local session = ngx.ctx.session
local modalPath = "/modals/adblck-config-modal.lp"

if session:hasAccess(modalPath) then
  local html = adblock_helper.getAdblockCardHTML()
  ngx.print('\
  <div class = "span3">\
    <div class = "smallcard">\
      '); ngx.print(ui_helper.createCardHeader(T"Adblock",modalPath)); ngx.print('\
      <div class="content card_bg adblock-card-content" data-bg-text="&#xf0b0;";">\
        ');
        ngx.print(html);
        ngx.print('\
      </div>\
    </div>\
  </div>\
  <script>\
  var adbFuncID;\
  function updateAdblockCard(){\
    $.post("/ajax/adblck-status.lua", [tch.elementCSRFtoken()], function(data){\
      $(".adblock-card-content").html(data["html"]);\
    }, "json")\
    .fail(function(response){\
      if(response.status==403||response.status==404){clearInterval(adbFuncID);}\
    });\
  }\
  $().ready(function(){adbFuncID=setInterval(updateAdblockCard,28000);});\
  </script>\
');
end
