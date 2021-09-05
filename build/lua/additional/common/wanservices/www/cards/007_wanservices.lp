--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local proxy = require("datamodel")
local ui_helper = require("web.ui_helper")
local content_helper = require("web.content_helper")
local gmatch, format, lower, match = string.gmatch, string.format, string.lower, string.match

local session = ngx.ctx.session
if session:hasAccess("/modals/wanservices-modal.lp") then
  local wanservicescard_helper = require("wanservicescard_helper")
  local html = wanservicescard_helper.getWANServicesCardHTML()

  ngx.print('\
  <div class="span3">\
    <div class="smallcard">\
      ');  ngx.print(ui_helper.createCardHeader(T"WAN Services", "modals/wanservices-modal.lp")); ngx.print('\
      <div id="wanservices-card-content" class="content card_bg" data-bg-text="&#xf0c2;">\
      ');  ngx.print(html); ngx.print('\
      </div>\
    </div>\
  </div>\
  <script>\
  var wanservicesFuncID;\
  function updateWANServicesCard(){\
    $.post("/ajax/wanservices-status.lua", [tch.elementCSRFtoken()], function(data){\
      $("#wanservices-card-content").html(data["html"]);\
    }, "json")\
    .fail(function(response){\
      if(response.status==403||response.status==404){clearInterval(wanservicesFuncID);}\
    });\
  }\
  $().ready(function(){wanservicesFuncID=setInterval(updateWANServicesCard,21000);window.intervalIDs.push(wanservicesFuncID);});\
  </script>\
  ');  
end
ngx.print('\
'); 