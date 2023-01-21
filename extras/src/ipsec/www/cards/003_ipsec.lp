--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-core')

local ipsec_helper = require("ipsec_helper")
local ui_helper = require("web.ui_helper")
local session = ngx.ctx.session
local modalPath = "/modals/ipsec-modal.lp"

if session:hasAccess(modalPath) then
  local html = ipsec_helper.getIPsecCardHTML()
  ngx.print('\
  <div class = "span3">\
    <div class = "smallcard">\
      ',ui_helper.createCardHeader(T"IPsec VPN",modalPath,nil,nil,{icon={class="icon-filter"}}),'\
      <div class="content card_bg ipsec-card-content" data-bg-text="&#xf0b0;">\
        ',html,'\
      </div>\
    </div>\
  </div>\
  <script>\
  var ipsecFuncID;\
  function updateIPsecCard(){\
    $.post("/ajax/ipsec-status.lua", [tch.elementCSRFtoken()], function(data){\
      $(".ipsec-card-content").html(data["html"]);\
    }, "json")\
    .fail(function(response){\
      if(response.status==403||response.status==404){clearInterval(ipsecFuncID);}\
    });\
  }\
  $().ready(function(){ipsecFuncID=setInterval(updateIPsecCard,31000);addRegisteredInterval(ipsecFuncID);});\
  </script>\
');
end
