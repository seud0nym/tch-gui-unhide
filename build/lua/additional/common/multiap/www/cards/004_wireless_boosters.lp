--pretranslated: do not change this file

-- Enable localization
gettext.textdomain('webui-qos')

local session = ngx.ctx.session
local content_helper = require("web.content_helper")

local content = {
  agent_enabled = "uci.multiap.agent.enabled",
  controller_enabled = "uci.multiap.controller.enabled",
}

content_helper.getExactContent(content)

local modalPath = "/modals/wireless-boosters-modal.lp" -- Need default for 070-Enable-Modals

if content.agent_enabled and content.agent_enabled ~= "" and content.controller_enabled and content.controller_enabled ~= ""  then
  local ui_helper = require("web.ui_helper")
  local booster_helper = require("booster_helper")

  local html, modalPath = booster_helper.getBoosterCardHTML(content.agent_enabled,content.controller_enabled, modalPath)

  ngx.print('\
  <div class="span3">\
    <div class="smallcard">\
        '); ngx.print( ui_helper.createCardHeader(T"Wi-Fi Boosters", modalPath) ); ngx.print('\
      <div id="booster-card-content" class="content card_bg" data-bg-text="&#xf0e8;">\
        ');  ngx.print(html); ngx.print('\
		  </div>\
    </div>\
  </div>\
  <script>\
  var multiapFuncID;\
  function updateBoosterCard(){\
    $.post("/ajax/booster-status.lua", [tch.elementCSRFtoken()], function(data){\
      $("#booster-card-content").html(data["html"]);\
    }, "json")\
    .fail(function(response){\
      if(response.status==403||response.status==404){clearInterval(multiapFuncID);}\
    });\
  }\
  $().ready(function(){multiapFuncID=setInterval(updateBoosterCard,20000);window.intervalIDs.push(multiapFuncID);});\
  </script>\
');
end
  ngx.print('\
');
