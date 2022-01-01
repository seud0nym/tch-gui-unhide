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
  local ui_helper = require("web.ui_helper")

  local modalPath = nil
  local session = ngx.ctx.session
  if session:hasAccess("/modals/internet-modal.lp") then
    modalPath = "/modals/internet-modal.lp"
  end

  ngx.print('\
<div class="span3">\
  <div class="smallcard">\
    ');  ngx.print(ui_helper.createCardHeader(T"Internet Access",modalPath)); ngx.print('\
    <div class="content card_bg" data-bg-text="&#xf0ac;">\
      <div id="internet-card-content">\
        '); ngx.print(ui_helper.createSimpleLight("3","Checking connection status...")); ngx.print('\
      </div>\
    </div>\
  </div>\
</div>\
<script src="/js/003_internet.js"></script>\
');
end