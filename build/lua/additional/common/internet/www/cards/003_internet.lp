--pretranslated: do not change this file
 
-- Enable localization
gettext.textdomain('webui-core')

local dyntab_helper = require("web.dyntab_helper")
local imh = require("internetmode_helper")

local mode_active = dyntab_helper.process(imh).current.name
if mode_active == "" then
  for _,v in ipairs(imh) do
    if v.default == true then
      mode_active = v.name
      break
    end
  end
end
  
if mode_active ~= "bridge" then
  local content_helper = require("web.content_helper")
  local ich = require("internetcard_helper")
  local ui_helper = require("web.ui_helper")

  local modalPath = nil
  local session = ngx.ctx.session
  if session:hasAccess("/modals/internet-modal.lp") then
    modalPath = "/modals/internet-modal.lp"
  end

  local html = ich.getInternetCardHTML(mode_active)

  ngx.print('\
  <div class="span3">\
    <div class="smallcard">\
      ');  ngx.print(ui_helper.createCardHeader(T"Internet Access", modalPath)); ngx.print('\
      <div class="content card_bg" data-bg-text="&#xf0ac;">\
        <div id="internet-card-content">\
        ');  ngx.print(html); ngx.print('\
        </div>\
      </div>\
    </div>\
  </div>\
  <script>\
  var iFuncID;\
  function updateInternetCard(){\
    $.post("/ajax/internet-status.lua", [tch.elementCSRFtoken()], function(data){\
      $("#internet-card-content").html(data["html"]);\
    }, "json")\
    .fail(function(response){\
      if(response.status==403||response.status==404){clearInterval(iFuncID);}\
    });\
  }\
  $().ready(function(){iFuncID=setInterval(updateInternetCard,20000);window.intervalIDs.push(iFuncID);});\
  </script>\
  '); 
end